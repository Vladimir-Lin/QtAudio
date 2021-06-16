/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/24                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CaALSA.hpp"

#warning the underrun issue happens when the buffer is bigger than feeding frames
#warning this requires a major changes to fix this problem, we will do this next version

/** Local error handler function type */
typedef void (*snd_local_error_handler_t)(const char *file, int line,
                      const char *func, int err,
                      const char *fmt, va_list arg);

snd_local_error_handler_t snd_lib_error_set_local(snd_local_error_handler_t func);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#define CA_MIN(x,y) ( (x) < (y) ? (x) : (y) )
#define CA_MAX(x,y) ( (x) > (y) ? (x) : (y) )

/* Add missing define (for compatibility with older ALSA versions) */
#ifndef SND_PCM_TSTAMP_ENABLE
  #define SND_PCM_TSTAMP_ENABLE SND_PCM_TSTAMP_MMAP
#endif

/* Combine version elements into a single (unsigned) integer */
#define ALSA_VERSION_INT(major, minor, subminor)  ((major << 16) | (minor << 8) | subminor)

/* The acceptable tolerance of sample rate set, to that requested (as a ratio, eg 50 is 2%, 100 is 1%) */
#define RATE_MAX_DEVIATE_RATIO 100

/* Utilize GCC branch prediction for error tests */
#if defined __GNUC__ && __GNUC__ >= 3
#define UNLIKELY(expr) __builtin_expect( (expr), 0 )
#else
#define UNLIKELY(expr) (expr)
#endif

#define STRINGIZE_HELPER(expr) #expr
#define STRINGIZE(expr) STRINGIZE_HELPER(expr)

/* Check return value of ALSA function, and map it to PaError */
#ifndef REMOVE_DEBUG_MESSAGE
#define ENSURE_(expr, code) \
    do { \
        int __ca_unsure_error_id;\
        if( UNLIKELY( (__ca_unsure_error_id = (expr)) < 0 ) ) \
        { \
            if( (code) == UnanticipatedHostError && pthread_equal( pthread_self(), caUnixMainThread) ) \
            { \
                SetLastHostErrorInfo( ALSA, __ca_unsure_error_id, alsa_snd_strerror( __ca_unsure_error_id ) ); \
            } \
            if ( NULL != globalDebugger ) { \
              globalDebugger -> printf ( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ) ; \
            } ; \
            if ( (code) == UnanticipatedHostError ) { \
              if ( NULL != globalDebugger ) { \
                globalDebugger -> printf ( "Host error description: %s\n", alsa_snd_strerror( __ca_unsure_error_id ) ) ; \
              } ; \
            } ; \
            result = (code); \
            goto error; \
        } \
    } while (0)
#else
#define ENSURE_(expr, code) \
    do { \
        int __ca_unsure_error_id;\
        if( UNLIKELY( (__ca_unsure_error_id = (expr)) < 0 ) ) \
        { \
            if( (code) == UnanticipatedHostError && pthread_equal( pthread_self(), caUnixMainThread) ) \
            { \
                SetLastHostErrorInfo( ALSA, __ca_unsure_error_id, alsa_snd_strerror( __ca_unsure_error_id ) ); \
            } \
            result = (code); \
            goto error; \
        } \
    } while (0)
#endif

#ifndef REMOVE_DEBUG_MESSAGE
#define CA_UNLESS(expr, code) \
        if( UNLIKELY( (expr) == 0 ) ) \
        { \
            if ( NULL != globalDebugger ) { \
              globalDebugger->printf ( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ) ; \
            } ; \
            result = (code); \
            goto error; \
        }
#else
#define CA_UNLESS(expr, code) \
        if( UNLIKELY( (expr) == 0 ) ) \
        { \
            result = (code); \
            goto error; \
        }
#endif

/* Used with CA_ENSURE */
static int caUtilErr_ ;

/* Check CaError */
#ifndef REMOVE_DEBUG_MESSAGE
#define CA_ENSURE(expr)                                  \
    if ( UNLIKELY( (caUtilErr_ = (expr)) < NoError ) ) { \
      if ( NULL != globalDebugger )                    { \
        globalDebugger -> printf                       ( \
          "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ) ; \
      }                                                ; \
      result = caUtilErr_                              ; \
      goto error                                       ; \
    }
#else
#define CA_ENSURE(expr)                                  \
    if ( UNLIKELY( (caUtilErr_ = (expr)) < NoError ) ) { \
      result = caUtilErr_                              ; \
      goto error                                       ; \
    }
#endif

#define CA_ASSERT_CALL(expr, success) caUtilErr_ = (expr) ;

#ifndef REMOVE_DEBUG_MESSAGE
#define CA_ENSURE_SYSTEM(expr, success)                                      \
  do                                                                       { \
    if ( UNLIKELY( (caUtilErr_ = (expr)) != success ) )                    { \
      if ( pthread_equal(pthread_self(), caUnixMainThread) )               { \
        SetLastHostErrorInfo( ALSA, caUtilErr_, strerror( caUtilErr_ ) )   ; \
      }                                                                      \
      if ( NULL != globalDebugger )                                        { \
        globalDebugger->printf ( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ); \
      }                                                                    ; \
      result = UnanticipatedHostError                                      ; \
      goto error                                                           ; \
    }                                                                        \
  } while ( 0 )
#else
#define CA_ENSURE_SYSTEM(expr, success)                                      \
  do                                                                       { \
    if ( UNLIKELY( (caUtilErr_ = (expr)) != success ) )                    { \
      if ( pthread_equal(pthread_self(), caUnixMainThread) )               { \
        SetLastHostErrorInfo( ALSA, caUtilErr_, strerror( caUtilErr_ ) )   ; \
      }                                                                      \
      result = UnanticipatedHostError                                      ; \
      goto error                                                           ; \
    }                                                                        \
  } while ( 0 )
#endif

///////////////////////////////////////////////////////////////////////////////

/* Defines Alsa function types and pointers to these functions. */
#define _CA_DEFINE_FUNC(x)  typedef typeof(x) x##_ft; static x##_ft *alsa_##x = 0
//#define _CA_DEFINE_FUNC(x)  typedef typeof(x) x##_t; static x##_t *alsa_##x = 0

/* Alloca helper. */
#define __alsa_snd_alloca(ptr,type) do { size_t __alsa_alloca_size = alsa_##type##_sizeof(); (*ptr) = (type##_t *) alloca(__alsa_alloca_size); memset(*ptr, 0, __alsa_alloca_size); } while (0)

_CA_DEFINE_FUNC(snd_pcm_open);
_CA_DEFINE_FUNC(snd_pcm_close);
_CA_DEFINE_FUNC(snd_pcm_nonblock);
_CA_DEFINE_FUNC(snd_pcm_frames_to_bytes);
_CA_DEFINE_FUNC(snd_pcm_prepare);
_CA_DEFINE_FUNC(snd_pcm_start);
_CA_DEFINE_FUNC(snd_pcm_resume);
_CA_DEFINE_FUNC(snd_pcm_wait);
_CA_DEFINE_FUNC(snd_pcm_state);
_CA_DEFINE_FUNC(snd_pcm_avail_update);
_CA_DEFINE_FUNC(snd_pcm_areas_silence);
_CA_DEFINE_FUNC(snd_pcm_mmap_begin);
_CA_DEFINE_FUNC(snd_pcm_mmap_commit);
_CA_DEFINE_FUNC(snd_pcm_readi);
_CA_DEFINE_FUNC(snd_pcm_readn);
_CA_DEFINE_FUNC(snd_pcm_writei);
_CA_DEFINE_FUNC(snd_pcm_writen);
_CA_DEFINE_FUNC(snd_pcm_drain);
_CA_DEFINE_FUNC(snd_pcm_recover);
_CA_DEFINE_FUNC(snd_pcm_drop);
_CA_DEFINE_FUNC(snd_pcm_area_copy);
_CA_DEFINE_FUNC(snd_pcm_poll_descriptors);
_CA_DEFINE_FUNC(snd_pcm_poll_descriptors_count);
_CA_DEFINE_FUNC(snd_pcm_poll_descriptors_revents);
_CA_DEFINE_FUNC(snd_pcm_format_size);
_CA_DEFINE_FUNC(snd_pcm_link);
_CA_DEFINE_FUNC(snd_pcm_delay);
_CA_DEFINE_FUNC(snd_pcm_hw_params_sizeof);
_CA_DEFINE_FUNC(snd_pcm_hw_params_malloc);
_CA_DEFINE_FUNC(snd_pcm_hw_params_free);
_CA_DEFINE_FUNC(snd_pcm_hw_params_any);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_access);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_format);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_channels);
//_CA_DEFINE_FUNC(snd_pcm_hw_params_set_periods_near);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_rate_near); //!!!
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_rate);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_rate_resample);
//_CA_DEFINE_FUNC(snd_pcm_hw_params_set_buffer_time_near);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_buffer_size);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_buffer_size_near); //!!!
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_buffer_size_min);
//_CA_DEFINE_FUNC(snd_pcm_hw_params_set_period_time_near);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_period_size_near);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_periods_integer);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_periods_min);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_buffer_size);
//_CA_DEFINE_FUNC(snd_pcm_hw_params_get_period_size);
//_CA_DEFINE_FUNC(snd_pcm_hw_params_get_access);
//_CA_DEFINE_FUNC(snd_pcm_hw_params_get_periods);
//_CA_DEFINE_FUNC(snd_pcm_hw_params_get_rate);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_channels_min);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_channels_max);
_CA_DEFINE_FUNC(snd_pcm_hw_params_test_period_size);
_CA_DEFINE_FUNC(snd_pcm_hw_params_test_format);
_CA_DEFINE_FUNC(snd_pcm_hw_params_test_access);
_CA_DEFINE_FUNC(snd_pcm_hw_params_dump);
_CA_DEFINE_FUNC(snd_pcm_hw_params);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_periods_min);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_periods_max);
_CA_DEFINE_FUNC(snd_pcm_hw_params_set_period_size);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_period_size_min);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_period_size_max);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_buffer_size_max);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_rate_min);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_rate_max);
_CA_DEFINE_FUNC(snd_pcm_hw_params_get_rate_numden);

#define alsa_snd_pcm_hw_params_alloca(ptr) __alsa_snd_alloca(ptr, snd_pcm_hw_params)

_CA_DEFINE_FUNC(snd_pcm_sw_params_sizeof);
_CA_DEFINE_FUNC(snd_pcm_sw_params_malloc);
_CA_DEFINE_FUNC(snd_pcm_sw_params_current);
_CA_DEFINE_FUNC(snd_pcm_sw_params_set_avail_min);
_CA_DEFINE_FUNC(snd_pcm_sw_params);
_CA_DEFINE_FUNC(snd_pcm_sw_params_free);
_CA_DEFINE_FUNC(snd_pcm_sw_params_set_start_threshold);
_CA_DEFINE_FUNC(snd_pcm_sw_params_set_stop_threshold);
_CA_DEFINE_FUNC(snd_pcm_sw_params_get_boundary);
_CA_DEFINE_FUNC(snd_pcm_sw_params_set_silence_threshold);
_CA_DEFINE_FUNC(snd_pcm_sw_params_set_silence_size);
_CA_DEFINE_FUNC(snd_pcm_sw_params_set_xfer_align);
_CA_DEFINE_FUNC(snd_pcm_sw_params_set_tstamp_mode);

#define alsa_snd_pcm_sw_params_alloca(ptr) __alsa_snd_alloca(ptr, snd_pcm_sw_params)

_CA_DEFINE_FUNC(snd_pcm_info);
_CA_DEFINE_FUNC(snd_pcm_info_sizeof);
_CA_DEFINE_FUNC(snd_pcm_info_malloc);
_CA_DEFINE_FUNC(snd_pcm_info_free);
_CA_DEFINE_FUNC(snd_pcm_info_set_device);
_CA_DEFINE_FUNC(snd_pcm_info_set_subdevice);
_CA_DEFINE_FUNC(snd_pcm_info_set_stream);
_CA_DEFINE_FUNC(snd_pcm_info_get_name);
_CA_DEFINE_FUNC(snd_pcm_info_get_card);

#define alsa_snd_pcm_info_alloca(ptr) __alsa_snd_alloca(ptr, snd_pcm_info)

_CA_DEFINE_FUNC(snd_ctl_pcm_next_device);
_CA_DEFINE_FUNC(snd_ctl_pcm_info);
_CA_DEFINE_FUNC(snd_ctl_open);
_CA_DEFINE_FUNC(snd_ctl_close);
_CA_DEFINE_FUNC(snd_ctl_card_info_malloc);
_CA_DEFINE_FUNC(snd_ctl_card_info_free);
_CA_DEFINE_FUNC(snd_ctl_card_info);
_CA_DEFINE_FUNC(snd_ctl_card_info_sizeof);
_CA_DEFINE_FUNC(snd_ctl_card_info_get_name);

#define alsa_snd_ctl_card_info_alloca(ptr) __alsa_snd_alloca(ptr, snd_ctl_card_info)

_CA_DEFINE_FUNC(snd_config);
_CA_DEFINE_FUNC(snd_config_update);
_CA_DEFINE_FUNC(snd_config_search);
_CA_DEFINE_FUNC(snd_config_iterator_entry);
_CA_DEFINE_FUNC(snd_config_iterator_first);
_CA_DEFINE_FUNC(snd_config_iterator_end);
_CA_DEFINE_FUNC(snd_config_iterator_next);
_CA_DEFINE_FUNC(snd_config_get_string);
_CA_DEFINE_FUNC(snd_config_get_id);
_CA_DEFINE_FUNC(snd_config_update_free_global);

_CA_DEFINE_FUNC(snd_pcm_status);
_CA_DEFINE_FUNC(snd_pcm_status_sizeof);
_CA_DEFINE_FUNC(snd_pcm_status_get_tstamp);
_CA_DEFINE_FUNC(snd_pcm_status_get_state);
_CA_DEFINE_FUNC(snd_pcm_status_get_trigger_tstamp);
_CA_DEFINE_FUNC(snd_pcm_status_get_delay);

_CA_DEFINE_FUNC(snd_lib_error_set_local);

#define alsa_snd_pcm_status_alloca(ptr) __alsa_snd_alloca(ptr, snd_pcm_status)

_CA_DEFINE_FUNC(snd_card_next);
_CA_DEFINE_FUNC(snd_asoundlib_version);
_CA_DEFINE_FUNC(snd_strerror);
_CA_DEFINE_FUNC(snd_output_stdio_attach);

#define alsa_snd_config_for_each(pos, next, node)\
    for (pos = alsa_snd_config_iterator_first(node),\
         next = alsa_snd_config_iterator_next(pos);\
         pos != alsa_snd_config_iterator_end(node); pos = next, next = alsa_snd_config_iterator_next(pos))

#undef _CA_DEFINE_FUNC

///////////////////////////////////////////////////////////////////////////////

/* Redefine 'CA_ALSA_PATHNAME' to a different Alsa library name if desired. */

#ifndef CA_ALSA_PATHNAME
  #define CA_ALSA_PATHNAME "libasound.so"
#endif

static const char * g_AlsaLibName = CA_ALSA_PATHNAME ;
/* Handle to dynamically loaded library. */
static void       * g_AlsaLib     = NULL             ;

///////////////////////////////////////////////////////////////////////////////

static int numPeriods_  =   4 ;
static int busyRetries_ = 100 ;

int SetNumPeriods(int numPeriods)
{
  numPeriods_ = numPeriods ;
  return 0                 ;
}

static int SetRetriesBusy(int retries)
{
  busyRetries_ = retries ;
  return 0               ;
}

///////////////////////////////////////////////////////////////////////////////

HwDevInfo predefinedNames[] = {
  { "center_lfe"                              , NULL, 0, 1, 0 } ,
/*  { "default"                                 , NULL, 0, 1, 1 }, */
  { "dmix"                                    , NULL, 0, 1, 0 } ,
/*  { "dpl"                                     , NULL, 0, 1, 0 }, */
/*  { "dsnoop"                                  , NULL, 0, 0, 1 }, */
  { "front"                                   , NULL, 0, 1, 0 } ,
  { "iec958"                                  , NULL, 0, 1, 0 } ,
/*  { "modem"                                   , NULL, 0, 1, 0 }, */
  { "rear"                                    , NULL, 0, 1, 0 } ,
  { "side"                                    , NULL, 0, 1, 0 } ,
/*  { "spdif"                                   , NULL, 0, 0, 0 }, */
  { "surround40"                              , NULL, 0, 1, 0 } ,
  { "surround41"                              , NULL, 0, 1, 0 } ,
  { "surround50"                              , NULL, 0, 1, 0 } ,
  { "surround51"                              , NULL, 0, 1, 0 } ,
  { "surround71"                              , NULL, 0, 1, 0 } ,
  { "AndroidPlayback_Earpiece_normal"         , NULL, 0, 1, 0 } ,
  { "AndroidPlayback_Speaker_normal"          , NULL, 0, 1, 0 } ,
  { "AndroidPlayback_Bluetooth_normal"        , NULL, 0, 1, 0 } ,
  { "AndroidPlayback_Headset_normal"          , NULL, 0, 1, 0 } ,
  { "AndroidPlayback_Speaker_Headset_normal"  , NULL, 0, 1, 0 } ,
  { "AndroidPlayback_Bluetooth-A2DP_normal"   , NULL, 0, 1, 0 } ,
  { "AndroidPlayback_ExtraDockSpeaker_normal" , NULL, 0, 1, 0 } ,
  { "AndroidPlayback_TvOut_normal",             NULL, 0, 1, 0 } ,
  { "AndroidRecord_Microphone"                , NULL, 0, 0, 1 } ,
  { "AndroidRecord_Earpiece_normal"           , NULL, 0, 0, 1 } ,
  { "AndroidRecord_Speaker_normal"            , NULL, 0, 0, 1 } ,
  { "AndroidRecord_Headset_normal"            , NULL, 0, 0, 1 } ,
  { "AndroidRecord_Bluetooth_normal"          , NULL, 0, 0, 1 } ,
  { "AndroidRecord_Speaker_Headset_normal"    , NULL, 0, 0, 1 } ,
  { NULL                                      , NULL, 0, 1, 0 }
};

///////////////////////////////////////////////////////////////////////////////

static const HwDevInfo * FindDeviceName(const char * name)
{
  int i                                                       ;
  for ( i = 0; predefinedNames[i].alsaName; i++ )             {
    if ( 0 == ::strcmp( name, predefinedNames[i].alsaName ) ) {
      return &predefinedNames[i]                              ;
    }                                                         ;
  }                                                           ;
  return NULL                                                 ;
}

///////////////////////////////////////////////////////////////////////////////

/* Extract buffer from channel area */
static unsigned char * ExtractAddress                          (
                         const snd_pcm_channel_area_t * area   ,
                               snd_pcm_uframes_t        offset )
{
  return (unsigned char *) area->addr + ( area->first + offset * area->step ) / 8 ;
}

///////////////////////////////////////////////////////////////////////////////

/* Return exact sample rate in param sampleRate */

static int GetExactSampleRate( snd_pcm_hw_params_t *hwParams, double *sampleRate )
{
  unsigned int num                                                           ;
  unsigned int den = 1                                                       ;
  int          err                                                           ;
  ////////////////////////////////////////////////////////////////////////////
  err = alsa_snd_pcm_hw_params_get_rate_numden ( hwParams , &num , &den )    ;
  *sampleRate = (double) num / den                                           ;
  return err                                                                 ;
}

///////////////////////////////////////////////////////////////////////////////

/* Retrieve the version of the runtime Alsa-lib, as a single number equivalent to
 * SND_LIB_VERSION.  Only a version string is available ("a.b.c") so this has to be converted.
 * Assume 'a' and 'b' are single digits only.
 */

static unsigned int AlsaVersionNum(void)
{
  char       * verStr                            ;
  unsigned int verNum                            ;
  verStr = (char *) alsa_snd_asoundlib_version() ;
  verNum = ALSA_VERSION_INT( atoi(verStr    )    ,
                             atoi(verStr + 2)    ,
                             atoi(verStr + 4)  ) ;
  return verNum                                  ;
}

///////////////////////////////////////////////////////////////////////////////

/* Disregard some standard plugins */

static const char * ignoredPlugins[] = {
                      "hw"             ,
                      "plughw"         ,
                      "plug"           ,
                      "dsnoop"         ,
                      "tee"            ,
                      "file"           ,
                      "null"           ,
                      "shm"            ,
                      "cards"          ,
                      "rate_convert"   ,
                      #ifndef ENABLE_ALSA_BLUETOOTH
                      "bluetooth"      ,
                      "rawbluetooth"   ,
                      #endif
                      NULL           } ;

static int IgnorePlugin(const char * pluginId)
{
  int i = 0                                           ;
  while ( ignoredPlugins[i] )                         {
    if ( ! ::strcmp ( pluginId, ignoredPlugins[i] ) ) {
      return 1                                        ;
    }                                                 ;
    ++i                                               ;
  }                                                   ;
  return 0                                            ;
}

///////////////////////////////////////////////////////////////////////////////

/* Skip past parts at the beginning of a (pcm) info name that are already in the card name, to avoid duplication */

static char * SkipCardDetailsInName( char *infoSkipName, char *cardRefName )
{
    char *lastSpacePosn = infoSkipName;

    /* Skip matching chars; but only in chunks separated by ' ' (not part words etc), so track lastSpacePosn */
    while( *cardRefName )
    {
        while( *infoSkipName && *cardRefName && *infoSkipName == *cardRefName)
        {
            infoSkipName++;
            cardRefName++;
            if( *infoSkipName == ' ' || *infoSkipName == '\0' )
                lastSpacePosn = infoSkipName;
        }
        infoSkipName = lastSpacePosn;
        /* Look for another chunk; post-increment means ends pointing to next char */
        while( *cardRefName && ( *cardRefName++ != ' ' ));
    }
    if( *infoSkipName == '\0' )
        return "-"; /* The 2 names were identical; instead of a nul-string, return a marker string */

    /* Now want to move to the first char after any spaces */
    while( *lastSpacePosn && *lastSpacePosn == ' ' )
        lastSpacePosn++;
    /* Skip a single separator char if present in the remaining pcm name; (pa will add its own) */
    if(( *lastSpacePosn == '-' || *lastSpacePosn == ':' ) && *(lastSpacePosn + 1) == ' ' )
        lastSpacePosn += 2;

    return lastSpacePosn;
}

///////////////////////////////////////////////////////////////////////////////

#define _CA_LOCAL_IMPL(x) __ca_local_##x

int _CA_LOCAL_IMPL(snd_pcm_hw_params_set_rate_near) (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    int ret;

    if(( ret = alsa_snd_pcm_hw_params_set_rate(pcm, params, (*val), (*dir)) ) < 0 )
        return ret;

    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_set_buffer_size_near) (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
    int ret;

    if(( ret = alsa_snd_pcm_hw_params_set_buffer_size(pcm, params, (*val)) ) < 0 )
        return ret;

    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_set_period_size_near) (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
{
    int ret;

    if(( ret = alsa_snd_pcm_hw_params_set_period_size(pcm, params, (*val), (*dir)) ) < 0 )
        return ret;

    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_get_channels_min) (const snd_pcm_hw_params_t *params, unsigned int *val)
{
    (*val) = 1;
    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_get_channels_max) (const snd_pcm_hw_params_t *params, unsigned int *val)
{
    (*val) = 2;
    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_get_periods_min) (const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (*val) = 2;
    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_get_periods_max) (const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (*val) = 8;
    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_get_period_size_min) (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir)
{
    (*frames) = 64;
    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_get_period_size_max) (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir)
{
    (*frames) = 512;
    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_get_buffer_size_max) (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
    int ret;
    int dir                = 0;
    snd_pcm_uframes_t pmax = 0;
    unsigned int      pcnt = 0;

    if(( ret = _CA_LOCAL_IMPL(snd_pcm_hw_params_get_period_size_max)(params, &pmax, &dir) ) < 0 )
        return ret;
    if(( ret = _CA_LOCAL_IMPL(snd_pcm_hw_params_get_periods_max)(params, &pcnt, &dir) ) < 0 )
        return ret;

    (*val) = pmax * pcnt;
    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_get_rate_min) (const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (*val) = 44100;
    return 0;
}

int _CA_LOCAL_IMPL(snd_pcm_hw_params_get_rate_max) (const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (*val) = 44100;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

void AlsaCiosErrorHandler (
       const char * file  ,
       int          line  ,
       const char * func  ,
       int          err   ,
       const char * fmt   ,
       va_list      arg   )
{
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( CaNotNull ( globalDebugger ) )    {
    char msg[4096]                       ;
    vsnprintf ( msg , 4000 , fmt , arg ) ;
    gPrintf   ( ( "CIOS/ALSA\t : %s\n"
                  "\tLine     : %d\n"
                  "\tFunction : %s\n"
                  "\tCode     : %d\n"
                  "\t[%s]\n"             ,
                  file                   ,
                  line                   ,
                  func                   ,
                  err                    ,
                  msg                ) ) ;
  }                                      ;
  #endif
}

//////////////////////////////////////////////////////////////////////////////

static int AlsaLoadLibrary(void)
{
  ::dlerror()                                                                ;
  g_AlsaLib = ::dlopen(g_AlsaLibName,(RTLD_NOW|RTLD_GLOBAL))                 ;
  if (g_AlsaLib == NULL)                                                     {
    gPrintf                                                                ( (
      "%s: failed dlopen() ALSA library file - %s, error: %s\n"              ,
      __FUNCTION__                                                           ,
      g_AlsaLibName                                                          ,
      dlerror()                                                          ) ) ;
    return 0                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  #define _CA_LOAD_FUNC(x,t) alsa_##x = (t)dlsym( g_AlsaLib, #x )
  #else
  #define _CA_LOAD_FUNC(x,t) do                                            { \
    alsa_##x = (t)dlsym( g_AlsaLib, #x )                                   ; \
    if ( ( NULL == alsa_##x ) && ( NULL != globalDebugger ) )              { \
      globalDebugger -> printf                                             ( \
        "%s: symbol [%s] not found in - %s, error: %s\n"                   , \
        __FUNCTION__                                                       , \
        #x                                                                 , \
        g_AlsaLibName                                                      , \
        dlerror()                                                        ) ; \
    }                                                                        \
  } while ( 0 )
  #endif

    _CA_LOAD_FUNC(snd_pcm_open,int(*)(snd_pcm_t**,const char*,snd_pcm_stream_t,int)) ;
    _CA_LOAD_FUNC(snd_pcm_close,int(*)(snd_pcm_t*));
    _CA_LOAD_FUNC(snd_pcm_nonblock,int(*)(snd_pcm_t*,int));
    _CA_LOAD_FUNC(snd_pcm_frames_to_bytes,ssize_t(*)(snd_pcm_t*,snd_pcm_sframes_t));
    _CA_LOAD_FUNC(snd_pcm_prepare,int(*)(snd_pcm_t*));
    _CA_LOAD_FUNC(snd_pcm_start,int(*)(snd_pcm_t*));
    _CA_LOAD_FUNC(snd_pcm_resume,int(*)(snd_pcm_t*));
    _CA_LOAD_FUNC(snd_pcm_wait,int(*)(snd_pcm_t*,int));
    _CA_LOAD_FUNC(snd_pcm_state,snd_pcm_state_t(*)(snd_pcm_t*));
    _CA_LOAD_FUNC(snd_pcm_avail_update,snd_pcm_sframes_t(*)(snd_pcm_t*));
    _CA_LOAD_FUNC(snd_pcm_areas_silence,int(*)(const snd_pcm_channel_area_t *, snd_pcm_uframes_t,unsigned int, snd_pcm_uframes_t, snd_pcm_format_t));
    _CA_LOAD_FUNC(snd_pcm_mmap_begin,int(*)(snd_pcm_t *,const snd_pcm_channel_area_t **,snd_pcm_uframes_t *,snd_pcm_uframes_t *));
    _CA_LOAD_FUNC(snd_pcm_mmap_commit,snd_pcm_sframes_t(*)(snd_pcm_t *,snd_pcm_uframes_t,snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_readi,snd_pcm_sframes_t(*)(snd_pcm_t *, void *, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_readn,snd_pcm_sframes_t(*)(snd_pcm_t *, void **, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_writei,snd_pcm_sframes_t(*)(snd_pcm_t *, const void *,snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_writen,snd_pcm_sframes_t(*)(snd_pcm_t *, void **, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_drain,int(*)(snd_pcm_t*));
    _CA_LOAD_FUNC(snd_pcm_recover,int(*)(snd_pcm_t *, int, int));
    _CA_LOAD_FUNC(snd_pcm_drop,int(*)(snd_pcm_t*));
    _CA_LOAD_FUNC(snd_pcm_area_copy,int(*)(const snd_pcm_channel_area_t *, snd_pcm_uframes_t,const snd_pcm_channel_area_t *, snd_pcm_uframes_t,unsigned int, snd_pcm_format_t));
    _CA_LOAD_FUNC(snd_pcm_poll_descriptors,int(*)(snd_pcm_t *, struct pollfd *, unsigned int));
    _CA_LOAD_FUNC(snd_pcm_poll_descriptors_count,int(*)(snd_pcm_t *));
    _CA_LOAD_FUNC(snd_pcm_poll_descriptors_revents,int(*)(snd_pcm_t *, struct pollfd *, unsigned int, unsigned short *));
    _CA_LOAD_FUNC(snd_pcm_format_size,ssize_t(*)(snd_pcm_format_t, size_t));
    _CA_LOAD_FUNC(snd_pcm_link,int(*)(snd_pcm_t *, snd_pcm_t *));
    _CA_LOAD_FUNC(snd_pcm_delay,int(*)(snd_pcm_t *, snd_pcm_sframes_t *));

    _CA_LOAD_FUNC(snd_pcm_hw_params_sizeof,size_t(*)());
    _CA_LOAD_FUNC(snd_pcm_hw_params_malloc,int(*)(snd_pcm_hw_params_t **));
    _CA_LOAD_FUNC(snd_pcm_hw_params_free,void(*)(snd_pcm_hw_params_t *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_any,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_access,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_access_t ));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_format,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_format_t));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_channels,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int));
//    _CA_LOAD_FUNC(snd_pcm_hw_params_set_periods_near);
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_rate_near,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_rate,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int, int));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_rate_resample,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int ));
//    _CA_LOAD_FUNC(snd_pcm_hw_params_set_buffer_time_near);
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_buffer_size,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_buffer_size_near,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_buffer_size_min,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t *));
//    _CA_LOAD_FUNC(snd_pcm_hw_params_set_period_time_near);
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_period_size_near,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t *, int *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_periods_integer,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_periods_min,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *));

    _CA_LOAD_FUNC(snd_pcm_hw_params_get_buffer_size,int(*)(const snd_pcm_hw_params_t *, snd_pcm_uframes_t *));
//    _CA_LOAD_FUNC(snd_pcm_hw_params_get_period_size);
//    _CA_LOAD_FUNC(snd_pcm_hw_params_get_access);
//    _CA_LOAD_FUNC(snd_pcm_hw_params_get_periods);
//    _CA_LOAD_FUNC(snd_pcm_hw_params_get_rate);
    _CA_LOAD_FUNC(snd_pcm_hw_params_get_channels_min,int(*)(const snd_pcm_hw_params_t *, unsigned int *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_get_channels_max,int(*)(const snd_pcm_hw_params_t *, unsigned int *));

    _CA_LOAD_FUNC(snd_pcm_hw_params_test_period_size,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t , int));
    _CA_LOAD_FUNC(snd_pcm_hw_params_test_format,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_format_t));
    _CA_LOAD_FUNC(snd_pcm_hw_params_test_access,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_access_t ));
    _CA_LOAD_FUNC(snd_pcm_hw_params_dump,int(*)(snd_pcm_hw_params_t *, snd_output_t *));
    _CA_LOAD_FUNC(snd_pcm_hw_params,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *));

    _CA_LOAD_FUNC(snd_pcm_hw_params_get_periods_min,int(*)(const snd_pcm_hw_params_t *, unsigned int *, int *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_get_periods_max,int(*)(const snd_pcm_hw_params_t *, unsigned int *, int *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_set_period_size,int(*)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t, int));
    _CA_LOAD_FUNC(snd_pcm_hw_params_get_period_size_min,int(*)(const snd_pcm_hw_params_t *, snd_pcm_uframes_t *, int *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_get_period_size_max,int(*)(const snd_pcm_hw_params_t *, snd_pcm_uframes_t *, int *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_get_buffer_size_max,int(*)(const snd_pcm_hw_params_t *, snd_pcm_uframes_t *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_get_rate_min,int(*)(const snd_pcm_hw_params_t *, unsigned int *, int *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_get_rate_max,int(*)(const snd_pcm_hw_params_t *, unsigned int *, int *));
    _CA_LOAD_FUNC(snd_pcm_hw_params_get_rate_numden,int(*)(const snd_pcm_hw_params_t *,unsigned int *,unsigned int *));

    _CA_LOAD_FUNC(snd_pcm_sw_params_sizeof,size_t(*)());
    _CA_LOAD_FUNC(snd_pcm_sw_params_malloc,int(*)(snd_pcm_sw_params_t **));
    _CA_LOAD_FUNC(snd_pcm_sw_params_current,int(*)(snd_pcm_t *, snd_pcm_sw_params_t *));
    _CA_LOAD_FUNC(snd_pcm_sw_params_set_avail_min,int(*)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_sw_params,int(*)(snd_pcm_t *, snd_pcm_sw_params_t *));
    _CA_LOAD_FUNC(snd_pcm_sw_params_free,void(*)(snd_pcm_sw_params_t *));
    _CA_LOAD_FUNC(snd_pcm_sw_params_set_start_threshold,int(*)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_sw_params_set_stop_threshold,int(*)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_sw_params_get_boundary,int(*)(const snd_pcm_sw_params_t *, snd_pcm_uframes_t *));
    _CA_LOAD_FUNC(snd_pcm_sw_params_set_silence_threshold,int(*)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_sw_params_set_silence_size,int(*)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_sw_params_set_xfer_align,int(*)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t));
    _CA_LOAD_FUNC(snd_pcm_sw_params_set_tstamp_mode,int(*)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_tstamp_t));

    _CA_LOAD_FUNC(snd_pcm_info,int(*)(snd_pcm_t *, snd_pcm_info_t *));
    _CA_LOAD_FUNC(snd_pcm_info_sizeof,size_t(*)());
    _CA_LOAD_FUNC(snd_pcm_info_malloc,int(*)(snd_pcm_info_t **));
    _CA_LOAD_FUNC(snd_pcm_info_free,void(*)(snd_pcm_info_t *));
    _CA_LOAD_FUNC(snd_pcm_info_set_device,void(*)(snd_pcm_info_t *, unsigned int));
    _CA_LOAD_FUNC(snd_pcm_info_set_subdevice,void(*)(snd_pcm_info_t *, unsigned int));
    _CA_LOAD_FUNC(snd_pcm_info_set_stream,void(*)(snd_pcm_info_t *, snd_pcm_stream_t));
    _CA_LOAD_FUNC(snd_pcm_info_get_name,const char *(*)(const snd_pcm_info_t *));
    _CA_LOAD_FUNC(snd_pcm_info_get_card,int(*)(const snd_pcm_info_t *));

    _CA_LOAD_FUNC(snd_ctl_pcm_next_device,int(*)(snd_ctl_t *, int *));
    _CA_LOAD_FUNC(snd_ctl_pcm_info,int(*)(snd_ctl_t *, snd_pcm_info_t *));
    _CA_LOAD_FUNC(snd_ctl_open,int(*)(snd_ctl_t **, const char *, int));
    _CA_LOAD_FUNC(snd_ctl_close,int(*)(snd_ctl_t *));
    _CA_LOAD_FUNC(snd_ctl_card_info_malloc,int(*)(snd_ctl_card_info_t **));
    _CA_LOAD_FUNC(snd_ctl_card_info_free,void(*)(snd_ctl_card_info_t *));
    _CA_LOAD_FUNC(snd_ctl_card_info,int(*)(snd_ctl_t *, snd_ctl_card_info_t *));
    _CA_LOAD_FUNC(snd_ctl_card_info_sizeof,size_t(*)());
    _CA_LOAD_FUNC(snd_ctl_card_info_get_name,const char *(*)(const snd_ctl_card_info_t *));

    _CA_LOAD_FUNC(snd_config,snd_config_t **);
    _CA_LOAD_FUNC(snd_config_update,int(*)());
    _CA_LOAD_FUNC(snd_config_search,int(*)(snd_config_t *, const char *,snd_config_t **));
    _CA_LOAD_FUNC(snd_config_iterator_entry,snd_config_t *(*)(const snd_config_iterator_t));
    _CA_LOAD_FUNC(snd_config_iterator_first,snd_config_iterator_t(*)(const snd_config_t *));
    _CA_LOAD_FUNC(snd_config_iterator_end,snd_config_iterator_t(*)(const snd_config_t *));
    _CA_LOAD_FUNC(snd_config_iterator_next,snd_config_iterator_t(*)(const snd_config_iterator_t));
    _CA_LOAD_FUNC(snd_config_get_string,int(*)(const snd_config_t *, const char **));
    _CA_LOAD_FUNC(snd_config_get_id,int(*)(const snd_config_t *, const char **));
    _CA_LOAD_FUNC(snd_config_update_free_global,int(*)());

    _CA_LOAD_FUNC(snd_pcm_status,int(*)(snd_pcm_t *, snd_pcm_status_t *));
    _CA_LOAD_FUNC(snd_pcm_status_sizeof,size_t(*)());
    _CA_LOAD_FUNC(snd_pcm_status_get_tstamp,void(*)(const snd_pcm_status_t *, snd_timestamp_t *));
    _CA_LOAD_FUNC(snd_pcm_status_get_state,snd_pcm_state_t(*)(const snd_pcm_status_t *));
    _CA_LOAD_FUNC(snd_pcm_status_get_trigger_tstamp,void(*)(const snd_pcm_status_t *, snd_timestamp_t *));
    _CA_LOAD_FUNC(snd_pcm_status_get_delay,snd_pcm_sframes_t(*)(const snd_pcm_status_t *));

    _CA_LOAD_FUNC(snd_card_next,int(*)(int *));
    _CA_LOAD_FUNC(snd_asoundlib_version,const char *(*)());
    _CA_LOAD_FUNC(snd_strerror,const char *(*)(int));
    _CA_LOAD_FUNC(snd_output_stdio_attach,int(*)(snd_output_t **, FILE *, int));

    _CA_LOAD_FUNC(snd_lib_error_set_local,snd_local_error_handler_t(*)(snd_local_error_handler_t)) ;

#undef _CA_LOAD_FUNC

    #define _CA_VALIDATE_LOAD_REPLACEMENT(x)\
    if ( NULL == alsa_##x ) alsa_##x = &_CA_LOCAL_IMPL(x)

    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_set_rate_near);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_set_buffer_size_near);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_set_period_size_near);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_channels_min);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_channels_max);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_periods_min);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_periods_max);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_period_size_min);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_period_size_max);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_buffer_size_max);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_rate_min);
    _CA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_rate_max);

#undef _CA_LOCAL_IMPL
#undef _CA_VALIDATE_LOAD_REPLACEMENT
  alsa_snd_lib_error_set_local ( AlsaCiosErrorHandler )                      ;
  return 1                                                                   ;
}

//////////////////////////////////////////////////////////////////////////////

void AlsaSetLibraryPathName(const char * pathName)
{
  g_AlsaLibName = pathName ;
}

///////////////////////////////////////////////////////////////////////////////

/* Close handle to Alsa library. */
static void AlsaCloseLibrary(void)
{
  ::dlclose ( g_AlsaLib ) ;
  g_AlsaLib = NULL        ;
}

///////////////////////////////////////////////////////////////////////////////

void InitializeStreamInfo(AlsaStreamInfo * info)
{
  info -> size         = sizeof (AlsaStreamInfo) ;
  info -> hostApiType  = ALSA                    ;
  info -> version      = 1                       ;
  info -> deviceString = NULL                    ;
}

//////////////////////////////////////////////////////////////////////////////

/** Align value in backward direction.
 *
 * @param v: Value to align.
 * @param align: Alignment.
 */
static unsigned long AlignBackward(unsigned long v,unsigned long align)
{
  return ( v - ( align ? v % align : 0 ) ) ;
}

//////////////////////////////////////////////////////////////////////////////

/** Align value in forward direction.
 *
 * @param v: Value to align.
 * @param align: Alignment.
 */
static unsigned long AlignForward(unsigned long v, unsigned long align)
{
  unsigned long remainder = ( align ? ( v % align ) : 0)    ;
  return ( remainder != 0 ? v + ( align - remainder ) : v ) ;
}

//////////////////////////////////////////////////////////////////////////////

/** Get size of host buffer maintained from the number of user frames, sample rate and suggested latency. Minimum double buffering
 *  is maintained to allow 100% CPU usage inside user callback.
 *
 * @param userFramesPerBuffer: User buffer size in number of frames.
 * @param suggestedLatency: User provided desired latency.
 * @param sampleRate: Sample rate.
 */
static unsigned long GetFramesPerHostBuffer              (
                       unsigned long userFramesPerBuffer ,
                       CaTime        suggestedLatency    ,
                       double        sampleRate          )
{
  unsigned long frames                                                 ;
  frames = userFramesPerBuffer                                         +
           CA_MAX ( userFramesPerBuffer                                ,
                    (unsigned long)( suggestedLatency * sampleRate ) ) ;
  return frames                                                        ;
}

///////////////////////////////////////////////////////////////////////////////

static snd_pcm_format_t AlsaFormat(CaSampleFormat paFormat)
{
  switch ( paFormat )             {
    case cafFloat32               :
    return SND_PCM_FORMAT_FLOAT   ;
    case cafInt16                 :
    return SND_PCM_FORMAT_S16     ;
    case cafInt24                 :
#ifdef CA_LITTLE_ENDIAN
    return SND_PCM_FORMAT_S24_3LE ;
#elif defined(CA_BIG_ENDIAN)
    return SND_PCM_FORMAT_S24_3BE ;
#endif
    case cafInt32                 :
    return SND_PCM_FORMAT_S32     ;
    case cafInt8                  :
    return SND_PCM_FORMAT_S8      ;
    case cafUint8                 :
    return SND_PCM_FORMAT_U8      ;
    default                       :
    return SND_PCM_FORMAT_UNKNOWN ;
  }
}

/** Open PCM device.
 *
 * Wrapper around alsa_snd_pcm_open which may repeatedly retry opening a device if it is busy, for
 * a certain time. This is because dmix may temporarily hold on to a device after it (dmix)
 * has been opened and closed.
 * @param mode: Open mode (e.g., SND_PCM_BLOCKING).
 * @param waitOnBusy: Retry opening busy device for up to one second?
 **/
static int OpenPcm(snd_pcm_t     ** pcmp       ,
                   const char    *  name       ,
                   snd_pcm_stream_t stream     ,
                   int              mode       ,
                   int              waitOnBusy )
{
  int ret                                                                    ;
  int tries                                                                  ;
  int maxTries = waitOnBusy ? busyRetries_ : 0                               ;
  ret = alsa_snd_pcm_open ( pcmp, name, stream, mode )                       ;
  for ( tries = 0; tries < maxTries && -EBUSY == ret; ++tries )              {
    Timer :: Sleep ( 10 )                                                    ;
    ret = alsa_snd_pcm_open ( pcmp, name, stream, mode )                     ;
    if ( -EBUSY != ret )                                                     {
      gPrintf                                                              ( (
        "%s: Successfully opened initially busy device after %d tries\n"     ,
        __FUNCTION__                                                         ,
        tries                                                            ) ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( -EBUSY == ret )                                                       {
    gPrintf ( ( "%s: Failed to open busy device '%s'\n"                      ,
                __FUNCTION__                                                 ,
                name                                                     ) ) ;
  } else                                                                     {
    if ( ret < 0 )                                                           {
      gPrintf                                                              ( (
        "%s: Opened device '%s' ptr[%p] - result: [%d:%s]\n"                 ,
        __FUNCTION__                                                         ,
        name                                                                 ,
        *pcmp                                                                ,
        ret                                                                  ,
        alsa_snd_strerror(ret)                                           ) ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return ret                                                                 ;
}

/* Set the stream sample rate to a nominal value requested; allow only a defined tolerance range */
static int SetApproximateSampleRate           (
             snd_pcm_t           * pcm        ,
             snd_pcm_hw_params_t * hwParams   ,
             double                sampleRate )
{
  CaError      result = NoError                                              ;
  unsigned int reqRate                                                       ;
  unsigned int setRate                                                       ;
  unsigned int deviation                                                     ;
  /* The Alsa sample rate is set by integer value; also the actual rate may differ */
  reqRate = setRate = (unsigned int) sampleRate                              ;
  ENSURE_( alsa_snd_pcm_hw_params_set_rate_near                              (
             pcm                                                             ,
             hwParams                                                        ,
             &setRate                                                        ,
             NULL                                                          ) ,
           UnanticipatedHostError                                          ) ;
  /* The value actually set will be put in 'setRate' (may be way off);
   * check the deviation as a proportion of the requested-rate with reference
   * to the max-deviate-ratio (larger values allow less deviation)          */
  deviation = abs ( setRate - reqRate )                                      ;
  if ( deviation > 1 && deviation * RATE_MAX_DEVIATE_RATIO > reqRate )       {
    result = InvalidSampleRate                                               ;
  }                                                                          ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  unsigned int _min = 0                                                      ;
  unsigned int _max = 0                                                      ;
  int          _dir = 0                                                      ;
  ENSURE_ ( alsa_snd_pcm_hw_params_get_rate_min ( hwParams, &_min, &_dir )   ,
            UnanticipatedHostError                                       )   ;
  ENSURE_ ( alsa_snd_pcm_hw_params_get_rate_max ( hwParams, &_max, &_dir )   ,
            UnanticipatedHostError                                         ) ;
    gPrintf                                                                ( (
      "%s: SR min = %u, max = %u, req = %u\n"                                ,
      __FUNCTION__                                                           ,
      _min                                                                   ,
      _max                                                                   ,
      reqRate                                                            ) ) ;
  goto end                                                                   ;
}

/* Given an open stream, what sample formats are available? */
static CaSampleFormat GetAvailableFormats(snd_pcm_t * pcm)
{
  int available = cafNothing                       ;
  snd_pcm_hw_params_t * hwParams                   ;
  alsa_snd_pcm_hw_params_alloca ( &hwParams      ) ;
  alsa_snd_pcm_hw_params_any    ( pcm , hwParams ) ;
  if ( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_FLOAT ) >= 0)
    available |= cafFloat32                        ;
  if ( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S32 ) >= 0)
    available |= cafInt32                          ;
#if   defined(CA_LITTLE_ENDIAN)
  if ( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S24_3LE ) >= 0)
    available |= cafInt24                          ;
#elif defined(CA_BIG_ENDIAN)
  if ( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S24_3BE ) >= 0)
    available |= cafInt24                          ;
#endif
  if ( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S16 ) >= 0)
    available |= cafInt16                          ;
  if ( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U8 ) >= 0)
    available |= cafUint8                          ;
  if ( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S8 ) >= 0)
    available |= cafInt8                           ;
  return (CaSampleFormat)available                 ;
}

/* Output to console all formats supported by device */
static void LogAllAvailableFormats(snd_pcm_t * pcm)
{
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( NULL == globalDebugger ) return ;
    CaSampleFormat available = cafNothing ;
    snd_pcm_hw_params_t *hwParams;
    alsa_snd_pcm_hw_params_alloca( &hwParams );

    alsa_snd_pcm_hw_params_any( pcm, hwParams );

    globalDebugger->printf( " --- Supported Formats ---\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S8 ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S8\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U8 ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U8\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S16_LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S16_LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S16_BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S16_BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U16_LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U16_LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U16_BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U16_BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S24_LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S24_LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S24_BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S24_BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U24_LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U24_LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U24_BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U24_BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_FLOAT_LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_FLOAT_LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_FLOAT_BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_FLOAT_BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_FLOAT64_LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_FLOAT64_LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_FLOAT64_BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_FLOAT64_BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_IEC958_SUBFRAME_LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_IEC958_SUBFRAME_LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_IEC958_SUBFRAME_BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_IEC958_SUBFRAME_BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_MU_LAW ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_MU_LAW\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_A_LAW ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_A_LAW\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_IMA_ADPCM ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_IMA_ADPCM\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_MPEG ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_MPEG\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_GSM ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_GSM\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_SPECIAL ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_SPECIAL\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S24_3LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S24_3LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S24_3BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S24_3BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U24_3LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U24_3LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U24_3BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U24_3BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S20_3LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S20_3LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S20_3BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S20_3BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U20_3LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U20_3LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U20_3BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U20_3BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S18_3LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S18_3LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S18_3BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S18_3BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U18_3LE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U18_3LE\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U18_3BE ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U18_3BE\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S16 ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S16\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U16 ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U16\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S24 ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S24\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U24 ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U24\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_S32 ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_S32\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_U32 ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_U32\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_FLOAT ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_FLOAT\n" );
    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_FLOAT64 ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_FLOAT64\n" );

    if( alsa_snd_pcm_hw_params_test_format( pcm, hwParams, SND_PCM_FORMAT_IEC958_SUBFRAME ) >= 0)
        globalDebugger->printf( "SND_PCM_FORMAT_IEC958_SUBFRAME\n" );
    globalDebugger->printf( " -------------------------\n" );
  #endif
}

///////////////////////////////////////////////////////////////////////////////

static void * CallbackThreadFunc(void * userData)
{
  if ( NULL == userData ) return NULL           ;
  AlsaStream * stream = (AlsaStream *) userData ;
  return stream -> Processor ( )                ;
}

///////////////////////////////////////////////////////////////////////////////

CaError AlsaInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError result = NoError                                                   ;
  /* Try loading Alsa library.                                              */
  if ( ! AlsaLoadLibrary ( ) ) return HostApiNotFound                        ;
  gPrintf ( ( "ALSA version <%s>\n" , alsa_snd_asoundlib_version() ) )       ;
  ////////////////////////////////////////////////////////////////////////////
  AlsaHostApi * alsaHostApi                                                  ;
  alsaHostApi                = new AlsaHostApi     ( )                       ;
  CA_UNLESS ( alsaHostApi                , InsufficientMemory )              ;
  alsaHostApi -> allocations = new AllocationGroup ( )                       ;
  CA_UNLESS ( alsaHostApi -> allocations , InsufficientMemory )              ;
  ////////////////////////////////////////////////////////////////////////////
  alsaHostApi -> hostApiIndex   = hostApiIndex                               ;
  alsaHostApi -> alsaLibVersion = AlsaVersionNum ( )                         ;
  ////////////////////////////////////////////////////////////////////////////
   *hostApi                          = (HostApi *)alsaHostApi                ;
  (*hostApi) -> info . structVersion = 1                                     ;
  (*hostApi) -> info . type          = ALSA                                  ;
  (*hostApi) -> info . index         = hostApiIndex                          ;
  (*hostApi) -> info . name          = "Advanced Linux Sound Architecture"   ;
  ////////////////////////////////////////////////////////////////////////////
  CA_ENSURE ( alsaHostApi->BuildDeviceList() )                               ;
  CA_ENSURE ( CaUnixThreadingInitialize()   )                                ;
  return result                                                              ;
error                                                                        :
  if ( CaNotNull ( alsaHostApi ) )                                           {
    if ( CaNotNull ( alsaHostApi->allocations ) )                            {
      alsaHostApi -> allocations -> release ( )                              ;
      delete alsaHostApi -> allocations                                      ;
      alsaHostApi -> allocations  = NULL                                     ;
    }                                                                        ;
    delete alsaHostApi                                                       ;
  }                                                                          ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

AlsaStreamComponent:: AlsaStreamComponent(void)
{
}

AlsaStreamComponent::~AlsaStreamComponent(void)
{
}

void AlsaStreamComponent::Reset(void)
{
  hostSampleFormat  = cafNothing             ;
  numUserChannels   = 0                      ;
  numHostChannels   = 0                      ;
  userInterleaved   = 0                      ;
  hostInterleaved   = 0                      ;
  canMmap           = 0                      ;
  nonMmapBuffer     = NULL                   ;
  nonMmapBufferSize = 0                      ;
  device            = 0                      ;
  deviceIsPlug      = 0                      ;
  useReventFix      = 0                      ;
  pcm               = NULL                   ;
  framesPerPeriod   = 0                      ;
  alsaBufferSize    = 0                      ;
  nativeFormat      = SND_PCM_FORMAT_UNKNOWN ;
  nfds              = 0                      ;
  ready             = 0                      ;
  userBuffers       = NULL                   ;
  offset            = 0                      ;
  streamDir         = StreamDirection_In     ;
  channelAreas      = NULL                   ;
  FixedFrameSize    = true                   ;
}

CaError AlsaStreamComponent::Initialize         (
          AlsaHostApi            * alsaApi      ,
          const StreamParameters * params       ,
          StreamDirection          streamDir    ,
          int                      callbackMode )
{
  CaError        result           = NoError                                  ;
  CaSampleFormat userSampleFormat = (CaSampleFormat)params->sampleFormat     ;
  CaError        hostSampleFormat = NoError                                  ;
  /* Make sure things have an initial value                                 */
  Reset ( )                                                                  ;
  if ( NULL == params->streamInfo )                           {
    const AlsaDeviceInfo * devInfo                                           ;
    devInfo = alsaApi->GetAlsaDeviceInfo(params->device)                     ;
    numHostChannels = CA_MAX( params             ->channelCount              ,
                              StreamDirection_In == streamDir                ?
                              devInfo            -> minInputChannels         :
                              devInfo            -> minOutputChannels      ) ;
    deviceIsPlug = devInfo->isPlug                                           ;
    gPrintf                                                                ( (
      "%s: Host Channels [%c] %i\n"                                          ,
      __FUNCTION__                                                           ,
      streamDir == StreamDirection_In ? 'C' : 'P'                            ,
      numHostChannels                                                    ) ) ;
  } else                                                                     {
    /* We're blissfully unaware of the minimum channelCount                 */
    numHostChannels = params->channelCount                                   ;
    /* Check if device name does not start with hw: to determine if it is a 'plug' device */
    if ( strncmp( "hw:", ((AlsaStreamInfo *)params->streamInfo)->deviceString, 3 ) != 0  ) {
      /* An Alsa plug device, not a direct hw device                        */
      deviceIsPlug = 1                                                       ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( deviceIsPlug && alsaApi->alsaLibVersion<ALSA_VERSION_INT(1,0,16) )    {
    /* Prior to Alsa1.0.16, plug devices may stutter without this fix       */
    useReventFix = 1                                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  device = params->device                                                    ;
  CA_ENSURE ( alsaApi -> Open ( params , streamDir , &pcm ) )                ;
  nfds = alsa_snd_pcm_poll_descriptors_count ( pcm )                         ;
  ////////////////////////////////////////////////////////////////////////////
  hostSampleFormat = ClosestFormat( GetAvailableFormats ( pcm )              ,
                                    userSampleFormat                       ) ;
  CA_ENSURE( hostSampleFormat )                                              ;
  this -> hostSampleFormat  = (CaSampleFormat)hostSampleFormat               ;
  this -> nativeFormat      = AlsaFormat( (CaSampleFormat)hostSampleFormat ) ;
  this -> userInterleaved   = ! ( userSampleFormat & cafNonInterleaved )     ;
  this -> hostInterleaved   = this   -> userInterleaved                      ;
  this -> numUserChannels   = params -> channelCount                         ;
  this -> streamDir         = streamDir                                      ;
  this -> canMmap           = 0                                              ;
  this -> nonMmapBuffer     = NULL                                           ;
  this -> nonMmapBufferSize = 0                                              ;
  ////////////////////////////////////////////////////////////////////////////
  if ( !callbackMode && ! this->userInterleaved )                            {
    /* Pre-allocate non-interleaved user provided buffers                   */
    userBuffers = (void **)Allocator :: allocate( sizeof(void *)             *
                                                  numUserChannels          ) ;
    CA_UNLESS ( userBuffers , InsufficientMemory )                           ;
  }                                                                          ;
error                                                                        :
  /* Log all available formats.                                             */
  if ( hostSampleFormat == SampleFormatNotSupported )                        {
    LogAllAvailableFormats ( pcm )                                           ;
    gPrintf                                                                ( (
      "%s: Please provide the log output to CIOS Audio Core developers, your hardware does not have any sample format implemented yet.\n",
      __FUNCTION__                                                       ) ) ;
  }                                                                          ;
  return result                                                              ;
}

void AlsaStreamComponent::Terminate(void)
{
  alsa_snd_pcm_close ( pcm           ) ;
  Allocator :: free  ( userBuffers   ) ;
  Allocator :: free  ( nonMmapBuffer ) ;
}

/* Initiate configuration, preparing for determining a period size suitable for both capture and playback components. */

CaError AlsaStreamComponent :: InitialConfigure (
          const StreamParameters * params       ,
          int                      primeBuffers ,
          snd_pcm_hw_params_t    * hwParams     ,
          double                 * sampleRate   )
{
  CaError          result     = NoError                                      ;
  int              dir        = 0                                            ;
  snd_pcm_t      * pcm        = this->pcm                                    ;
  double           sr         = *sampleRate                                  ;
  unsigned int     minPeriods = 2                                            ;
  snd_pcm_access_t accessMode                                                ;
  snd_pcm_access_t alternateAccessMode                                       ;
  // framesPerPeriod = framesPerHostBuffer                                      ;
  ENSURE_ ( alsa_snd_pcm_hw_params_any ( pcm , hwParams )                    ,
            UnanticipatedHostError                                         ) ;
  ENSURE_ ( alsa_snd_pcm_hw_params_set_periods_integer ( pcm , hwParams )    ,
            UnanticipatedHostError                                         ) ;
  /* I think there should be at least 2 periods (even though ALSA doesn't appear to enforce this) */
  dir = 0                                                                    ;
  ENSURE_ ( alsa_snd_pcm_hw_params_set_periods_min( pcm , hwParams , &minPeriods , &dir ),
            UnanticipatedHostError                                         ) ;
  if ( this->userInterleaved )                                               {
    accessMode          = SND_PCM_ACCESS_MMAP_INTERLEAVED                    ;
    alternateAccessMode = SND_PCM_ACCESS_MMAP_NONINTERLEAVED                 ;
    /* test if MMAP supported                                               */
    this->canMmap = alsa_snd_pcm_hw_params_test_access( pcm, hwParams, accessMode          ) >= 0 ||
                    alsa_snd_pcm_hw_params_test_access( pcm, hwParams, alternateAccessMode ) >= 0  ;
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( NULL != globalDebugger )                                            {
      globalDebugger -> printf ( "%s: device MMAP SND_PCM_ACCESS_MMAP_INTERLEAVED: %s\n"   ,
                                 __FUNCTION__                                ,
                                 ( alsa_snd_pcm_hw_params_test_access( pcm, hwParams, accessMode ) >= 0 ? "YES" : "NO" )  ) ;
      globalDebugger -> printf ( "%s: device MMAP SND_PCM_ACCESS_MMAP_NONINTERLEAVED: %s\n",
                                 __FUNCTION__                                ,
                                 ( alsa_snd_pcm_hw_params_test_access( pcm, hwParams, alternateAccessMode ) >= 0 ? "YES" : "NO" )  ) ;
    }                                                                        ;
    #endif
    if ( ! this->canMmap )                                                   {
      accessMode          = SND_PCM_ACCESS_RW_INTERLEAVED                    ;
      alternateAccessMode = SND_PCM_ACCESS_RW_NONINTERLEAVED                 ;
    }                                                                        ;
  } else                                                                     {
    accessMode          = SND_PCM_ACCESS_MMAP_NONINTERLEAVED                 ;
    alternateAccessMode = SND_PCM_ACCESS_MMAP_INTERLEAVED                    ;
    /* test if MMAP supported                                               */
    this->canMmap = alsa_snd_pcm_hw_params_test_access( pcm, hwParams, accessMode          ) >= 0 ||
                    alsa_snd_pcm_hw_params_test_access( pcm, hwParams, alternateAccessMode ) >= 0  ;
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( NULL != globalDebugger )                                            {
      globalDebugger -> printf ( " %s: device MMAP SND_PCM_ACCESS_MMAP_NONINTERLEAVED: %s\n",
                                 __FUNCTION__                                ,
                                 ( alsa_snd_pcm_hw_params_test_access( pcm, hwParams, accessMode ) >= 0 ? "YES" : "NO" ) ) ;
      globalDebugger -> printf ( "%s: device MMAP SND_PCM_ACCESS_MMAP_INTERLEAVED: %s\n",
                                 __FUNCTION__                                ,
                                 ( alsa_snd_pcm_hw_params_test_access( pcm, hwParams, alternateAccessMode ) >= 0 ? "YES" : "NO" ) ) ;
    }                                                                        ;
    #endif
    if ( ! this->canMmap )                                                   {
      accessMode          = SND_PCM_ACCESS_RW_NONINTERLEAVED                 ;
      alternateAccessMode = SND_PCM_ACCESS_RW_INTERLEAVED                    ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf                                                                  ( (
    "%s: device can MMAP: %s\n"                                              ,
    __FUNCTION__                                                             ,
    ( this->canMmap ? "YES" : "NO" )                                     ) ) ;
  /* If requested access mode fails, try alternate mode                     */
  if ( alsa_snd_pcm_hw_params_set_access( pcm, hwParams, accessMode ) < 0 )  {
    int err = 0                                                              ;
    err = alsa_snd_pcm_hw_params_set_access(pcm,hwParams,alternateAccessMode);
    if ( err < 0 )                                                           {
      result = UnanticipatedHostError                                        ;
      SetLastHostErrorInfo ( ALSA , err , alsa_snd_strerror ( err ) )        ;
      goto error                                                             ;
    }                                                                        ;
    this->hostInterleaved = ! this->userInterleaved                          ;
  }                                                                          ;
  ENSURE_ ( alsa_snd_pcm_hw_params_set_format(pcm,hwParams,nativeFormat)     ,
            UnanticipatedHostError                                         ) ;
  result = SetApproximateSampleRate ( pcm , hwParams , sr )                  ;
  if ( result != UnanticipatedHostError )                                    {
    ENSURE_( GetExactSampleRate ( hwParams , &sr ), UnanticipatedHostError ) ;
    if ( result == InvalidSampleRate )                                       {
      gPrintf ( ( "%s: Wanted %.3f, closest sample rate was %.3f\n"          ,
                  __FUNCTION__                                               ,
                  *sampleRate                                                ,
                  sr                                                     ) ) ;
      gPrintf ( ( "CIOS Audio Core for ALSA did not implement resampling,"
                  " please try another sampling rate.\n"                 ) ) ;
      CA_ENSURE ( InvalidSampleRate )                                        ;
    }                                                                        ;
  } else                                                                     {
    CA_ENSURE ( UnanticipatedHostError )                                     ;
  }                                                                          ;
  ENSURE_ ( alsa_snd_pcm_hw_params_set_channels(pcm,hwParams,numHostChannels),
            InvalidChannelCount                                            ) ;
  *sampleRate = sr                                                           ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

CaError AlsaStreamComponent::FinishConfigure    (
          snd_pcm_hw_params_t    * hwParams     ,
          const StreamParameters * params       ,
          int                      primeBuffers ,
          double                   sampleRate   ,
          CaTime                 * latency      )
{
  CaError               result = NoError                                     ;
  snd_pcm_sw_params_t * swParams                                             ;
  snd_pcm_uframes_t     bufSz = 0                                            ;
  int                   r                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  *latency = -1.0                                                            ;
  alsa_snd_pcm_sw_params_alloca ( &swParams )                                ;
  bufSz = ( params->suggestedLatency * sampleRate ) + framesPerPeriod        ;
  ENSURE_ ( alsa_snd_pcm_hw_params_set_buffer_size_near(pcm,hwParams,&bufSz) ,
            UnanticipatedHostError                                         ) ;
  /* Set the parameters!                                                    */
  r = alsa_snd_pcm_hw_params ( pcm , hwParams )                              ;
  ENSURE_ ( r , UnanticipatedHostError )                                     ;
  if ( alsa_snd_pcm_hw_params_get_buffer_size != NULL )                      {
    ENSURE_(alsa_snd_pcm_hw_params_get_buffer_size(hwParams,&alsaBufferSize) ,
            UnanticipatedHostError                                         ) ;
  } else                                                                     {
    alsaBufferSize = bufSz                                                   ;
  }                                                                          ;
  /* Latency in seconds                                                     */
  *latency = ( alsaBufferSize - framesPerPeriod ) / sampleRate               ;
  /* Now software parameters...                                             */
  ENSURE_( alsa_snd_pcm_sw_params_current(pcm,swParams)                      ,
           UnanticipatedHostError                                          ) ;
  ENSURE_( alsa_snd_pcm_sw_params_set_start_threshold(pcm,swParams,framesPerPeriod),
           UnanticipatedHostError                                          ) ;
  ENSURE_( alsa_snd_pcm_sw_params_set_stop_threshold(pcm,swParams,alsaBufferSize),
           UnanticipatedHostError                                          ) ;
  /* Silence buffer in the case of underrun                                 */
  if ( ! primeBuffers )                                                      {
    snd_pcm_uframes_t boundary                                               ;
    ENSURE_( alsa_snd_pcm_sw_params_get_boundary(swParams,&boundary)         ,
             UnanticipatedHostError                                        ) ;
    ENSURE_( alsa_snd_pcm_sw_params_set_silence_threshold(pcm,swParams,0)    ,
             UnanticipatedHostError                                        ) ;
    ENSURE_( alsa_snd_pcm_sw_params_set_silence_size(pcm,swParams,boundary)  ,
             UnanticipatedHostError                                        ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ENSURE_( alsa_snd_pcm_sw_params_set_avail_min(pcm,swParams,framesPerPeriod),
           UnanticipatedHostError                                          ) ;
  ENSURE_( alsa_snd_pcm_sw_params_set_xfer_align(pcm,swParams,1)             ,
           UnanticipatedHostError                                          ) ;
  ENSURE_( alsa_snd_pcm_sw_params_set_tstamp_mode(pcm,swParams,SND_PCM_TSTAMP_ENABLE),
           UnanticipatedHostError                                          ) ;
  /* Set the parameters!                                                    */
  ENSURE_( alsa_snd_pcm_sw_params(pcm,swParams)                              ,
           UnanticipatedHostError                                          ) ;
error                                                                        :
  return result                                                              ;
}

CaError AlsaStreamComponent ::     DetermineFramesPerBuffer  (
          const StreamParameters * params                    ,
          unsigned long            framesPerUserBuffer       ,
          double                   sampleRate                ,
          snd_pcm_hw_params_t    * hwParams                  ,
          int                    * accurate                  )
{
  CaError       result = NoError                                             ;
  int           dir    = 0                                                   ;
  unsigned long bufferSize                                                   ;
  unsigned long framesPerHostBuffer = framesPerUserBuffer                    ;
  /* Calculate host buffer size                                             */
  bufferSize = GetFramesPerHostBuffer ( framesPerUserBuffer                  ,
                                        params->suggestedLatency             ,
                                        sampleRate                         ) ;
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( NULL != globalDebugger )                                              {
    globalDebugger -> printf ( "%s: user-buffer (frames)           = %lu\n"  ,
                               __FUNCTION__                                  ,
                               framesPerUserBuffer                         ) ;
    globalDebugger -> printf ( "%s: user-buffer (sec)              = %f\n"   ,
                               __FUNCTION__                                  ,
                               (double)(framesPerUserBuffer / sampleRate)  ) ;
    globalDebugger -> printf ( "%s: suggested latency (sec)        = %f\n"   ,
                               __FUNCTION__                                  ,
                               params->suggestedLatency                    ) ;
    globalDebugger -> printf ( "%s: suggested host buffer (frames) = %lu\n"  ,
                               __FUNCTION__                                  ,
                               bufferSize                                  ) ;
    globalDebugger -> printf ( "%s: suggested host buffer (sec)    = %f\n"   ,
                               __FUNCTION__                                  ,
                               (double)(bufferSize / sampleRate)           ) ;
  }                                                                          ;
  #endif
  ////////////////////////////////////////////////////////////////////////////
  {                                                                          ;
     unsigned numPeriods = numPeriods_                                       ;
     unsigned maxPeriods = 0                                                 ;
     unsigned minPeriods = numPeriods_                                       ;
     /* It may be that the device only supports 2 periods for instance      */
     dir = 0                                                                 ;
     ENSURE_(alsa_snd_pcm_hw_params_get_periods_min(hwParams,&minPeriods,&dir),
             UnanticipatedHostError                                        ) ;
     ENSURE_(alsa_snd_pcm_hw_params_get_periods_max(hwParams,&maxPeriods,&dir),
             UnanticipatedHostError                                        ) ;
     /* Clamp to min/max                                                    */
     numPeriods = CA_MIN ( maxPeriods , CA_MAX ( minPeriods , numPeriods ) ) ;
     #ifndef REMOVE_DEBUG_MESSAGE
     if ( NULL != globalDebugger )                                           {
       globalDebugger -> printf ( "%s: periods min = %lu, max = %lu, req = %lu \n",
                                  __FUNCTION__                               ,
                                  minPeriods                                 ,
                                  maxPeriods                                 ,
                                  numPeriods                               ) ;
       globalDebugger -> printf ( "%s: suggested host buffer period   = %lu \n",
                                  __FUNCTION__                               ,
                                  framesPerHostBuffer                      ) ;
     }                                                                       ;
     #endif
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  {                                                                          ;
    if ( ! FixedFrameSize )                                                  {
      /* Get min/max period sizes and adjust our chosen                     */
      snd_pcm_uframes_t min = 0                                              ;
      snd_pcm_uframes_t max = 0                                              ;
      snd_pcm_uframes_t minmax_diff                                          ;
      ENSURE_( alsa_snd_pcm_hw_params_get_period_size_min(hwParams,&min,NULL),
               UnanticipatedHostError                                      ) ;
      ENSURE_( alsa_snd_pcm_hw_params_get_period_size_max(hwParams,&max,NULL),
               UnanticipatedHostError                                      ) ;
      minmax_diff = max - min                                                ;
      if ( framesPerHostBuffer < min )                                       {
        gPrintf                                                            ( (
          "%s: The determined period size (%lu) is less than minimum (%lu)\n",
          __FUNCTION__                                                       ,
          framesPerHostBuffer                                                ,
          min                                                            ) ) ;
        framesPerHostBuffer = (( minmax_diff == 2 ) ? min + 1 : min )        ;
      } else
      if ( framesPerHostBuffer > max )                                       {
        gPrintf                                                            ( (
          "%s: The determined period size (%lu) is greater than maximum (%lu)\n",
          __FUNCTION__                                                       ,
          framesPerHostBuffer                                                ,
          max                                                            ) ) ;
        framesPerHostBuffer = ( ( minmax_diff == 2 ) ? max - 1 : max )       ;
      }                                                                      ;
      #ifndef REMOVE_DEBUG_MESSAGE
      if ( NULL != globalDebugger )                                          {
        globalDebugger -> printf ( "%s: device period minimum        = %lu\n",
                                   __FUNCTION__                              ,
                                   min                                     ) ;
        globalDebugger -> printf ( "%s: device period maximum        = %lu\n",
                                   __FUNCTION__                              ,
                                   max                                     ) ;
        globalDebugger -> printf ( "%s: host buffer period           = %lu\n",
                                   __FUNCTION__                              ,
                                   framesPerHostBuffer                     ) ;
        globalDebugger -> printf ( "%s: host buffer period latency   = %f\n" ,
                                   __FUNCTION__                              ,
                                   (double)(framesPerHostBuffer/sampleRate)) ;
      }                                                                      ;
      #endif
    }                                                                        ;
    /* Try setting period size                                              */
    dir = 0                                                                  ;
    ENSURE_ ( alsa_snd_pcm_hw_params_set_period_size_near                    (
                pcm                                                          ,
                hwParams                                                     ,
                &framesPerHostBuffer                                         ,
                &dir                                                       ) ,
              UnanticipatedHostError                                       ) ;
    if ( dir != 0 )                                                          {
      gPrintf ( ( "%s: The configured period size is non-integer.\n"         ,
                  __FUNCTION__                                               ,
                  dir                                                    ) ) ;
      *accurate = 0                                                          ;
    }                                                                        ;
  }                                                                          ;
  /* Set result                                                             */
  this->framesPerPeriod = framesPerHostBuffer                                ;
error                                                                        :
  return result                                                              ;
}

/* Called after buffer processing is finished.
 * A number of mmapped frames is committed, it is possible that an xrun has occurred in the meantime. */

CaError AlsaStreamComponent::EndProcessing(unsigned long numFrames,int * xrun)
{
  CaError           result      = NoError                                    ;
  int               res         = 0                                          ;
  if ( ! ready ) goto end                                                    ;
  if ( ! canMmap && StreamDirection_Out == streamDir )                       {
    /* Play sound                                                           */
    if ( hostInterleaved )                                                   {
      res = alsa_snd_pcm_writei ( pcm , nonMmapBuffer , numFrames )          ;
    } else                                                                   {
      void          * bufs [ numHostChannels ]                               ;
      unsigned char * buffer  = (unsigned char *)nonMmapBuffer               ;
      int             bufsize                                                ;
      int             i                                                      ;
      bufsize = alsa_snd_pcm_format_size ( nativeFormat,framesPerPeriod+1  ) ;
      for ( i = 0 ; i < numHostChannels ; ++i )                              {
        bufs[i] = buffer                                                     ;
        buffer += bufsize                                                    ;
      }                                                                      ;
      res = alsa_snd_pcm_writen ( pcm , bufs , numFrames )                   ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( canMmap )                                                             {
    res = alsa_snd_pcm_mmap_commit ( pcm , offset , numFrames )              ;
  }                                                                          ;
  if ( res == -EPIPE || res == -ESTRPIPE )                                   {
    *xrun = 1                                                                ;
  } else                                                                     {
    ENSURE_ ( res , UnanticipatedHostError )                                 ;
  }                                                                          ;
end                                                                          :
error                                                                        :
  return result                                                              ;
}

/* Do necessary adaption between user and host channels. */
CaError AlsaStreamComponent::DoChannelAdaption(BufferProcessor * bp,int numFrames)
{
  CaError         result = NoError                                           ;
  unsigned char * p                                                          ;
  int             i                                                          ;
  int             unusedChans = numHostChannels - numUserChannels            ;
  unsigned char * src                                                        ;
  unsigned char * dst                                                        ;
  int             convertMono = ( numHostChannels % 2 ) == 0                &&
                                ( numUserChannels % 2 ) != 0                 ;
  ////////////////////////////////////////////////////////////////////////////
  if ( hostInterleaved )                                                     {
    int             swidth = alsa_snd_pcm_format_size(nativeFormat,1)        ;
    unsigned char * buffer = (unsigned char *) ( canMmap                     ?
                             ExtractAddress(channelAreas,offset)             :
                             nonMmapBuffer                                 ) ;
    /* Start after the last user channel                                    */
    p = buffer + numUserChannels * swidth                                    ;
    if ( convertMono )                                                       {
      /* Convert the last user channel into stereo pair                     */
      src = buffer + ( numUserChannels - 1 ) * swidth                        ;
      for ( i = 0; i < numFrames; ++i )                                      {
        dst  = src + swidth                                                  ;
        memcpy ( dst, src, swidth )                                          ;
        src += numHostChannels * swidth                                      ;
      }                                                                      ;
      /* Don't touch the channel we just wrote to                           */
      p += swidth                                                            ;
      --unusedChans                                                          ;
    }                                                                        ;
    if ( unusedChans > 0 )                                                   {
      /* Silence unused output channels                                     */
      for ( i = 0; i < numFrames; ++i )                                      {
        memset ( p , 0 , swidth * unusedChans )                              ;
        p += numHostChannels * swidth                                        ;
      }                                                                      ;
    }                                                                        ;
  } else                                                                     {
    /* We extract the last user channel                                     */
    if ( convertMono )                                                       {
      ENSURE_ ( alsa_snd_pcm_area_copy                                       (
                  channelAreas + numUserChannels                             ,
                  offset                                                     ,
                  channelAreas + numUserChannels - 1                         ,
                  offset                                                     ,
                  numFrames                                                  ,
                  nativeFormat                                             ) ,
                UnanticipatedHostError                                     ) ;
      --unusedChans                                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( unusedChans > 0 )                                                   {
      alsa_snd_pcm_areas_silence                                             (
        channelAreas + numHostChannels - unusedChans                         ,
        offset                                                               ,
        unusedChans                                                          ,
        numFrames                                                            ,
        nativeFormat                                                       ) ;
    }                                                                        ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError AlsaStreamComponent::AvailableFrames(CaUint32 * numFrames,int * xrunOccurred)
{
  CaError           result      = NoError                        ;
  snd_pcm_sframes_t framesAvail = alsa_snd_pcm_avail_update(pcm) ;
  *xrunOccurred = 0                                              ;
  if ( -EPIPE == framesAvail )                                   {
    *xrunOccurred = 1                                            ;
    framesAvail   = 0                                            ;
  }  else                                                        {
    ENSURE_ ( framesAvail , UnanticipatedHostError )             ;
  }                                                              ;
  *numFrames = framesAvail                                       ;
error                                                            :
  return result                                                  ;
}

CaError AlsaStreamComponent::BeginPolling(struct pollfd * pfds)
{
  CaError result = NoError                             ;
  int     ret                                          ;
  ret   = alsa_snd_pcm_poll_descriptors(pcm,pfds,nfds) ;
  ready = 0                                            ;
  return result                                        ;
}

CaError AlsaStreamComponent::EndPolling(struct pollfd * pfds,int * shouldPoll,int * xrun)
{
  CaError        result = NoError                                            ;
  unsigned short revents                                                     ;
  ////////////////////////////////////////////////////////////////////////////
  ENSURE_ ( alsa_snd_pcm_poll_descriptors_revents( pcm,pfds,nfds,&revents  ) ,
            UnanticipatedHostError                                         ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( revents != 0 )                                                        {
    if ( revents & POLLERR )                                                 {
      *xrun = 1                                                              ;
    } else
    if ( revents & POLLHUP )                                                 {
      *xrun = 1                                                              ;
      #ifndef REMOVE_DEBUG_MESSAGE
      if ( NULL != globalDebugger )                                          {
        globalDebugger -> printf                                             (
          "%s: revents has POLLHUP, processing as XRUN\n"                    ,
          __FUNCTION__                                                     ) ;
      }                                                                      ;
      #endif
    } else ready = 1                                                         ;
    *shouldPoll = 0                                                          ;
  } else
  if ( useReventFix )                                                        {
    ready       = 1                                                          ;
    *shouldPoll = 0                                                          ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError AlsaStreamComponent::RegisterChannels (
          BufferProcessor * bp                ,
          unsigned long   * numFrames         ,
          int             * xrun              )
{
  CaError                        result = NoError                            ;
  const snd_pcm_channel_area_t * areas                                       ;
  const snd_pcm_channel_area_t * area                                        ;
  unsigned char                * buffer                                      ;
  unsigned char                * p                                           ;
  int                            i                                           ;
  CaUint32                       framesAvail                                 ;
  ////////////////////////////////////////////////////////////////////////////
  /* This _must_ be called before mmap_begin                                */
  CA_ENSURE ( AvailableFrames ( &framesAvail, xrun ) )                       ;
  if ( *xrun )                                                               {
    *numFrames = 0                                                           ;
    goto end                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( this -> canMmap )                                                     {
    ENSURE_( alsa_snd_pcm_mmap_begin( this->pcm                              ,
                                      &areas                                 ,
                                      &this->offset                          ,
                                      numFrames                            ) ,
             UnanticipatedHostError                                        ) ;
    this -> channelAreas = (snd_pcm_channel_area_t *)areas                   ;
  } else                                                                     {
    unsigned int bufferSize = this -> numHostChannels                        *
                              alsa_snd_pcm_format_size( this->nativeFormat   ,
                                                        *numFrames         ) ;
    if ( bufferSize > this->nonMmapBufferSize )                              {
      this->nonMmapBuffer = realloc ( this->nonMmapBuffer                    ,
                                    ( this->nonMmapBufferSize=bufferSize ) ) ;
      if ( ! this -> nonMmapBuffer )                                         {
        result = InsufficientMemory                                          ;
        goto error                                                           ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( this->hostInterleaved )                                               {
    int swidth = alsa_snd_pcm_format_size( this->nativeFormat , 1 )          ;
    p = buffer = (unsigned char *)( this->canMmap                            ?
                   ExtractAddress ( areas , this->offset )                   :
                   this->nonMmapBuffer                                     ) ;
    for ( i = 0 ; i < this->numUserChannels; ++i )                           {
      /* We're setting the channels up to userChannels, but the stride will
       * be hostChannels samples                                            */
      if ( StreamDirection_In == this -> streamDir )                         {
        bp -> setInputChannel  ( 0 , i , p , this->numHostChannels )         ;
      } else                                                                 {
        bp -> setOutputChannel ( 0 , i , p , this->numHostChannels )         ;
      }                                                                      ;
      p += swidth                                                            ;
    }                                                                        ;
  } else                                                                     {
    if ( this->canMmap )                                                     {
      for ( i = 0; i < this->numUserChannels; ++i )                          {
        area = areas + i                                                     ;
        buffer = ExtractAddress ( area , this -> offset )                    ;
        if ( StreamDirection_In == this -> streamDir )                       {
          bp -> setInputChannel  ( 0 , i , buffer , 1 )                      ;
        } else                                                               {
          bp -> setOutputChannel ( 0 , i , buffer , 1 )                      ;
        }                                                                    ;
      }                                                                      ;
    } else                                                                   {
      unsigned int buf_per_ch_size                                           ;
      buf_per_ch_size = this->nonMmapBufferSize / this->numHostChannels      ;
      buffer          = (unsigned char *)this->nonMmapBuffer                 ;
      for ( i = 0 ; i < this -> numUserChannels ; ++i )                      {
        if ( StreamDirection_In == this -> streamDir )                       {
          bp -> setInputChannel  ( 0 , i , buffer , 1 )                      ;
        } else                                                               {
          bp -> setOutputChannel ( 0 , i , buffer , 1 )                      ;
        }                                                                    ;
        buffer += buf_per_ch_size                                            ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ! this->canMmap && StreamDirection_In == this->streamDir )            {
    /* Read sound                                                           */
    int res                                                                  ;
    if ( this->hostInterleaved )                                             {
      res = alsa_snd_pcm_readi ( this->pcm,this->nonMmapBuffer,*numFrames )  ;
    } else                                                                   {
      void          * bufs [ this -> numHostChannels ]                       ;
      unsigned char * buffer = (unsigned char *)this->nonMmapBuffer          ;
      unsigned int    buf_per_ch_size                                        ;
      int             i                                                      ;
      buf_per_ch_size = this -> nonMmapBufferSize / this -> numHostChannels  ;
      for ( i = 0 ; i < this->numHostChannels ; ++i )                        {
        bufs[i] = buffer                                                     ;
        buffer += buf_per_ch_size                                            ;
      }                                                                      ;
      res = alsa_snd_pcm_readn ( this -> pcm , bufs , *numFrames )           ;
    }                                                                        ;
    if ( res == -EPIPE || res == -ESTRPIPE )                                 {
      *xrun      = 1                                                         ;
      *numFrames = 0                                                         ;
    }                                                                        ;
  }                                                                          ;
end                                                                          :
error                                                                        :
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

AlsaStream:: AlsaStream (void)
           : Stream     (    )
{
  framesPerUserBuffer    = 0                                                 ;
  maxFramesPerHostBuffer = 0                                                 ;
  primeBuffers           = 0                                                 ;
  callbackMode           = 0                                                 ;
  pcmsSynced             = 0                                                 ;
  pfds                   = NULL                                              ;
  pollTimeout            = 0                                                 ;
  callback_finished      = 1                                                 ;
  callbackAbort          = 0                                                 ;
  isActive               = 0                                                 ;
  neverDropInput         = 0                                                 ;
  underrun               = 0                                                 ;
  overrun                = 0                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  capture                . Reset ( )                                         ;
  playback               . Reset ( )                                         ;
  ////////////////////////////////////////////////////////////////////////////
  thread                 = 0                                                 ;
  rtSched                = 0                                                 ;
  parentWaiting          = 0                                                 ;
  stopRequested          = 0                                                 ;
  locked                 = 0                                                 ;
  stopRequest            = 0                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  memset ( &stateMtx   , 0 , sizeof(pthread_mutex_t) )                       ;
  memset ( &threadCond , 0 , sizeof(pthread_cond_t ) )                       ;
}

AlsaStream::~AlsaStream(void)
{
}

CaError AlsaStream::ThreadStart(void)
{
  CaError        result  = NoError                                           ;
  int            started = 0                                                 ;
  pthread_attr_t attr                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  thread        = 0                                                          ;
  rtSched       = 0                                                          ;
  parentWaiting = 0                                                          ;
  stopRequested = 0                                                          ;
  locked        = 0                                                          ;
  stopRequest   = 0                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  memset ( &stateMtx   , 0 , sizeof(pthread_mutex_t) )                       ;
  memset ( &threadCond , 0 , sizeof(pthread_cond_t ) )                       ;
  memset ( &attr       , 0 , sizeof(pthread_attr_t ) )                       ;
  ////////////////////////////////////////////////////////////////////////////
  MutexInitialize ( )                                                        ;
  CA_ASSERT_CALL  ( ::pthread_cond_init ( &threadCond, NULL ) , 0 )          ;
  parentWaiting = 1                                                          ;
  CA_UNLESS ( !pthread_attr_init ( &attr ) , InternalError )                 ;
  CA_UNLESS ( !pthread_attr_setscope ( &attr, PTHREAD_SCOPE_SYSTEM )         ,
              InternalError                                                ) ;
  CA_UNLESS ( !pthread_create ( &thread, &attr, CallbackThreadFunc, this )   ,
              InternalError                                                ) ;
  started = 1                                                                ;
  if ( 0 != rtSched )                                                        {
    int                policy                                                ;
    struct sched_param spm                                                   ;
    CA_ENSURE ( BoostPriority ( ) )                                          ;
    ::pthread_getschedparam ( thread , &policy , &spm )                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( parentWaiting )                                                       {
    CaTime          till                                                     ;
    struct timespec ts                                                       ;
    int             res = 0                                                  ;
    CaTime          now                                                      ;
    //////////////////////////////////////////////////////////////////////////
    CA_ENSURE ( MutexLock ( ) )                                              ;
    /* Wait for stream to be started                                        */
    now  = Timer::Time()                                                     ;
    till = now + 1.0                                                         ;
    while ( ( 0 != parentWaiting ) && ( 0 == res ) )                         {
      ts.tv_sec = (time_t) floor ( till )                                    ;
      ts.tv_nsec = (long) ( ( till - floor ( till ) ) * 1e9 )                ;
      res = ::pthread_cond_timedwait ( &threadCond , &stateMtx , &ts       ) ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    CA_ENSURE ( MutexUnlock ( )                                            ) ;
    CA_UNLESS ( ( 0 == res ) || ( ETIMEDOUT == res ) , InternalError       ) ;
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( NULL != debugger )                                                  {
      debugger -> printf                                                     (
        "%s: Waited for %g seconds for stream to start\n"                    ,
        __FUNCTION__                                                         ,
        Timer::Time() - now                                                ) ;
    }                                                                        ;
    #endif
    if ( ETIMEDOUT == res )                                                  {
      CA_ENSURE ( TimedOut )                                                 ;
    }                                                                        ;
  }                                                                          ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  if ( started )                                                             {
    ThreadTerminate ( 0 , NULL )                                             ;
  }                                                                          ;
  goto end                                                                   ;
  return NoError                                                             ;
}

CaError AlsaStream::ThreadTerminate(int wait,CaError * exitResult)
{
  CaError result = NoError                                                   ;
  void *  pret                                                               ;
  if ( NULL != exitResult ) *exitResult = NoError                            ;
  stopRequested = wait                                                       ;
  stopRequest   = wait                                                       ;
#warning pthread_cancel need to have a replacement
//  ::pthread_cancel ( thread )                                                ;
  while ( 1 != callback_finished ) Timer :: Sleep ( 10 )                     ;
  ////////////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( NULL != debugger )                                                    {
    if ( 0 == wait )                                                         {
      debugger -> printf                                                     (
        "%s: Canceling thread %d\n"                                          ,
        __FUNCTION__                                                         ,
        thread                                                             ) ;
    }                                                                        ;
    debugger -> printf ( "%s: Joining thread %d\n" , __FUNCTION__ , thread ) ;
  }                                                                          ;
  #endif
  CA_ENSURE_SYSTEM ( ::pthread_join( thread, &pret ) , 0 )                   ;
  ////////////////////////////////////////////////////////////////////////////
  /* !wait means the thread may have been canceled                          */
  if ( ( NULL != pret ) && ( 0 != wait ) )                                   {
    if ( NULL != exitResult ) *exitResult = *(CaError *)pret                 ;
    ::free ( pret )                                                          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  ////////////////////////////////////////////////////////////////////////////
  CA_ASSERT_CALL ( MutexTerminate         (             ) , NoError )        ;
  CA_ASSERT_CALL ( ::pthread_cond_destroy ( &threadCond ) , 0       )        ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

CaError AlsaStream::PrepareNotify(void)
{
  CaError result = NoError                    ;
  CA_UNLESS ( parentWaiting , InternalError ) ;
  CA_ENSURE ( MutexLock ( )                 ) ;
  locked = 1                                  ;
error                                         :
  return result                               ;
}

CaError AlsaStream::NotifyParent(void)
{
  CaError result = NoError                    ;
  CA_UNLESS ( parentWaiting , InternalError ) ;
  if ( 0 == locked )                          {
    CA_ENSURE ( MutexLock ( ) )               ;
    locked = 1                                ;
  }                                           ;
  parentWaiting = 0                           ;
  ::pthread_cond_signal ( &threadCond )       ;
  CA_ENSURE ( MutexUnlock ( ) )               ;
  locked = 0                                  ;
error                                         :
  return result                               ;
}

CaError AlsaStream::BoostPriority(void)
{
  CaError            result = NoError                                        ;
  struct sched_param spm    = { 0 }                                          ;
  /* Priority should only matter between contending FIFO threads?           */
  spm . sched_priority = 1                                                   ;
  if ( 0 != pthread_setschedparam ( thread , SCHED_FIFO , &spm ) )           {
    /* Lack permission to raise priority                                    */
    CA_UNLESS ( errno == EPERM, InternalError )                              ;
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( NULL != debugger )                                                  {
      debugger -> printf ( "Failed bumping priority\n" )                     ;
    }                                                                        ;
    #endif
    result = 0                                                               ;
  } else                                                                     {
    /* Success                                                              */
    result = 1                                                               ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError AlsaStream::Start(void)
{
  CaError result        = NoError                                            ;
  int     streamStarted = 0                                                  ;
  ////////////////////////////////////////////////////////////////////////////
  /* Ready the processor                                                    */
  bufferProcessor . Reset ( )                                                ;
  /* Set now, so we can test for activity further down                      */
  isActive = 1                                                               ;
  if ( callbackMode )                                                        {
    CA_ENSURE( ThreadStart ( ) )                                             ;
  } else                                                                     {
    CA_ENSURE( Start ( 0 ) )                                                 ;
    streamStarted = 1                                                        ;
  }                                                                          ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  if ( streamStarted ) Abort ( )                                             ;
  isActive = 0                                                               ;
  goto end                                                                   ;
}

CaError AlsaStream::Stop(void)
{
  return RealStop ( 0 ) ;
}

CaError AlsaStream::Close(void)
{
  CaError result = NoError        ;
  bufferProcessor . Terminate ( ) ;
  Terminate                   ( ) ;
  return result                   ;
}

CaError AlsaStream::Abort(void)
{
  return RealStop ( 1 ) ;
}

CaError AlsaStream::IsStopped(void)
{
  return ( 0 == isActive ) && ( 0 != callback_finished ) ;
}

CaError AlsaStream::IsActive(void)
{
  return ( 0 != isActive ) ;
}

CaTime AlsaStream::GetTime(void)
{
  snd_timestamp_t    timestamp                                   ;
  snd_pcm_status_t * status                                      ;
  ////////////////////////////////////////////////////////////////
  alsa_snd_pcm_status_alloca ( &status )                         ;
  if ( capture  . pcm )                                          {
    alsa_snd_pcm_status ( capture  . pcm , status )              ;
  } else
  if ( playback . pcm )                                          {
    alsa_snd_pcm_status ( playback . pcm , status )              ;
  }                                                              ;
  alsa_snd_pcm_status_get_tstamp ( status, &timestamp )          ;
  return timestamp . tv_sec + (CaTime) timestamp . tv_usec / 1e6 ;
}

double AlsaStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer . Value ( ) ;
}

CaInt32 AlsaStream::ReadAvailable(void)
{
  CaError  result = NoError                                   ;
  CaUint32 avail                                              ;
  int      xrun                                               ;
  CA_ENSURE ( capture  . AvailableFrames ( &avail , &xrun ) ) ;
  if ( xrun )                                                 {
    CA_ENSURE( HandleXrun ( )                               ) ;
    CA_ENSURE( capture . AvailableFrames ( &avail , &xrun ) ) ;
    if ( xrun ) CA_ENSURE( InputOverflowed )                  ;
  }                                                           ;
  return (CaInt32)avail                                       ;
error                                                         :
  return result                                               ;
}

CaInt32 AlsaStream::WriteAvailable(void)
{
  CaError  result = NoError                                   ;
  CaUint32 avail                                              ;
  int      xrun                                               ;
  CA_ENSURE ( playback . AvailableFrames ( &avail , &xrun ) ) ;
  if ( xrun )                                                 {
    snd_pcm_sframes_t savail                                  ;
    CA_ENSURE( HandleXrun ( ) )                               ;
    savail = alsa_snd_pcm_avail_update ( playback . pcm )     ;
    ENSURE_( savail , UnanticipatedHostError )                ;
    avail = (unsigned long) savail                            ;
  }                                                           ;
  return (CaInt32) avail                                      ;
error                                                         :
  return result                                               ;
}

CaError AlsaStream::Read(void * buffer,unsigned long frames)
{
  CaError       result = NoError                                             ;
  snd_pcm_t   * save   = playback . pcm                                      ;
  unsigned long framesGot                                                    ;
  unsigned long framesAvail                                                  ;
  void        * userBuffer                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  CA_UNLESS ( capture . pcm , CanNotReadFromAnOutputOnlyStream )             ;
  playback . pcm = NULL                                                      ;
  if ( overrun > 0.0 )                                                       {
    result  = InputOverflowed                                                ;
    overrun = 0.0                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( capture . userInterleaved )                                           {
    userBuffer = buffer                                                      ;
  } else                                                                     {
    userBuffer = capture . userBuffers                                       ;
    ::memcpy ( userBuffer,buffer,sizeof(void *)*capture.numUserChannels )    ;
  }                                                                          ;
  /* Start stream if in prepared state                                      */
  if ( alsa_snd_pcm_state( capture.pcm ) == SND_PCM_STATE_PREPARED )         {
    ENSURE_ ( alsa_snd_pcm_start( capture.pcm ) , UnanticipatedHostError   ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  while ( frames > 0 )                                                       {
    int xrun = 0                                                             ;
    CA_ENSURE( WaitForFrames ( &framesAvail , &xrun ) )                      ;
    framesGot = CA_MIN ( framesAvail , frames )                              ;
    CA_ENSURE( SetUpBuffers  ( &framesGot   , &xrun ) )                      ;
    if ( framesGot > 0 )                                                     {
      framesGot = bufferProcessor . CopyInput ( &userBuffer , framesGot )    ;
      CA_ENSURE ( EndProcessing ( framesGot , &xrun ) )                      ;
      frames -= framesGot                                                    ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
end                                                                          :
  playback . pcm = save                                                      ;
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

CaError AlsaStream::Write(const void * buffer,unsigned long frames)
{
  CaError           result = NoError                                         ;
  snd_pcm_t       * save   = capture . pcm                                   ;
  snd_pcm_uframes_t framesGot                                                ;
  snd_pcm_uframes_t framesAvail                                              ;
  const void      * userBuffer                                               ;
  signed long       err                                                      ;
  ////////////////////////////////////////////////////////////////////////////
  CA_UNLESS ( playback . pcm , CanNotWriteToAnInputOnlyStream )              ;
  /* Disregard capture                                                      */
  capture . pcm = NULL                                                       ;
  if ( underrun > 0.0 )                                                      {
    result   = OutputUnderflowed                                             ;
    underrun = 0.0                                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( playback . userInterleaved )                                          {
    userBuffer = buffer                                                      ;
  } else                                                                     {
    userBuffer = playback . userBuffers                                      ;
    memcpy ( (void *)userBuffer                                              ,
             buffer                                                          ,
             sizeof(void *) * playback . numUserChannels                   ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  while ( frames > 0 )                                                       {
    int               xrun = 0                                               ;
    snd_pcm_uframes_t hwAvail                                                ;
    //////////////////////////////////////////////////////////////////////////
    CA_ENSURE ( WaitForFrames ( &framesAvail , &xrun ) )                     ;
    framesGot = CA_MIN ( framesAvail, frames )                               ;
    CA_ENSURE ( SetUpBuffers  ( &framesGot   , &xrun ) )                     ;
    //////////////////////////////////////////////////////////////////////////
    if ( framesGot > 0 )                                                     {
      framesGot = bufferProcessor . CopyOutput ( &userBuffer , framesGot )   ;
      CA_ENSURE ( EndProcessing ( framesGot , &xrun ) )                      ;
      frames   -= framesGot                                                  ;
    }                                                                        ;
    /* Start stream after one period of samples worth                       */
    /* Frames residing in buffer                                            */
    CA_ENSURE ( err = WriteAvailable ( ) )                                   ;
    framesAvail = err                                                        ;
    hwAvail     = playback . alsaBufferSize - framesAvail                    ;
    if ( ( alsa_snd_pcm_state( playback.pcm ) == SND_PCM_STATE_PREPARED  )  &&
         ( hwAvail >= playback.framesPerPeriod                           ) ) {
      ENSURE_ ( alsa_snd_pcm_start(playback.pcm) , UnanticipatedHostError  ) ;
    }                                                                        ;
  }                                                                          ;
end                                                                          :
  capture . pcm = save                                                       ;
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

bool AlsaStream::hasVolume(void)
{
  snd_mixer_t          * handle = NULL ;
  snd_mixer_elem_t     * elem   = NULL ;
  snd_mixer_selem_id_t * sid    = NULL ;
  return true                          ;
}

CaVolume AlsaStream::MinVolume(void)
{
  return 0.0 ;
}

CaVolume AlsaStream::MaxVolume(void)
{
  return 10000.0 ;
}

#warning Do not forget ALSA Volume Control functionalities

CaVolume AlsaStream::Volume(int atChannel)
{
  return 0 ;
}

CaVolume AlsaStream::setVolume(CaVolume volume,int atChannel)
{
  return 0 ;
}

#ifdef XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

static const char* mix_name = "Master";
static const char* card = "default";
static int mix_index = 0;

long pmin, pmax;
long get_vol, set_vol;
float f_multi;

snd_mixer_selem_id_alloca(&sid);

//sets simple-mixer index and name
snd_mixer_selem_id_set_index(sid, mix_index);
snd_mixer_selem_id_set_name(sid, mix_name);

    if ((snd_mixer_open(&handle, 0)) < 0)
    return -1;
if ((snd_mixer_attach(handle, card)) < 0) {
    snd_mixer_close(handle);
    return -2;
}
if ((snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
    snd_mixer_close(handle);
    return -3;
}
ret = snd_mixer_load(handle);
if (ret < 0) {
    snd_mixer_close(handle);
    return -4;
}
elem = snd_mixer_find_selem(handle, sid);
if (!elem) {
    snd_mixer_close(handle);
    return -5;
}

long minv, maxv;

snd_mixer_selem_get_playback_volume_range (elem, &minv, &maxv);
fprintf(stderr, "Volume range <%i,%i>\n", minv, maxv);

if(action == AUDIO_VOLUME_GET) {
    if(snd_mixer_selem_get_playback_volume(elem, 0, outvol) < 0) {
        snd_mixer_close(handle);
        return -6;
    }

    fprintf(stderr, "Get volume %i with status %i\n", *outvol, ret);
    /* make the value bound to 100 */
    *outvol -= minv;
    maxv -= minv;
    minv = 0;
    *outvol = 100 * (*outvol) / maxv; // make the value bound from 0 to 100
}
else if(action == AUDIO_VOLUME_SET) {
    if(*outvol < 0 || *outvol > VOLUME_BOUND) // out of bounds
        return -7;
    *outvol = (*outvol * (maxv - minv) / (100-1)) + minv;

    if(snd_mixer_selem_set_playback_volume(elem, 0, *outvol) < 0) {
        snd_mixer_close(handle);
        return -8;
    }
    if(snd_mixer_selem_set_playback_volume(elem, 1, *outvol) < 0) {
        snd_mixer_close(handle);
        return -9;
    }
    fprintf(stderr, "Set volume %i with status %i\n", *outvol, ret);
}

snd_mixer_close(handle);
return 0;

#endif

void AlsaStream::EnableRealtimeScheduling(int enable)
{
  rtSched = enable ;
}

CaError AlsaStream::GetStreamInputCard(int * card)
{
  CaError          result = NoError                           ;
  snd_pcm_info_t * pcmInfo                                    ;
  CA_UNLESS ( capture . pcm , DeviceUnavailable )             ;
  alsa_snd_pcm_info_alloca ( &pcmInfo )                       ;
  CA_ENSURE ( alsa_snd_pcm_info ( capture . pcm , pcmInfo ) ) ;
  *card = alsa_snd_pcm_info_get_card ( pcmInfo )              ;
error                                                         :
  return result                                               ;
}

CaError AlsaStream::GetStreamOutputCard(int * card)
{
  CaError          result  = NoError                           ;
  snd_pcm_info_t * pcmInfo = NULL                              ;
  //////////////////////////////////////////////////////////////
  CA_UNLESS ( playback . pcm , DeviceUnavailable )             ;
  alsa_snd_pcm_info_alloca ( &pcmInfo )                        ;
  CA_ENSURE ( alsa_snd_pcm_info ( playback . pcm , pcmInfo ) ) ;
  *card = alsa_snd_pcm_info_get_card ( pcmInfo )               ;
error                                                          :
  return result                                                ;
}

CaError AlsaStream::SetUpBuffers(unsigned long * numFrames,int * xrunOccurred)
{
  CaError       result         = NoError                                     ;
  unsigned long captureFrames  = ULONG_MAX                                   ;
  unsigned long playbackFrames = ULONG_MAX                                   ;
  unsigned long commonFrames   = 0                                           ;
  int           xrun           = 0                                           ;
  ////////////////////////////////////////////////////////////////////////////
  if ( *xrunOccurred )                                                       {
    *numFrames = 0                                                           ;
    return result                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  CA_UNLESS ( this->capture.ready || this->playback.ready , InternalError )  ;
  if ( this->capture.pcm && this->capture.ready )                            {
    captureFrames = *numFrames                                               ;
    CA_ENSURE ( this -> capture . RegisterChannels                           (
                  &this->bufferProcessor                                     ,
                  &captureFrames                                             ,
                  &xrun                                                  ) ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( this->playback.pcm && this->playback.ready )                          {
    playbackFrames = *numFrames                                              ;
    CA_ENSURE ( this -> playback . RegisterChannels                          (
                  &this->bufferProcessor                                     ,
                  &playbackFrames                                            ,
                  &xrun                                                  ) ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( xrun )                                                                {
    /* Nothing more to do                                                   */
    assert( 0 == commonFrames )                                              ;
    goto end                                                                 ;
  }                                                                          ;
  commonFrames = CA_MIN ( captureFrames , playbackFrames )                   ;
  if ( commonFrames > *numFrames )                                           {
    //////////////////////////////////////////////////////////////////////////
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( NULL != debugger )                                            {
      debugger -> printf                                               (
        "%s: Common available frames are reported to be more than number requested: %lu, %lu, callbackMode: %d\n",
        __FUNCTION__                                                         ,
        commonFrames                                                         ,
        *numFrames                                                           ,
        this -> callbackMode                                               ) ;
    }                                                                        ;
    #endif
    //////////////////////////////////////////////////////////////////////////
    if ( this -> capture  . pcm )                                            {
      #ifndef REMOVE_DEBUG_MESSAGE
      if ( NULL != debugger )                                          {
        debugger -> printf                                             (
          "%s: captureFrames: %lu, capture.ready: %d\n"                      ,
          __FUNCTION__                                                       ,
          captureFrames                                                      ,
          this -> capture . ready                                          ) ;
      }                                                                      ;
      #endif
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( this -> playback . pcm )                                            {
      #ifndef REMOVE_DEBUG_MESSAGE
      if ( NULL != debugger )                                          {
        debugger -> printf                                             (
          "%s: playbackFrames: %lu, playback.ready: %d\n"                    ,
          __FUNCTION__                                                       ,
          playbackFrames                                                     ,
          this -> playback . ready                                         ) ;
      }                                                                      ;
      #endif
    }                                                                        ;
    commonFrames = 0                                                         ;
    goto end                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( this -> capture . pcm )                                               {
    if ( this -> capture . ready )                                           {
      this -> bufferProcessor . setInputFrameCount ( 0 , commonFrames )      ;
    } else                                                                   {
      /* We have input underflow                                            */
      this -> bufferProcessor . setNoInput ( )                               ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( this->playback.pcm )                                                  {
    if ( this->playback.ready )                                              {
      this -> bufferProcessor . setOutputFrameCount ( 0 , commonFrames )     ;
    } else                                                                   {
      /* We have output underflow, but keeping input data (NeverDropInput)  */
      assert( this -> neverDropInput        )                                ;
      assert( this -> capture . pcm != NULL )                                ;
      #ifndef REMOVE_DEBUG_MESSAGE
      if ( NULL != debugger )                                          {
        debugger -> printf                                             (
          "%s: Setting output buffers to NULL\n"                             ,
          __FUNCTION__                                                     ) ;
      }                                                                      ;
      #endif
      this -> bufferProcessor . setNoOutput ( )                              ;
    }                                                                        ;
  }                                                                          ;
end                                                                          :
  *numFrames = commonFrames                                                  ;
error                                                                        :
  if ( xrun )                                                                {
    CA_ENSURE ( HandleXrun( ) )                                              ;
    *numFrames = 0                                                           ;
  }                                                                          ;
  *xrunOccurred = xrun                                                       ;
  return result                                                              ;
}

CaError AlsaStream::AvailableFrames     (
          int             queryCapture  ,
          int             queryPlayback ,
          unsigned long * available     ,
          int           * xrunOccurred  )
{
  CaError  result = NoError                                                  ;
  CaUint32 captureFrames                                                     ;
  CaUint32 playbackFrames                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  *xrunOccurred = 0                                                          ;
  if ( queryCapture )                                                        {
    CA_ENSURE( capture . AvailableFrames ( &captureFrames , xrunOccurred ) ) ;
    if ( *xrunOccurred ) goto end                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( queryPlayback )                                                       {
    CA_ENSURE( playback. AvailableFrames ( &playbackFrames, xrunOccurred ) ) ;
    if ( *xrunOccurred ) goto end                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( queryCapture && queryPlayback )                                       {
    *available = CA_MIN ( captureFrames , playbackFrames )                   ;
  } else
  if ( queryCapture )                                                        {
    *available = captureFrames                                               ;
  } else                                                                     {
    *available = playbackFrames                                              ;
  }                                                                          ;
end                                                                          :
error                                                                        :
  return result                                                              ;
}

CaError AlsaStream::WaitForFrames(unsigned long * framesAvail,int * xrunOccurred)
{
  CaError result       = NoError                                             ;
  int     pollPlayback = playback . pcm != NULL                              ;
  int     pollCapture  = capture  . pcm != NULL                              ;
  int     PollTimeout  = pollTimeout                                         ;
  int     xrun         = 0                                                   ;
  int     timeouts     = 0                                                   ;
  int     pollResults                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  assert ( framesAvail )                                                     ;
  if ( 0 == callbackMode )                                                   {
    CA_ENSURE ( AvailableFrames                                              (
                  capture  . pcm != NULL                                     ,
                  playback . pcm != NULL                                     ,
                  framesAvail                                                ,
                  &xrun                                                  ) ) ;
    if ( xrun ) goto end                                                     ;
    if ( *framesAvail > 0 )                                                  {
      if ( NULL != capture  . pcm ) capture  . ready = 1                     ;
      if ( NULL != playback . pcm ) playback . ready = 1                     ;
      goto end                                                               ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  while ( pollPlayback || pollCapture )                                      {
    int             totalFds     = 0                                         ;
    struct pollfd * capturePfds  = NULL                                      ;
    struct pollfd * playbackPfds = NULL                                      ;
    //////////////////////////////////////////////////////////////////////////
#warning pthread_testcancel ( ) need to have a replacement
//    pthread_testcancel ( )                                                   ;
    if ( pollCapture )                                                       {
      capturePfds = pfds                                                     ;
      CA_ENSURE ( capture . BeginPolling ( capturePfds ) )                   ;
      totalFds += capture . nfds                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( pollPlayback )                                                      {
      playbackPfds = pfds + ( pollCapture ? capture.nfds : 0)                ;
      CA_ENSURE ( playback . BeginPolling ( playbackPfds ) )                 ;
      totalFds += playback . nfds                                            ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    pollResults = poll ( pfds , totalFds , PollTimeout )                     ;
    if ( pollResults < 0 )                                                   {
      if ( errno == EINTR )                                                  {
        Timer :: Sleep ( 1 )                                                 ;
        continue                                                             ;
      }                                                                      ;
      CA_ENSURE ( InternalError )                                            ;
    } else
    if ( pollResults == 0 )                                                  {
      ++ timeouts                                                            ;
      if ( timeouts > 1 ) Timer :: Sleep ( 1 )                               ;
      if ( timeouts >= PollTimeout )                                         {
        *framesAvail = 0                                                     ;
        xrun = 1                                                             ;
        #ifndef REMOVE_DEBUG_MESSAGE
        if ( NULL != debugger )                                              {
          debugger -> printf ("%s: poll timed out\n",__FUNCTION__,timeouts)  ;
        }                                                                    ;
        #endif
        goto end                                                             ;
      }                                                                      ;
    } else
    if ( pollResults > 0 )                                                   {
      timeouts = 0                                                           ;
      if ( pollCapture )                                                     {
        CA_ENSURE ( capture  . EndPolling                                    (
                      capturePfds                                            ,
                      &pollCapture                                           ,
                      &xrun                                              ) ) ;
      }                                                                      ;
      if ( pollPlayback )                                                    {
        CA_ENSURE ( playback . EndPolling                                    (
                      playbackPfds                                           ,
                      &pollPlayback                                          ,
                      &xrun                                              ) ) ;
      }                                                                      ;
      if ( xrun > 0 ) break                                                  ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( NULL != capture.pcm ) && ( NULL != playback.pcm ) )               {
      if ( pollCapture && !pollPlayback )                                    {
        CA_ENSURE ( ContinuePoll ( StreamDirection_In                        ,
                                   &PollTimeout                              ,
                                   &pollCapture                          ) ) ;
      } else
      if ( pollPlayback && ! pollCapture )                                   {
        CA_ENSURE ( ContinuePoll ( StreamDirection_Out                       ,
                                   &PollTimeout                              ,
                                   &pollPlayback                         ) ) ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == xrun )                                                           {
    int captureReady  = ( NULL != capture  . pcm ) ? capture  . ready : 0    ;
    int playbackReady = ( NULL != playback . pcm ) ? playback . ready : 0    ;
    //////////////////////////////////////////////////////////////////////////
    CA_ENSURE ( AvailableFrames ( captureReady                               ,
                                  playbackReady                              ,
                                  framesAvail                                ,
                                  &xrun                                  ) ) ;
    if ( ( NULL != capture . pcm ) && ( NULL != playback . pcm ) )           {
      if ( ! playback.ready && ! neverDropInput )                            {
        assert ( capture . ready )                                           ;
        capture . EndProcessing                                              (
          CA_MIN ( capture . framesPerPeriod , *framesAvail )                ,
          &xrun                                                            ) ;
        *framesAvail    = 0                                                  ;
        capture . ready = 0                                                  ;
      }                                                                      ;
    } else
    if ( NULL != capture . pcm ) assert ( capture  . ready )                 ;
                            else assert ( playback . ready )                 ;
  }                                                                          ;
end                                                                          :
error                                                                        :
  if ( 0 != xrun )                                                           {
    CA_ENSURE ( HandleXrun ( ) )                                             ;
    *framesAvail = 0                                                         ;
  } else                                                                     {
    if ( 0 != *framesAvail )                                                 {
      CA_UNLESS ( capture . ready || playback . ready , InternalError      ) ;
    }                                                                        ;
  }                                                                          ;
  *xrunOccurred = xrun                                                       ;
  return result                                                              ;
}

CaError AlsaStream::EndProcessing(unsigned long numFrames,int * xrunOccurred)
{
  CaError result = NoError                                                   ;
  int     xrun   = 0                                                         ;
  if ( capture.pcm )                                                         {
    CA_ENSURE( capture . EndProcessing ( numFrames , &xrun ) )               ;
  }                                                                          ;
  if ( playback.pcm )                                                        {
    if ( playback.numHostChannels > playback.numUserChannels )               {
      CA_ENSURE( playback . DoChannelAdaption(&bufferProcessor,numFrames)  ) ;
    }                                                                        ;
    CA_ENSURE( playback . EndProcessing ( numFrames , &xrun ) )              ;
  }                                                                          ;
error                                                                        :
  *xrunOccurred = xrun                                                       ;
  return result                                                              ;
}

void AlsaStream::CalculateTimeInfo(Conduit * CONDUIT)
{
  snd_pcm_status_t * capture_status                                          ;
  snd_pcm_status_t * playback_status                                         ;
  snd_timestamp_t    capture_timestamp                                       ;
  snd_timestamp_t    playback_timestamp                                      ;
  CaTime             capture_time  = 0.0                                     ;
  CaTime             playback_time = 0.0                                     ;
  ////////////////////////////////////////////////////////////////////////////
  alsa_snd_pcm_status_alloca ( &capture_status  )                            ;
  alsa_snd_pcm_status_alloca ( &playback_status )                            ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != capture . pcm )                                               {
    snd_pcm_sframes_t capture_delay                                          ;
    alsa_snd_pcm_status            ( capture.pcm    ,  capture_status      ) ;
    alsa_snd_pcm_status_get_tstamp ( capture_status , &capture_timestamp   ) ;
    capture_time = capture_timestamp.tv_sec                                  +
                  ( (CaTime)capture_timestamp.tv_usec / 1000000.0 )          ;
    CONDUIT -> Input.CurrentTime = capture_time                              ;
    capture_delay = alsa_snd_pcm_status_get_delay ( capture_status )         ;
    CONDUIT -> Input.AdcDac = CONDUIT -> Input.CurrentTime                   -
                                    (CaTime)capture_delay / sampleRate       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != playback . pcm )                                              {
    snd_pcm_sframes_t playback_delay                                         ;
    alsa_snd_pcm_status            ( playback . pcm  ,  playback_status    ) ;
    alsa_snd_pcm_status_get_tstamp ( playback_status , &playback_timestamp ) ;
    playback_time = playback_timestamp.tv_sec                                +
                    ((CaTime)playback_timestamp.tv_usec / 1000000.0 )        ;
    if ( NULL != capture . pcm )                                             {
      if ( fabs ( capture_time - playback_time ) > 0.01 )                    {
        dPrintf                                                            ( (
          "Capture time and playback time differ by %f\n"                    ,
          fabs( capture_time-playback_time )                             ) ) ;
      }                                                                      ;
    } else CONDUIT -> Output.CurrentTime = playback_time                     ;
    playback_delay = alsa_snd_pcm_status_get_delay ( playback_status )       ;
    CONDUIT -> Output . AdcDac = CONDUIT -> Output . CurrentTime             +
                                     (CaTime)playback_delay / sampleRate     ;
  }                                                                          ;
}

CaError AlsaStream::Restart(void)
{
  CaError result = NoError                               ;
  MutexLock   (             )                            ;
  CA_ENSURE   ( Stop  ( 0 ) )                            ;
  CA_ENSURE   ( Start ( 0 ) )                            ;
  dPrintf ( ( "%s: Restarted audio\n" , __FUNCTION__ ) ) ;
error                                                    :
  MutexUnlock (             )                            ;
  return result                                          ;
}

CaError AlsaStream::HandleXrun(void)
{
  CaError            result      = NoError                                   ;
  CaTime             now         = Timer::Time()                             ;
  int                restartAlsa = 0                                         ;
  snd_pcm_status_t * st                                                      ;
  snd_timestamp_t    t                                                       ;
  /* do not restart Alsa by default                                         */
  alsa_snd_pcm_status_alloca ( &st )                                         ;
  if ( playback . pcm )                                                      {
    alsa_snd_pcm_status ( playback . pcm , st )                              ;
    if ( alsa_snd_pcm_status_get_state( st ) == SND_PCM_STATE_XRUN )         {
      alsa_snd_pcm_status_get_trigger_tstamp ( st , &t )                     ;
      underrun = now * 1000 - ( (CaTime)t.tv_sec * 1000 + (CaTime)t.tv_usec / 1000 ) ;
      if ( ! playback . canMmap )                                            {
        if ( alsa_snd_pcm_recover ( playback.pcm , -EPIPE, 0 ) < 0 )         {
          dPrintf                                                          ( (
            "%s: [playback] non-MMAP-PCM failed recovering from XRUN, will restart Alsa\n",
            __FUNCTION__                                                 ) ) ;
          ++ restartAlsa                                                     ;
          /* did not manage to recover                                      */
        }                                                                    ;
        Timer :: Sleep ( 10 )                                                ;
      } else ++ restartAlsa                                                  ;
      /* always restart MMAPed device                                       */
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( capture . pcm )                                                       {
    alsa_snd_pcm_status ( capture . pcm , st )                               ;
    if ( alsa_snd_pcm_status_get_state ( st ) == SND_PCM_STATE_XRUN )        {
      alsa_snd_pcm_status_get_trigger_tstamp ( st , &t )                     ;
      overrun = now * 1000 - ((CaTime) t.tv_sec * 1000 + (CaTime) t.tv_usec / 1000) ;
      if ( ! capture . canMmap )                                             {
        if ( alsa_snd_pcm_recover ( capture . pcm , -EPIPE , 0 ) < 0)        {
          dPrintf                                                          ( (
            "%s: [capture] non-MMAP-PCM failed recovering from XRUN, will restart Alsa\n",
            __FUNCTION__                                                 ) ) ;
           ++ restartAlsa                                                    ;
           /* did not manage to recover                                     */
        }                                                                    ;
        Timer :: Sleep ( 10 )                                                ;
      } else ++ restartAlsa                                                  ;
      /* always restart MMAPed device                                       */
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( restartAlsa )                                                         {
    dPrintf                                                                ( (
      "%s: restarting Alsa to recover from XRUN\n"                           ,
      __FUNCTION__                                                       ) ) ;
    CA_ENSURE ( Restart ( ) )                                                ;
  }                                                                          ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

CaError AlsaStream::ContinuePoll       (
          StreamDirection streamDir    ,
          int           * pollTimeout  ,
          int           * continuePoll )
{
  CaError                     result         = NoError                       ;
  snd_pcm_sframes_t           delay                                          ;
  snd_pcm_sframes_t           margin                                         ;
  int                         err                                            ;
  const AlsaStreamComponent * component      = NULL                          ;
  const AlsaStreamComponent * otherComponent = NULL                          ;
  ////////////////////////////////////////////////////////////////////////////
  *continuePoll = 1                                                          ;
  if ( StreamDirection_In == streamDir )                                     {
    component      = &(this -> capture  )                                    ;
    otherComponent = &(this -> playback )                                    ;
  } else                                                                     {
    component      = &(this -> playback )                                    ;
    otherComponent = &(this -> capture  )                                    ;
  }                                                                          ;
  /* ALSA docs say that negative delay should indicate xrun, but in my
   * experience alsa_snd_pcm_delay returns -EPIPE                           */
  err = alsa_snd_pcm_delay ( otherComponent->pcm , &delay )                  ;
  if ( err < 0 )                                                             {
    if ( err == -EPIPE )                                                     {
      *continuePoll = 0                                                      ;
      goto error                                                             ;
    }                                                                        ;
    ENSURE_ ( err , UnanticipatedHostError )                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( StreamDirection_Out == streamDir )                                    {
    /* Number of eligible frames before capture overrun                     */
    delay = otherComponent->alsaBufferSize - delay                           ;
  }                                                                          ;
  margin = delay - otherComponent->framesPerPeriod / 2                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( margin < 0 )                                                          {
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( NULL != debugger )                                                  {
      debugger -> printf                                                     (
        "%s: Stopping poll for %s\n"                                         ,
        __FUNCTION__                                                         ,
        StreamDirection_In == streamDir ? "capture" : "playback"           ) ;
    }                                                                        ;
    #endif
    *continuePoll = 0                                                        ;
  } else
  if ( margin < otherComponent->framesPerPeriod )                            {
    if ( margin > framesPerUserBuffer ) margin = framesPerUserBuffer         ;
    *pollTimeout = CalculatePollTimeout ( margin )                           ;
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( NULL != debugger )                                                  {
      debugger -> printf                                                     (
        "%s: Trying to poll again for %s frames, pollTimeout: %d\n"          ,
        __FUNCTION__                                                         ,
        StreamDirection_In == streamDir ? "capture" : "playback"             ,
        *pollTimeout                                                       ) ;
    }                                                                        ;
    #endif
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

void AlsaStream::SilenceBuffer(void)
{
  const snd_pcm_channel_area_t * areas;
  snd_pcm_uframes_t offset                                                    ;
  snd_pcm_uframes_t frames = ( snd_pcm_uframes_t ) alsa_snd_pcm_avail_update(playback.pcm );
  alsa_snd_pcm_mmap_begin    ( playback.pcm, &areas, &offset, &frames );
  alsa_snd_pcm_areas_silence ( areas, offset, playback.numHostChannels, frames, playback.nativeFormat ) ;
  alsa_snd_pcm_mmap_commit   ( playback.pcm , offset, frames );
}

CaError AlsaStream::Start(int priming)
{
  CaError result = NoError                                                   ;
  if ( playback.pcm )                                                        {
    if ( callbackMode )                                                      {
      if ( ! priming )                                                       {
        ENSURE_( alsa_snd_pcm_prepare(playback.pcm),UnanticipatedHostError ) ;
        if ( playback . canMmap ) SilenceBuffer ( )                          ;
      }                                                                      ;
      if ( playback.canMmap )                                                {
        ENSURE_( alsa_snd_pcm_start(playback.pcm),UnanticipatedHostError )   ;
      }                                                                      ;
    } else                                                                   {
      ENSURE_( alsa_snd_pcm_prepare(playback.pcm),UnanticipatedHostError )   ;
    }                                                                        ;
  }                                                                          ;
  if ( capture.pcm && ! pcmsSynced )                                         {
    ENSURE_( alsa_snd_pcm_prepare(capture.pcm ),UnanticipatedHostError )     ;
    ENSURE_( alsa_snd_pcm_start  (capture.pcm ),UnanticipatedHostError )     ;
  }                                                                          ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

/* Stop PCM handle, either softly or abruptly. */
CaError AlsaStream::Stop(int abort)
{
  CaError result = NoError                                                   ;
  if ( abort )                                                               {
    if ( playback . pcm )                                                    {
      ENSURE_ ( alsa_snd_pcm_drop(playback.pcm) , UnanticipatedHostError )   ;
    }                                                                        ;
    if ( capture.pcm && ! pcmsSynced )                                       {
      ENSURE_ ( alsa_snd_pcm_drop(capture .pcm) , UnanticipatedHostError )   ;
    }                                                                        ;
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( NULL != debugger )                                            {
      debugger -> printf ( "%s: Dropped frames\n" , __FUNCTION__   )   ;
    }                                                                        ;
    #endif
  } else                                                                     {
    if ( playback . pcm )                                                    {
      ENSURE_ (alsa_snd_pcm_nonblock(playback.pcm,0),UnanticipatedHostError) ;
      if ( alsa_snd_pcm_drain ( playback . pcm ) < 0 )                       {
        #ifndef REMOVE_DEBUG_MESSAGE
        if ( NULL != debugger )                                        {
          debugger -> printf                                           (
            "%s: Draining playback handle failed!\n"                         ,
            __FUNCTION__                                                   ) ;
        }                                                                    ;
        #endif
      }                                                                      ;
    }                                                                        ;
    if ( capture . pcm && ! pcmsSynced )                                     {
      if ( alsa_snd_pcm_drain( capture . pcm ) < 0 )                         {
        #ifndef REMOVE_DEBUG_MESSAGE
        if ( NULL != debugger )                                        {
          debugger -> printf                                           (
            "%s: Draining capture handle failed!\n"                          ,
            __FUNCTION__                                                   ) ;
        }                                                                    ;
        #endif
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

CaError AlsaStream::RealStop(int abort)
{
  CaError result = NoError                                                   ;
  if ( callbackMode )                                                        {
    CaError threadRes                                                        ;
    callbackAbort = abort                                                    ;
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( !abort )                                                            {
      if ( NULL != debugger )                                          {
        debugger -> printf ( "Stopping callback\n" )                   ;
      }                                                                      ;
    }                                                                        ;
    #endif
    CA_ENSURE( ThreadTerminate( !abort, &threadRes ) )                       ;
    #ifndef REMOVE_DEBUG_MESSAGE
    if ( threadRes != NoError )                                              {
      if ( NULL != debugger )                                          {
        debugger->printf("Callback thread returned: %d\n",threadRes)   ;
      }                                                                      ;
    }                                                                        ;
    #endif
    callback_finished = 0                                                    ;
  } else                                                                     {
    CA_ENSURE( Stop ( abort ) )                                              ;
  }                                                                          ;
  isActive = 0                                                               ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

CaError AlsaStream::DetermineFramesPerBuffer            (
          double                    sampleRate          ,
          const  StreamParameters * inputParameters     ,
          const  StreamParameters * outputParameters    ,
          unsigned long             framesPerUserBuffer ,
          snd_pcm_hw_params_t     * hwParamsCapture     ,
          snd_pcm_hw_params_t     * hwParamsPlayback    ,
          CaHostBufferSizeMode    * hostBufferSizeMode  )
{
  CaError       result              = NoError                                ;
  unsigned long framesPerHostBuffer = 0                                      ;
  int           dir                 = 0                                      ;
  int           accurate            = 1                                      ;
  unsigned      numPeriods          = numPeriods_                            ;
  ////////////////////////////////////////////////////////////////////////////
  if ( this -> capture . pcm && this -> playback . pcm )                     {
    if ( framesPerUserBuffer == Stream::FramesPerBufferUnspecified )         {
      /* Come up with a common desired latency                              */
      snd_pcm_uframes_t desiredBufSz                                         ;
      snd_pcm_uframes_t e                                                    ;
      snd_pcm_uframes_t minPeriodSize                                        ;
      snd_pcm_uframes_t maxPeriodSize                                        ;
      snd_pcm_uframes_t optimalPeriodSize                                    ;
      snd_pcm_uframes_t periodSize                                           ;
      snd_pcm_uframes_t minCapture                                           ;
      snd_pcm_uframes_t minPlayback                                          ;
      snd_pcm_uframes_t maxCapture                                           ;
      snd_pcm_uframes_t maxPlayback                                          ;
      ////////////////////////////////////////////////////////////////////////
      dir = 0                                                                ;
      ENSURE_ ( alsa_snd_pcm_hw_params_get_period_size_min ( hwParamsCapture  , &minCapture  , &dir ) ,
                UnanticipatedHostError                                     ) ;
      dir = 0                                                                ;
      ENSURE_ ( alsa_snd_pcm_hw_params_get_period_size_min ( hwParamsPlayback , &minPlayback , &dir ) ,
                UnanticipatedHostError                                     ) ;
      dir = 0                                                                ;
      ENSURE_( alsa_snd_pcm_hw_params_get_period_size_max  ( hwParamsCapture  , &maxCapture  , &dir ) ,
               UnanticipatedHostError                                      ) ;
      dir = 0                                                                ;
      ENSURE_( alsa_snd_pcm_hw_params_get_period_size_max  ( hwParamsPlayback , &maxPlayback , &dir ) ,
               UnanticipatedHostError                                      ) ;
      minPeriodSize = CA_MAX ( minPlayback , minCapture )                    ;
      maxPeriodSize = CA_MIN ( maxPlayback , maxCapture )                    ;
      CA_UNLESS ( minPeriodSize <= maxPeriodSize , BadIODeviceCombination  ) ;
      ////////////////////////////////////////////////////////////////////////
      desiredBufSz = (snd_pcm_uframes_t)                                     (
                     CA_MIN ( outputParameters -> suggestedLatency           ,
                              inputParameters  -> suggestedLatency           )
                            * sampleRate                                   ) ;
      /* Clamp desiredBufSz                                                 */
      {                                                                      ;
        snd_pcm_uframes_t maxBufferSize                                      ;
        snd_pcm_uframes_t maxBufferSizeCapture                               ;
        snd_pcm_uframes_t maxBufferSizePlayback                              ;
        ENSURE_( alsa_snd_pcm_hw_params_get_buffer_size_max ( hwParamsCapture  , &maxBufferSizeCapture  ) ,
                 UnanticipatedHostError                                    ) ;
        ENSURE_( alsa_snd_pcm_hw_params_get_buffer_size_max ( hwParamsPlayback , &maxBufferSizePlayback ) ,
                 UnanticipatedHostError                                    ) ;
        maxBufferSize = CA_MIN ( maxBufferSizeCapture, maxBufferSizePlayback ) ;
        desiredBufSz  = CA_MIN ( desiredBufSz, maxBufferSize               ) ;
      }                                                                      ;
      /* Find the closest power of 2                                        */
      e = ilogb ( minPeriodSize )                                            ;
      if ( minPeriodSize & ( minPeriodSize - 1 ) ) e += 1                    ;
      periodSize = (snd_pcm_uframes_t) pow ( 2, e )                          ;
      while ( periodSize <= maxPeriodSize )                                  {
        if ( alsa_snd_pcm_hw_params_test_period_size ( this -> playback . pcm , hwParamsPlayback , periodSize , 0 ) >= 0  &&
             alsa_snd_pcm_hw_params_test_period_size ( this -> capture  . pcm , hwParamsCapture  , periodSize , 0 ) >= 0 ) {
          break                                                              ;
        }                                                                    ;
        periodSize *= 2                                                      ;
      }                                                                      ;
      optimalPeriodSize = CA_MAX ( desiredBufSz / numPeriods,minPeriodSize ) ;
      optimalPeriodSize = CA_MIN ( optimalPeriodSize , maxPeriodSize       ) ;
      /* Find the closest power of 2                                        */
      e = ilogb ( optimalPeriodSize )                                        ;
      if ( optimalPeriodSize & (optimalPeriodSize - 1) ) e += 1              ;
      optimalPeriodSize = (snd_pcm_uframes_t) pow ( 2, e )                   ;
      while ( optimalPeriodSize >= periodSize )                              {
        if ( alsa_snd_pcm_hw_params_test_period_size(this->capture.pcm ,hwParamsCapture ,optimalPeriodSize,0) >= 0  &&
             alsa_snd_pcm_hw_params_test_period_size(this->playback.pcm,hwParamsPlayback,optimalPeriodSize,0) >= 0 ) {
          break                                                              ;
        }                                                                    ;
        optimalPeriodSize /= 2                                               ;
      }                                                                      ;
      if ( optimalPeriodSize > periodSize ) periodSize = optimalPeriodSize   ;
      if ( periodSize <= maxPeriodSize )                                     {
        /* Looks good, the periodSize _should_ be acceptable by both devices */
        ENSURE_( alsa_snd_pcm_hw_params_set_period_size(this->capture .pcm,hwParamsCapture ,periodSize,0) ,
                 UnanticipatedHostError                                    ) ;
        ENSURE_( alsa_snd_pcm_hw_params_set_period_size(this->playback.pcm,hwParamsPlayback,periodSize,0) ,
                 UnanticipatedHostError                                    ) ;
        this->capture.framesPerPeriod = this->playback.framesPerPeriod = periodSize ;
        framesPerHostBuffer = periodSize                                     ;
      } else                                                                 {
        /* Unable to find a common period size, oh well                     */
        optimalPeriodSize = CA_MAX( desiredBufSz/numPeriods,minPeriodSize  ) ;
        optimalPeriodSize = CA_MIN( optimalPeriodSize, maxPeriodSize       ) ;
        this->capture.framesPerPeriod = optimalPeriodSize                    ;
        dir = 0                                                              ;
        ENSURE_ ( alsa_snd_pcm_hw_params_set_period_size_near(this->capture.pcm,hwParamsCapture,&this->capture.framesPerPeriod,&dir) ,
                  UnanticipatedHostError                                   ) ;
        this->playback.framesPerPeriod = optimalPeriodSize                   ;
        dir = 0                                                              ;
        ENSURE_ ( alsa_snd_pcm_hw_params_set_period_size_near(this->playback.pcm,hwParamsPlayback,&this->playback.framesPerPeriod,&dir) ,
                  UnanticipatedHostError                                   ) ;
        framesPerHostBuffer = CA_MAX( this->capture.framesPerPeriod,this->playback.framesPerPeriod ) ;
        *hostBufferSizeMode = cabBoundedHostBufferSize                       ;
      }                                                                      ;
    } else                                                                   {
      unsigned                 maxPeriods        = 0                         ;
      AlsaStreamComponent    * first             = &this->capture            ;
      AlsaStreamComponent    * second            = &this->playback           ;
      const StreamParameters * firstStreamParams = inputParameters           ;
      snd_pcm_hw_params_t    * firstHwParams     = hwParamsCapture           ;
      snd_pcm_hw_params_t    * secondHwParams    = hwParamsPlayback          ;
      ////////////////////////////////////////////////////////////////////////
      dir = 0                                                                ;
      ENSURE_ ( alsa_snd_pcm_hw_params_get_periods_max(hwParamsPlayback,&maxPeriods,&dir) ,
                UnanticipatedHostError                                     ) ;
      if ( maxPeriods < numPeriods )                                         {
        /* The playback component is trickier to get right, try that first  */
        first             = &this->playback                                  ;
        second            = &this->capture                                   ;
        firstStreamParams = outputParameters                                 ;
        firstHwParams     = hwParamsPlayback                                 ;
        secondHwParams    = hwParamsCapture                                  ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      CA_ENSURE ( first -> DetermineFramesPerBuffer                          (
                    firstStreamParams                                        ,
                    framesPerUserBuffer                                      ,
                    sampleRate                                               ,
                    firstHwParams                                            ,
                    &accurate                                            ) ) ;
      second -> framesPerPeriod = first -> framesPerPeriod                   ;
      dir                       = 0                                          ;
      ////////////////////////////////////////////////////////////////////////
      ENSURE_ ( alsa_snd_pcm_hw_params_set_period_size_near(second->pcm,secondHwParams,&second->framesPerPeriod,&dir) ,
                UnanticipatedHostError                                     ) ;
      if ( this->capture.framesPerPeriod == this->playback.framesPerPeriod ) {
        framesPerHostBuffer = this->capture.framesPerPeriod                  ;
      } else                                                                 {
        framesPerHostBuffer = CA_MAX ( this -> capture  . framesPerPeriod    ,
                                       this -> playback . framesPerPeriod  ) ;
        *hostBufferSizeMode = cabBoundedHostBufferSize                       ;
      }                                                                      ;
    }                                                                        ;
  } else                                                                     {
    /* half-duplex is a slightly simpler case                               */
    if ( this -> capture . pcm )                                             {
      CA_ENSURE ( this -> capture . DetermineFramesPerBuffer                 (
                    inputParameters                                          ,
                    framesPerUserBuffer                                      ,
                    sampleRate                                               ,
                    hwParamsCapture                                          ,
                    &accurate                                            ) ) ;
      framesPerHostBuffer = this -> capture  . framesPerPeriod               ;
    } else                                                                   {
      assert ( this -> playback . pcm )                                      ;
      CA_ENSURE ( this -> playback . DetermineFramesPerBuffer                (
                    outputParameters                                         ,
                    framesPerUserBuffer                                      ,
                    sampleRate                                               ,
                    hwParamsPlayback                                         ,
                    &accurate                                            ) ) ;
      framesPerHostBuffer = this -> playback . framesPerPeriod               ;
    }                                                                        ;
  }                                                                          ;
  CA_UNLESS ( framesPerHostBuffer != 0 , InternalError )                     ;
  this->maxFramesPerHostBuffer = framesPerHostBuffer                         ;
  if ( ! this->playback.canMmap || !accurate )                               {
    /* Don't know the exact size per host buffer                            */
    *hostBufferSizeMode = cabBoundedHostBufferSize                           ;
    /* Raise upper bound                                                    */
    if ( !accurate ) ++this->maxFramesPerHostBuffer                          ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError AlsaStream ::              Configure           (
          const StreamParameters * inParams            ,
          const StreamParameters * outParams           ,
          double                   sampleRate          ,
          unsigned long            framesPerUserBuffer ,
          double                 * inputLatency        ,
          double                 * outputLatency       ,
          CaHostBufferSizeMode   * hostBufferSizeMode  )
{
  CaError               result           = NoError                           ;
  double                realSr           = sampleRate                        ;
  snd_pcm_hw_params_t * hwParamsCapture  = NULL                              ;
  snd_pcm_hw_params_t * hwParamsPlayback = NULL                              ;
  ////////////////////////////////////////////////////////////////////////////
  alsa_snd_pcm_hw_params_alloca ( &hwParamsCapture  )                        ;
  alsa_snd_pcm_hw_params_alloca ( &hwParamsPlayback )                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != capture.pcm )                                                 {
    capture.framesPerPeriod = framesPerUserBuffer                            ;
    CA_ENSURE ( this -> capture . InitialConfigure                           (
                  inParams                                                   ,
                  this -> primeBuffers                                       ,
                  hwParamsCapture                                            ,
                  &realSr                                                ) ) ;
  }                                                                          ;
  if ( NULL != playback.pcm )                                                {
    playback . framesPerPeriod = framesPerUserBuffer                         ;
    CA_ENSURE ( this -> playback . InitialConfigure                          (
                  outParams                                                  ,
                  this -> primeBuffers                                       ,
                  hwParamsPlayback                                           ,
                  &realSr                                                ) ) ;
  }                                                                          ;
  CA_ENSURE ( DetermineFramesPerBuffer                                       (
                realSr                                                       ,
                inParams                                                     ,
                outParams                                                    ,
                framesPerUserBuffer                                          ,
                hwParamsCapture                                              ,
                hwParamsPlayback                                             ,
                hostBufferSizeMode                                       ) ) ;
  if ( capture . pcm )                                                       {
    assert ( this -> capture . framesPerPeriod != 0 )                        ;
    CA_ENSURE( this -> capture . FinishConfigure                             (
                 hwParamsCapture                                             ,
                 inParams                                                    ,
                 this -> primeBuffers                                        ,
                 realSr                                                      ,
                 inputLatency                                            ) ) ;
    dPrintf                                                                ( (
      "%s: Capture period size: %lu, latency: %f\n"                          ,
      __FUNCTION__                                                           ,
      this -> capture . framesPerPeriod                                      ,
      *inputLatency                                                      ) ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( this -> playback . pcm )                                              {
    assert ( this -> playback . framesPerPeriod != 0 )                       ;
    CA_ENSURE( this -> playback . FinishConfigure                            (
                 hwParamsPlayback                                            ,
                 outParams                                                   ,
                 this -> primeBuffers                                        ,
                 realSr                                                      ,
                 outputLatency                                           ) ) ;
    dPrintf                                                                ( (
      "%s: Playback period size: %lu, latency: %f\n"                         ,
      __FUNCTION__                                                           ,
      this -> playback . framesPerPeriod                                     ,
       *outputLatency                                                    ) ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Should be exact now                                                    */
  this -> sampleRate = realSr                                                ;
  /* this will cause the two streams to automatically start/stop/prepare in sync.
   * We only need to execute these operations on one of the pair.
   * A: We don't want to do this on a blocking stream.                      */
  if ( this->callbackMode && this->capture.pcm && this->playback.pcm )       {
    int err = alsa_snd_pcm_link ( this->capture.pcm , this->playback.pcm )   ;
    if ( err == 0 ) this -> pcmsSynced = 1 ; else                            {
      #ifndef REMOVE_DEBUG_MESSAGE
      if ( NULL != debugger )                                          {
        debugger -> printf                                             (
          "%s: Unable to sync pcms: %s\n"                                    ,
          __FUNCTION__                                                       ,
          alsa_snd_strerror( err )                                         ) ;
      }                                                                      ;
      #endif
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  {                                                                          ;
    unsigned long minFramesPerHostBuffer                                     ;
    minFramesPerHostBuffer = CA_MIN                                          (
                               this -> capture  . pcm                        ?
                               this -> capture  . framesPerPeriod            :
                               ULONG_MAX                                     ,
                               this -> playback . pcm                        ?
                               this -> playback . framesPerPeriod            :
                               ULONG_MAX                                   ) ;
    if ( minFramesPerHostBuffer > framesPerUserBuffer )                      {
      this->pollTimeout = CalculatePollTimeout( framesPerUserBuffer    )     ;
    } else                                                                   {
      this->pollTimeout = CalculatePollTimeout( minFramesPerHostBuffer )     ;
    }                                                                        ;
    /* Period in msecs, rounded up                                          */
    /* Time before watchdog unthrottles realtime thread == 1/4 of period time in msecs */
    /* this->threading.throttledSleepTime = (unsigned long) (minFramesPerHostBuffer / sampleRate / 4 * 1000); */
  }                                                                          ;
  if ( this -> callbackMode )                                                {
    if ( this->framesPerUserBuffer != Stream::FramesPerBufferUnspecified )   {
    }                                                                        ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

void AlsaStream::Terminate(void)
{
  if ( NULL != capture.pcm  ) capture  . Terminate ( ) ;
  if ( NULL != playback.pcm ) playback . Terminate ( ) ;
  Allocator :: free ( pfds  )                          ;
  MutexTerminate    (       )                          ;
}

int AlsaStream::CalculatePollTimeout(unsigned long frames)
{ /* Period in msecs, rounded up                  */
  return (int)::ceil( 1000 * frames / sampleRate ) ;
}

CaError AlsaStream::Initialize                         (
          AlsaHostApi            * alsaApi             ,
          const StreamParameters * inParams            ,
          const StreamParameters * outParams           ,
          double                   sampleRate          ,
          unsigned long            FramesPerUserBuffer ,
          Conduit                * CONDUIT             )
{
  CaError result = NoError                                                   ;
  conduit = CONDUIT                                                          ;
  if ( NULL != CONDUIT ) callbackMode = 1                                    ;
                    else callbackMode = 0                                    ;
  ////////////////////////////////////////////////////////////////////////////
  framesPerUserBuffer = FramesPerUserBuffer                                  ;
  if ( NULL != CONDUIT )                                                     {
    neverDropInput = conduit->Input.StatusFlags & csfNeverDropInput          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  capture  . Reset ( )                                                       ;
  playback . Reset ( )                                                       ;
  if ( NULL != inParams  )                                                   {
    CA_ENSURE( capture  . Initialize                                         (
                 alsaApi                                                     ,
                 inParams                                                    ,
                 StreamDirection_In                                          ,
                 NULL != CONDUIT                                         ) ) ;
  }                                                                          ;
  if ( NULL != outParams )                                                   {
    CA_ENSURE( playback . Initialize                                         (
                 alsaApi                                                     ,
                 outParams                                                   ,
                 StreamDirection_Out                                         ,
                 NULL != CONDUIT                                         ) ) ;
  }                                                                          ;
  pfds = (struct pollfd *)Allocator::allocate                                (
                            ( capture.nfds + playback.nfds )                 *
                            sizeof( struct pollfd )                        ) ;
  CA_UNLESS ( pfds , InsufficientMemory )                                    ;
  cpuLoadMeasurer . Initialize ( sampleRate )                                ;
  MutexInitialize ( )                                                        ;
error                                                                        :
  return result                                                              ;
}

void AlsaStream::OnExit(void)
{
  bool isInput  = ( NULL != capture  . pcm )                                 ;
  bool isOutput = ( NULL != playback . pcm )                                 ;
  cpuLoadMeasurer . Reset ( )                                                ;
  callback_finished = 1                                                      ;
  /* Let the outside world know stream was stopped in callback              */
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( NULL != debugger )                                              {
    debugger -> printf ( "%s: Stopping ALSA handles\n", __FUNCTION__ ) ;
  }                                                                          ;
  #endif
  Stop ( callbackAbort )                                                     ;
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( NULL != debugger )                                              {
    debugger -> printf ( "%s: Stoppage\n", __FUNCTION__              ) ;
  }                                                                          ;
  #endif
  /* Eventually notify user all buffers have played                         */
  if ( NULL != conduit )                                                     {
    if ( isInput  ) conduit -> finish ( Conduit::InputDirection              ,
                                        Conduit::Correct                   ) ;
    if ( isOutput ) conduit -> finish ( Conduit::OutputDirection             ,
                                        Conduit::Correct                   ) ;
  }                                                                          ;
  isActive   = 0                                                               ;
}

void * AlsaStream::Processor(void)
{
  dPrintf ( ( "Advanced Linux Sound Architecture started\n" ) )              ;
  ////////////////////////////////////////////////////////////////////////////
  CaError           result         = NoError                                 ;
  snd_pcm_sframes_t startThreshold = 0                                       ;
  snd_pcm_sframes_t avail          = 0                                       ;
  int               callbackResult = Conduit::Continue                       ;
  CaStreamFlags     cbFlags        = 0                                       ;
  int               streamStarted  = 0                                       ;
  CaError         * pres           = NULL                                    ;
  Timer             T                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != primeBuffers )                                                {
    if ( NULL != playback.pcm )                                              {
      ENSURE_ ( alsa_snd_pcm_prepare ( playback . pcm )                      ,
                UnanticipatedHostError                                     ) ;
      avail = alsa_snd_pcm_avail_update ( playback . pcm )                   ;
      startThreshold = avail - ( avail % playback . framesPerPeriod )        ;
      assert ( startThreshold >= playback . framesPerPeriod )                ;
    }                                                                        ;
    if ( ( NULL != capture.pcm ) && ( NULL == pcmsSynced ) )                 {
      ENSURE_ ( alsa_snd_pcm_prepare ( capture  . pcm )                      ,
                UnanticipatedHostError                                     ) ;
    }                                                                        ;
  } else                                                                     {
    CA_ENSURE ( PrepareNotify (   ) )                                        ;
    CA_ENSURE ( Start         ( 0 ) )                                        ;
    CA_ENSURE ( NotifyParent  (   ) )                                        ;
    streamStarted = 1                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  #warning this is underrun situtation handling, it does not work very well
  if ( NULL != playback.pcm )                                                {
    avail = alsa_snd_pcm_avail_update ( playback . pcm )                     ;
    if ( ! ( CaOR ( CaIsEqual ( avail , 0                          )         ,
                    CaIsEqual ( avail , playback . framesPerPeriod )   ) ) ) {
      alsa_snd_pcm_recover ( playback.pcm , -EPIPE    , 0 )                  ;
      Timer :: Sleep ( 50 )                                                  ;
      Restart ( )                                                            ;
      avail = alsa_snd_pcm_avail_update ( playback . pcm )                   ;
      if ( ! ( CaOR ( CaIsEqual ( avail , 0                          )       ,
                      CaIsEqual ( avail , playback . framesPerPeriod ) ) ) ) {
        alsa_snd_pcm_recover ( playback.pcm , -ESTRPIPE , 0 )                ;
        Timer :: Sleep ( 50 )                                                ;
        Restart ( )                                                          ;
        avail = alsa_snd_pcm_avail_update ( playback . pcm )                 ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( ! ( CaOR ( CaIsEqual ( avail , 0                          )         ,
                    CaIsEqual ( avail , playback . framesPerPeriod )   ) ) ) {
      dPrintf ( ( "Frame count and output buffer mismatch."
                  "  Audio might play incorrectly.\n"                    ) ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  T . Anchor ( framesPerUserBuffer , sampleRate )                            ;
  while ( 1 )                                                                {
    unsigned long framesAvail = 0                                            ;
    unsigned long framesGot   = 0                                            ;
    int           xrun        = 0                                            ;
    //////////////////////////////////////////////////////////////////////////
    #warning pthread_cancel need to have a replacement
//    pthread_testcancel ( )                                                   ;
    if ( ( 0 != stopRequest ) && ( Conduit::Continue == callbackResult ) )   {
      dPrintf ( ( "Setting callbackResult to Complete\n" ) )                 ;
      callbackResult = Conduit::Complete                                     ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( Conduit::Continue != callbackResult )                               {
      callbackAbort = ( Conduit::Abort == callbackResult )                   ;
      if ( callbackAbort || bufferProcessor.isOutputEmpty()) goto end        ;
      dPrintf ( ( "%s: Flushing buffer processor\n" , __FUNCTION__       ) ) ;
      /* There is still buffered output that needs to be processed          */
    }                                                                        ;
    CA_ENSURE ( WaitForFrames ( &framesAvail, &xrun ) )                      ;
    if ( xrun )                                                              {
      assert ( 0 == framesAvail )                                            ;
      continue                                                               ;
    }                                                                        ;
    if ( ! T.isArrival() )                                                   {
      CaTime dT = T.dT(1000)                                                 ;
      int    ms = 1                                                          ;
      if ( dT >  5 ) ms =  3                                                 ;
      if ( dT > 10 ) ms =  5                                                 ;
      if ( dT > 20 ) ms = 10                                                 ;
      if ( dT > 30 ) ms = 15                                                 ;
      if ( dT > 40 ) ms = 20                                                 ;
      if ( dT > 50 ) ms = 30                                                 ;
      if ( dT > 70 ) ms = 40                                                 ;
      T . Sleep ( ms )                                                       ;
      if ( ms > 2 ) continue                                                 ;
    }                                                                        ;
    T . Anchor ( framesPerUserBuffer , sampleRate )                          ;
    while ( framesAvail > 0 )                                                {
      xrun = 0                                                               ;
      #warning pthread_cancel need to have a replacement
//      pthread_testcancel ( )                                                 ;
      if ( underrun > 0.0 )                                                  {
        cbFlags |= StreamIO::OutputUnderflow                                  ;
        underrun = 0.0                                                       ;
      }                                                                      ;
      if ( overrun > 0.0 )                                                   {
        cbFlags |= StreamIO::InputOverflow                                    ;
        overrun = 0.0                                                        ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      if ( ( NULL != capture.pcm ) && ( NULL != playback.pcm ) )             {
        if ( ! capture  . ready )                                            {
          cbFlags |= StreamIO::InputUnderflow                                ;
          dPrintf ( ( "%s: Input underflow\n", __FUNCTION__ ) )              ;
        } else
        if ( ! playback . ready )                                            {
          cbFlags |= StreamIO::OutputOverflow                                ;
          dPrintf ( ( "%s: Output overflow\n", __FUNCTION__ ) )              ;
        }                                                                    ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      CalculateTimeInfo       ( conduit )                                    ;
      bufferProcessor . Begin ( conduit )                                    ;
      cbFlags = 0                                                            ;
      cpuLoadMeasurer . Begin (         )                                    ;
      framesGot = framesAvail                                                ;
      if ( cabFixedHostBufferSize == bufferProcessor.hostBufferSizeMode )    {
        framesGot = framesGot >= framesPerUserBuffer ? framesPerUserBuffer : 0 ;
      } else                                                                 {
        assert( cabBoundedHostBufferSize == bufferProcessor.hostBufferSizeMode ) ;
        framesGot = CA_MIN ( framesGot , framesPerUserBuffer )               ;
      }                                                                      ;
      CA_ENSURE ( SetUpBuffers ( &framesGot , &xrun ) )                      ;
      /* Check the host buffer size against the buffer processor configuration */
      framesAvail -= framesGot                                               ;
      if ( framesGot > 0 )                                                   {
        assert ( !xrun )                                                     ;
        bufferProcessor . End ( &callbackResult )                            ;
        CA_ENSURE ( EndProcessing ( framesGot , &xrun ) )                    ;
      }                                                                      ;
      cpuLoadMeasurer . End ( framesGot )                                    ;
      if ( 0 == framesGot )                                                  {
        /* Go back to polling for more frames                               */
        break                                                                ;
      }                                                                      ;
      if ( ( NULL != playback.pcm                                        )  &&
           ( framesGot == framesPerUserBuffer                            ) ) {
        break                                                                ;
      }                                                                      ;
      if ( Conduit::Continue != callbackResult ) break                       ;
    }                                                                        ;
  }                                                                          ;
end                                                                          :
  OnExit ( )                                                                 ;
  dPrintf                                                                  ( (
    "%s: Thread %d exiting\n"                                                ,
    __FUNCTION__                                                             ,
    pthread_self()                                                       ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NoError != result )                                                   {
    pres  = (CaError *)malloc(sizeof(CaError))                               ;
    *pres = result                                                           ;
  }                                                                          ;
  ::pthread_exit ( pres )                                                    ;
  return NULL                                                                ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  dPrintf                                                                  ( (
    "%s: Thread %d is canceled due to error %d\n "                           ,
    __FUNCTION__                                                             ,
    pthread_self()                                                           ,
    result                                                               ) ) ;
  goto end                                                                   ;
  return NULL                                                                ;
}

CaError AlsaStream::MutexInitialize(void)
{
  CaError result = NoError                                       ;
  CA_ASSERT_CALL ( pthread_mutex_init ( &stateMtx , NULL ) , 0 ) ;
  return result                                                  ;
}

CaError AlsaStream::MutexTerminate(void)
{
  CaError result = NoError                                   ;
  CA_ASSERT_CALL ( pthread_mutex_destroy ( &stateMtx ) , 0 ) ;
  return result                                              ;
}

CaError AlsaStream::MutexLock(void)
{
  CaError result = NoError                                  ;
  CA_ENSURE_SYSTEM ( pthread_mutex_lock ( &stateMtx ) , 0 ) ;
error                                                       :
  return result                                             ;
}

CaError AlsaStream::MutexUnlock(void)
{
  CaError result = NoError                                    ;
  CA_ENSURE_SYSTEM ( pthread_mutex_unlock ( &stateMtx ) , 0 ) ;
error                                                         :
  return result                                               ;
}

///////////////////////////////////////////////////////////////////////////////

AlsaDeviceInfo:: AlsaDeviceInfo (void)
               : DeviceInfo     (    )
{
  alsaName          = NULL ;
  isPlug            = 0    ;
  minInputChannels  = 0    ;
  minOutputChannels = 0    ;
}

AlsaDeviceInfo::~AlsaDeviceInfo (void)
{
}

void AlsaDeviceInfo::Initialize(void)
{
  structVersion            = -1   ;
  name                     = NULL ;
  hostApi                  = -1   ;
  maxInputChannels         =  0   ;
  maxOutputChannels        =  0   ;
  defaultLowInputLatency   = -1.0 ;
  defaultLowOutputLatency  = -1.0 ;
  defaultHighInputLatency  = -1.0 ;
  defaultHighOutputLatency = -1.0 ;
  defaultSampleRate        = -1.0 ;
  alsaName                 = NULL ;
  isPlug                   = 0    ;
  minInputChannels         = 0    ;
  minOutputChannels        = 0    ;
}

CaError AlsaDeviceInfo::GropeDevice(snd_pcm_t * pcm,int isPlug,StreamDirection mode,int openBlocking)
{
  CaError               result = NoError                                     ;
  snd_pcm_hw_params_t * hwParams                                             ;
  snd_pcm_uframes_t     alsaBufferFrames                                     ;
  snd_pcm_uframes_t     alsaPeriodFrames                                     ;
  unsigned int          minChans                                             ;
  unsigned int          maxChans                                             ;
  int                 * minChannels                                          ;
  int                 * maxChannels                                          ;
  double              * defaultLowLatency                                    ;
  double              * defaultHighLatency                                   ;
  double              * defaultSampleRate = &(this->defaultSampleRate)       ;
  double                defaultSr         = *defaultSampleRate               ;
  int                   dir                                                  ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "%s: collecting info ..\n" , __FUNCTION__ ) )                  ;
  ////////////////////////////////////////////////////////////////////////////
  if ( StreamDirection_In == mode )                                          {
    minChannels        = & ( this -> minInputChannels         )              ;
    maxChannels        = & ( this -> maxInputChannels         )              ;
    defaultLowLatency  = & ( this -> defaultLowInputLatency   )              ;
    defaultHighLatency = & ( this -> defaultHighInputLatency  )              ;
  } else                                                                     {
    minChannels        = & ( this -> minOutputChannels        )              ;
    maxChannels        = & ( this -> maxOutputChannels        )              ;
    defaultLowLatency  = & ( this -> defaultLowOutputLatency  )              ;
    defaultHighLatency = & ( this -> defaultHighOutputLatency )              ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ENSURE_ ( alsa_snd_pcm_nonblock( pcm, 0 ) , UnanticipatedHostError )       ;
  alsa_snd_pcm_hw_params_alloca ( &hwParams      )                           ;
  alsa_snd_pcm_hw_params_any    ( pcm , hwParams )                           ;
  ////////////////////////////////////////////////////////////////////////////
  if ( defaultSr >= 0 )                                                      {
    if ( SetApproximateSampleRate( pcm, hwParams, defaultSr ) < 0 )          {
      defaultSr = -1.0                                                       ;
      alsa_snd_pcm_hw_params_any( pcm, hwParams )                            ;
      /* Clear any params (rate) that might have been set                   */
      gPrintf                                                              ( (
        "%s: Original default samplerate failed, trying again ..\n"          ,
        __FUNCTION__                                                     ) ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( defaultSr < 0.0 )                                                     {
    /* Default sample rate not set                                          */
    unsigned int sampleRate = 44100                                          ;
    /* Will contain approximate rate returned by alsa-lib                   */
    /* Don't allow rate resampling when probing for the default rate (but ignore if this call fails) */
    alsa_snd_pcm_hw_params_set_rate_resample( pcm, hwParams, 0 )             ;
    if ( alsa_snd_pcm_hw_params_set_rate_near( pcm, hwParams, &sampleRate, NULL ) < 0 ) {
      result = UnanticipatedHostError                                        ;
      goto error                                                             ;
    }                                                                        ;
    ENSURE_( GetExactSampleRate( hwParams, &defaultSr ),UnanticipatedHostError ) ;
  }                                                                          ;
  ENSURE_( alsa_snd_pcm_hw_params_get_channels_min( hwParams, &minChans ),UnanticipatedHostError ) ;
  ENSURE_( alsa_snd_pcm_hw_params_get_channels_max( hwParams, &maxChans ),UnanticipatedHostError ) ;
  if ( isPlug && maxChans > 128 )                                            {
    maxChans = 128                                                           ;
    gPrintf                                                                ( (
      "%s: Limiting number of plugin channels to %u\n"                       ,
      __FUNCTION__                                                           ,
      maxChans                                                           ) ) ;
  }                                                                          ;
  /* Try low latency values, (sometimes the buffer & period that result are larger) */
  alsaBufferFrames = 512                                                     ;
  alsaPeriodFrames = 128                                                     ;
  ENSURE_( alsa_snd_pcm_hw_params_set_buffer_size_near( pcm, hwParams, &alsaBufferFrames       ), UnanticipatedHostError );
  ENSURE_( alsa_snd_pcm_hw_params_set_period_size_near( pcm, hwParams, &alsaPeriodFrames, &dir ), UnanticipatedHostError );
  *defaultLowLatency = (double) (alsaBufferFrames - alsaPeriodFrames) / defaultSr ;
  /* Base the high latency case on values four times larger                 */
  alsaBufferFrames = 2048                                                    ;
  alsaPeriodFrames =  512                                                    ;
  /* Have to reset hwParams, to set new buffer size; need to also set sample rate again */
  ENSURE_( alsa_snd_pcm_hw_params_any( pcm, hwParams ),UnanticipatedHostError );
  ENSURE_( SetApproximateSampleRate( pcm, hwParams, defaultSr ),UnanticipatedHostError );
  ENSURE_( alsa_snd_pcm_hw_params_set_buffer_size_near( pcm, hwParams, &alsaBufferFrames       ),UnanticipatedHostError ) ;
  ENSURE_( alsa_snd_pcm_hw_params_set_period_size_near( pcm, hwParams, &alsaPeriodFrames, &dir ),UnanticipatedHostError ) ;
  *defaultHighLatency = (double) (alsaBufferFrames - alsaPeriodFrames) / defaultSr ;
  *minChannels       = (int)minChans                                         ;
  *maxChannels       = (int)maxChans                                         ;
  *defaultSampleRate = defaultSr                                             ;
end                                                                          :
  alsa_snd_pcm_close ( pcm )                                                 ;
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

///////////////////////////////////////////////////////////////////////////////

AlsaHostApiInfo:: AlsaHostApiInfo (void)
                : HostApiInfo     (    )
{
}

AlsaHostApiInfo::~AlsaHostApiInfo (void)
{
}

///////////////////////////////////////////////////////////////////////////////

AlsaHostApi:: AlsaHostApi (void)
            : HostApi     (    )
{
  allocations    = NULL        ;
  hostApiIndex   = Development ;
  alsaLibVersion = 0           ;
}

AlsaHostApi::~AlsaHostApi(void)
{
  CaEraseAllocation ;
}

HostApi::Encoding AlsaHostApi::encoding(void) const
{
  return UTF8 ;
}

bool AlsaHostApi::hasDuplex(void) const
{
  return true ;
}

CaError AlsaHostApi::Open                            (
          Stream                ** s                 ,
          const StreamParameters * inputParameters   ,
          const StreamParameters * outputParameters  ,
          double                   sampleRate        ,
          CaUint32                 framesPerCallback ,
          CaStreamFlags            streamFlags       ,
          Conduit                * conduit           )
{
  CaError              result                 = NoError                      ;
  AlsaStream         * stream                 = NULL                         ;
  CaSampleFormat       hostInputSampleFormat  = cafNothing                   ;
  CaSampleFormat       hostOutputSampleFormat = cafNothing                   ;
  CaSampleFormat       inputSampleFormat      = cafNothing                   ;
  CaSampleFormat       outputSampleFormat     = cafNothing                   ;
  int                  numInputChannels       = 0                            ;
  int                  numOutputChannels      = 0                            ;
  CaTime               inputLatency           = 0                            ;
  CaTime               outputLatency          = 0                            ;
  CaHostBufferSizeMode hostBufferSizeMode     = cabFixedHostBufferSize       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( streamFlags & csfPlatformSpecificFlags ) != 0 ) return InvalidFlag  ;
  if ( CaNotNull ( inputParameters  ) )                                      {
    CA_ENSURE ( ValidateParameters ( inputParameters,StreamDirection_In )  ) ;
    numInputChannels  = inputParameters->channelCount                        ;
    inputSampleFormat = inputParameters->sampleFormat                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( outputParameters ) )                                      {
    CA_ENSURE( ValidateParameters( outputParameters,StreamDirection_Out )  ) ;
    numOutputChannels  = outputParameters->channelCount                      ;
    outputSampleFormat = outputParameters->sampleFormat                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* XXX: Why do we support this anyway?                                    */
  if ( framesPerCallback == Stream::FramesPerBufferUnspecified              &&
       getenv( "CA_ALSA_PERIODSIZE" ) != NULL                              ) {
    dPrintf                                                                ( (
      "%s: Getting framesPerBuffer (Alsa period-size) from environment\n"    ,
      __FUNCTION__                                                       ) ) ;
    framesPerCallback = atoi( getenv("CA_ALSA_PERIODSIZE") )                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream = new AlsaStream ( )                                                ;
  CA_UNLESS( stream , InsufficientMemory )                                   ;
  stream -> conduit  = conduit                                               ;
  stream -> debugger = debugger                                              ;
  CA_ENSURE( stream -> Initialize                                            (
               this                                                          ,
               inputParameters                                               ,
               outputParameters                                              ,
               sampleRate                                                    ,
               framesPerCallback                                             ,
               conduit                                                   ) ) ;
  CA_ENSURE( stream -> Configure                                             (
               inputParameters                                               ,
               outputParameters                                              ,
               sampleRate                                                    ,
               framesPerCallback                                             ,
               &inputLatency                                                 ,
               &outputLatency                                                ,
               &hostBufferSizeMode                                       ) ) ;
  hostInputSampleFormat  = (CaSampleFormat)                                  (
                             stream -> capture  . hostSampleFormat           |
                            ( ! stream -> capture  . hostInterleaved         ?
                                cafNonInterleaved : 0                    ) ) ;
  hostOutputSampleFormat = (CaSampleFormat)                                  (
                            stream -> playback . hostSampleFormat            |
                            ( ! stream -> playback . hostInterleaved         ?
                                cafNonInterleaved : 0                    ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  CA_ENSURE ( stream -> bufferProcessor . Initialize                         (
                numInputChannels                                             ,
                inputSampleFormat                                            ,
                hostInputSampleFormat                                        ,
                numOutputChannels                                            ,
                outputSampleFormat                                           ,
                hostOutputSampleFormat                                       ,
                sampleRate                                                   ,
                streamFlags                                                  ,
                framesPerCallback                                            ,
                stream->maxFramesPerHostBuffer                               ,
                hostBufferSizeMode                                           ,
                conduit                                                  ) ) ;
  /* Ok, buffer processor is initialized, now we can deduce it's latency    */
  if ( CaIsGreater ( numInputChannels  , 0 ) )                               {
    stream -> inputLatency  = inputLatency                                   +
                              (CaTime)(stream->bufferProcessor.InputLatencyFrames () / sampleRate ) ;
    if ( CaNotNull ( conduit ) )                                             {
      conduit -> Input  . setSample ( inputSampleFormat                      ,
                                      numInputChannels                     ) ;
    }                                                                        ;
  }                                                                          ;
  if ( CaIsGreater ( numOutputChannels , 0 ) )                               {
    stream -> outputLatency = outputLatency                                  +
                              (CaTime)(stream->bufferProcessor.OutputLatencyFrames() / sampleRate ) ;
    if ( CaNotNull ( conduit ) )                                             {
      conduit -> Output . setSample ( outputSampleFormat                     ,
                                      numOutputChannels                    ) ;
    }                                                                        ;
  }                                                                          ;
  dPrintf                                                                  ( (
    "%s: Stream: framesPerBuffer = %lu, maxFramesPerHostBuffer = %lu, latency i=%f, o=%f\n",
    __FUNCTION__                                                             ,
    framesPerCallback                                                        ,
    stream->maxFramesPerHostBuffer                                           ,
    stream->inputLatency                                                     ,
    stream->outputLatency                                                ) ) ;
  *s = (Stream *) stream                                                     ;
  return result                                                              ;
error                                                                        :
  if ( CaNotNull ( stream ) )                                                {
    dPrintf ( ( "%s: Stream in error, terminating\n" , __FUNCTION__      ) ) ;
    stream -> Terminate ( )                                                  ;
    delete stream                                                            ;
    stream = NULL                                                            ;
  }                                                                          ;
  return result                                                              ;
}

void AlsaHostApi::Terminate(void)
{
  CaEraseAllocation                      ;
  alsa_snd_config_update_free_global ( ) ;
  AlsaCloseLibrary                   ( ) ;
}

CaError AlsaHostApi::isSupported                      (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double                    sampleRate       )
{
  int            inputChannelCount  = 0                                      ;
  int            outputChannelCount = 0                                      ;
  CaError        result             = NoError                                ;
  CaSampleFormat inputSampleFormat                                           ;
  CaSampleFormat outputSampleFormat                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( inputParameters )                                                     {
    CA_ENSURE ( ValidateParameters(inputParameters  , StreamDirection_In ) ) ;
    inputChannelCount  = inputParameters  -> channelCount                    ;
    inputSampleFormat  = inputParameters  -> sampleFormat                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( outputParameters )                                                    {
    CA_ENSURE ( ValidateParameters(outputParameters , StreamDirection_Out) ) ;
    outputChannelCount = outputParameters -> channelCount                    ;
    outputSampleFormat = outputParameters -> sampleFormat                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( inputChannelCount )                                                   {
    result = TestParameters ( inputParameters                                ,
                              sampleRate                                     ,
                              StreamDirection_In                           ) ;
    if ( result != NoError ) goto error                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( outputChannelCount )                                                  {
    result = TestParameters ( outputParameters                               ,
                              sampleRate                                     ,
                              StreamDirection_Out                          ) ;
    if ( result != NoError ) goto error                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
error                                                                        :
  return result                                                              ;
}

CaError AlsaHostApi ::             Open      (
          const StreamParameters * params    ,
                StreamDirection    streamDir ,
          snd_pcm_t             ** pcm       )
{
  CaError                result     = NoError                                ;
  int                    ret                                                 ;
  const char           * deviceName = ""                                     ;
  const AlsaDeviceInfo * deviceInfo = NULL                                   ;
  AlsaStreamInfo       * streamInfo = (AlsaStreamInfo *)params->streamInfo ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ! streamInfo )                                                        {
     deviceInfo = GetAlsaDeviceInfo ( params->device )                       ;
     deviceName = deviceInfo -> alsaName                                     ;
  } else deviceName = streamInfo->deviceString                               ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "%s: Opening device %s\n" , __FUNCTION__ , deviceName      ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  ret = OpenPcm ( pcm                                                        ,
                  deviceName                                                 ,
                  streamDir == StreamDirection_In                            ?
                  SND_PCM_STREAM_CAPTURE                                     :
                  SND_PCM_STREAM_PLAYBACK                                    ,
                  SND_PCM_NONBLOCK                                           ,
                  1                                                        ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ret < 0 )                                                             {
    *pcm = NULL                                                              ;
    ENSURE_ ( ret , -EBUSY==ret ? DeviceUnavailable:BadIODeviceCombination ) ;
  }                                                                          ;
  ENSURE_ ( alsa_snd_pcm_nonblock( *pcm, 0 ) , UnanticipatedHostError )      ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

CaError AlsaHostApi :: TestParameters         (
          const StreamParameters * parameters ,
          double                   sampleRate ,
          StreamDirection          streamDir  )
{
  CaError               result = NoError                                     ;
  snd_pcm_t           * pcm    = NULL                                        ;
  CaSampleFormat        availableFormats                                     ;
  unsigned int          numHostChannels                                      ;
  CaSampleFormat        hostFormat                                           ;
  snd_pcm_hw_params_t * hwParams                                             ;
  ////////////////////////////////////////////////////////////////////////////
  alsa_snd_pcm_hw_params_alloca ( &hwParams )                                ;
  if ( ! parameters->streamInfo )                             {
    const AlsaDeviceInfo * devInfo = GetAlsaDeviceInfo(parameters->device )  ;
    numHostChannels = CA_MAX( parameters->channelCount                       ,
                              StreamDirection_In == streamDir                ?
                              devInfo -> minInputChannels                    :
                              devInfo -> minOutputChannels                 ) ;
  } else numHostChannels = parameters->channelCount                          ;
  ////////////////////////////////////////////////////////////////////////////
  CA_ENSURE ( Open ( parameters, streamDir, &pcm ) )                         ;
  alsa_snd_pcm_hw_params_any ( pcm , hwParams )                              ;
  if ( SetApproximateSampleRate( pcm, hwParams, sampleRate ) < 0 )           {
    result = InvalidSampleRate                                               ;
    goto error                                                               ;
  }                                                                          ;
  if (alsa_snd_pcm_hw_params_set_channels(pcm,hwParams,numHostChannels )<0)  {
    result = InvalidChannelCount                                             ;
    goto error                                                               ;
  }                                                                          ;
  /* See if we can find a best possible match                               */
  availableFormats = GetAvailableFormats ( pcm )                             ;
  CA_ENSURE( hostFormat = ClosestFormat( availableFormats                    ,
                                         parameters->sampleFormat        ) ) ;
  /* Some specific hardware (reported: Audio8 DJ) can fail with assertion during this step. */
  ENSURE_ ( alsa_snd_pcm_hw_params_set_format                                (
              pcm                                                            ,
              hwParams                                                       ,
              AlsaFormat(hostFormat)                                       ) ,
            UnanticipatedHostError                                         ) ;
  {                                                                          ;
    /* It happens that this call fails because the device is busy           */
    int ret = 0                                                              ;
    ret = alsa_snd_pcm_hw_params( pcm, hwParams )                            ;
    if ( ret < 0 )                                                           {
      if ( -EINVAL == ret )                                                  {
        /* Don't know what to return here                                   */
        result = BadIODeviceCombination                                      ;
        goto error                                                           ;
      } else
      if ( -EBUSY == ret )                                                   {
        result = DeviceUnavailable                                           ;
        gPrintf ( ( "%s: Device is busy\n" , __FUNCTION__ ) )                ;
      } else                                                                 {
        result = UnanticipatedHostError                                      ;
      }                                                                      ;
      ENSURE_ ( ret, result )                                                ;
    }                                                                        ;
  }                                                                          ;
end                                                                          :
  if ( pcm ) alsa_snd_pcm_close ( pcm )                                      ;
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

CaError AlsaHostApi::FillInDevInfo(
          HwDevInfo      * deviceHwInfo ,
          int              blocking     ,
          AlsaDeviceInfo * devInfo      ,
          int            * devIdx       )
{
  CaError      result         = NoError                                      ;
  snd_pcm_t  * pcm            = NULL                                         ;
  DeviceInfo * baseDeviceInfo = (DeviceInfo *)devInfo                        ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf                                                                  ( (
    "%s: Filling device info for: %s\n"                                      ,
    __FUNCTION__                                                             ,
    deviceHwInfo -> name                                                 ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  /* Zero fields                                                            */
  devInfo -> Initialize ( )                                                  ;
  /* Query capture                                                          */
  if ( deviceHwInfo->hasCapture                                             &&
       OpenPcm ( &pcm                                                        ,
                 deviceHwInfo->alsaName                                      ,
                 SND_PCM_STREAM_CAPTURE                                      ,
                 blocking                                                    ,
                 0                                                  ) >= 0 ) {
    if ( devInfo -> GropeDevice ( pcm                                        ,
                       deviceHwInfo->isPlug                                  ,
                       StreamDirection_In                                    ,
                       blocking ) != NoError                               ) {
      gPrintf                                                              ( (
        "%s: Failed groping %s for capture\n"                                ,
        __FUNCTION__                                                         ,
        deviceHwInfo -> alsaName                                         ) ) ;
      goto end                                                               ;
    }                                                                        ;
  }                                                                          ;
  /* Query playback                                                         */
  if ( ( 0    != deviceHwInfo -> hasPlayback )                              &&
       ( NULL != deviceHwInfo -> alsaName    )                              &&
       OpenPcm ( &pcm                                                        ,
                 deviceHwInfo->alsaName                                      ,
                 SND_PCM_STREAM_PLAYBACK                                     ,
                 blocking                                                    ,
                 0 ) >= 0                                                  ) {
    if ( devInfo -> GropeDevice ( pcm                                        ,
                                  deviceHwInfo->isPlug                       ,
                                  StreamDirection_Out                        ,
                                  blocking                    ) != NoError ) {
      gPrintf ( ( "%s: Failed groping %s for playback\n"                     ,
                  __FUNCTION__                                               ,
                  deviceHwInfo -> alsaName                               ) ) ;
      goto end                                                               ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  baseDeviceInfo -> structVersion = 2                                        ;
  baseDeviceInfo -> hostApi       = hostApiIndex                             ;
  baseDeviceInfo -> hostType      = ALSA                                     ;
  baseDeviceInfo -> name          = deviceHwInfo -> name                     ;
  devInfo        -> alsaName      = (char *)deviceHwInfo -> alsaName         ;
  devInfo        -> isPlug        = deviceHwInfo -> isPlug                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( baseDeviceInfo -> maxInputChannels  > 0                              ||
       baseDeviceInfo -> maxOutputChannels > 0                             ) {
    /* Make device default if there isn't already one or it is the ALSA "default" device */
    if ( ( info.defaultInputDevice == CaNoDevice                            ||
           ! strcmp( deviceHwInfo->alsaName, "default"                  ) ) &&
           baseDeviceInfo->maxInputChannels > 0 )                            {
      info . defaultInputDevice = *devIdx                                    ;
      gPrintf ( ( "Default input device: %s\n" , deviceHwInfo->name      ) ) ;
    }                                                                        ;
    if ( ( info.defaultOutputDevice == CaNoDevice                           ||
           !strcmp( deviceHwInfo->alsaName, "default"                   ) ) &&
           baseDeviceInfo->maxOutputChannels > 0 )                           {
      info . defaultOutputDevice = *devIdx                                   ;
      gPrintf ( ( "Default output device: %s\n" , deviceHwInfo->name     ) ) ;
    }                                                                        ;
    gPrintf ( ( "%s: Adding device %s: %d\n"                                 ,
                __FUNCTION__                                                 ,
                deviceHwInfo -> name                                         ,
                *devIdx                                                  ) ) ;
    deviceInfos[*devIdx] = (DeviceInfo *) devInfo                            ;
    (*devIdx) += 1                                                           ;
  } else                                                                     {
    gPrintf                                                                ( (
      "%s: Skipped device: %s, all channels == 0\n"                          ,
      __FUNCTION__                                                           ,
      deviceHwInfo -> name                                               ) ) ;
  }                                                                          ;
end                                                                          :
  return result                                                              ;
}

/* Build PaDeviceInfo list, ignore devices for which we cannot determine capabilities (possibly busy, sigh) */
CaError AlsaHostApi::BuildDeviceList(void)
{
  CaError               result         = NoError                             ;
  AlsaDeviceInfo      * deviceInfoArray                                      ;
  int                   cardIdx        = -1                                  ;
  int                   devIdx         = 0                                   ;
  snd_ctl_card_info_t * cardInfo                                             ;
  size_t                numDeviceNames = 0                                   ;
  size_t                maxDeviceNames = 1                                   ;
  size_t                i                                                    ;
  HwDevInfo           * hwDevInfos     = NULL                                ;
  snd_config_t        * topNode        = NULL                                ;
  snd_pcm_info_t      * pcmInfo                                              ;
  int                   res                                                  ;
  int                   blocking       = SND_PCM_NONBLOCK                    ;
  int                   usePlughw      = 0                                   ;
  char                * hwPrefix       = ""                                  ;
  char                  alsaCardName [ 50 ]                                  ;
  ////////////////////////////////////////////////////////////////////////////
  if ( getenv (          "CA_ALSA_INITIALIZE_BLOCK" )                       &&
       atoi   ( getenv ( "CA_ALSA_INITIALIZE_BLOCK" )                    ) ) {
    blocking = 0                                                             ;
  }                                                                          ;
  /* If PA_ALSA_PLUGHW is 1 (non-zero), use the plughw: pcm throughout instead of hw: */
  if ( getenv (          "CA_ALSA_PLUGHW" )                                 &&
       atoi   ( getenv ( "CA_ALSA_PLUGHW" )                              ) ) {
    usePlughw = 1                                                            ;
    hwPrefix  = "plug"                                                       ;
    gPrintf ( ( "%s: Using Plughw\n" , __FUNCTION__ ) )                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* These two will be set to the first working input and output device, respectively */
  info . defaultInputDevice  = CaNoDevice                                    ;
  info . defaultOutputDevice = CaNoDevice                                    ;
  cardIdx = -1                                                               ;
  ENSURE_ ( alsa_snd_config_update ( ) , UnanticipatedHostError            ) ;
  ////////////////////////////////////////////////////////////////////////////
  alsa_snd_ctl_card_info_alloca ( &cardInfo )                                ;
  alsa_snd_pcm_info_alloca      ( &pcmInfo  )                                ;
  ////////////////////////////////////////////////////////////////////////////
  while ( alsa_snd_card_next( &cardIdx ) == 0 && cardIdx >= 0 )              {
    char      * cardName                                                     ;
    int         devIdx = -1                                                  ;
    snd_ctl_t * ctl                                                          ;
    char        buf [ 50 ]                                                   ;
    //////////////////////////////////////////////////////////////////////////
    ::snprintf( alsaCardName, sizeof (alsaCardName), "hw:%d", cardIdx )      ;
    /* Acquire name of card                                                 */
    if ( alsa_snd_ctl_open ( &ctl , alsaCardName , 0 ) < 0 )                 {
      /* Unable to open card :(                                             */
      gPrintf                                                              ( (
        "%s: Unable to open device %s\n"                                     ,
        __FUNCTION__                                                         ,
        alsaCardName                                                     ) ) ;
      continue                                                               ;
    }                                                                        ;
    alsa_snd_ctl_card_info ( ctl , cardInfo )                                ;
    CA_ENSURE ( StrDup( &cardName                                            ,
                        alsa_snd_ctl_card_info_get_name( cardInfo )      ) ) ;
    while ( alsa_snd_ctl_pcm_next_device( ctl, &devIdx ) == 0               &&
            devIdx >= 0                                                    ) {
      char * alsaDeviceName = NULL                                           ;
      char * deviceName     = NULL                                           ;
      char * infoName       = NULL                                           ;
      size_t len            = 0                                              ;
      int    hasPlayback    = 0                                              ;
      int    hasCapture     = 0                                              ;
      ////////////////////////////////////////////////////////////////////////
      snprintf ( buf,sizeof (buf),"%s%s,%d",hwPrefix,alsaCardName,devIdx )   ;
      /* Obtain info about this particular device                           */
      alsa_snd_pcm_info_set_device    ( pcmInfo , devIdx                   ) ;
      alsa_snd_pcm_info_set_subdevice ( pcmInfo , 0                        ) ;
      alsa_snd_pcm_info_set_stream    ( pcmInfo , SND_PCM_STREAM_CAPTURE   ) ;
      if ( alsa_snd_ctl_pcm_info( ctl, pcmInfo ) >= 0 ) hasCapture  = 1      ;
      alsa_snd_pcm_info_set_stream    ( pcmInfo , SND_PCM_STREAM_PLAYBACK  ) ;
      if ( alsa_snd_ctl_pcm_info( ctl, pcmInfo ) >= 0 ) hasPlayback = 1      ;
      if ( !hasPlayback && !hasCapture ) continue                            ;
      infoName = SkipCardDetailsInName( (char *)alsa_snd_pcm_info_get_name( pcmInfo ), cardName ) ;
      /* The length of the string written by snprintf plus terminating 0    */
      len = snprintf( NULL, 0, "%s: %s (%s)", cardName, infoName, buf ) + 1  ;
      deviceName = (char *)allocations->alloc(len )                          ;
      CA_UNLESS ( deviceName , InsufficientMemory )                          ;
      snprintf  ( deviceName, len, "%s: %s (%s)", cardName, infoName, buf )  ;
      ++numDeviceNames                                                       ;
      if ( !hwDevInfos || numDeviceNames > maxDeviceNames )                  {
        maxDeviceNames *= 2                                                  ;
        hwDevInfos = (HwDevInfo *) realloc( hwDevInfos                       ,
                                            maxDeviceNames                   *
                                            sizeof (HwDevInfo)             ) ;
        CA_UNLESS ( hwDevInfos , InsufficientMemory )                        ;
      }                                                                      ;
      CA_ENSURE ( StrDup ( &alsaDeviceName, buf ) )                          ;
      ////////////////////////////////////////////////////////////////////////
      hwDevInfos[ numDeviceNames - 1 ] . alsaName    = alsaDeviceName        ;
      hwDevInfos[ numDeviceNames - 1 ] . name        = deviceName            ;
      hwDevInfos[ numDeviceNames - 1 ] . isPlug      = usePlughw             ;
      hwDevInfos[ numDeviceNames - 1 ] . hasPlayback = hasPlayback           ;
      hwDevInfos[ numDeviceNames - 1 ] . hasCapture  = hasCapture            ;
    }                                                                        ;
    alsa_snd_ctl_close ( ctl )                                               ;
  }                                                                          ;
  /* Iterate over plugin devices                                            */
  if ( CaIsNull(*alsa_snd_config) )                                          {
    /* alsa_snd_config_update is called implicitly by some functions, if this
     * hasn't happened snd_config will be NULL (bleh)                       */
    ENSURE_ ( alsa_snd_config_update ( ) , UnanticipatedHostError          ) ;
    gPrintf ( ( "Updating snd_config\n" ) )                                  ;
    if ( CaIsNull(*alsa_snd_config)) goto error                              ;
  }                                                                          ;
  assert ( CaNotNull(*alsa_snd_config) )                                     ;
  res = alsa_snd_config_search( *alsa_snd_config, "pcm", &topNode )          ;
  if ( res >= 0 )                                                            {
    snd_config_iterator_t i, next                                            ;
    alsa_snd_config_for_each ( i, next, topNode  )                           {
      const char      * tpStr          = "unknown"                           ;
      const char      * idStr          = NULL                                ;
      const HwDevInfo * predefined     = NULL                                ;
      int               err            = 0                                   ;
      char            * alsaDeviceName = NULL                                ;
      char            * deviceName     = NULL                                ;
      snd_config_t    * tp             = NULL                                ;
      snd_config_t    * n = alsa_snd_config_iterator_entry ( i )             ;
      ////////////////////////////////////////////////////////////////////////
      err = alsa_snd_config_search ( n , "type" , &tp )                      ;
      if ( err < 0 )                                                         {
        if ( -ENOENT != err )                                                {
          ENSURE_ ( err , UnanticipatedHostError )                           ;
        }                                                                    ;
      } else                                                                 {
        ENSURE_ ( alsa_snd_config_get_string ( tp , &tpStr )                 ,
                  UnanticipatedHostError                                   ) ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      ENSURE_ ( alsa_snd_config_get_id(n,&idStr) , UnanticipatedHostError  ) ;
      if ( IgnorePlugin ( idStr ) )                                          {
        gPrintf                                                            ( (
          "%s: Ignoring ALSA plugin device [%s] of type [%s]\n"              ,
          __FUNCTION__                                                       ,
          idStr                                                              ,
          tpStr                                                          ) ) ;
        continue                                                             ;
      }                                                                      ;
      gPrintf                                                              ( (
        "%s: Found plugin [%s] of type [%s]\n"                               ,
        __FUNCTION__                                                         ,
        idStr                                                                ,
        tpStr                                                            ) ) ;
      ////////////////////////////////////////////////////////////////////////
      alsaDeviceName = (char *)allocations -> alloc ( strlen(idStr) + 6 )    ;
      CA_UNLESS ( alsaDeviceName , InsufficientMemory )                      ;
      strcpy    ( alsaDeviceName , idStr              )                      ;
      deviceName     = (char *)allocations -> alloc ( strlen(idStr) + 1 )    ;
      CA_UNLESS ( deviceName , InsufficientMemory )                          ;
      strcpy    ( deviceName , idStr              )                          ;
      ++numDeviceNames                                                       ;
      if ( !hwDevInfos || numDeviceNames > maxDeviceNames )                  {
        maxDeviceNames *= 2                                                  ;
        hwDevInfos      = (HwDevInfo *) realloc( hwDevInfos                  ,
                                                 maxDeviceNames              *
                                                 sizeof (HwDevInfo)        ) ;
        CA_UNLESS ( hwDevInfos , InsufficientMemory )                        ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      predefined = FindDeviceName ( alsaDeviceName )                         ;
      hwDevInfos[numDeviceNames - 1].alsaName = alsaDeviceName               ;
      hwDevInfos[numDeviceNames - 1].name     = deviceName                   ;
      hwDevInfos[numDeviceNames - 1].isPlug   = 1                            ;
      ////////////////////////////////////////////////////////////////////////
      if ( predefined )                                                      {
        hwDevInfos[numDeviceNames - 1].hasPlayback = predefined->hasPlayback ;
        hwDevInfos[numDeviceNames - 1].hasCapture  = predefined->hasCapture  ;
      } else                                                                 {
        hwDevInfos[numDeviceNames - 1].hasPlayback = 1                       ;
        hwDevInfos[numDeviceNames - 1].hasCapture  = 1                       ;
      }                                                                      ;
    }                                                                        ;
  } else                                                                     {
    gPrintf                                                                ( (
      "%s: Iterating over ALSA plugins failed: %s\n"                         ,
      __FUNCTION__                                                           ,
      alsa_snd_strerror( res )                                           ) ) ;
  }                                                                          ;
  /* allocate deviceInfo memory based on the number of devices              */
  deviceInfos = (DeviceInfo **) new DeviceInfo * [ numDeviceNames ]          ;
  CA_UNLESS ( deviceInfos , InsufficientMemory )                             ;
  /* allocate all device info structs in a contiguous block                 */
  deviceInfoArray = (AlsaDeviceInfo *) new AlsaDeviceInfo [ numDeviceNames ] ;
  CA_UNLESS ( deviceInfoArray , InsufficientMemory )                         ;
  gPrintf                                                                  ( (
    "%s: Filling device info for %d devices\n"                               ,
    __FUNCTION__                                                             ,
    numDeviceNames                                                       ) ) ;
  for ( i = 0, devIdx = 0; i < numDeviceNames; ++i )                         {
    AlsaDeviceInfo * devInfo = (AlsaDeviceInfo *)&deviceInfoArray[i]         ;
    HwDevInfo      * hwInfo  = &hwDevInfos[i]                                ;
    if ( ! strcmp( hwInfo->name, "dmix"                                   ) ||
         ! strcmp( hwInfo->name, "default"                              ) )  {
      continue                                                               ;
    }                                                                        ;
    CA_ENSURE ( FillInDevInfo( hwInfo , blocking , devInfo , &devIdx ) )     ;
  }                                                                          ;
  assert ( devIdx < numDeviceNames )                                         ;
  /* Now inspect 'dmix' and 'default' plugins                               */
  for ( i = 0 ; i < numDeviceNames ; ++i )                                   {
    AlsaDeviceInfo * devInfo = (AlsaDeviceInfo *) &deviceInfoArray[i]        ;
    HwDevInfo      * hwInfo  = &hwDevInfos[i]                                ;
    if ( strcmp( hwInfo->name, "dmix"                                     ) &&
         strcmp( hwInfo->name, "default"                                ) )  {
      continue                                                               ;
    }                                                                        ;
    CA_ENSURE ( FillInDevInfo ( hwInfo , blocking , devInfo , &devIdx ) )    ;
  }                                                                          ;
  free ( hwDevInfos )                                                        ;
  info.deviceCount = devIdx                                                  ;
end                                                                          :
  return result                                                              ;
error                                                                        :
  goto end                                                                   ;
}

/* Check against known device capabilities                                  */
CaError AlsaHostApi::ValidateParameters       (
          const StreamParameters * parameters ,
          StreamDirection          mode       )
{
  int                    maxChans                                            ;
  CaError                result     = NoError                                ;
  const AlsaDeviceInfo * deviceInfo = NULL                                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( parameters->device != CaUseHostApiSpecificDeviceSpecification )       {
    CA_UNLESS( parameters->streamInfo == NULL, BadIODeviceCombination ) ;
    deviceInfo = GetAlsaDeviceInfo ( parameters->device )                    ;
  } else                                                                     {
    const AlsaStreamInfo * streamInfo                                        ;
    streamInfo = (const AlsaStreamInfo *)parameters->streamInfo ;
    CA_UNLESS ( parameters->device == CaUseHostApiSpecificDeviceSpecification, InvalidDevice );
    CA_UNLESS ( streamInfo->size   == sizeof (AlsaStreamInfo) && streamInfo->version == 1,
               IncompatibleStreamInfo                       ) ;
    CA_UNLESS ( streamInfo->deviceString != NULL , InvalidDevice           ) ;
    /* Skip further checking                                                */
    return NoError                                                           ;
  }                                                                          ;
  maxChans = ( StreamDirection_In == mode                                    ?
               deviceInfo -> maxInputChannels                                :
               deviceInfo -> maxOutputChannels                             ) ;
  CA_UNLESS  ( parameters -> channelCount <= maxChans,InvalidChannelCount  ) ;
error                                                                        :
  return result                                                              ;
}

CaError AlsaHostApi::StrDup(char **dst,const char * src)
{
  CaError result = NoError                    ;
  int     len    = strlen(src) + 1            ;
  /////////////////////////////////////////////
  *dst = (char *)allocations -> alloc ( len ) ;
  CA_UNLESS ( *dst , InsufficientMemory     ) ;
  ::strncpy ( *dst, src, len                ) ;
error                                         :
  return result                               ;
}

const AlsaDeviceInfo * AlsaHostApi::GetAlsaDeviceInfo(int device)
{
  return (const AlsaDeviceInfo *)deviceInfos[device] ;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
