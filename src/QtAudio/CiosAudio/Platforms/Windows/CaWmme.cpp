/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/19                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/


#include "CaWmme.hpp"

#if defined(WIN32) || defined(_WIN32)
#else
#error "Windows Multimedia Extensions works only on Windows platform"
#endif

///////////////////////////////////////////////////////////////////////////////

#ifndef DWORD_PTR
  #if defined(_WIN64)
    #define DWORD_PTR unsigned __int64
  #else
    #define DWORD_PTR unsigned long
  #endif
#endif

/* use CreateThread for CYGWIN, _beginthreadex for all others */
#if !defined(__CYGWIN__) && !defined(_WIN32_WCE)
#define CREATE_THREAD (HANDLE)_beginthreadex( 0, 0, ProcessingThreadProc, this, 0, &processingThreadId )
#define CA_THREAD_FUNC static unsigned WINAPI
#else
#define CREATE_THREAD CreateThread( 0, 0, ProcessingThreadProc, this, 0, (LPDWORD)&processingThreadId )
#define CA_THREAD_FUNC static DWORD WINAPI
#endif

#define CA_IS_INPUT_STREAM_(stream)       ( stream -> input  . waveHandles )
#define CA_IS_OUTPUT_STREAM_(stream)      ( stream -> output . waveHandles )
#define CA_IS_FULL_DUPLEX_STREAM_(stream) ( ( NULL != stream -> input  . waveHandles ) && ( NULL != stream -> output . waveHandles ) )
#define CA_IS_HALF_DUPLEX_STREAM_(stream) ( ! ( ( NULL != stream -> input  . waveHandles ) && ( NULL != stream -> output . waveHandles ) ) )

/* The following are flags which can be set in CaMmeStreamInfo's flags field.*/

#define CaMmeUseLowLevelLatencyParameters (0x01)
#define CaMmeUseMultipleDevices           (0x02) /* use mme specific multiple device feature */
#define CaMmeUseChannelMask               (0x04)

/* By default, the mme implementation drops the processing thread's priority
    to THREAD_PRIORITY_NORMAL and sleeps the thread if the CPU load exceeds 100%
    This flag disables any priority throttling. The processing thread will always
    run at THREAD_PRIORITY_TIME_CRITICAL.                                    */
#define CaMmeDontThrottleOverloadedProcessingThread  (0x08)

/*  Flags for non-PCM spdif passthrough.                                     */
#define CaMmeWaveFormatDolbyAc3Spdif                 (0x10)
#define CaMmeWaveFormatWmaSpdif                      (0x20)

#ifndef DRV_QUERYDEVICEINTERFACE
#define DRV_QUERYDEVICEINTERFACE     (DRV_RESERVED + 12)
#endif

#ifndef DRV_QUERYDEVICEINTERFACESIZE
#define DRV_QUERYDEVICEINTERFACESIZE (DRV_RESERVED + 13)
#endif

/************************************************* Constants ********/

#define CA_MME_USE_HIGH_DEFAULT_LATENCY_    (0)  /* For debugging glitches. */

#if CA_MME_USE_HIGH_DEFAULT_LATENCY_
 #define CA_MME_WIN_9X_DEFAULT_LATENCY_                             (0.4)
 #define CA_MME_MIN_HOST_OUTPUT_BUFFER_COUNT_                       (4)
 #define CA_MME_MIN_HOST_INPUT_BUFFER_COUNT_FULL_DUPLEX_            (4)
 #define CA_MME_MIN_HOST_INPUT_BUFFER_COUNT_HALF_DUPLEX_            (4)
 #define CA_MME_HOST_BUFFER_GRANULARITY_FRAMES_WHEN_UNSPECIFIED_    (16)
 #define CA_MME_MAX_HOST_BUFFER_SECS_                               (0.3)       /* Do not exceed unless user buffer exceeds */
 #define CA_MME_MAX_HOST_BUFFER_BYTES_                              (32 * 1024) /* Has precedence over PA_MME_MAX_HOST_BUFFER_SECS_, some drivers are known to crash with buffer sizes > 32k */
#else
 #define CA_MME_WIN_9X_DEFAULT_LATENCY_                             (0.2)
 #define CA_MME_MIN_HOST_OUTPUT_BUFFER_COUNT_                       (2)
 #define CA_MME_MIN_HOST_INPUT_BUFFER_COUNT_FULL_DUPLEX_            (3)         /* always use at least 3 input buffers for full duplex */
 #define CA_MME_MIN_HOST_INPUT_BUFFER_COUNT_HALF_DUPLEX_            (2)
 #define CA_MME_HOST_BUFFER_GRANULARITY_FRAMES_WHEN_UNSPECIFIED_    (16)
 #define CA_MME_MAX_HOST_BUFFER_SECS_                               (0.1)       /* Do not exceed unless user buffer exceeds */
 #define CA_MME_MAX_HOST_BUFFER_BYTES_                              (32 * 1024) /* Has precedence over PA_MME_MAX_HOST_BUFFER_SECS_, some drivers are known to crash with buffer sizes > 32k */
#endif

/* Use higher latency for NT because it is even worse at real-time
   operation than Win9x.
*/
#define CA_MME_WIN_NT_DEFAULT_LATENCY_                              (0.4)

/* Default low latency for WDM based systems. This is based on a rough
   survey of workable latency settings using patest_wmme_find_best_latency_params.c.
   See pdf attached to ticket 185 for a graph of the survey results:
   http://www.portaudio.com/trac/ticket/185

   Workable latencies varied between 40ms and ~80ms on different systems (different
   combinations of hardware, 32 and 64 bit, WinXP, Vista and Win7. We didn't
   get enough Vista results to know if Vista has systemically worse latency.
   For now we choose a safe value across all Windows versions here.
*/
#define CA_MME_WIN_WDM_DEFAULT_LATENCY_                             (0.090)

/* When client suggestedLatency could result in many host buffers, we aim to have around 8,
   based off Windows documentation that suggests that the kmixer uses 8 buffers. This choice
   is somewhat arbitrary here, since we havn't observed significant stability degredation
   with using either more, or less buffers.
*/
#define CA_MME_TARGET_HOST_BUFFER_COUNT_    8
#define CA_MME_MIN_TIMEOUT_MSEC_        (1000)

/********************************************************************/

/* macros for setting last host error information */

#ifdef UNICODE

#define CA_MME_SET_LAST_WAVEIN_ERROR( mmresult )                            \
    {                                                                       \
      wchar_t mmeErrorTextWide [ MAXERRORLENGTH ] ;                         \
      char    mmeErrorText     [ MAXERRORLENGTH ] ;                         \
      waveInGetErrorText   ( mmresult, mmeErrorTextWide, MAXERRORLENGTH ) ; \
      WideCharToMultiByte  ( CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR   , \
                             mmeErrorTextWide                             , \
                             -1                                           , \
                             mmeErrorText                                 , \
                             MAXERRORLENGTH                               , \
                             NULL                                         , \
                             NULL                                       ) ; \
      SetLastHostErrorInfo ( MME, mmresult, mmeErrorText                ) ; \
    }

#define CA_MME_SET_LAST_WAVEOUT_ERROR( mmresult )                           \
    {                                                                       \
      wchar_t mmeErrorTextWide [ MAXERRORLENGTH ] ;                         \
      char    mmeErrorText     [ MAXERRORLENGTH ] ;                         \
      waveOutGetErrorText  ( mmresult, mmeErrorTextWide, MAXERRORLENGTH ) ; \
      WideCharToMultiByte  ( CP_ACP                                       , \
                             WC_COMPOSITECHECK                            | \
                             WC_DEFAULTCHAR                               , \
                             mmeErrorTextWide                             , \
                             -1                                           , \
                             mmeErrorText                                 , \
                             MAXERRORLENGTH                               , \
                             NULL                                         , \
                             NULL                                       ) ; \
      SetLastHostErrorInfo ( MME, mmresult, mmeErrorText                ) ; \
    }

#else /* !UNICODE */

#define CA_MME_SET_LAST_WAVEIN_ERROR( mmresult )                        \
    {                                                                   \
      char mmeErrorText[ MAXERRORLENGTH ];                              \
      waveInGetErrorText   ( mmresult, mmeErrorText, MAXERRORLENGTH ) ; \
      SetLastHostErrorInfo ( MME, mmresult, mmeErrorText            ) ; \
    }

#define CA_MME_SET_LAST_WAVEOUT_ERROR( mmresult )                       \
    {                                                                   \
      char mmeErrorText[ MAXERRORLENGTH ] ;                             \
      waveOutGetErrorText  ( mmresult, mmeErrorText, MAXERRORLENGTH ) ; \
      SetLastHostErrorInfo ( MME, mmresult, mmeErrorText            ) ; \
    }

#endif /* UNICODE */

#define CA_ENV_BUF_SIZE_         (32)
#define CA_REC_IN_DEV_ENV_NAME_  ("CA_RECOMMENDED_INPUT_DEVICE")
#define CA_REC_OUT_DEV_ENV_NAME_ ("CA_RECOMMENDED_OUTPUT_DEVICE")

#define CA_DEFAULTSAMPLERATESEARCHORDER_COUNT_  (14) /* must match array length below */

#define CA_CIRCULAR_INCREMENT_( current, max ) ( (((current) + 1) >= (max)) ? (0) : (current+1) )

#define CA_CIRCULAR_DECREMENT_( current, max ) ( ((current) == 0) ? ((max)-1) : (current-1) )

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

static const char constInputMapperSuffix_       [ ] = " - Input"  ;
static const char constOutputMapperSuffix_      [ ] = " - Output" ;
static double   * defaultSampleRateSearchOrder_     = DefaultSampleRateSearchOrders ;

/* copies TCHAR string to explicit char string */
char * StrTCpyToC(char * to,const TCHAR * from)
{
  #if !defined(_UNICODE) && !defined(UNICODE)
  return ::strcpy ( to , from )                ;
  #else
  int count = ::wcslen ( from )                ;
  if ( 0 != count )                            {
    if ( ::WideCharToMultiByte ( CP_ACP        ,
                                 0             ,
                                 from          ,
                                 count         ,
                                 to            ,
                                 count         ,
                                 NULL          ,
                                 NULL ) == 0 ) {
      return NULL                              ;
    }                                          ;
  }                                            ;
  return to                                    ;
#endif
}

/* returns length of TCHAR string */
size_t StrTLen(const TCHAR * str)
{
  #if !defined(_UNICODE) && !defined(UNICODE)
  return ::strlen(str);
  #else
  return ::wcslen(str);
  #endif
}

static void InitializeSingleDirectionHandlesAndBuffers                  (
              CaMmeSingleDirectionHandlesAndBuffers * handlesAndBuffers )
{
  handlesAndBuffers -> bufferEvent = 0 ;
  handlesAndBuffers -> waveHandles = 0 ;
  handlesAndBuffers -> deviceCount = 0 ;
  handlesAndBuffers -> waveHeaders = 0 ;
  handlesAndBuffers -> bufferCount = 0 ;
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

///////////////////////////////////////////////////////////////////////////////

static CaError QueryInputWaveFormatEx( int            deviceId     ,
                                       WAVEFORMATEX * waveFormatEx )
{
  MMRESULT mmresult                             ;
  mmresult = ::waveInOpen ( NULL                ,
                            deviceId            ,
                            waveFormatEx        ,
                            0                   ,
                            0                   ,
                            WAVE_FORMAT_QUERY ) ;
  switch ( mmresult )                           {
    case MMSYSERR_NOERROR                       :
    return NoError                              ;
    case MMSYSERR_ALLOCATED                     : /* Specified resource is already allocated. */
    return DeviceUnavailable                    ;
    case MMSYSERR_NODRIVER                      : /* No device driver is present. */
    return DeviceUnavailable                    ;
    case MMSYSERR_NOMEM                         : /* Unable to allocate or lock memory. */
    return InsufficientMemory                   ;
    case WAVERR_BADFORMAT                       : /* Attempted to open with an unsupported waveform-audio format. */
    return SampleFormatNotSupported             ;
    case MMSYSERR_BADDEVICEID                   : /* Specified device identifier is out of range. */
    /* falls through                           */
    default                                     :
      CA_MME_SET_LAST_WAVEIN_ERROR( mmresult )  ;
    return UnanticipatedHostError               ;
  }                                             ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError QueryOutputWaveFormatEx ( int            deviceId     ,
                                         WAVEFORMATEX * waveFormatEx )
{
  MMRESULT mmresult;
  mmresult = ::waveOutOpen ( NULL                ,
                             deviceId            ,
                             waveFormatEx        ,
                             0                   ,
                             0                   ,
                             WAVE_FORMAT_QUERY ) ;
  switch( mmresult )                             {
    case MMSYSERR_NOERROR                        :
    return NoError                               ;
    case MMSYSERR_ALLOCATED                      : /* Specified resource is already allocated. */
    return DeviceUnavailable                     ;
    case MMSYSERR_NODRIVER                       : /* No device driver is present. */
    return DeviceUnavailable                     ;
    case MMSYSERR_NOMEM                          : /* Unable to allocate or lock memory. */
    return InsufficientMemory                    ;
    case WAVERR_BADFORMAT                        : /* Attempted to open with an unsupported waveform-audio format. */
    return SampleFormatNotSupported              ;
    case MMSYSERR_BADDEVICEID                    : /* Specified device identifier is out of range. */
    /* falls through                            */
    default                                      :
      CA_MME_SET_LAST_WAVEOUT_ERROR( mmresult )  ;
    return UnanticipatedHostError                ;
  }                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static void CaMmeSetLastSystemError( DWORD errorCode )
{
  char * lpMsgBuf                             ;
  FormatMessage                               (
    FORMAT_MESSAGE_ALLOCATE_BUFFER            |
    FORMAT_MESSAGE_FROM_SYSTEM                ,
    NULL                                      ,
    errorCode                                 ,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT) ,
    (LPTSTR) &lpMsgBuf                        ,
    0                                         ,
    NULL                                    ) ;
    SetLastHostErrorInfo ( MME                ,
                           errorCode          ,
                           lpMsgBuf         ) ;
  ::LocalFree ( lpMsgBuf )                    ;
}

#define CA_MME_SET_LAST_SYSTEM_ERROR( errorCode ) CaMmeSetLastSystemError( errorCode )

/* PaError returning wrappers for some commonly used win32 functions
    note that we allow passing a null ptr to have no effect.                 */
///////////////////////////////////////////////////////////////////////////////

static CaError CreateEventWithCaError                    (
                 HANDLE              * handle            ,
                 LPSECURITY_ATTRIBUTES lpEventAttributes ,
                 BOOL                  bManualReset      ,
                 BOOL                  bInitialState     ,
                 LPCTSTR               lpName            )
{
  CaError result = NoError                          ;
  *handle = NULL                                    ;
  *handle = CreateEvent( lpEventAttributes          ,
                         bManualReset               ,
                         bInitialState              ,
                         lpName                   ) ;
  if ( NULL == (*handle) )                          {
    result = UnanticipatedHostError                 ;
    CA_MME_SET_LAST_SYSTEM_ERROR ( GetLastError() ) ;
  }                                                 ;
  return result                                     ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError ResetEventWithCaError( HANDLE handle )
{
  CaError result = NoError                            ;
  if ( 0 != handle )                                  {
    if ( 0 == ::ResetEvent( handle ) )                {
      result = UnanticipatedHostError                 ;
      CA_MME_SET_LAST_SYSTEM_ERROR ( GetLastError() ) ;
    }                                                 ;
  }                                                   ;
  return result                                       ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError CloseHandleWithCaError( HANDLE handle )
{
  CaError result = NoError                           ;
  if ( 0 != handle )                                 {
    if ( 0 == ::CloseHandle( handle ) )              {
      result = UnanticipatedHostError                ;
      CA_MME_SET_LAST_SYSTEM_ERROR( GetLastError() ) ;
    }                                                ;
  }                                                  ;
  return result                                      ;
}

///////////////////////////////////////////////////////////////////////////////

static CaDeviceIndex GetEnvDefaultDeviceID (char * envName)
{
  CaDeviceIndex recommendedIndex = CaNoDevice                            ;
  DWORD         hresult                                                  ;
  char          envbuf[CA_ENV_BUF_SIZE_]                                 ;
  #ifndef WIN32_PLATFORM_PSPC /* no GetEnvironmentVariable on PocketPC  */
  /* Let user determine default device by setting environment variable. */
  hresult = ::GetEnvironmentVariableA(envName,envbuf,CA_ENV_BUF_SIZE_)   ;
  if ( ( hresult > 0 ) && ( hresult < CA_ENV_BUF_SIZE_ ) )               {
    recommendedIndex = ::atoi ( envbuf )                                 ;
  }                                                                      ;
  #endif
  return recommendedIndex                                                ;
}

///////////////////////////////////////////////////////////////////////////////

static void GetDefaultLatencies( CaTime * defaultLowLatency  ,
                                 CaTime * defaultHighLatency )
{
  OSVERSIONINFO osvi                                                   ;
  osvi . dwOSVersionInfoSize = sizeof( osvi )                          ;
  GetVersionEx ( &osvi )                                               ;
  /* Check for NT                                                     */
  if ( ( osvi . dwMajorVersion == 4) && ( osvi . dwPlatformId == 2 ) ) {
    *defaultLowLatency  = CA_MME_WIN_NT_DEFAULT_LATENCY_               ;
  } else
  if ( osvi.dwMajorVersion >= 5 )                                      {
    *defaultLowLatency  = CA_MME_WIN_WDM_DEFAULT_LATENCY_              ;
  } else                                                               {
    *defaultLowLatency  = CA_MME_WIN_9X_DEFAULT_LATENCY_               ;
  }                                                                    ;
  *defaultHighLatency = (*defaultLowLatency) * 2                       ;
}

///////////////////////////////////////////////////////////////////////////////

static unsigned long ComputeHostBufferCountForFixedBufferSizeFrames (
                       unsigned long suggestedLatencyFrames         ,
                       unsigned long hostBufferSizeFrames           ,
                       unsigned long minimumBufferCount             )
{
  /* Calculate the number of buffers of length hostFramesPerBuffer
     that fit in suggestedLatencyFrames, rounding up to the next integer.
     The value (hostBufferSizeFrames - 1) below is to ensure the buffer count is rounded up.
   */
  unsigned long resultBufferCount = ( ( suggestedLatencyFrames               +
                                      ( hostBufferSizeFrames - 1 )         ) /
                                        hostBufferSizeFrames               ) ;
  /* We always need one extra buffer for processing while the rest are queued/playing.
     i.e. latency is framesPerBuffer * (bufferCount - 1)                    */
  resultBufferCount += 1                                                     ;
  if ( resultBufferCount < minimumBufferCount )                              {
    /* clamp to minimum buffer count                                        */
    resultBufferCount = minimumBufferCount                                   ;
  }                                                                          ;
  return resultBufferCount                                                   ;
}

///////////////////////////////////////////////////////////////////////////////

static unsigned long ComputeHostBufferSizeGivenHardUpperLimit        (
                       unsigned long userFramesPerBuffer             ,
                       unsigned long absoluteMaximumBufferSizeFrames )
{
  static unsigned long primes_[] = {  2,  3,  5,  7, 11                      ,
                                     13, 17, 19, 23, 29                      ,
                                     31, 37, 41, 43, 47                      ,
                                     53, 59, 61, 67, 0                     } ;
  /* zero terminated                                                        */
  unsigned long result = userFramesPerBuffer                                 ;
  int           i                                                            ;
//  assert( absoluteMaximumBufferSizeFrames > 67 ); /* assume maximum is large and we're only factoring by small primes */
  /* search for the largest integer factor of userFramesPerBuffer less
     than or equal to absoluteMaximumBufferSizeFrames                       */
  /* repeatedly divide by smallest prime factors until a buffer size
     smaller than absoluteMaximumBufferSizeFrames is found                  */
  while ( result > absoluteMaximumBufferSizeFrames )                         {
    /* search for the smallest prime factor of result                       */
    for ( i = 0 ; primes_ [ i ] != 0 ; ++i )                                 {
      unsigned long p       = primes_[i]                                     ;
      unsigned long divided = result / p                                     ;
      if ( ( divided * p ) == result )                                       {
        result = divided                                                     ;
        break                                                                ;
        /* continue with outer while loop                                   */
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( 0 == primes_ [ i ] )                                                {
      /* loop failed to find a prime factor, return an approximate result   */
      unsigned long d = ( userFramesPerBuffer                                +
                          absoluteMaximumBufferSizeFrames                    -
                          1                                                ) /
                          absoluteMaximumBufferSizeFrames                    ;
      return userFramesPerBuffer / d                                         ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError SelectHostBufferSizeFramesAndHostBufferCount       (
                 unsigned long   suggestedLatencyFrames           ,
                 unsigned long   userFramesPerBuffer              ,
                 unsigned long   minimumBufferCount               ,
                 unsigned long   preferredMaximumBufferSizeFrames , /* try not to exceed this. for example, don't exceed when coalescing buffers */
                 unsigned long   absoluteMaximumBufferSizeFrames  ,  /* never exceed this, a hard limit */
                 unsigned long * hostBufferSizeFrames             ,
                 unsigned long * hostBufferCount                  )
{
  unsigned long effectiveUserFramesPerBuffer                                 ;
  unsigned long numberOfUserBuffersPerHostBuffer                             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( Stream::FramesPerBufferUnspecified == userFramesPerBuffer )           {
    effectiveUserFramesPerBuffer = CA_MME_HOST_BUFFER_GRANULARITY_FRAMES_WHEN_UNSPECIFIED_;
  } else                                                                     {
    if ( userFramesPerBuffer > absoluteMaximumBufferSizeFrames )             {
      /* user has requested a user buffer that's larger than absoluteMaximumBufferSizeFrames.
         try to choose a buffer size that is equal or smaller than absoluteMaximumBufferSizeFrames
         but is also an integer factor of userFramesPerBuffer, so as to distribute computation evenly.
         the buffer processor will handle the block adaption between host and user buffer sizes.
         see http://www.portaudio.com/trac/ticket/189 for discussion.       */
      effectiveUserFramesPerBuffer = ComputeHostBufferSizeGivenHardUpperLimit(
                                       userFramesPerBuffer                   ,
                                       absoluteMaximumBufferSizeFrames     ) ;
//      assert( effectiveUserFramesPerBuffer <= absoluteMaximumBufferSizeFrames ) ;
      /* try to ensure that duration of host buffering is at least as large as
       * duration of user buffer.                                           */
      if ( suggestedLatencyFrames < userFramesPerBuffer )                    {
        suggestedLatencyFrames = userFramesPerBuffer                         ;
      }                                                                      ;
    } else                                                                   {
      effectiveUserFramesPerBuffer = userFramesPerBuffer                     ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* compute a host buffer count based on suggestedLatencyFrames and our granularity */
  *hostBufferSizeFrames = effectiveUserFramesPerBuffer                       ;
  *hostBufferCount      = ComputeHostBufferCountForFixedBufferSizeFrames     (
                            suggestedLatencyFrames                           ,
                            *hostBufferSizeFrames                            ,
                            minimumBufferCount                             ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( (*hostBufferSizeFrames) >= userFramesPerBuffer )                      {
    /* If there are too many host buffers we would like to coalesce
       them by packing an integer number of user buffers into each host buffer.
       We try to coalesce such that hostBufferCount will lie between
       CA_MME_TARGET_HOST_BUFFER_COUNT_ and (CA_MME_TARGET_HOST_BUFFER_COUNT_*2)-1.
       We limit coalescing to avoid exceeding either absoluteMaximumBufferSizeFrames and
       preferredMaximumBufferSizeFrames.

       First, compute a coalescing factor: the number of user buffers per host buffer.
       The goal is to achieve CA_MME_TARGET_HOST_BUFFER_COUNT_ total buffer count.
       Since our latency is computed based on (*hostBufferCount - 1) we compute a
       coalescing factor based on (*hostBufferCount - 1) and (CA_MME_TARGET_HOST_BUFFER_COUNT_-1).

       The + (CA_MME_TARGET_HOST_BUFFER_COUNT_-2) term below is intended to round up.
     */
    numberOfUserBuffersPerHostBuffer = ((*hostBufferCount - 1)               +
                                        (CA_MME_TARGET_HOST_BUFFER_COUNT_-2))/
                                        (CA_MME_TARGET_HOST_BUFFER_COUNT_-1) ;
    if ( numberOfUserBuffersPerHostBuffer > 1 )                              {
      unsigned long maxCoalescedBufferSizeFrames                             =
                   ( absoluteMaximumBufferSizeFrames                         <
                     preferredMaximumBufferSizeFrames                      ) ?
                     absoluteMaximumBufferSizeFrames                         :
                     preferredMaximumBufferSizeFrames                        ;
      unsigned long maxUserBuffersPerHostBuffer                              =
                    maxCoalescedBufferSizeFrames                             /
                    effectiveUserFramesPerBuffer                             ;
      /* don't coalesce more than this                                      */
      if ( numberOfUserBuffersPerHostBuffer > maxUserBuffersPerHostBuffer )  {
        numberOfUserBuffersPerHostBuffer = maxUserBuffersPerHostBuffer       ;
      }                                                                      ;
      *hostBufferSizeFrames = effectiveUserFramesPerBuffer                   *
                              numberOfUserBuffersPerHostBuffer               ;
      /* recompute hostBufferCount to approximate suggestedLatencyFrames now that hostBufferSizeFrames is larger */
      *hostBufferCount = ComputeHostBufferCountForFixedBufferSizeFrames      (
                           suggestedLatencyFrames                            ,
                           *hostBufferSizeFrames                             ,
                           minimumBufferCount                              ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError CalculateMaxHostSampleFrameSizeBytes                (
                 int                      channelCount             ,
                 CaSampleFormat           hostSampleFormat         ,
                 const CaWmmeStreamInfo * streamInfo               ,
                 int                    * hostSampleFrameSizeBytes )
{
  int          maxDeviceChannelCount = channelCount                          ;
  int          hostSampleSizeBytes   = Core::SampleSize( hostSampleFormat )  ;
  unsigned int i                                                             ;
  /* PA WMME streams may aggregate multiple WMME devices. When the stream addresses
     more than one device in a single direction, maxDeviceChannelCount is the maximum
     number of channels used by a single device.                            */
  if ( hostSampleSizeBytes < 0 )                                             {
    /* the value of hostSampleSize here is an error code, not a sample size */
    return hostSampleSizeBytes                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( NULL != streamInfo                          )                      &&
       ( streamInfo->flags & CaMmeUseMultipleDevices )                     ) {
    maxDeviceChannelCount = streamInfo->devices[0].channelCount              ;
    for ( i = 1 ; i < streamInfo -> deviceCount ; ++i )                      {
      if ( streamInfo->devices[i].channelCount > maxDeviceChannelCount )     {
        maxDeviceChannelCount = streamInfo -> devices [ i ] . channelCount   ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *hostSampleFrameSizeBytes = hostSampleSizeBytes * maxDeviceChannelCount    ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

///////////////////////////////////////////////////////////////////////////////

/* CalculateBufferSettings() fills the framesPerHostInputBuffer, hostInputBufferCount,
   framesPerHostOutputBuffer and hostOutputBufferCount parameters based on the values
   of the other parameters.                                                  */

static CaError CalculateBufferSettings                              (
                 unsigned long          * hostFramesPerInputBuffer  ,
                 unsigned long          * hostInputBufferCount      ,
                 unsigned long          * hostFramesPerOutputBuffer ,
                 unsigned long          * hostOutputBufferCount     ,
                 int                      inputChannelCount         ,
                 CaSampleFormat           hostInputSampleFormat     ,
                 CaTime                   suggestedInputLatency     ,
                 const CaWmmeStreamInfo * inputStreamInfo           ,
                 int                      outputChannelCount        ,
                 CaSampleFormat           hostOutputSampleFormat    ,
                 CaTime                   suggestedOutputLatency    ,
                 const CaWmmeStreamInfo * outputStreamInfo          ,
                 double                   sampleRate                ,
                 unsigned long            userFramesPerBuffer       )
{
  CaError result = NoError                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( inputChannelCount > 0 )                                               {
    /* stream has input                                                     */
    int hostInputFrameSizeBytes                                              ;
    result = CalculateMaxHostSampleFrameSizeBytes                            (
               inputChannelCount                                             ,
               hostInputSampleFormat                                         ,
               inputStreamInfo                                               ,
               &hostInputFrameSizeBytes                                    ) ;
    if ( NoError != result ) goto error                                      ;
    if ( ( NULL != inputStreamInfo                                        ) &&
         ( inputStreamInfo->flags & CaMmeUseLowLevelLatencyParameters   ) )  {
      /* input - using low level latency parameters if provided             */
      if ( ( inputStreamInfo -> bufferCount     <= 0 )                      ||
           ( inputStreamInfo -> framesPerBuffer <= 0 )                     ) {
        result = IncompatibleStreamInfo                       ;
        goto error                                                           ;
      }                                                                      ;
      *hostFramesPerInputBuffer = inputStreamInfo -> framesPerBuffer         ;
      *hostInputBufferCount     = inputStreamInfo -> bufferCount             ;
    } else                                                                   {
      /* input - not using low level latency parameters, so compute
         hostFramesPerInputBuffer and hostInputBufferCount
         based on userFramesPerBuffer and suggestedInputLatency.            */
      unsigned long minimumBufferCount = (outputChannelCount > 0)            ?
                    CA_MME_MIN_HOST_INPUT_BUFFER_COUNT_FULL_DUPLEX_          :
                    CA_MME_MIN_HOST_INPUT_BUFFER_COUNT_HALF_DUPLEX_          ;
      result = SelectHostBufferSizeFramesAndHostBufferCount                  (
                 (unsigned long)(suggestedInputLatency * sampleRate)         , /* (truncate) */
                 userFramesPerBuffer                                         ,
                 minimumBufferCount                                          ,
                 (unsigned long)(CA_MME_MAX_HOST_BUFFER_SECS_ * sampleRate)  , /* in frames. preferred maximum */
                 (CA_MME_MAX_HOST_BUFFER_BYTES_ / hostInputFrameSizeBytes)   , /* in frames. a hard limit. note truncation due to
                                                                                  division is intentional here to limit max bytes */
                 hostFramesPerInputBuffer                                    ,
                 hostInputBufferCount                                      ) ;
      if ( NoError != result ) goto error                                    ;
    }                                                                        ;
  } else                                                                     {
    *hostFramesPerInputBuffer = 0                                            ;
    *hostInputBufferCount     = 0                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( outputChannelCount > 0 )                                              {
    /* stream has output                                                    */
    if ( ( NULL != outputStreamInfo                                       ) &&
         ( outputStreamInfo->flags & CaMmeUseLowLevelLatencyParameters  ) )  {
      /* output - using low level latency parameters                        */
      if ( ( outputStreamInfo -> bufferCount     <= 0 )                     ||
           ( outputStreamInfo -> framesPerBuffer <= 0 )                    ) {
        result = IncompatibleStreamInfo                       ;
        goto error                                                           ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      *hostFramesPerOutputBuffer = outputStreamInfo -> framesPerBuffer       ;
      *hostOutputBufferCount     = outputStreamInfo -> bufferCount           ;
      ////////////////////////////////////////////////////////////////////////
      if ( inputChannelCount > 0 )                                           {
        /* full duplex                                                      */
        /* harmonize hostFramesPerInputBuffer and hostFramesPerOutputBuffer */
        if ( (*hostFramesPerInputBuffer) != (*hostFramesPerOutputBuffer) )   {
          if ( ( NULL != inputStreamInfo                                  ) &&
               ( inputStreamInfo->flags & CaMmeUseLowLevelLatencyParameters ) ) {
            /* a custom StreamInfo was used for specifying both input
               and output buffer sizes. We require that the larger buffer size
               must be a multiple of the smaller buffer size                */
            if ( (*hostFramesPerInputBuffer) < (*hostFramesPerOutputBuffer) ) {
              if ( 0 != ( (*hostFramesPerOutputBuffer) % (*hostFramesPerInputBuffer) ) ) {
                result = IncompatibleStreamInfo               ;
                goto error                                                   ;
              }                                                              ;
            } else                                                           {
//              assert( *hostFramesPerInputBuffer > *hostFramesPerOutputBuffer );
              if ( 0 != ( (*hostFramesPerInputBuffer) % (*hostFramesPerOutputBuffer) ) ) {
                result = IncompatibleStreamInfo               ;
                goto error                                                   ;
              }                                                              ;
            }                                                                ;
          } else                                                             {
            /* a custom StreamInfo was not used for specifying the input buffer size,
               so use the output buffer size, and approximately the suggested input latency. */
            *hostFramesPerInputBuffer = *hostFramesPerOutputBuffer           ;
            *hostInputBufferCount     = ComputeHostBufferCountForFixedBufferSizeFrames         (
                                          (unsigned long)(suggestedInputLatency * sampleRate)  ,
                                          *hostFramesPerInputBuffer                            ,
                                          CA_MME_MIN_HOST_INPUT_BUFFER_COUNT_FULL_DUPLEX_    ) ;
          }                                                                  ;
        }                                                                    ;
      }                                                                      ;
    } else                                                                   {
      /* output - no low level latency parameters, so compute hostFramesPerOutputBuffer and hostOutputBufferCount
         based on userFramesPerBuffer and suggestedOutputLatency.           */
      int hostOutputFrameSizeBytes                                           ;
      result = CalculateMaxHostSampleFrameSizeBytes                          (
                 outputChannelCount                                          ,
                 hostOutputSampleFormat                                      ,
                 outputStreamInfo                                            ,
                 &hostOutputFrameSizeBytes                                 ) ;
      if ( NoError != result ) goto error                                    ;
      /* compute the output buffer size and count                           */
      result = SelectHostBufferSizeFramesAndHostBufferCount                  (
                 (unsigned long)(suggestedOutputLatency * sampleRate)        , /* (truncate) */
                 userFramesPerBuffer                                         ,
                 CA_MME_MIN_HOST_OUTPUT_BUFFER_COUNT_                        ,
                 (unsigned long)(CA_MME_MAX_HOST_BUFFER_SECS_ * sampleRate)  , /* in frames. preferred maximum */
                 (CA_MME_MAX_HOST_BUFFER_BYTES_ / hostOutputFrameSizeBytes)  ,  /* in frames. a hard limit. note truncation due to
                                                                               division is intentional here to limit max bytes */
                 hostFramesPerOutputBuffer                                   ,
                 hostOutputBufferCount                                     ) ;
      if ( NoError != result ) goto error                                    ;
      if ( inputChannelCount > 0 )                                           {
        /* full duplex                                                      */
        /* harmonize hostFramesPerInputBuffer and hostFramesPerOutputBuffer */
        /* ensure that both input and output buffer sizes are the same.
           if they don't match at this stage, choose the smallest one
           and use that for input and output and recompute the corresponding
           buffer count accordingly.                                        */
        if ( (*hostFramesPerOutputBuffer) != (*hostFramesPerInputBuffer) )   {
          if ( hostFramesPerInputBuffer < hostFramesPerOutputBuffer )        {
            *hostFramesPerOutputBuffer = *hostFramesPerInputBuffer           ;
            *hostOutputBufferCount     = ComputeHostBufferCountForFixedBufferSizeFrames        (
                                          (unsigned long)(suggestedOutputLatency * sampleRate) ,
                                          *hostOutputBufferCount                               ,
                                          CA_MME_MIN_HOST_OUTPUT_BUFFER_COUNT_               ) ;
          } else                                                             {
            *hostFramesPerInputBuffer = *hostFramesPerOutputBuffer           ;
            *hostInputBufferCount     = ComputeHostBufferCountForFixedBufferSizeFrames         (
                                          (unsigned long)(suggestedInputLatency * sampleRate)  ,
                                          *hostFramesPerInputBuffer                            ,
                                          CA_MME_MIN_HOST_INPUT_BUFFER_COUNT_FULL_DUPLEX_    ) ;
          }                                                                  ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
  } else                                                                     {
    *hostFramesPerOutputBuffer = 0                                           ;
    *hostOutputBufferCount     = 0                                           ;
  }                                                                          ;
error:
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static int SampleFormatAndWinWmmeSpecificFlagsToLinearWaveFormatTag (
             CaSampleFormat sampleFormat                            ,
             unsigned long  winMmeSpecificFlags                     )
{
  int waveFormatTag = 0 ;
  if ( winMmeSpecificFlags  & CaMmeWaveFormatDolbyAc3Spdif )             {
    waveFormatTag = CA_WAVE_FORMAT_DOLBY_AC3_SPDIF                       ;
  } else
  if ( winMmeSpecificFlags & CaMmeWaveFormatWmaSpdif       )             {
    waveFormatTag = CA_WAVE_FORMAT_WMA_SPDIF                             ;
  } else                                                                 {
    waveFormatTag = CaSampleFormatToLinearWaveFormatTag ( sampleFormat ) ;
  }                                                                      ;
  return waveFormatTag                                                   ;
}

///////////////////////////////////////////////////////////////////////////////

static int QueryWaveOutKSFilterMaxChannels( int waveOutDeviceId, int *maxChannels )
{
  void   * devicePath                                                        ;
  DWORD    devicePathSize                                                    ;
  int      result = 0                                                        ;
  MMRESULT rt                                                                ;
  ////////////////////////////////////////////////////////////////////////////
  rt = ::waveOutMessage  ( (HWAVEOUT)waveOutDeviceId                         ,
                           DRV_QUERYDEVICEINTERFACESIZE                      ,
                           (DWORD_PTR)&devicePathSize                        ,
                           0                                               ) ;
  if ( MMSYSERR_NOERROR != rt ) return 0                                     ;
  ////////////////////////////////////////////////////////////////////////////
  devicePath = Allocator :: allocate ( devicePathSize )                      ;
  if ( NULL == devicePath ) return 0                                         ;
  ////////////////////////////////////////////////////////////////////////////
  rt = ::waveOutMessage ( (HWAVEOUT)waveOutDeviceId                          ,
                          DRV_QUERYDEVICEINTERFACE                           ,
                          (DWORD_PTR)devicePath                              ,
                          devicePathSize                                   ) ;
  /* apparently DRV_QUERYDEVICEINTERFACE returns a unicode interface path,
   * although this is undocumented                                          */
  if ( MMSYSERR_NOERROR == rt )                                              {
    int count = CaWdmksQueryFilterMaximumChannelCount ( devicePath , 0 )     ;
    if ( count > 0 )                                                         {
      *maxChannels = count                                                   ;
      result       = 1                                                       ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  Allocator :: free ( devicePath )                                           ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static int QueryWaveInKSFilterMaxChannels(int waveInDeviceId,int * maxChannels)
{
  void   * devicePath                                                        ;
  DWORD    devicePathSize                                                    ;
  int      result = 0                                                        ;
  MMRESULT rt                                                                ;
  ////////////////////////////////////////////////////////////////////////////
  rt = ::waveInMessage ( (HWAVEIN)waveInDeviceId                             ,
                         DRV_QUERYDEVICEINTERFACESIZE                        ,
                         (DWORD_PTR)&devicePathSize                          ,
                         0                                                 ) ;
  if ( MMSYSERR_NOERROR != rt ) return 0                                     ;
  ////////////////////////////////////////////////////////////////////////////
  devicePath = Allocator :: allocate ( devicePathSize )                      ;
  if ( NULL == devicePath ) return 0                                         ;
  ////////////////////////////////////////////////////////////////////////////
  /* apparently DRV_QUERYDEVICEINTERFACE returns a unicode interface path,
   * although this is undocumented                                          */
  rt = ::waveInMessage ( (HWAVEIN)waveInDeviceId                             ,
                         DRV_QUERYDEVICEINTERFACE                            ,
                         (DWORD_PTR)devicePath                               ,
                         devicePathSize                                    ) ;
  if ( MMSYSERR_NOERROR == rt )                                              {
    int count = CaWdmksQueryFilterMaximumChannelCount(devicePath,1)          ;
    if ( count > 0 )                                                         {
      *maxChannels = count                                                   ;
      result       = 1                                                       ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  Allocator :: free ( devicePath )                                           ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static void InitializeDefaultDeviceIdsFromEnv( WmmeHostApi * hostApi )
{
  CaDeviceIndex device                                                       ;
  ////////////////////////////////////////////////////////////////////////////
  /* input                                                                  */
  device = GetEnvDefaultDeviceID ( (char *)CA_REC_IN_DEV_ENV_NAME_  )        ;
  if ( ( CaNoDevice != device )                                             &&
       ( ( device >= 0                             )                        &&
         ( device <  hostApi -> info . deviceCount )                      ) &&
       hostApi -> deviceInfos [ device ] -> maxInputChannels > 0 )           {
    hostApi -> info . defaultInputDevice = device                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* output                                                                 */
  device = GetEnvDefaultDeviceID ( (char *)CA_REC_OUT_DEV_ENV_NAME_ )        ;
  if ( ( CaNoDevice != device )                                             &&
       ( ( device >= 0                             )                        &&
         ( device <  hostApi -> info . deviceCount )                      ) &&
       hostApi -> deviceInfos [ device ] -> maxOutputChannels > 0 )          {
    hostApi -> info . defaultOutputDevice = device                           ;
  }                                                                          ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError QueryFormatSupported                                       (
                 DeviceInfo  * deviceInfo                                 ,
                 CaError (*waveFormatExQueryFunction)(int, WAVEFORMATEX*) ,
                 int           winMmeDeviceId                             ,
                 int           channels                                   ,
                 double        sampleRate                                 ,
                 unsigned long winMmeSpecificFlags                        )
{
  WmmeDeviceInfo * winMmeDeviceInfo = (WmmeDeviceInfo *) deviceInfo          ;
  CaWaveFormat     waveFormat                                                ;
  CaSampleFormat   sampleFormat                                              ;
  int              waveFormatTag                                             ;
  /* @todo at the moment we only query with 16 bit sample format and
   * directout speaker config                                               */
  sampleFormat  = cafInt16                                                   ;
  waveFormatTag = SampleFormatAndWinWmmeSpecificFlagsToLinearWaveFormatTag   (
                    sampleFormat                                             ,
                    winMmeSpecificFlags                                    ) ;
  ////////////////////////////////////////////////////////////////////////////
  waveFormatTag = CaSampleFormatToLinearWaveFormatTag ( cafInt16 )           ;
  if ( 0 != waveFormatTag )                                                  {
    /* attempt bypass querying the device for linear formats                */
    if ( ( sampleRate == 11025.0 )                                          &&
         ( ( ( channels == 1 )                                              &&
             ( winMmeDeviceInfo->dwFormats & WAVE_FORMAT_1M16 )           ) ||
           ( ( channels == 2 )                                              &&
             ( winMmeDeviceInfo->dwFormats & WAVE_FORMAT_1S16 ) )        ) ) {
      return NoError                                                         ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( sampleRate == 22050.0 )                                          &&
         ( ( ( channels == 1 )                                              &&
             ( winMmeDeviceInfo->dwFormats & WAVE_FORMAT_2M16 )           ) ||
           ( ( channels == 2 )                                              &&
             ( winMmeDeviceInfo->dwFormats & WAVE_FORMAT_2S16 ) )        ) ) {
      return NoError                                                         ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( sampleRate == 44100.0 )                                          &&
         ( ( ( channels == 1 )                                              &&
             ( winMmeDeviceInfo->dwFormats & WAVE_FORMAT_4M16 )           ) ||
           ( ( channels == 2 )                                              &&
             ( winMmeDeviceInfo->dwFormats & WAVE_FORMAT_4S16 ) ) )        ) {
      return NoError                                                         ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* first, attempt to query the device using WAVEFORMATEXTENSIBLE           ,
   * if this fails we fall back to WAVEFORMATEX                             */
  CaInitializeWaveFormatExtensible                                           (
    &waveFormat                                                              ,
    channels                                                                 ,
    sampleFormat                                                             ,
    waveFormatTag                                                            ,
    sampleRate                                                               ,
    CA_SPEAKER_DIRECTOUT                                                   ) ;
  if ( NoError == waveFormatExQueryFunction                                  (
                    winMmeDeviceId                                           ,
                    (WAVEFORMATEX*)&waveFormat                           ) ) {
    return NoError                                                           ;
  }                                                                          ;
  CaInitializeWaveFormatEx ( &waveFormat                                     ,
                             channels                                        ,
                             sampleFormat                                    ,
                             waveFormatTag                                   ,
                             sampleRate                                    ) ;
  ////////////////////////////////////////////////////////////////////////////
  return waveFormatExQueryFunction ( winMmeDeviceId                          ,
                                     (WAVEFORMATEX *)&waveFormat           ) ;
}

///////////////////////////////////////////////////////////////////////////////

static void DetectDefaultSampleRate                                    (
              WmmeDeviceInfo * winMmeDeviceInfo                        ,
              int              winMmeDeviceId                          ,
              CaError (*waveFormatExQueryFunction)(int, WAVEFORMATEX*) ,
              int              maxChannels                             )
{
  DeviceInfo * deviceInfo = (DeviceInfo *)winMmeDeviceInfo         ;
  int          i                                                   ;
  deviceInfo -> defaultSampleRate = 0.0                            ;
  for ( i = 0 ; i < CA_DEFAULTSAMPLERATESEARCHORDER_COUNT_ ; ++i ) {
    double  sampleRate = defaultSampleRateSearchOrder_ [ i ]       ;
    CaError paerror                                                ;
    paerror = QueryFormatSupported                                 (
                deviceInfo                                         ,
                waveFormatExQueryFunction                          ,
                winMmeDeviceId                                     ,
                maxChannels                                        ,
                sampleRate                                         ,
                0                                                ) ;
    if ( NoError == paerror )                                      {
      deviceInfo -> defaultSampleRate = sampleRate                 ;
      break                                                        ;
    }                                                              ;
  }                                                                ;
}

///////////////////////////////////////////////////////////////////////////////

/* return non-zero if all current buffers are done                           */

static int BuffersAreDone             (
             WAVEHDR   ** waveHeaders ,
             unsigned int deviceCount ,
             int          bufferIndex )
{
  unsigned int i                                                         ;
  for ( i = 0 ; i < deviceCount ; ++i )                                  {
    if ( ! ( waveHeaders [ i ] [ bufferIndex ] . dwFlags & WHDR_DONE ) ) {
      return 0                                                           ;
    }                                                                    ;
  }                                                                      ;
  return 1                                                               ;
}

///////////////////////////////////////////////////////////////////////////////

/** Convert external CA ID to a windows multimedia device ID                 */
static UINT LocalDeviceIndexToWinMmeDeviceId( WmmeHostApi * hostApi, CaDeviceIndex device )
{
//  assert( device >= 0 && device < hostApi->inputDeviceCount + hostApi->outputDeviceCount );
  return hostApi -> winMmeDeviceIds [ device ] ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError InitializeInputDeviceInfo              (
                 WmmeHostApi    * winMmeHostApi       ,
                 WmmeDeviceInfo * winMmeDeviceInfo    ,
                 UINT             winMmeInputDeviceId ,
                 int            * success             )
{
  CaError      result     = NoError                                          ;
  char       * deviceName = NULL                                             ;
  MMRESULT     mmresult                                                      ;
  WAVEINCAPS   wic                                                           ;
  DeviceInfo * deviceInfo = (DeviceInfo *)winMmeDeviceInfo                   ;
  ////////////////////////////////////////////////////////////////////////////
  *success = 0                                                               ;
  mmresult = ::waveInGetDevCaps( winMmeInputDeviceId                         ,
                                 &wic                                        ,
                                 sizeof( WAVEINCAPS )                      ) ;
  if ( MMSYSERR_NOMEM == mmresult )                                          {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  } else
  if ( MMSYSERR_NOERROR != mmresult )                                        {
    /* instead of returning UnanticipatedHostError we return NoError,
     * but leave success set as 0. This allows CaInitialize to just ignore
     * this device, without failing the entire initialisation process.      */
    return NoError                                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( WAVE_MAPPER == winMmeInputDeviceId )                                  {
    /* Append I/O suffix to WAVE_MAPPER device.                             */
    deviceName = (char *)winMmeHostApi->allocations->alloc                   (
                   StrTLen( wic.szPname ) + 1                                +
                   sizeof(constInputMapperSuffix_)                         ) ;
    if ( NULL == deviceName )                                                {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    StrTCpyToC ( deviceName , wic . szPname           )                      ;
    ::strcat   ( deviceName , constInputMapperSuffix_ )                      ;
  } else                                                                     {
    deviceName = (char *)winMmeHostApi->allocations->alloc                   (
                   StrTLen( wic.szPname ) + 1                              ) ;
    if ( NULL == deviceName )                                                {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    StrTCpyToC ( deviceName, wic.szPname  )                                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  deviceInfo->name = deviceName                                              ;
  ////////////////////////////////////////////////////////////////////////////
  if ( wic.wChannels == 0xFFFF || wic.wChannels < 1 || wic.wChannels > 255 ) {
    /* For Windows versions using WDM (possibly Windows 98 ME and later)
     * the kernel mixer sits between the application and the driver. As a result,
     * wave*GetDevCaps often kernel mixer channel counts, which are unlimited.
     * When this happens we assume the device is stereo and set a flag
     * so that other channel counts can be tried with OpenStream -- i.e. when
     * device*ChannelCountIsKnown is false, OpenStream will try whatever
     * channel count you supply.
     * see also InitializeOutputDeviceInfo() below.                         */
    gPrintf                                                                ( (
      "GetDeviceInfo: Num input channels reported as %d! Changed to 2.\n"    ,
      wic.wChannels                                                      ) ) ;
    deviceInfo       -> maxInputChannels               = 2                   ;
    winMmeDeviceInfo -> deviceInputChannelCountIsKnown = 0                   ;
  } else                                                                     {
    deviceInfo       -> maxInputChannels               = wic . wChannels     ;
    winMmeDeviceInfo -> deviceInputChannelCountIsKnown = 1                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  winMmeDeviceInfo -> deviceInputChannelCountIsKnown                         =
    QueryWaveInKSFilterMaxChannels ( winMmeInputDeviceId                     ,
                                     &deviceInfo->maxInputChannels         ) ;
  winMmeDeviceInfo->dwFormats = wic.dwFormats                                ;
  DetectDefaultSampleRate                                                    (
    winMmeDeviceInfo                                                         ,
    winMmeInputDeviceId                                                      ,
    QueryInputWaveFormatEx                                                   ,
    deviceInfo->maxInputChannels                                           ) ;
  *success = 1                                                               ;
error:
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError InitializeOutputDeviceInfo              (
                 WmmeHostApi    * winMmeHostApi        ,
                 WmmeDeviceInfo * winMmeDeviceInfo     ,
                 UINT             winMmeOutputDeviceId ,
                 int            * success              )
{
  CaError      result     = NoError                                          ;
  char       * deviceName                                                    ;
  MMRESULT     mmresult                                                      ;
  WAVEOUTCAPS  woc                                                           ;
  DeviceInfo * deviceInfo = (DeviceInfo *) winMmeDeviceInfo                  ;
  int          wdmksDeviceOutputChannelCountIsKnown                          ;
  ////////////////////////////////////////////////////////////////////////////
  *success = 0                                                               ;
  mmresult = waveOutGetDevCaps( winMmeOutputDeviceId                         ,
                                &woc                                         ,
                                sizeof( WAVEOUTCAPS )                      ) ;
  if ( MMSYSERR_NOMEM == mmresult )                                          {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  } else
  if ( MMSYSERR_NOERROR != mmresult )                                        {
    /* instead of returning paUnanticipatedHostError we return NoError,
     * but leave success set as 0. This allows CaInitialize to just ignore
     * this device, without failing the entire initialisation process.      */
    return NoError                                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( WAVE_MAPPER == winMmeOutputDeviceId )                                 {
    /* Append I/O suffix to WAVE_MAPPER device.                             */
    deviceName = (char *)winMmeHostApi->allocations->alloc                   (
                           StrTLen( woc.szPname ) + 1                        +
                           sizeof(constOutputMapperSuffix_)                ) ;
    if ( NULL == deviceName )                                                {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    StrTCpyToC ( deviceName , woc.szPname              )                     ;
    ::strcat   ( deviceName , constOutputMapperSuffix_ )                     ;
  } else                                                                     {
    deviceName = (char *)winMmeHostApi->allocations->alloc                   (
                           StrTLen( woc.szPname ) + 1                      ) ;
    if ( NULL == deviceName )                                                {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    StrTCpyToC ( deviceName, woc.szPname  )                                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  deviceInfo -> name = deviceName                                            ;
  ////////////////////////////////////////////////////////////////////////////
  if ( woc.wChannels == 0xFFFF || woc.wChannels < 1 || woc.wChannels > 255 ) {
    /* For Windows versions using WDM (possibly Windows 98 ME and later)
     * the kernel mixer sits between the application and the driver. As a result,
     * wave*GetDevCaps often kernel mixer channel counts, which are unlimited.
     * When this happens we assume the device is stereo and set a flag
     * so that other channel counts can be tried with OpenStream -- i.e. when
     * device*ChannelCountIsKnown is false, OpenStream will try whatever
     * channel count you supply.
     * see also InitializeInputDeviceInfo() above.                          */
    gPrintf                                                                ( (
      "GetDeviceInfo: Num output channels reported as %d! Changed to 2.\n"   ,
      woc.wChannels                                                      ) ) ;
    deviceInfo       ->maxOutputChannels                = 2                  ;
    winMmeDeviceInfo ->deviceOutputChannelCountIsKnown  = 0                  ;
  } else                                                                     {
    deviceInfo       -> maxOutputChannels               = woc . wChannels    ;
    winMmeDeviceInfo -> deviceOutputChannelCountIsKnown = 1                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  wdmksDeviceOutputChannelCountIsKnown = QueryWaveOutKSFilterMaxChannels     (
                                           winMmeOutputDeviceId              ,
                                           &deviceInfo->maxOutputChannels  ) ;
  if ( ( 0 != wdmksDeviceOutputChannelCountIsKnown )                        &&
       ( 0 == winMmeDeviceInfo->deviceOutputChannelCountIsKnown ) )          {
    winMmeDeviceInfo -> deviceOutputChannelCountIsKnown = 1                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  winMmeDeviceInfo -> dwFormats = woc.dwFormats                              ;
  DetectDefaultSampleRate                                                    (
    winMmeDeviceInfo                                                         ,
    winMmeOutputDeviceId                                                     ,
    QueryOutputWaveFormatEx                                                  ,
    deviceInfo->maxOutputChannels                                          ) ;
  *success = 1                                                               ;
error:
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

/* return non-zero if any buffers are queued */
static int NoBuffersAreQueued ( CaMmeSingleDirectionHandlesAndBuffers * handlesAndBuffers )
{
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == handlesAndBuffers->waveHandles ) return 1                        ;
  ////////////////////////////////////////////////////////////////////////////
  unsigned int i                                                             ;
  unsigned int j                                                             ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < handlesAndBuffers -> bufferCount ; ++i )                 {
    for ( j = 0 ; j < handlesAndBuffers -> deviceCount ; ++j )               {
      if ( !(handlesAndBuffers->waveHeaders[ j ][ i ].dwFlags & WHDR_DONE) ) {
        return 0                                                             ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return 1                                                                   ;
}

///////////////////////////////////////////////////////////////////////////////

static signed long GetAvailableFrames( CaMmeSingleDirectionHandlesAndBuffers * handlesAndBuffers )
{
  signed long  result = 0                                                    ;
  unsigned int i                                                             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( BuffersAreDone ( handlesAndBuffers -> waveHeaders                     ,
                        handlesAndBuffers -> deviceCount                     ,
                        handlesAndBuffers -> currentBufferIndex )          ) {
    /* we could calculate the following in O(1) if we kept track of the last done buffer */
    result = handlesAndBuffers -> framesPerBuffer                            -
             handlesAndBuffers -> framesUsedInCurrentBuffer                  ;
    i = CA_CIRCULAR_INCREMENT_ ( handlesAndBuffers -> currentBufferIndex     ,
                                 handlesAndBuffers -> bufferCount          ) ;
    while ( i != handlesAndBuffers -> currentBufferIndex )                   {
      if ( BuffersAreDone( handlesAndBuffers -> waveHeaders                  ,
                           handlesAndBuffers -> deviceCount                  ,
                           i                                             ) ) {
        result += handlesAndBuffers->framesPerBuffer                         ;
        i       = CA_CIRCULAR_INCREMENT_(i,handlesAndBuffers->bufferCount)   ;
      } else break                                                           ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError TerminateWaveHandles                                          (
                 CaMmeSingleDirectionHandlesAndBuffers * handlesAndBuffers   ,
                 int                                     isInput             ,
                 int                              currentlyProcessingAnError )
{
  CaError    result = NoError                                                ;
  MMRESULT   mmresult                                                        ;
  signed int i                                                               ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != handlesAndBuffers->waveHandles )                                 {
    for ( i = handlesAndBuffers -> deviceCount - 1 ; i >= 0 ; --i )          {
      if ( 0 != isInput )                                                    {
        if ( 0 != ((HWAVEIN *)handlesAndBuffers->waveHandles)[i] )           {
          mmresult = ::waveInClose(((HWAVEIN *)handlesAndBuffers->waveHandles)[i]) ;
        } else mmresult = MMSYSERR_NOERROR                                   ;
      } else                                                                 {
                if( ((HWAVEOUT*)handlesAndBuffers->waveHandles)[i] )
                    mmresult = waveOutClose( ((HWAVEOUT*)handlesAndBuffers->waveHandles)[i] );
                else
                    mmresult = MMSYSERR_NOERROR;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      if ( ( MMSYSERR_NOERROR != mmresult                                 ) &&
           ( 0                == currentlyProcessingAnError             ) )  {
        /* don't update the error state if we're already processing an error */
        result = UnanticipatedHostError                                      ;
        if ( 0 != isInput )                                                  {
          CA_MME_SET_LAST_WAVEIN_ERROR  ( mmresult )                         ;
        } else                                                               {
          CA_MME_SET_LAST_WAVEOUT_ERROR ( mmresult )                         ;
        }                                                                    ;
        /* note that we don't break here, we try to continue closing devices */
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    Allocator :: free ( handlesAndBuffers->waveHandles )                     ;
    handlesAndBuffers -> waveHandles = 0                                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != handlesAndBuffers->bufferEvent )                                 {
    result = CloseHandleWithCaError ( handlesAndBuffers -> bufferEvent )     ;
    handlesAndBuffers -> bufferEvent = 0                                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static void TerminateWaveHeaders                                        (
              CaMmeSingleDirectionHandlesAndBuffers * handlesAndBuffers ,
              int                                     isInput           )
{
  if ( NULL == handlesAndBuffers->waveHeaders ) return                       ;
  signed int i                                                               ;
  signed int j                                                               ;
  WAVEHDR *  deviceWaveHeaders                                               ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = handlesAndBuffers -> deviceCount - 1 ; i >= 0 ; --i )            {
    /* wave headers for device i                                            */
    deviceWaveHeaders = handlesAndBuffers -> waveHeaders [ i ]               ;
    if ( NULL != deviceWaveHeaders )                                         {
      for ( j = handlesAndBuffers -> bufferCount - 1 ; j >= 0 ; --j )        {
        if ( NULL != deviceWaveHeaders[j].lpData )                           {
          if ( 0xFFFFFFFF != deviceWaveHeaders[j].dwUser )                   {
            if ( 0 != isInput )                                              {
              ::waveInUnprepareHeader                                        (
                ((HWAVEIN *)handlesAndBuffers->waveHandles)[i]               ,
                &deviceWaveHeaders[j]                                        ,
                sizeof(WAVEHDR)                                            ) ;
            } else                                                           {
              ::waveOutUnprepareHeader                                       (
                ((HWAVEOUT*)handlesAndBuffers->waveHandles)[i]               ,
                &deviceWaveHeaders[j]                                        ,
                sizeof(WAVEHDR)                                            ) ;
            }                                                                ;
          }                                                                  ;
          Allocator :: free ( deviceWaveHeaders [ j ] . lpData )             ;
        }                                                                    ;
      }                                                                      ;
      Allocator :: free ( deviceWaveHeaders )                                ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  Allocator :: free ( handlesAndBuffers->waveHeaders )                       ;
  handlesAndBuffers -> waveHeaders = 0                                       ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError InitializeWaveHandles                                         (
                 WmmeHostApi                           * winMmeHostApi       ,
                 CaMmeSingleDirectionHandlesAndBuffers * handlesAndBuffers   ,
                 unsigned long                           winMmeSpecificFlags ,
                 unsigned long                           bytesPerHostSample  ,
                 double                                  sampleRate          ,
                 CaMmeDeviceAndChannelCount            * devices             ,
                 unsigned int                            deviceCount         ,
                 CaWaveFormatChannelMask                 channelMask         ,
                 int                                     isInput             )
{
  CaError        result                                                      ;
  MMRESULT       mmresult                                                    ;
  signed int     i                                                           ;
  signed int     j                                                           ;
  CaSampleFormat sampleFormat                                                ;
  int            waveFormatTag                                               ;
  ////////////////////////////////////////////////////////////////////////////
  /* for error cleanup we expect that InitializeSingleDirectionHandlesAndBuffers()
     has already been called to zero some fields                            */
  result = CreateEventWithCaError ( & handlesAndBuffers -> bufferEvent       ,
                                    NULL                                     ,
                                    FALSE                                    ,
                                    FALSE                                    ,
                                    NULL                                   ) ;
  if ( NoError != result ) goto error                                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != isInput )                                                        {
    handlesAndBuffers -> waveHandles = (void *)Allocator::allocate(sizeof(HWAVEIN )*deviceCount) ;
  } else                                                                     {
    handlesAndBuffers -> waveHandles = (void *)Allocator::allocate(sizeof(HWAVEOUT)*deviceCount) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL == handlesAndBuffers->waveHandles )                              {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  handlesAndBuffers -> deviceCount = deviceCount                             ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < (signed int) deviceCount ; ++i )                         {
    if ( 0 != isInput )                                                      {
      ((HWAVEIN *)handlesAndBuffers->waveHandles)[i] = 0                     ;
    } else                                                                   {
      ((HWAVEOUT*)handlesAndBuffers->waveHandles)[i] = 0                     ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* @todo at the moment we only use 16 bit sample format                   */
  sampleFormat  = cafInt16                                                   ;
  waveFormatTag = SampleFormatAndWinWmmeSpecificFlagsToLinearWaveFormatTag   (
                    sampleFormat                                             ,
                    winMmeSpecificFlags                                    ) ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < (signed int) deviceCount ; ++i )                         {
    CaWaveFormat waveFormat                                                  ;
    UINT         winMmeDeviceId                                              ;
    winMmeDeviceId = LocalDeviceIndexToWinMmeDeviceId( winMmeHostApi         ,
                                                       devices[i].device   ) ;
   /* @todo: consider providing a flag or #define to not try waveformat extensible
      this could just initialize j to 1 the first time round.               */
   for ( j = 0 ; j < 2 ; ++j )                                               {
     switch ( j )                                                            {
       case 0                                                                :
         /* first, attempt to open the device using WAVEFORMATEXTENSIBLE,
            if this fails we fall back to WAVEFORMATEX                      */
         CaInitializeWaveFormatExtensible                                    (
           &waveFormat                                                       ,
           devices[i].channelCount                                           ,
           sampleFormat                                                      ,
           waveFormatTag                                                     ,
           sampleRate                                                        ,
           channelMask                                                     ) ;
       break                                                                 ;
       case 1                                                                :
         /* retry with WAVEFORMATEX                                         */
         CaInitializeWaveFormatEx                                            (
           &waveFormat                                                       ,
           devices [ i ] . channelCount                                      ,
           sampleFormat                                                      ,
           waveFormatTag                                                     ,
           sampleRate                                                      ) ;
       break                                                                 ;
     }                                                                       ;
     /////////////////////////////////////////////////////////////////////////
     /* REVIEW: consider not firing an event for input when a full duplex
                stream is being used. this would probably depend on the
                neverDropInput flag.                                        */
     if ( 0 != isInput )                                                     {
       mmresult = ::waveInOpen                                               (
                    &((HWAVEIN*)handlesAndBuffers->waveHandles)[i]           ,
                    winMmeDeviceId                                           ,
                    (WAVEFORMATEX*)&waveFormat                               ,
                    (DWORD_PTR)handlesAndBuffers->bufferEvent                ,
                    (DWORD_PTR)0                                             ,
                    CALLBACK_EVENT                                         ) ;
     } else                                                                  {
       mmresult = ::waveOutOpen                                              (
                    &((HWAVEOUT*)handlesAndBuffers->waveHandles)[i]          ,
                    winMmeDeviceId                                           ,
                    (WAVEFORMATEX*)&waveFormat                               ,
                    (DWORD_PTR)handlesAndBuffers->bufferEvent                ,
                    (DWORD_PTR)0                                             ,
                    CALLBACK_EVENT                                         ) ;
     }                                                                       ;
     /////////////////////////////////////////////////////////////////////////
     if ( MMSYSERR_NOERROR == mmresult )                                     {
       /* success                                                           */
       break                                                                 ;
     } else
     if ( 0 == j )                                                           {
       /* try again with WAVEFORMATEX                                       */
       continue                                                              ;
     } else                                                                  {
       switch ( mmresult )                                                   {
         case MMSYSERR_ALLOCATED                                             :
           /* Specified resource is already allocated.                      */
           result = DeviceUnavailable                                        ;
         break                                                               ;
         case MMSYSERR_NODRIVER                                              :
           /* No device driver is present.                                  */
           result = DeviceUnavailable                                        ;
         break                                                               ;
         case MMSYSERR_NOMEM                                                 :
           /* Unable to allocate or lock memory.                            */
           result = InsufficientMemory                                       ;
         break                                                               ;
         case MMSYSERR_BADDEVICEID                                           :
           /* Specified device identifier is out of range.                  */
         case WAVERR_BADFORMAT                                               :
           /* Attempted to open with an unsupported waveform-audio format.  */
           /* This can also occur if we try to open the device with an unsupported
            * number of channels. This is attempted when device*ChannelCountIsKnown is
            * set to 0.                                                     */
         default                                                             :
           result = UnanticipatedHostError                                   ;
           if ( 0 != isInput )                                               {
             CA_MME_SET_LAST_WAVEIN_ERROR  ( mmresult )                      ;
           } else                                                            {
             CA_MME_SET_LAST_WAVEOUT_ERROR ( mmresult )                      ;
           }                                                                 ;
        }                                                                    ;
        goto error                                                           ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  return result                                                              ;
error:
//  TerminateWaveHandles( handlesAndBuffers, isInput, 1 /* currentlyProcessingAnError */ );
  return result                                                              ;
}

static CaError InitializeWaveHeaders                                         (
                 CaMmeSingleDirectionHandlesAndBuffers * handlesAndBuffers   ,
                 unsigned long                           hostBufferCount     ,
                 CaSampleFormat                          hostSampleFormat    ,
                 unsigned long                           framesPerHostBuffer ,
                 CaMmeDeviceAndChannelCount            * devices             ,
                 int                                     isInput             )
{
  CaError    result = NoError                                                ;
  MMRESULT   mmresult                                                        ;
  WAVEHDR *  deviceWaveHeaders                                               ;
  signed int i                                                               ;
  signed int j                                                               ;
  ////////////////////////////////////////////////////////////////////////////
  /* for error cleanup we expect that InitializeSingleDirectionHandlesAndBuffers()
     has already been called to zero some fields                            */
  /* allocate an array of pointers to arrays of wave headers, one array of
     wave headers per device                                                */
  handlesAndBuffers->waveHeaders = (WAVEHDR **) Allocator::allocate(sizeof(WAVEHDR *)*handlesAndBuffers->deviceCount) ;
  if ( NULL == handlesAndBuffers->waveHeaders )                              {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < (signed int) handlesAndBuffers -> deviceCount ; ++i )    {
    handlesAndBuffers -> waveHeaders [ i ] = 0                               ;
  }                                                                          ;
  handlesAndBuffers -> bufferCount = hostBufferCount                         ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < (signed int)handlesAndBuffers -> deviceCount ; ++i )     {
    int bufferBytes = Core::SampleSize( hostSampleFormat )                   *
                      framesPerHostBuffer                                    *
                      devices [ i ] . channelCount                           ;
    if ( bufferBytes < 0 )                                                   {
      result = InternalError                                                 ;
      goto error                                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    /* Allocate an array of wave headers for device i                       */
    deviceWaveHeaders = (WAVEHDR *) Allocator::allocate(sizeof(WAVEHDR)*hostBufferCount) ;
    if ( NULL == deviceWaveHeaders )                                         {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    for ( j = 0 ; j < (signed int) hostBufferCount ; ++j )                   {
      deviceWaveHeaders [ j ] . lpData = 0                                   ;
    }                                                                        ;
    handlesAndBuffers -> waveHeaders [ i ] = deviceWaveHeaders               ;
    /* Allocate a buffer for each wave header                               */
    for ( j = 0 ; j < (signed int) hostBufferCount ; ++j )                   {
      deviceWaveHeaders[j].lpData = (char *)Allocator::allocate(bufferBytes) ;
      if ( NULL == deviceWaveHeaders[j].lpData )                             {
        result = InsufficientMemory                                          ;
        goto error                                                           ;
      }                                                                      ;
      deviceWaveHeaders [ j ] . dwBufferLength = bufferBytes                 ;
      deviceWaveHeaders [ j ] . dwUser         = 0xFFFFFFFF                  ;
      /* indicates that *PrepareHeader() has not yet been called, for error clean up code */
      if ( 0 != isInput )                                                    {
        mmresult = ::waveInPrepareHeader                                     (
                    ((HWAVEIN*)handlesAndBuffers->waveHandles)[i]            ,
                    &deviceWaveHeaders[j]                                    ,
                    sizeof(WAVEHDR)                                        ) ;
        if ( MMSYSERR_NOERROR != mmresult )                                  {
          result = UnanticipatedHostError                                    ;
          CA_MME_SET_LAST_WAVEIN_ERROR ( mmresult )                          ;
          goto error                                                         ;
        }                                                                    ;
      } else                                                                 {
        /* output                                                           */
        mmresult = ::waveOutPrepareHeader                                    (
                    ((HWAVEOUT*)handlesAndBuffers->waveHandles)[i]           ,
                    &deviceWaveHeaders[j]                                    ,
                    sizeof(WAVEHDR)                                        ) ;
        if ( MMSYSERR_NOERROR != mmresult )                                  {
          result = UnanticipatedHostError                                    ;
          CA_MME_SET_LAST_WAVEIN_ERROR ( mmresult )                          ;
          goto error                                                         ;
        }                                                                    ;
      }                                                                      ;
      deviceWaveHeaders [ j ] . dwUser = devices [ i ] . channelCount        ;
    }                                                                        ;
  }                                                                          ;
  return result                                                              ;
error:
  TerminateWaveHeaders ( handlesAndBuffers , isInput )                       ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError ValidateWinMmeSpecificStreamInfo                              (
                 const StreamParameters * streamParameters                   ,
                 const CaWmmeStreamInfo * streamInfo                         ,
                 unsigned long          * winMmeSpecificFlags                ,
                 char                   * throttleProcessingThreadOnOverload ,
                 unsigned long          * deviceCount                        )
{
  if ( NULL == streamInfo ) return NoError                                   ;
  if ( ( streamInfo->size != sizeof( CaWmmeStreamInfo ) )                   ||
       ( streamInfo->version != 1                       )                  ) {
    return IncompatibleStreamInfo                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *winMmeSpecificFlags = streamInfo->flags                                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != ( streamInfo->flags & CaMmeDontThrottleOverloadedProcessingThread ) ) {
    *throttleProcessingThreadOnOverload = 0                                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != ( streamInfo->flags & CaMmeUseMultipleDevices ) )                {
    if ( streamParameters->device != CaUseHostApiSpecificDeviceSpecification ) {
      return InvalidDevice                                                   ;
    }                                                                        ;
    *deviceCount = streamInfo -> deviceCount                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

///////////////////////////////////////////////////////////////////////////////

CA_THREAD_FUNC ProcessingThreadProc(void * pArg)
{
  WmmeStream  * stream = (WmmeStream *)pArg                                  ;
  if ( NULL == stream ) return NoError                                       ;
  HANDLE        events[3]                                                    ;
  int           eventCount = 0                                               ;
  DWORD         result = NoError                                             ;
  DWORD         waitResult                                                   ;
  DWORD         timeout = (unsigned long)(stream->allBuffersDurationMs*0.5)  ;
  int           hostBuffersAvailable                                         ;
  signed int    hostInputBufferIndex                                         ;
  signed int    hostOutputBufferIndex                                        ;
  CaStreamFlags statusFlags                                                  ;
  int           callbackResult                                               ;
  int           done = 0                                                     ;
  unsigned int  channel                                                      ;
  unsigned int  i                                                            ;
  unsigned long framesProcessed                                              ;
  ////////////////////////////////////////////////////////////////////////////
  /* prepare event array for call to WaitForMultipleObjects()               */
  if ( 0 != stream->input.bufferEvent )                                      {
    events [ eventCount++ ] = stream -> input  . bufferEvent                 ;
  }                                                                          ;
  if ( 0 != stream->output.bufferEvent )                                     {
    events [ eventCount++ ] = stream -> output . bufferEvent                 ;
  }                                                                          ;
  events [ eventCount++ ] = stream -> abortEvent                             ;
  statusFlags             = 0                                                ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "Windows Multimedia Extension started\n" ) )                   ;
  /* @todo support InputUnderflow, OutputOverflow and NeverDropInput        */
  /* loop until something causes us to stop                                 */
  do                                                                         {
    /* wait for MME to signal that a buffer is available, or for
       the PA abort event to be signaled.

       When this indicates that one or more buffers are available
       NoBuffersAreQueued() and Current*BuffersAreDone are used below to
       poll for additional done buffers. NoBuffersAreQueued() will fail
       to identify an underrun/overflow if the driver doesn't mark all done
       buffers prior to signalling the event. Some drivers do this
       (eg RME Digi96, and others don't eg VIA PC 97 input). This isn't a
       huge problem, it just means that we won't always be able to detect
       underflow/overflow.                                                  */
    waitResult = ::WaitForMultipleObjects                                    (
                   eventCount                                                ,
                   events                                                    ,
                   FALSE /* wait all = FALSE */                              ,
                   timeout                                                 ) ;
    if ( WAIT_FAILED == waitResult )                                         {
      result = UnanticipatedHostError                                        ;
      /* @todo FIXME/REVIEW: can't return host error info from an asyncronous
       * thread. see http://www.portaudio.com/trac/ticket/143               */
      done = 1                                                               ;
    } else
    if ( WAIT_TIMEOUT == waitResult )                                        {
      /* if a timeout is encountered, continue                              */
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( 0 != stream -> abortProcessing )                                    {
      /* Pa_AbortStream() has been called, stop processing immediately      */
      done = 1                                                               ;
    } else
    if ( 0 != stream -> stopProcessing  )                                    {
      /* Pa_StopStream() has been called or the user callback returned
       * non-zero, processing will continue until all output buffers are
       * marked as done. The stream will stop immediately if it is
       * input-only.                                                        */
      if ( CA_IS_OUTPUT_STREAM_(stream) )                                    {
        if ( NoBuffersAreQueued ( &stream -> output ) )                      {
          done = 1                                                           ;
          /* Will cause thread to return.                                   */
        }                                                                    ;
      } else                                                                 {
        /* input only stream                                                */
        done = 1                                                             ;
        /* Will cause thread to return.                                     */
      }                                                                      ;
    } else                                                                   {
      hostBuffersAvailable = 1                                               ;
      /* process all available host buffers                                 */
      do                                                                     {
        //////////////////////////////////////////////////////////////////////
        hostInputBufferIndex  = -1                                           ;
        hostOutputBufferIndex = -1                                           ;
        //////////////////////////////////////////////////////////////////////
        if ( CA_IS_INPUT_STREAM_(stream) )                                   {
          if ( stream -> CurrentInputBuffersAreDone ( ) )                    {
            if ( NoBuffersAreQueued ( &stream->input ) )                     {
              /* @todo
                 if all of the other buffers are also ready then
                 we discard all but the most recent. This is an
                 input buffer overflow. FIXME: these buffers should
                 be passed to the callback in a paNeverDropInput
                 stream. http://www.portaudio.com/trac/ticket/142
                 note that it is also possible for an input overflow
                 to happen while the callback is processing a buffer.
                 that is handled further down.                              */
              result = stream->CatchUpInputBuffers()                         ;
              if ( NoError != result ) done = 1                              ;
              statusFlags |= StreamIO::InputOverflow                         ;
            }                                                                ;
            hostInputBufferIndex = stream->input.currentBufferIndex          ;
          }                                                                  ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        if ( CA_IS_OUTPUT_STREAM_(stream) )                                  {
          if ( stream -> CurrentOutputBuffersAreDone ( ) )                   {
            /* ok, we have an output buffer                                 */
            if ( NoBuffersAreQueued ( &stream->output ) )                    {
              /* if all of the other buffers are also ready, catch up by copying
                 the most recently generated buffer into all but one of the output
                 buffers.
                 note that this catch up code only handles the case where all
                 buffers have been played out due to this thread not having
                 woken up at all. a more common case occurs when this thread
                 is woken up, processes one buffer, but takes too long, and as
                 a result all the other buffers have become un-queued. that
                 case is handled further down.                              */
              result = stream -> CatchUpOutputBuffers ( )                    ;
              if ( NoError != result ) done = 1                              ;
              statusFlags |= StreamIO::OutputUnderflow                       ;
            }                                                                ;
            hostOutputBufferIndex = stream->output.currentBufferIndex        ;
          }                                                                  ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        if ( ( CA_IS_FULL_DUPLEX_STREAM_(stream)                            &&
             ( hostInputBufferIndex  != -1 )                                &&
             ( hostOutputBufferIndex != -1 )                              ) ||
             ( CA_IS_HALF_DUPLEX_STREAM_(stream)                            &&
           ( ( hostInputBufferIndex  != -1 )                                ||
             ( hostOutputBufferIndex != -1 )                           ) ) ) {
          CaTime inputBufferAdcTime  = 0                                     ;
          CaTime currentTime         = 0                                     ;
          CaTime outputBufferDacTime = 0                                     ;
          ////////////////////////////////////////////////////////////////////
          if ( CA_IS_OUTPUT_STREAM_(stream) )                                {
            /* set currentTime and calculate outputBufferDacTime from the
             * current wave out position                                    */
            MMTIME   mmtime                                                  ;
            double   timeBeforeGetPosition                                   ;
            double   timeAfterGetPosition                                    ;
            double   time                                                    ;
            long     framesInBufferRing                                      ;
            long     writePosition                                           ;
            long     playbackPosition                                        ;
            HWAVEOUT firstWaveOutDevice = ( (HWAVEOUT *) stream -> output . waveHandles ) [ 0 ] ;
            //////////////////////////////////////////////////////////////////
            mmtime . wType = TIME_SAMPLES                                    ;
            timeBeforeGetPosition = stream -> GetTime ( )                    ;
            ::waveOutGetPosition ( firstWaveOutDevice                        ,
                                   &mmtime                                   ,
                                   sizeof(MMTIME)                          ) ;
            timeAfterGetPosition  = stream -> GetTime ( )                    ;
            currentTime           = timeAfterGetPosition                     ;
            /* approximate time at which wave out position was measured as
             * half way between timeBeforeGetPosition and timeAfterGetPosition */
            time = timeBeforeGetPosition + (timeAfterGetPosition - timeBeforeGetPosition) * 0.5 ;
            framesInBufferRing = stream->output.bufferCount                  *
                                 stream->bufferProcessor.framesPerHostBuffer ;
            playbackPosition   = mmtime.u.sample % framesInBufferRing        ;
            writePosition      = stream->output.currentBufferIndex           *
                                 stream->bufferProcessor.framesPerHostBuffer +
                                 stream->output.framesUsedInCurrentBuffer    ;
            if ( playbackPosition >= writePosition )                         {
              outputBufferDacTime = time                                     +
                                    ((double)( writePosition                 +
                                     (framesInBufferRing-playbackPosition))  *
                                     stream->bufferProcessor.samplePeriod  ) ;
            } else                                                           {
              outputBufferDacTime = time                                     +
                                    ((double)( writePosition                 -
                                               playbackPosition            ) *
                                     stream->bufferProcessor.samplePeriod  ) ;
            }                                                                ;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          stream -> conduit -> Input  . AdcDac      = inputBufferAdcTime     ;
          stream -> conduit -> Input  . CurrentTime = currentTime            ;
          stream -> conduit -> Output . AdcDac      = outputBufferDacTime    ;
          stream -> cpuLoadMeasurer . Begin (                   )            ;
          stream -> bufferProcessor . Begin ( stream -> conduit )            ;
          /* reset status flags once they have been passed to the buffer processor */
          statusFlags = 0                                                    ;
          ////////////////////////////////////////////////////////////////////
          if ( CA_IS_INPUT_STREAM_(stream) )                                 {
            stream -> bufferProcessor . setInputFrameCount ( 0 , 0 )         ;
            channel = 0                                                      ;
            for ( i = 0 ; i < stream -> input . deviceCount ; ++i )          {
              /* we have stored the number of channels in the buffer in dwUser */
              int channelCount = (int)stream->input.waveHeaders[i][hostInputBufferIndex].dwUser ;
              stream -> bufferProcessor . setInterleavedInputChannels        (
                0                                                            ,
                channel                                                      ,
                stream->input.waveHeaders[i][ hostInputBufferIndex ].lpData  +
                stream->input.framesUsedInCurrentBuffer                      *
                channelCount                                                 *
                stream->bufferProcessor.bytesPerHostInputSample              ,
                channelCount                                               ) ;
              channel += channelCount                                        ;
            }                                                                ;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          if ( CA_IS_OUTPUT_STREAM_(stream) )                                {
            stream -> bufferProcessor . setOutputFrameCount ( 0 , 0 )        ;
            channel = 0                                                      ;
            for ( i = 0 ; i < stream -> output . deviceCount ; ++i )         {
              /* we have stored the number of channels in the buffer in dwUser */
              int channelCount = (int)stream->output.waveHeaders[i][hostOutputBufferIndex].dwUser ;
              stream -> bufferProcessor . setInterleavedOutputChannels       (
                0                                                            ,
                channel                                                      ,
                stream->output.waveHeaders[i][hostOutputBufferIndex].lpData  +
                stream->output.framesUsedInCurrentBuffer                     *
                channelCount                                                 *
                stream->bufferProcessor.bytesPerHostOutputSample             ,
                channelCount                                               ) ;
              channel += channelCount                                        ;
            }                                                                ;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          callbackResult  = Conduit::Continue                                ;
          framesProcessed = stream->bufferProcessor.End(&callbackResult )    ;
          ////////////////////////////////////////////////////////////////////
          stream -> input  . framesUsedInCurrentBuffer += framesProcessed    ;
          stream -> output . framesUsedInCurrentBuffer += framesProcessed    ;
          stream -> cpuLoadMeasurer . End ( framesProcessed )                ;
          ////////////////////////////////////////////////////////////////////
          if ( Conduit::Continue == callbackResult )                         {
            /* nothing special to do                                        */
          } else
          if ( Conduit::Abort    == callbackResult )                         {
            stream -> abortProcessing = 1                                    ;
            done                      = 1                                    ;
            result                    = NoError                              ;
            /* FIXME: should probably reset the output device immediately
             *        once the callback returns Abort                       */
          } else
          if ( Conduit::Postpone == callbackResult )                         {
            /* nothing special to do                                        */
          } else                                                             {
            /* User callback has asked us to stop with paComplete or other non-zero value */
            /* stop once currently queued audio has finished                */
            stream -> stopProcessing = 1                                     ;
            result                   = NoError                               ;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          if ( CA_IS_INPUT_STREAM_(stream)                                  &&
               ( 0 == stream->stopProcessing                              ) &&
               ( 0 == stream->abortProcessing                             ) &&
               ( stream->input.framesUsedInCurrentBuffer == stream->input.framesPerBuffer ) ) {
            if ( NoBuffersAreQueued( &stream->input ) )                      {
              /* need to handle PaNeverDropInput here where necessary       */
              result = stream -> CatchUpInputBuffers ( )                     ;
              if ( NoError != result ) done = 1                              ;
              statusFlags |= StreamIO::InputOverflow                          ;
            }                                                                ;
            result = stream -> AdvanceToNextInputBuffer ( )                  ;
            if ( NoError != result ) done = 1                                ;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          if ( CA_IS_OUTPUT_STREAM_(stream)                                 &&
               ( 0 == stream->abortProcessing )                            ) {
            if ( ( 0 != stream->stopProcessing )                            &&
                 ( stream->output.framesUsedInCurrentBuffer < stream->output.framesPerBuffer ) ) {
              /* zero remaining samples in output output buffer and flush   */
              stream -> output . framesUsedInCurrentBuffer                  +=
              stream -> bufferProcessor . ZeroOutput                         (
                stream -> output . framesPerBuffer                           -
                stream -> output . framesUsedInCurrentBuffer               ) ;
             /* we send the entire buffer to the output devices, but we could
                just send a partial buffer, rather than zeroing the unused
                samples.                                                    */
            }                                                                ;
            //////////////////////////////////////////////////////////////////
            if ( stream->output.framesUsedInCurrentBuffer == stream->output.framesPerBuffer ) {
              /* check for underflow before enquing the just-generated buffer,
                 but recover from underflow after enquing it. This ensures
                 that the most recent audio segment is repeated             */
              int outputUnderflow = NoBuffersAreQueued( &stream->output )    ;
              result = stream -> AdvanceToNextOutputBuffer ( )               ;
              if ( NoError != result ) done = 1                              ;
              if ( ( 0 != outputUnderflow                                 ) &&
                   ( 0 == done                                            ) &&
                   ( 0 == stream->stopProcessing                        ) )  {
                /* Recover from underflow in the case where the underflow
                 * occured while processing the buffer we just finished     */
                result = stream -> CatchUpOutputBuffers ( )                  ;
                if ( NoError != result ) done = 1                            ;
                statusFlags |= StreamIO::OutputUnderflow                      ;
              }                                                              ;
            }                                                                ;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          if ( 0 != stream->throttleProcessingThreadOnOverload )             {
            if ( ( 0 != stream->stopProcessing  )                           ||
                 ( 0 != stream->abortProcessing )                          ) {
              if ( stream->processingThreadPriority != stream->highThreadPriority ) {
                ::SetThreadPriority ( stream->processingThread               ,
                                      stream->highThreadPriority           ) ;
                stream->processingThreadPriority = stream->highThreadPriority;
              }                                                              ;
            } else
            if ( stream -> GetCpuLoad ( ) > 1.0 )                            {
              if ( stream->processingThreadPriority != stream->throttledThreadPriority ) {
                ::SetThreadPriority ( stream -> processingThread             ,
                                      stream -> throttledThreadPriority    ) ;
                stream->processingThreadPriority = stream->throttledThreadPriority;
              }                                                              ;
              /* sleep to give other processes a go                         */
              ::Sleep( stream->throttledSleepMsecs )                         ;
            } else                                                           {
              if ( stream->processingThreadPriority != stream->highThreadPriority ) {
                ::SetThreadPriority ( stream -> processingThread             ,
                                      stream -> highThreadPriority         ) ;
                stream->processingThreadPriority = stream->highThreadPriority;
              }                                                              ;
            }                                                                ;
          }                                                                  ;
        } else                                                               {
          hostBuffersAvailable = 0                                           ;
        }                                                                    ;
      } while ( ( 0 != hostBuffersAvailable      )                          &&
                ( 0 == stream -> stopProcessing  )                          &&
                ( 0 == stream -> abortProcessing )                          &&
                ( 0 == done                      )                         ) ;
    }                                                                        ;
  } while ( ! done )                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> isActive = 0                                                     ;
  if ( NULL != stream->conduit ) stream->conduit->finish()                   ;
  stream -> cpuLoadMeasurer . Reset ( )                                      ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

CaError WmmeInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError          result = NoError                                          ;
  int              i                                                         ;
  int              inputDeviceCount                                          ;
  int              outputDeviceCount                                         ;
  int              maximumPossibleDeviceCount                                ;
  int              deviceInfoInitializationSucceeded = 0                     ;
  DWORD            waveInPreferredDevice                                     ;
  DWORD            waveOutPreferredDevice                                    ;
  DWORD            preferredDeviceStatusFlags                                ;
  CaTime           defaultLowLatency                                         ;
  CaTime           defaultHighLatency                                        ;
  WmmeHostApi    * winMmeHostApi   = NULL                                    ;
  WmmeDeviceInfo * deviceInfoArray = NULL                                    ;
  ////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////
  winMmeHostApi = new WmmeHostApi ()                                         ;
  if ( NULL == winMmeHostApi )                                               {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  winMmeHostApi->allocations = new AllocationGroup ( )                       ;
  if ( NULL == winMmeHostApi->allocations )                                  {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  gPrintf ( ( "Set up MME allocation group\n" ) )                            ;
  ////////////////////////////////////////////////////////////////////////////
  *hostApi       = winMmeHostApi                                             ;
  winMmeHostApi -> info . structVersion = 1                                  ;
  winMmeHostApi -> info . type          = MME                                ;
  winMmeHostApi -> info . index         = hostApiIndex                       ;
  winMmeHostApi -> info . name          = "Windows Multimedia Extensions"    ;
  winMmeHostApi -> info . deviceCount         = 0                            ;
  winMmeHostApi -> info . defaultInputDevice  = CaNoDevice                   ;
  winMmeHostApi -> info . defaultOutputDevice = CaNoDevice                   ;
  winMmeHostApi -> inputDeviceCount           = 0                            ;
  winMmeHostApi -> outputDeviceCount          = 0                            ;
  ////////////////////////////////////////////////////////////////////////////
  #if !defined(DRVM_MAPPER_PREFERRED_GET)
  /* DRVM_MAPPER_PREFERRED_GET is defined in mmddk.h but we avoid a
   * dependency on the DDK by defining it here                              */
  #define DRVM_MAPPER_PREFERRED_GET    (0x2000+21)
  #endif
  /* the following calls assume that if wave*Message fails the preferred device parameter won't be modified */
  preferredDeviceStatusFlags =  0                                            ;
  waveInPreferredDevice      = -1                                            ;
  preferredDeviceStatusFlags =  0                                            ;
  waveOutPreferredDevice     = -1                                            ;
  maximumPossibleDeviceCount =  0                                            ;
  ::waveInMessage  ( (HWAVEIN)WAVE_MAPPER                                    ,
                     DRVM_MAPPER_PREFERRED_GET                               ,
                     (DWORD_PTR)&waveInPreferredDevice                       ,
                     (DWORD_PTR)&preferredDeviceStatusFlags                ) ;
  ::waveOutMessage ( (HWAVEOUT)WAVE_MAPPER                                   ,
                     DRVM_MAPPER_PREFERRED_GET                               ,
                     (DWORD_PTR)&waveOutPreferredDevice                      ,
                     (DWORD_PTR)&preferredDeviceStatusFlags                ) ;
  inputDeviceCount  = ::waveInGetNumDevs  ( )                                ;
  if ( inputDeviceCount > 0 )                                                {
    /* assume there is a WAVE_MAPPER                                        */
    maximumPossibleDeviceCount += inputDeviceCount  + 1                      ;
  }                                                                          ;
  outputDeviceCount = ::waveOutGetNumDevs ( )                                ;
  if ( outputDeviceCount > 0 )                                               {
    /* assume there is a WAVE_MAPPER                                        */
    maximumPossibleDeviceCount += outputDeviceCount + 1                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( maximumPossibleDeviceCount > 0 )                                      {
    (*hostApi)->deviceInfos = (DeviceInfo **)new DeviceInfo * [ maximumPossibleDeviceCount ]  ;
    if ( NULL == (*hostApi)->deviceInfos )                                   {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    gPrintf ( ( "Allocate DeviceInfo array\n" ) )                            ;
    //////////////////////////////////////////////////////////////////////////
    /* allocate all PaDeviceInfo structs in a contiguous block              */
    deviceInfoArray = new WmmeDeviceInfo [ maximumPossibleDeviceCount ]      ;
    if ( NULL == deviceInfoArray )                                           {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    gPrintf ( ( "Allocate DeviceInfo real location\n" ) )                    ;
    //////////////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < maximumPossibleDeviceCount ; ++i )                     {
      DeviceInfo * deviceInfo    = (DeviceInfo *)&deviceInfoArray[i]         ;
      deviceInfo->structVersion  = 2                                         ;
      deviceInfo->hostApi        = hostApiIndex                              ;
      deviceInfo->hostType       = MME                                       ;
      deviceInfo->name           = 0                                         ;
      (*hostApi)->deviceInfos[i] = deviceInfo                                ;
    }                                                                        ;
    gPrintf ( ( "Place DeviceInfo at right place\n" ) )                      ;
    //////////////////////////////////////////////////////////////////////////
    winMmeHostApi -> winMmeDeviceIds                                         =
      (UINT *)winMmeHostApi->allocations->alloc                              (
        sizeof(UINT) * maximumPossibleDeviceCount                          ) ;
    if ( NULL == winMmeHostApi->winMmeDeviceIds )                            {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    GetDefaultLatencies ( &defaultLowLatency, &defaultHighLatency )          ;
    if ( inputDeviceCount > 0 )                                              {
      /* -1 is the WAVE_MAPPER                                              */
      for ( i = -1 ; i < inputDeviceCount ; ++i )                            {
        UINT             winMmeDeviceId = (UINT)((i==-1) ? WAVE_MAPPER : i)  ;
        WmmeDeviceInfo * wmmeDeviceInfo = &deviceInfoArray[ (*hostApi)->info.deviceCount ] ;
        DeviceInfo     * deviceInfo     = (DeviceInfo *)wmmeDeviceInfo       ;
        //////////////////////////////////////////////////////////////////////
        deviceInfo     -> structVersion                   = 2                  ;
        deviceInfo     -> hostApi                         = hostApiIndex       ;
        deviceInfo     -> maxInputChannels                = 0                  ;
        wmmeDeviceInfo -> deviceInputChannelCountIsKnown  = 1                  ;
        deviceInfo     -> maxOutputChannels               = 0                  ;
        wmmeDeviceInfo -> deviceOutputChannelCountIsKnown = 1                  ;
        deviceInfo     -> defaultLowInputLatency          = defaultLowLatency  ;
        deviceInfo     -> defaultLowOutputLatency         = defaultLowLatency  ;
        deviceInfo     -> defaultHighInputLatency         = defaultHighLatency ;
        deviceInfo     -> defaultHighOutputLatency        = defaultHighLatency ;
        //////////////////////////////////////////////////////////////////////
        result = InitializeInputDeviceInfo                                   (
                    winMmeHostApi                                            ,
                    wmmeDeviceInfo                                           ,
                    winMmeDeviceId                                           ,
                    &deviceInfoInitializationSucceeded                     ) ;
        if ( NoError != result ) goto error                                  ;
        if ( 0 != deviceInfoInitializationSucceeded )                        {
          if ( CaNoDevice == (*hostApi)->info.defaultInputDevice  )          {
            /* if there is currently no default device, use the first one available */
            (*hostApi)->info.defaultInputDevice = (*hostApi)->info.deviceCount;
          } else
          if ( winMmeDeviceId == waveInPreferredDevice )                     {
            /* set the default device to the system preferred device        */
            (*hostApi)->info.defaultInputDevice = (*hostApi)->info.deviceCount;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          winMmeHostApi -> winMmeDeviceIds [ (*hostApi)->info.deviceCount ] = winMmeDeviceId ;
          (*hostApi)    -> deviceInfos     [ (*hostApi)->info.deviceCount ] = deviceInfo     ;
          ////////////////////////////////////////////////////////////////////
          gPrintf                                                          ( (
            "Input device (%d): %s\n"                                        ,
            winMmeHostApi -> info . deviceCount                              ,
            deviceInfo    -> name                                        ) ) ;
          ////////////////////////////////////////////////////////////////////
          winMmeHostApi -> inputDeviceCount ++                               ;
          (*hostApi)    -> info . deviceCount ++                             ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( outputDeviceCount > 0 )                                             {
      /* -1 is the WAVE_MAPPER                                              */
      for ( i = -1; i < outputDeviceCount; ++i )                             {
        UINT             winMmeDeviceId = (UINT)((i==-1) ? WAVE_MAPPER : i)  ;
        WmmeDeviceInfo * wmmeDeviceInfo = &deviceInfoArray[ (*hostApi)->info.deviceCount ] ;
        DeviceInfo     * deviceInfo     = (DeviceInfo *)wmmeDeviceInfo       ;
        //////////////////////////////////////////////////////////////////////
        deviceInfo     -> structVersion                   = 2                  ;
        deviceInfo     -> hostApi                         = hostApiIndex       ;
        deviceInfo     -> maxInputChannels                = 0                  ;
        wmmeDeviceInfo -> deviceInputChannelCountIsKnown  = 1                  ;
        deviceInfo     -> maxOutputChannels               = 0                  ;
        wmmeDeviceInfo -> deviceOutputChannelCountIsKnown = 1                  ;
        deviceInfo     -> defaultLowInputLatency          = defaultLowLatency  ;
        deviceInfo     -> defaultLowOutputLatency         = defaultLowLatency  ;
        deviceInfo     -> defaultHighInputLatency         = defaultHighLatency ;
        deviceInfo     -> defaultHighOutputLatency        = defaultHighLatency ;
        //////////////////////////////////////////////////////////////////////
        result = InitializeOutputDeviceInfo                                  (
                   winMmeHostApi                                             ,
                   wmmeDeviceInfo                                            ,
                   winMmeDeviceId                                            ,
                   &deviceInfoInitializationSucceeded                      ) ;
        if ( NoError != result ) goto error                                  ;
        if ( 0 != deviceInfoInitializationSucceeded )                        {
          if ( CaNoDevice == (*hostApi)->info.defaultOutputDevice )          {
            /* if there is currently no default device, use the first one available */
            (*hostApi)->info.defaultOutputDevice = (*hostApi)->info.deviceCount;
          } else
          if ( winMmeDeviceId == waveOutPreferredDevice )                    {
            /* set the default device to the system preferred device        */
            (*hostApi)->info.defaultOutputDevice = (*hostApi)->info.deviceCount;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          winMmeHostApi -> winMmeDeviceIds [ (*hostApi)->info.deviceCount ] = winMmeDeviceId ;
          (*hostApi)    -> deviceInfos     [ (*hostApi)->info.deviceCount ] = deviceInfo     ;
          ////////////////////////////////////////////////////////////////////
          gPrintf                                                          ( (
            "Output device (%d): %s\n"                                       ,
            winMmeHostApi -> info . deviceCount                              ,
            deviceInfo    -> name                                        ) ) ;
          ////////////////////////////////////////////////////////////////////
          winMmeHostApi -> outputDeviceCount  ++                             ;
         (*hostApi)     -> info . deviceCount ++                             ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  InitializeDefaultDeviceIdsFromEnv ( winMmeHostApi )                        ;
  return result                                                              ;
error:
  if ( NULL != winMmeHostApi )                                               {
    if ( NULL != winMmeHostApi->allocations )                                {
      winMmeHostApi->allocations->release()                                  ;
      delete winMmeHostApi->allocations                                      ;
      winMmeHostApi->allocations = NULL                                      ;
    }                                                                        ;
    delete winMmeHostApi                                                     ;
  }                                                                          ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

WmmeStream:: WmmeStream (void)
           : Stream     (    )
{
  primeStreamUsingCallback           = 0                                     ;
  abortEvent                         = 0                                     ;
  processingThread                   = 0                                     ;
  processingThreadId                 = 0                                     ;
  throttleProcessingThreadOnOverload = 0                                     ;
  processingThreadPriority           = 0                                     ;
  highThreadPriority                 = 0                                     ;
  throttledThreadPriority            = 0                                     ;
  throttledSleepMsecs                = 0                                     ;
  isStopped                          = 1                                     ;
  isActive                           = 0                                     ;
  stopProcessing                     = 0                                     ;
  abortProcessing                    = 0                                     ;
  allBuffersDurationMs               = 0                                     ;
  ::memset ( &input  , 0 , sizeof(CaMmeSingleDirectionHandlesAndBuffers) )   ;
  ::memset ( &output , 0 , sizeof(CaMmeSingleDirectionHandlesAndBuffers) )   ;
}

WmmeStream::~WmmeStream(void)
{
}

CaError WmmeStream::Start(void)
{
  CaError       result              = 0                                      ;
  MMRESULT      mmresult            = 0                                      ;
  unsigned int  i                   = 0                                      ;
  unsigned int  j                   = 0                                      ;
  int           callbackResult      = 0                                      ;
  unsigned int  channel             = 0                                      ;
  unsigned long framesProcessed     = 0                                      ;
  ////////////////////////////////////////////////////////////////////////////
  bufferProcessor . Reset ( )                                                ;
  if ( CA_IS_INPUT_STREAM_( this ) )                                         {
    for ( i = 0 ; i < input . bufferCount ; ++i )                            {
      for ( j = 0 ; j < input . deviceCount ; ++j )                          {
        input.waveHeaders[j][i].dwFlags &= ~WHDR_DONE                        ;
        mmresult = ::waveInAddBuffer                                         (
                     ((HWAVEIN *)input.waveHandles)[j]                       ,
                     &input.waveHeaders[j][i]                                ,
                     sizeof(WAVEHDR)                                       ) ;
        if ( MMSYSERR_NOERROR != mmresult )                                  {
          result = UnanticipatedHostError                                    ;
          CA_MME_SET_LAST_WAVEIN_ERROR ( mmresult )                          ;
          goto error                                                         ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
    input . currentBufferIndex        = 0                                    ;
    input . framesUsedInCurrentBuffer = 0                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CA_IS_OUTPUT_STREAM_ ( this ) )                                       {
    for ( i = 0 ; i < output . deviceCount ; ++i )                           {
      mmresult = ::waveOutPause( ((HWAVEOUT *)output.waveHandles)[i] )       ;
      if ( MMSYSERR_NOERROR != mmresult )                                    {
        result = UnanticipatedHostError                                      ;
        CA_MME_SET_LAST_WAVEOUT_ERROR ( mmresult )                           ;
        goto error                                                           ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < output . bufferCount ; ++i )                           {
      if ( 0 != primeStreamUsingCallback )                                   {
        output . framesUsedInCurrentBuffer = 0                               ;
        do                                                                   {
          conduit -> Output . CurrentTime = GetTime ( )                      ;
          conduit -> Output . StatusFlags = StreamIO::PrimingOutput          |
                                   ( ( input . bufferCount > 0 )             ?
                                     StreamIO::InputUnderflow : 0         )  ;
          bufferProcessor . Begin ( conduit )                                ;
          if ( input . bufferCount > 0 ) bufferProcessor.setNoInput()        ;
          bufferProcessor . setOutputFrameCount ( 0 , 0 )                    ;
          channel = 0                                                        ;
          ////////////////////////////////////////////////////////////////////
          for ( j = 0 ; j < output . deviceCount ; ++j )                     {
           /* we have stored the number of channels in the buffer in dwUser */
           int channelCount = (int)output.waveHeaders[j][i].dwUser           ;
           bufferProcessor . setInterleavedOutputChannels                    (
             0                                                               ,
             channel                                                         ,
             output . waveHeaders [ j ] [ i ] . lpData                       +
             output . framesUsedInCurrentBuffer                              *
             channelCount                                                    *
             bufferProcessor.bytesPerHostOutputSample                        ,
             channelCount                                                  ) ;
            /* we have stored the number of channels in the buffer in dwUser */
            channel += channelCount                                          ;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          callbackResult = Conduit::Continue                                 ;
          framesProcessed = bufferProcessor . End ( &callbackResult )        ;
          output . framesUsedInCurrentBuffer += framesProcessed              ;
          if ( Conduit::Continue != callbackResult )                         {
           /* fix this, what do we do if callback result is non-zero during stream
              priming?
              for complete: play out primed waveHeaders as usual
              for abort: clean up immediately.                              */
          }                                                                  ;
        } while ( output.framesUsedInCurrentBuffer!=output.framesPerBuffer ) ;
      } else                                                                 {
        for ( j = 0 ; j < output . deviceCount ; ++j )                       {
           ZeroMemory( output . waveHeaders [ j ] [ i ] . lpData             ,
                       output . waveHeaders [ j ] [ i ] . dwBufferLength   ) ;
        }                                                                    ;
      }                                                                      ;
      /* we queue all channels of a single buffer frame (accross all devices,
       * because some multidevice multichannel drivers work better this way   */
      for ( j = 0 ; j < output . deviceCount ; ++j )                         {
        mmresult = ::waveOutWrite( ((HWAVEOUT *)output.waveHandles)[j]       ,
                                   &output.waveHeaders[j][i]                 ,
                                   sizeof(WAVEHDR)                         ) ;
        if ( MMSYSERR_NOERROR != mmresult )                                  {
          result = UnanticipatedHostError                                    ;
          CA_MME_SET_LAST_WAVEOUT_ERROR ( mmresult )                         ;
          goto error                                                         ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
    output . currentBufferIndex        = 0                                   ;
    output . framesUsedInCurrentBuffer = 0                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  isStopped       = 0                                                        ;
  isActive        = 1                                                        ;
  stopProcessing  = 0                                                        ;
  abortProcessing = 0                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  result = ResetEventWithCaError ( input  . bufferEvent )                    ;
  if ( NoError != result ) goto error                                        ;
  result = ResetEventWithCaError ( output . bufferEvent )                    ;
  if ( NoError != result ) goto error                                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != conduit )                                                     {
    /* callback stream                                                      */
    result = ResetEventWithCaError ( abortEvent )                            ;
    if ( NoError != result ) goto error                                      ;
    /* Create thread that waits for audio buffers to be ready for processing. */
    processingThread = CREATE_THREAD                                         ;
    if ( 0 == processingThread )                                             {
      result = UnanticipatedHostError                                        ;
      CA_MME_SET_LAST_SYSTEM_ERROR ( GetLastError() )                        ;
      goto error                                                             ;
    }                                                                        ;
    /* could have mme specific stream parameters to allow the user to set the
     * callback thread priorities                                           */
   highThreadPriority      = THREAD_PRIORITY_TIME_CRITICAL                   ;
   throttledThreadPriority = THREAD_PRIORITY_NORMAL                          ;
   if ( ! SetThreadPriority(processingThread,highThreadPriority ) )          {
     result = UnanticipatedHostError                                         ;
     CA_MME_SET_LAST_SYSTEM_ERROR ( GetLastError() )                         ;
     goto error                                                              ;
    }                                                                        ;
    processingThreadPriority = highThreadPriority                            ;
  } else                                                                     {
    /* blocking read/write stream                                           */
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CA_IS_INPUT_STREAM_(this) )                                           {
    for ( i = 0 ; i < input . deviceCount ; ++i )                            {
      mmresult = ::waveInStart( ((HWAVEIN *)input.waveHandles)[i] )          ;
      dPrintf ( ( "Start: waveInStart returned = 0x%X.\n", mmresult ) )      ;
      if ( MMSYSERR_NOERROR != mmresult )                                    {
        result = UnanticipatedHostError                                      ;
        CA_MME_SET_LAST_WAVEIN_ERROR ( mmresult )                            ;
        goto error                                                           ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CA_IS_OUTPUT_STREAM_(this) )                                          {
    for ( i = 0 ; i < output . deviceCount ; ++i )                           {
      mmresult = ::waveOutRestart ( ((HWAVEOUT *)output.waveHandles)[i] )    ;
      if ( MMSYSERR_NOERROR != mmresult )                                    {
        result = UnanticipatedHostError                                      ;
        CA_MME_SET_LAST_WAVEOUT_ERROR ( mmresult )                           ;
        goto error                                                           ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
error:
  return result                                                              ;
}

CaError WmmeStream::Stop(void)
{
  CaError      result = NoError                                              ;
  int          timeout                                                       ;
  DWORD        waitResult                                                    ;
  MMRESULT     mmresult                                                      ;
  signed int   hostOutputBufferIndex                                         ;
  unsigned int channel                                                       ;
  unsigned int waitCount                                                     ;
  unsigned int i                                                             ;
  /* REVIEW: the error checking in this function needs review. the basic
      idea is to return from this function in a known state - for example
      there is no point avoiding calling waveInReset just because
      the thread times out.                                                 */
  if ( 0 != processingThread )                                               {
    /* callback stream                                                      */
    /* Tell processing thread to stop generating more data and to let current data play out. */
    stopProcessing = 1                                                       ;
    /* Calculate timeOut longer than longest time it could take to return all buffers. */
    timeout = (int)( allBuffersDurationMs * 1.5 )                            ;
    if ( timeout < CA_MME_MIN_TIMEOUT_MSEC_ )                                {
      timeout = CA_MME_MIN_TIMEOUT_MSEC_                                     ;
    }                                                                        ;
    dPrintf ( ( "WinMME Stop: waiting for background thread.\n" ) )          ;
    //////////////////////////////////////////////////////////////////////////
    waitResult = ::WaitForSingleObject ( processingThread , timeout )        ;
    if ( WAIT_TIMEOUT == waitResult )                                        {
      /* try to abort                                                       */
      abortProcessing = 1                                                    ;
      SetEvent ( abortEvent )                                                ;
      waitResult = ::WaitForSingleObject ( processingThread , timeout )      ;
      if ( WAIT_TIMEOUT == waitResult )                                      {
        dPrintf ( ( "WinMME Stop: timed out while waiting for background thread to finish.\n" ) ) ;
        result = TimedOut                                                    ;
      }                                                                      ;
    }                                                                        ;
    CloseHandle ( processingThread )                                         ;
    processingThread = NULL                                                  ;
  } else                                                                     {
    /* blocking read / write stream                                         */
    if ( CA_IS_OUTPUT_STREAM_(this) )                                        {
      if ( output . framesUsedInCurrentBuffer > 0 )                          {
        /* there are still unqueued frames in the current buffer, so flush them */
        hostOutputBufferIndex = output . currentBufferIndex                  ;
        bufferProcessor . setOutputFrameCount                                (
          0                                                                  ,
          output.framesPerBuffer - output.framesUsedInCurrentBuffer        ) ;
        channel = 0                                                          ;
        for ( i = 0 ; i < output . deviceCount ; ++i )                       {
          /* we have stored the number of channels in the buffer in dwUser  */
          int channelCount = (int)output.waveHeaders[i][hostOutputBufferIndex].dwUser ;
          bufferProcessor . setInterleavedOutputChannels                     (
            0                                                                ,
            channel                                                          ,
            output.waveHeaders[i][hostOutputBufferIndex].lpData              +
            output.framesUsedInCurrentBuffer                                 *
            channelCount                                                     *
            bufferProcessor.bytesPerHostOutputSample                         ,
            channelCount                                                   ) ;
          channel += channelCount                                            ;
        }                                                                    ;
        bufferProcessor . ZeroOutput                                         (
          output . framesPerBuffer                                           -
          output . framesUsedInCurrentBuffer                               ) ;
        /* we send the entire buffer to the output devices, but we could just
         * send a partial buffer, rather than zeroing the unused samples.   */
        AdvanceToNextOutputBuffer ( )                                        ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      timeout = (allBuffersDurationMs / output.bufferCount) + 1              ;
      if ( timeout < CA_MME_MIN_TIMEOUT_MSEC_ )                              {
        timeout = CA_MME_MIN_TIMEOUT_MSEC_                                   ;
      }                                                                      ;
      waitCount = 0                                                          ;
      while ( ( ! NoBuffersAreQueued( &output ) )                           &&
              ( waitCount <= output.bufferCount )                          ) {
        /* wait for MME to signal that a buffer is available                */
        waitResult = ::WaitForSingleObject(output.bufferEvent,timeout)       ;
        if ( WAIT_FAILED == waitResult )                                     {
          break                                                              ;
        } else
        if ( WAIT_TIMEOUT == waitResult )                                    {
          /* keep waiting                                                   */
        }                                                                    ;
        ++waitCount                                                          ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CA_IS_OUTPUT_STREAM_(this) )                                          {
    for ( i = 0 ; i < output . deviceCount ; ++i )                           {
      mmresult = ::waveOutReset( ((HWAVEOUT *)output.waveHandles)[i] )       ;
      if ( MMSYSERR_NOERROR != mmresult )                                    {
        result = UnanticipatedHostError                                      ;
        CA_MME_SET_LAST_WAVEOUT_ERROR ( mmresult )                           ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CA_IS_INPUT_STREAM_(this) )                                           {
    for ( i = 0 ; i < input . deviceCount ; ++i )                            {
      mmresult = ::waveInReset( ( ( HWAVEIN *)input . waveHandles ) [ i ] )  ;
      if ( MMSYSERR_NOERROR != mmresult )                                    {
        result = UnanticipatedHostError                                      ;
        CA_MME_SET_LAST_WAVEIN_ERROR ( mmresult )                            ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  isStopped = 1                                                              ;
  isActive  = 0                                                              ;
  return result                                                              ;
}

CaError WmmeStream::Close(void)
{
  CaError result                                                             ;
  result = CloseHandleWithCaError ( abortEvent )                             ;
  if ( NoError != result ) return result                                     ;
  ////////////////////////////////////////////////////////////////////////////
  TerminateWaveHeaders ( &output , 0 /* not isInput */ )                     ;
  TerminateWaveHeaders ( &input  , 1 /* isInput     */ )                     ;
  TerminateWaveHandles ( &output , 0 /* not isInput */ , 0 /* not currentlyProcessingAnError */ ) ;
  TerminateWaveHandles ( &input  , 1 /* isInput     */ , 0 /* not currentlyProcessingAnError */ ) ;
  bufferProcessor.Terminate();
  /* REVIEW: what is the best way to clean up a stream if an error is detected? */
  return result                                                              ;
}

CaError WmmeStream::Abort(void)
{
  CaError      result = NoError                                              ;
  int          timeout                                                       ;
  DWORD        waitResult                                                    ;
  MMRESULT     mmresult                                                      ;
  unsigned int i                                                             ;
  /* REVIEW: the error checking in this function needs review. the basic
     idea is to return from this function in a known state - for example
     there is no point avoiding calling waveInReset just because
     the thread times out.                                                  */
  if ( 0 != processingThread )                                               {
    /* callback stream                                                      */
    /* Tell processing thread to abort immediately                          */
    abortProcessing = 1                                                      ;
    SetEvent ( abortEvent )                                                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CA_IS_OUTPUT_STREAM_(this) )                                          {
    for ( i = 0 ; i < output . deviceCount ; ++i )                           {
      mmresult = ::waveOutReset( ((HWAVEOUT *)output.waveHandles)[i] )       ;
      if ( MMSYSERR_NOERROR != mmresult )                                    {
        CA_MME_SET_LAST_WAVEOUT_ERROR ( mmresult )                           ;
        return UnanticipatedHostError                                        ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CA_IS_INPUT_STREAM_(this) )                                           {
    for ( i = 0 ; i < input . deviceCount ; ++i )                            {
      mmresult = ::waveInReset( ((HWAVEIN *)input.waveHandles)[i] )          ;
      if ( MMSYSERR_NOERROR != mmresult )                                    {
        CA_MME_SET_LAST_WAVEIN_ERROR ( mmresult )                            ;
        return UnanticipatedHostError                                        ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != processingThread )                                               {
    /* callback stream                                                      */
    dPrintf ( ( "WinMME Abort: waiting for background thread.\n" ) )         ;
    /* Calculate timeOut longer than longest time it could take to return all buffers. */
    timeout = (int)(allBuffersDurationMs * 1.5)                              ;
    if ( timeout < CA_MME_MIN_TIMEOUT_MSEC_ )                                {
      timeout = CA_MME_MIN_TIMEOUT_MSEC_                                     ;
    }                                                                        ;
    waitResult = ::WaitForSingleObject ( processingThread , timeout )        ;
    if ( WAIT_TIMEOUT == waitResult )                                        {
      dPrintf ( ( "WinMME Abort: timed out while waiting for background thread to finish.\n" ) ) ;
      return TimedOut                                                        ;
    }                                                                        ;
    CloseHandle ( processingThread )                                         ;
    processingThread = NULL                                                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  isStopped = 1                                                              ;
  isActive  = 0                                                              ;
  return result                                                              ;
}

CaError WmmeStream::IsStopped(void)
{
  return isStopped ;
}

CaError WmmeStream::IsActive(void)
{
  return isActive  ;
}

CaTime WmmeStream::GetTime(void)
{
  return Timer :: Time ( ) ;
}

double WmmeStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer . Value ( ) ;
}

CaInt32 WmmeStream::ReadAvailable(void)
{
  if ( CA_IS_INPUT_STREAM_(this) )        {
    return GetAvailableFrames ( &input )  ;
  }                                       ;
  return CanNotReadFromAnOutputOnlyStream ;
}

CaInt32 WmmeStream::WriteAvailable(void)
{
  if ( CA_IS_OUTPUT_STREAM_(this) )       {
    return GetAvailableFrames ( &output ) ;
  }                                       ;
  return CanNotWriteToAnInputOnlyStream   ;
}

CaError WmmeStream::Read(void * buffer,unsigned long frames)
{
  CaError       result = NoError                                             ;
  void        * userBuffer                                                   ;
  unsigned long framesRead = 0                                               ;
  unsigned long framesProcessed                                              ;
  signed int    hostInputBufferIndex                                         ;
  DWORD         waitResult                                                   ;
  DWORD         timeout = (unsigned long)(allBuffersDurationMs * 0.5)        ;
  unsigned int  channel                                                      ;
  unsigned int  i                                                            ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CA_IS_INPUT_STREAM_(this) )                                           {
    /* make a local copy of the user buffer pointer(s). this is necessary
       because PaUtil_CopyInput() advances these pointers every time it is called. */
    if ( 0 != bufferProcessor.userInputIsInterleaved )                       {
      userBuffer = buffer                                                    ;
    }  else                                                                  {
      userBuffer = (void *)alloca(sizeof(void*)*bufferProcessor.inputChannelCount) ;
      if ( NULL == userBuffer ) return InsufficientMemory                    ;
      for ( i = 0 ; i < bufferProcessor . inputChannelCount ; ++i )          {
        ((void**)userBuffer)[i] = ((void**)buffer)[i]                        ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    do                                                                       {
      if ( CurrentInputBuffersAreDone() )                                    {
        if ( NoBuffersAreQueued( &input ) )                                  {
          /* REVIEW: consider what to do if the input overflows. do we requeue
           * all of the buffers? should we be running a thread to make sure
           * they are always queued?                                        */
          result = InputOverflowed                                           ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        hostInputBufferIndex = input.currentBufferIndex                      ;
        bufferProcessor . setInputFrameCount                                 (
          0                                                                  ,
          input . framesPerBuffer                                            -
          input . framesUsedInCurrentBuffer                                ) ;
        channel = 0                                                          ;
        for ( i = 0 ; i < input . deviceCount ; ++i )                        {
          /* we have stored the number of channels in the buffer in dwUser  */
          int channelCount = (int)input.waveHeaders[i][hostInputBufferIndex].dwUser ;
          bufferProcessor . setInterleavedInputChannels                      (
            0                                                                ,
            channel                                                          ,
            input.waveHeaders[i][hostInputBufferIndex].lpData                +
            input.framesUsedInCurrentBuffer                                  *
            channelCount                                                     *
            bufferProcessor.bytesPerHostInputSample                          ,
            channelCount                                                   ) ;
          channel += channelCount                                            ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        framesProcessed = bufferProcessor . CopyInput                        (
                            &userBuffer                                      ,
                            frames - framesRead                            ) ;
        input . framesUsedInCurrentBuffer += framesProcessed                 ;
        if ( input.framesUsedInCurrentBuffer == input.framesPerBuffer )      {
          result = AdvanceToNextInputBuffer()                                ;
          if ( NoError != result ) break                                     ;
        }                                                                    ;
        framesRead += framesProcessed                                        ;
      } else                                                                 {
        /* wait for MME to signal that a buffer is available                */
        waitResult = ::WaitForSingleObject(input.bufferEvent,timeout)        ;
        if ( WAIT_FAILED == waitResult )                                     {
          result = UnanticipatedHostError                                    ;
          break                                                              ;
        } else
        if ( WAIT_TIMEOUT == waitResult )                                    {
          /* if a timeout is encountered, continue, perhaps we should give up eventually */
        }                                                                    ;
      }                                                                      ;
    } while( framesRead < frames )                                           ;
  } else                                                                     {
    result = CanNotReadFromAnOutputOnlyStream                                ;
  }                                                                          ;
  return result                                                              ;
}

CaError WmmeStream::Write(const void * buffer,unsigned long frames)
{
  CaError       result = NoError                                             ;
  const void  * userBuffer                                                   ;
  unsigned long framesWritten = 0                                            ;
  unsigned long framesProcessed                                              ;
  signed int    hostOutputBufferIndex                                        ;
  DWORD         waitResult                                                   ;
  DWORD         timeout = (unsigned long)(allBuffersDurationMs * 0.5)        ;
  unsigned int  channel                                                      ;
  unsigned int  i                                                            ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CA_IS_OUTPUT_STREAM_(this) )                                          {
    /* make a local copy of the user buffer pointer(s). this is necessary
       because PaUtil_CopyOutput() advances these pointers every time
       it is called.                                                        */
    if ( 0 != bufferProcessor.userOutputIsInterleaved )                      {
      userBuffer = buffer                                                    ;
    } else                                                                   {
      userBuffer = (const void*)alloca( sizeof(void*) * bufferProcessor.outputChannelCount ) ;
      if ( NULL == userBuffer )return InsufficientMemory                     ;
      for ( i = 0 ; i < bufferProcessor . outputChannelCount ; ++i )         {
        ( (const void **)userBuffer)[i] = ((const void**)buffer)[i]          ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    do                                                                       {
      if ( CurrentOutputBuffersAreDone() )                                   {
        if ( NoBuffersAreQueued( &output ) )                                 {
          /* REVIEW: consider what to do if the output underflows. do we
           * requeue all the existing buffers with zeros? should we run a
           * separate thread to keep the buffers enqueued at all times?     */
          result = OutputUnderflowed                                         ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        hostOutputBufferIndex = output . currentBufferIndex                  ;
        bufferProcessor . setOutputFrameCount                                (
          0                                                                  ,
          output . framesPerBuffer                                           -
          output . framesUsedInCurrentBuffer                               ) ;
        channel = 0                                                          ;
        for ( i = 0 ; i < output . deviceCount ; ++i )                       {
          /* we have stored the number of channels in the buffer in dwUser  */
          int channelCount = (int)output.waveHeaders[i][hostOutputBufferIndex].dwUser ;
          bufferProcessor . setInterleavedOutputChannels                     (
            0                                                                ,
            channel                                                          ,
            output.waveHeaders[i][hostOutputBufferIndex].lpData              +
            output.framesUsedInCurrentBuffer                                 *
            channelCount                                                     *
            bufferProcessor.bytesPerHostOutputSample                         ,
            channelCount                                                   ) ;
          channel += channelCount                                            ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        framesProcessed = bufferProcessor . CopyOutput                       (
                            &userBuffer                                      ,
                            frames - framesWritten                         ) ;
        output . framesUsedInCurrentBuffer += framesProcessed                ;
        if ( output.framesUsedInCurrentBuffer == output.framesPerBuffer )    {
          result = AdvanceToNextOutputBuffer()                               ;
          if ( NoError != result ) break                                     ;
        }                                                                    ;
        framesWritten += framesProcessed                                     ;
      } else                                                                 {
        /* wait for MME to signal that a buffer is available                */
        waitResult = ::WaitForSingleObject(output.bufferEvent,timeout)       ;
        if ( WAIT_FAILED == waitResult )                                     {
          result = UnanticipatedHostError                                    ;
          break                                                              ;
        } else
        if ( WAIT_TIMEOUT == waitResult )                                    {
          /* if a timeout is encountered, continue,
             perhaps we should give up eventually                           */
        }                                                                    ;
      }                                                                      ;
    } while ( framesWritten < frames )                                       ;
  } else                                                                     {
    result = CanNotWriteToAnInputOnlyStream                                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

bool WmmeStream::hasVolume(void)
{
  if ( CA_IS_INPUT_STREAM_ ( this ) ) return false ;
  if ( CA_IS_OUTPUT_STREAM_( this ) ) return true  ;
  return false                                     ;
}

CaVolume WmmeStream::MinVolume(void)
{
  return 0     ;
}

CaVolume WmmeStream::MaxVolume(void)
{
  return 10000 ;
}

CaVolume WmmeStream::Volume(int atChannel)
{
  CaVolume V = 0                                                     ;
  if ( CA_IS_INPUT_STREAM_( this ) ) return V                        ;
  ////////////////////////////////////////////////////////////////////
  if ( CA_IS_OUTPUT_STREAM_ ( this ) && ( output.deviceCount > 0 ) ) {
    DWORD    LR                                                      ;
    MMRESULT mmresult                                                ;
    mmresult = ::waveOutGetVolume                                    (
                 ((HWAVEOUT *)output.waveHandles)[0]                 ,
                 &LR                                               ) ;
    if ( MMSYSERR_NOERROR == mmresult )                              {
      CaVolume L = (   LR         & 0xFFFF )                         ;
      CaVolume R = ( ( LR >> 16 ) & 0xFFFF )                         ;
      if ( 0 == atChannel ) V = L ;                               else
      if ( 1 == atChannel ) V = R ;                             else {
        if ( L > R ) V = L ; else V = R                              ;
      }                                                              ;
    }                                                                ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  V *= 10000                                                         ;
  V /= 0xFFFF                                                        ;
  ////////////////////////////////////////////////////////////////////

  return V                                                           ;
}

CaVolume WmmeStream::setVolume(CaVolume volume,int atChannel)
{
  if ( CA_IS_INPUT_STREAM_( this ) ) return Volume ( ) ;
  if ( output.deviceCount <= 0     ) return Volume ( ) ;
  CaVolume L                                           ;
  CaVolume R                                           ;
  DWORD    LR                                          ;
  MMRESULT mmresult                                    ;
  //////////////////////////////////////////////////////
  if ( volume > 10000  ) volume = 10000                ;
  volume *= 0xFFFF                                     ;
  volume /= 10000                                      ;
  if ( volume > 0xFFFF ) volume = 0xFFFF               ;
  //////////////////////////////////////////////////////
  L = volume                                           ;
  R = volume                                           ;
  //////////////////////////////////////////////////////
  if ( atChannel >= 0 )                                {
    mmresult = ::waveOutGetVolume                      (
                 ((HWAVEOUT *)output.waveHandles)[0]   ,
                 &LR                                 ) ;
    if ( MMSYSERR_NOERROR == mmresult )                {
      L = (   LR         & 0xFFFF )                    ;
      R = ( ( LR >> 16 ) & 0xFFFF )                    ;
    }                                                  ;
  }                                                    ;
  //////////////////////////////////////////////////////
  if ( 0 == atChannel ) L = volume ;                else
  if ( 1 == atChannel ) R = volume                     ;
  //////////////////////////////////////////////////////
  LR   = (int)R                                        ;
  LR <<= 16                                            ;
  LR  |= (int)L                                        ;
  mmresult = ::waveOutSetVolume                        (
               ((HWAVEOUT *)output.waveHandles)[0]     ,
               LR                                    ) ;
  return Volume ( atChannel )                          ;
}

CaError WmmeStream::AdvanceToNextInputBuffer(void)
{
  CaError      result = NoError                                              ;
  MMRESULT     mmresult                                                      ;
  unsigned int i                                                             ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < input . deviceCount ; ++i )                              {
    input.waveHeaders[i][input.currentBufferIndex].dwFlags &= ~WHDR_DONE     ;
    mmresult = ::waveInAddBuffer                                             (
                 ((HWAVEIN *) input . waveHandles)[i]                        ,
                 &input.waveHeaders [ i ] [ input . currentBufferIndex ]     ,
                 sizeof(WAVEHDR)                                           ) ;
    if ( MMSYSERR_NOERROR != mmresult )                                      {
      result = UnanticipatedHostError                                        ;
      CA_MME_SET_LAST_WAVEIN_ERROR ( mmresult )                              ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  input . currentBufferIndex = CA_CIRCULAR_INCREMENT_                        (
                                 input . currentBufferIndex                  ,
                                 input . bufferCount                       ) ;
  input . framesUsedInCurrentBuffer = 0                                      ;
  return result                                                              ;
}

CaError WmmeStream::AdvanceToNextOutputBuffer(void)
{
  CaError      result = NoError                                              ;
  MMRESULT     mmresult                                                      ;
  unsigned int i                                                             ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < output . deviceCount ; ++i )                             {
    mmresult = ::waveOutWrite                                                (
                 ((HWAVEOUT *)output.waveHandles)[i]                         ,
                 &output.waveHeaders[i][output.currentBufferIndex]           ,
                 sizeof(WAVEHDR)                                           ) ;
    if ( MMSYSERR_NOERROR != mmresult )                                      {
      result = UnanticipatedHostError                                        ;
      CA_MME_SET_LAST_WAVEOUT_ERROR ( mmresult )                             ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  output . currentBufferIndex = CA_CIRCULAR_INCREMENT_                       (
                                  output . currentBufferIndex                ,
                                  output . bufferCount                     ) ;
  output . framesUsedInCurrentBuffer = 0                                     ;
  return result                                                              ;
}

/* requeue all but the most recent input with the driver. Used for catching
    up after a total input buffer underrun */
CaError WmmeStream::CatchUpInputBuffers(void)
{
  CaError      result = NoError                                              ;
  unsigned int i                                                             ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < input . bufferCount - 1 ; ++i )                          {
    result = AdvanceToNextInputBuffer ()                                     ;
    if ( NoError != result ) break                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

/* take the most recent output and duplicate it to all other output buffers
   and requeue them. Used for catching up after a total output buffer underrun. */
CaError WmmeStream::CatchUpOutputBuffers(void)
{
  CaError      result = NoError                                              ;
  unsigned int i                                                             ;
  unsigned int j                                                             ;
  unsigned int previousBufferIndex                                           ;
  ////////////////////////////////////////////////////////////////////////////
  previousBufferIndex = CA_CIRCULAR_DECREMENT_( output . currentBufferIndex  ,
                                                output . bufferCount       ) ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < output . bufferCount - 1 ; ++i )                         {
    for ( j = 0 ; j < output . deviceCount ; ++j )                           {
      if ( output.waveHeaders[j][ output.currentBufferIndex ].lpData        !=
           output.waveHeaders[j][ previousBufferIndex       ].lpData       ) {
        CopyMemory                                                           (
          output.waveHeaders[j][output.currentBufferIndex].lpData            ,
          output.waveHeaders[j][previousBufferIndex      ].lpData            ,
          output.waveHeaders[j][output.currentBufferIndex].dwBufferLength  ) ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    result = AdvanceToNextOutputBuffer()                                     ;
    if ( NoError != result ) break                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

int WmmeStream::CurrentInputBuffersAreDone(void)
{
  return BuffersAreDone                  (
           input  . waveHeaders          ,
           input  . deviceCount          ,
           input  . currentBufferIndex ) ;
}

int WmmeStream::CurrentOutputBuffersAreDone(void)
{
  return BuffersAreDone                  (
           output . waveHeaders          ,
           output . deviceCount          ,
           output . currentBufferIndex ) ;
}

///////////////////////////////////////////////////////////////////////////////

WmmeDeviceInfo:: WmmeDeviceInfo (void)
               : DeviceInfo     (    )
{
  dwFormats                       = 0 ;
  deviceInputChannelCountIsKnown  = 0 ;
  deviceOutputChannelCountIsKnown = 0 ;
}

WmmeDeviceInfo::~WmmeDeviceInfo (void)
{
}

CaError WmmeDeviceInfo::IsInputChannelCountSupported(int channelCount)
{
  CaError result = NoError                                 ;
  if ( ( channelCount >  0                              ) &&
       ( 0            != deviceInputChannelCountIsKnown ) &&
       ( channelCount > maxInputChannels              ) )  {
    result = InvalidChannelCount                           ;
  }                                                        ;
  return result                                            ;
}

CaError WmmeDeviceInfo::IsOutputChannelCountSupported(int channelCount)
{
  CaError result = NoError                                  ;
  if ( ( channelCount >  0                               ) &&
       ( 0            != deviceOutputChannelCountIsKnown ) &&
       ( channelCount >  maxOutputChannels             ) )  {
    result = InvalidChannelCount                            ;
  }                                                         ;
  return result                                             ;
}

///////////////////////////////////////////////////////////////////////////////

WmmeHostApiInfo:: WmmeHostApiInfo (void)
                : HostApiInfo     (    )
{
}

WmmeHostApiInfo::~WmmeHostApiInfo (void)
{
}

///////////////////////////////////////////////////////////////////////////////

WmmeHostApi:: WmmeHostApi             ( void )
            : HostApi                 (      )
            , allocations             ( NULL )
            , winMmeDeviceIds         ( NULL )
            , inputDeviceCount        ( 0    )
            , outputDeviceCount       ( 0    )
{
}

WmmeHostApi::~WmmeHostApi(void)
{
}

HostApi::Encoding WmmeHostApi::encoding(void) const
{
  return NATIVE ;
}

bool WmmeHostApi::hasDuplex(void) const
{
  return true ;
}

CaError WmmeHostApi::Open                            (
          Stream                ** s                 ,
          const StreamParameters * inputParameters   ,
          const StreamParameters * outputParameters  ,
          double                   sampleRate        ,
          CaUint32                 framesPerCallback ,
          CaStreamFlags            streamFlags       ,
          Conduit                * conduit           )
{
  CaError                      result                                        ;
  WmmeStream                 * stream                            = NULL      ;
  int                          bufferProcessorIsInitialized      = 0         ;
  int                          streamRepresentationIsInitialized = 0         ;
  CaSampleFormat               hostInputSampleFormat                         ;
  CaSampleFormat               hostOutputSampleFormat                        ;
  int                          inputChannelCount                             ;
  int                          outputChannelCount                            ;
  CaSampleFormat               inputSampleFormat                             ;
  CaSampleFormat               outputSampleFormat                            ;
  double                       suggestedInputLatency                         ;
  double                       suggestedOutputLatency                        ;
  CaWmmeStreamInfo           * inputStreamInfo                               ;
  CaWmmeStreamInfo           * outputStreamInfo                              ;
  CaWaveFormatChannelMask      inputChannelMask                              ;
  CaWaveFormatChannelMask      outputChannelMask                             ;
  unsigned long                framesPerHostInputBuffer                      ;
  unsigned long                hostInputBufferCount                          ;
  unsigned long                framesPerHostOutputBuffer                     ;
  unsigned long                hostOutputBufferCount                         ;
  unsigned long                framesPerBufferProcessorCall                  ;
  CaMmeDeviceAndChannelCount * inputDevices                       = 0        ;
  unsigned long                winMmeSpecificInputFlags           = 0        ;
  unsigned long                inputDeviceCount                   = 0        ;
  CaMmeDeviceAndChannelCount * outputDevices                      = 0        ;
  unsigned long                winMmeSpecificOutputFlags          = 0        ;
  unsigned long                outputDeviceCount                  = 0        ;
  char                         throttleProcessingThreadOnOverload = 1        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    inputChannelCount     = inputParameters->channelCount                    ;
    inputSampleFormat     = inputParameters->sampleFormat                    ;
    suggestedInputLatency = inputParameters->suggestedLatency                ;
    inputDeviceCount      = 1                                                ;
    /* validate input streamInfo                             */
    inputStreamInfo = (CaWmmeStreamInfo *)inputParameters->streamInfo ;
    result = ValidateWinMmeSpecificStreamInfo                                (
               inputParameters                                               ,
               inputStreamInfo                                               ,
               &winMmeSpecificInputFlags                                     ,
               &throttleProcessingThreadOnOverload                           ,
               &inputDeviceCount                                           ) ;
    if ( NoError != result ) return result                                   ;
    inputDevices = new CaMmeDeviceAndChannelCount [ inputDeviceCount ]       ;
    if ( NULL == inputDevices ) return InsufficientMemory                    ;
    result = RetrieveDevicesFromStreamParameters                             (
               inputParameters                                               ,
               inputStreamInfo                                               ,
               inputDevices                                                  ,
               inputDeviceCount                                            ) ;
    if ( NoError != result ) return result                                   ;
    result = ValidateInputChannelCounts(inputDevices,inputDeviceCount)       ;
    if ( NoError != result ) return result                                   ;
    hostInputSampleFormat = ClosestFormat ( cafInt16 /* native formats */, inputSampleFormat ) ;
    if ( 1 != inputDeviceCount )                                             {
      /* always use direct speakers when using multi-device multichannel mode */
      inputChannelMask = CA_SPEAKER_DIRECTOUT                                ;
    } else                                                                   {
      if ( ( NULL != inputStreamInfo                                      ) &&
           ( 0 != ( inputStreamInfo->flags & CaMmeUseChannelMask ) )      )  {
        inputChannelMask = inputStreamInfo->channelMask                      ;
      } else                                                                 {
        inputChannelMask = CaDefaultChannelMask(inputDevices[0].channelCount) ;
      }                                                                      ;
    }                                                                        ;
    if ( NULL != conduit )                                                   {
      conduit->Input . BytesPerSample  = inputChannelCount                   ;
      conduit->Input . BytesPerSample *= Core::SampleSize(inputSampleFormat) ;
    }                                                                        ;
  } else                                                                     {
    inputChannelCount     = 0                                                ;
    inputSampleFormat     = cafNothing                                       ;
    suggestedInputLatency = 0.0                                              ;
    inputStreamInfo       = 0                                                ;
    hostInputSampleFormat = cafNothing                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputChannelCount     = outputParameters -> channelCount                ;
    outputSampleFormat     = outputParameters -> sampleFormat                ;
    suggestedOutputLatency = outputParameters -> suggestedLatency            ;
    outputDeviceCount      = 1                                               ;
    /* validate output streamInfo                            */
    outputStreamInfo = (CaWmmeStreamInfo *)outputParameters->streamInfo ;
    result = ValidateWinMmeSpecificStreamInfo                                (
               outputParameters                                              ,
               outputStreamInfo                                              ,
               &winMmeSpecificOutputFlags                                    ,
               &throttleProcessingThreadOnOverload                           ,
               &outputDeviceCount                                          ) ;
    if ( NoError != result ) return result                                   ;
    outputDevices = new CaMmeDeviceAndChannelCount [outputDeviceCount ]      ;
    if ( NULL == outputDevices ) return InsufficientMemory                   ;
    result = RetrieveDevicesFromStreamParameters                             (
               outputParameters                                              ,
               outputStreamInfo                                              ,
               outputDevices                                                 ,
               outputDeviceCount                                           ) ;
    if ( NoError != result ) return result                                   ;
    result = ValidateOutputChannelCounts(outputDevices,outputDeviceCount)    ;
    if ( NoError != result ) return result                                   ;
    hostOutputSampleFormat = ClosestFormat(cafInt16 /* native formats */,outputSampleFormat) ;
    if ( 1 != outputDeviceCount )                                            {
      /* always use direct speakers when using multi-device multichannel mode */
      outputChannelMask = CA_SPEAKER_DIRECTOUT                               ;
    } else                                                                   {
      if ( ( NULL != outputStreamInfo                                    )  &&
           ( 0    != ( outputStreamInfo->flags & CaMmeUseChannelMask ) ) )   {
        outputChannelMask = outputStreamInfo->channelMask                    ;
      } else                                                                 {
        outputChannelMask = CaDefaultChannelMask(outputDevices[0].channelCount) ;
      }                                                                      ;
    }                                                                        ;
    if ( NULL != conduit )                                                   {
      conduit->Output . BytesPerSample  = outputChannelCount                 ;
      conduit->Output . BytesPerSample *= Core::SampleSize(outputSampleFormat) ;
    }                                                                        ;
  } else                                                                     {
    outputChannelCount     = 0                                               ;
    outputSampleFormat     = cafNothing                                      ;
    outputStreamInfo       = 0                                               ;
    hostOutputSampleFormat = cafNothing                                      ;
    suggestedOutputLatency = 0.0                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* - alter sampleRate to a close allowable rate if possible / necessary   */
  /* validate platform specific flags                                       */
  if ( 0 != ( streamFlags & csfPlatformSpecificFlags) )                      {
    /* unexpected platform specific flag                                    */
    return InvalidFlag                                                       ;
  }                                                                          ;
  /* always disable clipping and dithering if we are outputting a raw spdif stream */
  if ( ( 0 != ( winMmeSpecificOutputFlags & CaMmeWaveFormatDolbyAc3Spdif ) )||
       ( 0 != ( winMmeSpecificOutputFlags & CaMmeWaveFormatWmaSpdif    ) ) ) {
    streamFlags = streamFlags | csfClipOff | csfDitherOff                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  result = CalculateBufferSettings                                           (
              &framesPerHostInputBuffer                                      ,
              &hostInputBufferCount                                          ,
              &framesPerHostOutputBuffer                                     ,
              &hostOutputBufferCount                                         ,
              inputChannelCount                                              ,
              hostInputSampleFormat                                          ,
              suggestedInputLatency                                          ,
              inputStreamInfo                                                ,
              outputChannelCount                                             ,
              hostOutputSampleFormat                                         ,
              suggestedOutputLatency                                         ,
              outputStreamInfo                                               ,
              sampleRate                                                     ,
              framesPerCallback                                            ) ;
  if ( NoError != result ) goto error                                        ;
  ////////////////////////////////////////////////////////////////////////////
  stream  = new WmmeStream ( )                                               ;
  stream -> debugger = debugger                                              ;
  if ( NULL == stream )                                                      {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  InitializeSingleDirectionHandlesAndBuffers ( &stream->input  )             ;
  InitializeSingleDirectionHandlesAndBuffers ( &stream->output )             ;
  stream -> abortEvent              = 0                                      ;
  stream -> processingThread        = 0                                      ;
  stream -> throttleProcessingThreadOnOverload = throttleProcessingThreadOnOverload ;
  stream -> conduit                 = conduit                                ;
  stream -> cpuLoadMeasurer . Initialize ( sampleRate )                      ;
  streamRepresentationIsInitialized = 1                                      ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( NULL != inputParameters ) && ( NULL != outputParameters ) )         {
    /* full duplex                                                          */
    if ( framesPerHostInputBuffer < framesPerHostOutputBuffer )              {
      /* CalculateBufferSettings() should guarantee this condition          */
//      assert( (framesPerHostOutputBuffer % framesPerHostInputBuffer) == 0 )  ;
      framesPerBufferProcessorCall = framesPerHostInputBuffer;
    } else                                                                   {
      /* CalculateBufferSettings() should guarantee this condition          */
//      assert( (framesPerHostInputBuffer % framesPerHostOutputBuffer) == 0 )  ;
      framesPerBufferProcessorCall = framesPerHostOutputBuffer               ;
    }                                                                        ;
  } else
  if ( NULL != inputParameters  )                                            {
    framesPerBufferProcessorCall = framesPerHostInputBuffer                  ;
  } else
  if ( NULL != outputParameters )                                            {
    framesPerBufferProcessorCall = framesPerHostOutputBuffer                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> input   . framesPerBuffer = framesPerHostInputBuffer             ;
  stream -> output  . framesPerBuffer = framesPerHostOutputBuffer            ;
  result  = stream -> bufferProcessor . Initialize                           (
                        inputChannelCount                                    ,
                        inputSampleFormat                                    ,
                        hostInputSampleFormat                                ,
                        outputChannelCount                                   ,
                        outputSampleFormat                                   ,
                        hostOutputSampleFormat                               ,
                        sampleRate                                           ,
                        streamFlags                                          ,
                        framesPerCallback                                    ,
                        framesPerBufferProcessorCall                         ,
                        cabFixedHostBufferSize                               ,
                        conduit                                            ) ;
  if ( NoError != result ) goto error                                        ;
  ////////////////////////////////////////////////////////////////////////////
  bufferProcessorIsInitialized = 1                                           ;
  /* stream info input latency is the minimum buffering latency (unlike suggested and default which are *maximums*) */
  stream -> inputLatency                                                     =
          (double) ( stream->bufferProcessor.InputLatencyFrames()            +
                     framesPerHostInputBuffer                              ) /
                     sampleRate                                              ;
  stream -> outputLatency                                                    =
          (double) ( stream->bufferProcessor.OutputLatencyFrames()           +
                     ( framesPerHostOutputBuffer                             *
                       ( hostOutputBufferCount - 1 )                     ) ) /
                     sampleRate                                              ;
  stream -> sampleRate = sampleRate                                          ;
  stream -> primeStreamUsingCallback                                         =
               ( ( streamFlags & csfPrimeOutputBuffersUsingStreamCallback ) &&
                 ( NULL != conduit ) )                                       ?
                1 : 0                                                        ;
  /* time to sleep when throttling due to >100% cpu usage.
      -a quater of a buffer's duration                                      */
  stream -> throttledSleepMsecs                                              =
          (unsigned long)(stream->bufferProcessor.framesPerHostBuffer        *
                          stream->bufferProcessor.samplePeriod * .25 * 1000) ;
  stream -> isStopped = 1                                                    ;
  stream -> isActive  = 0                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  /* for maximum compatibility with multi-device multichannel drivers,
      we first open all devices, then we prepare all buffers, finally
      we start all devices ( in StartStream() ). teardown in reverse order. */
  if ( NULL != inputParameters )                                             {
    result = InitializeWaveHandles                                           (
               this                                                          ,
               &stream->input                                                ,
               winMmeSpecificInputFlags                                      ,
               stream->bufferProcessor.bytesPerHostInputSample               ,
               sampleRate                                                    ,
               inputDevices                                                  ,
               inputDeviceCount                                              ,
               inputChannelMask                                              ,
               1 /* isInput */                                             ) ;
    if ( NoError != result ) goto error                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    result = InitializeWaveHandles                                           (
               this                                                          ,
               &stream->output                                               ,
               winMmeSpecificOutputFlags                                     ,
               stream->bufferProcessor.bytesPerHostOutputSample              ,
               sampleRate                                                    ,
               outputDevices                                                 ,
               outputDeviceCount                                             ,
               outputChannelMask                                             ,
               0 /* isInput */                                             ) ;
    if ( NoError != result ) goto error                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    result = InitializeWaveHeaders                                           (
               &stream->input                                                ,
               hostInputBufferCount                                          ,
               hostInputSampleFormat                                         ,
               framesPerHostInputBuffer                                      ,
               inputDevices                                                  ,
               1 /* isInput */                                             ) ;
    if ( NoError != result ) goto error                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    result = InitializeWaveHeaders                                           (
               &stream->output                                               ,
               hostOutputBufferCount                                         ,
               hostOutputSampleFormat                                        ,
               framesPerHostOutputBuffer                                     ,
               outputDevices                                                 ,
               0 /* not isInput */                                         ) ;
    if ( NoError != result ) goto error                                      ;
    stream->allBuffersDurationMs = (DWORD) ( 1000.0                          *
                                           ( framesPerHostOutputBuffer       *
                                             stream->output.bufferCount    ) /
                                             sampleRate                    ) ;
  } else                                                                     {
    stream->allBuffersDurationMs = (DWORD) ( 1000.0                          *
                                           ( framesPerHostInputBuffer        *
                                             stream->input.bufferCount     ) /
                                             sampleRate                    ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != conduit )                                                     {
      /* abort event is only needed for callback streams                    */
    result = CreateEventWithCaError                                          (
               &stream->abortEvent                                           ,
               NULL                                                          ,
               TRUE                                                          ,
               FALSE                                                         ,
               NULL                                                        ) ;
    if ( NoError != result ) goto error                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *s = (Stream *)stream                                                      ;
  return result                                                              ;
error:
  if ( NULL == stream ) return result                                        ;
  if ( 0 != stream->abortEvent ) CloseHandle( stream->abortEvent )           ;
  TerminateWaveHeaders ( &stream -> output , 0 /* not isInput */ )           ;
  TerminateWaveHeaders ( &stream -> input  , 1 /* isInput     */ )           ;
  TerminateWaveHandles ( &stream -> output , 0 /* not isInput */, 1 /* currentlyProcessingAnError */ ) ;
  TerminateWaveHandles ( &stream -> input  , 1 /* isInput     */, 1 /* currentlyProcessingAnError */ ) ;
  if ( 0 != bufferProcessorIsInitialized )                                   {
    stream -> bufferProcessor . Terminate ( )                                ;
  }                                                                          ;
  delete stream                                                              ;
  return result                                                              ;
}

void WmmeHostApi::Terminate(void)
{
  CaEraseAllocation ;
}

CaError WmmeHostApi::isSupported                     (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double                    sampleRate       )
{
  DeviceInfo       * inputDeviceInfo                                         ;
  DeviceInfo       * outputDeviceInfo                                        ;
  int                inputChannelCount                                       ;
  int                outputChannelCount                                      ;
  int                inputMultipleDeviceChannelCount                         ;
  int                outputMultipleDeviceChannelCount                        ;
  CaSampleFormat     inputSampleFormat                                       ;
  CaSampleFormat     outputSampleFormat                                      ;
  CaWmmeStreamInfo * inputStreamInfo                                         ;
  CaWmmeStreamInfo * outputStreamInfo                                        ;
  UINT               winMmeInputDeviceId                                     ;
  UINT               winMmeOutputDeviceId                                    ;
  unsigned int       i                                                       ;
  CaError            paerror                                                 ;
  /* The calls to QueryFormatSupported below are intended to detect invalid
      sample rates. If we assume that the channel count and format are OK,
      then the only thing that could fail is the sample rate. This isn't
      strictly true, but I can't think of a better way to test that the
      sample rate is valid.                                                 */
  if ( NULL != inputParameters )                                             {
    inputChannelCount = inputParameters -> channelCount                      ;
    inputSampleFormat = inputParameters -> sampleFormat                      ;
    inputStreamInfo   = (CaWmmeStreamInfo *)inputParameters -> streamInfo ;
    /* all standard sample formats are supported by the buffer adapter,
       this implementation doesn't support any custom sample formats        */
    if ( inputSampleFormat & cafCustomFormat )                               {
      return SampleFormatNotSupported                                        ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( inputParameters->device == CaUseHostApiSpecificDeviceSpecification ) &&
         ( NULL != inputStreamInfo )                                        &&
         ( 0 != ( inputStreamInfo->flags & CaMmeUseMultipleDevices    ) ) )  {
      inputMultipleDeviceChannelCount = 0                                    ;
      ////////////////////////////////////////////////////////////////////////
      for ( i = 0 ; i < inputStreamInfo -> deviceCount ; ++i )               {
        inputMultipleDeviceChannelCount += inputStreamInfo->devices[i].channelCount ;
        inputDeviceInfo                  = deviceInfos[inputStreamInfo->devices[i].device] ;
        /* check that input device can support inputChannelCount            */
        if ( inputStreamInfo->devices[i].channelCount < 1 )                  {
          return InvalidChannelCount                                         ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        paerror = ((WmmeDeviceInfo *)inputDeviceInfo)->IsInputChannelCountSupported (
                      inputStreamInfo->devices[i].channelCount             ) ;
        if ( NoError !=  paerror ) return paerror                            ;
        /* test for valid sample rate, see comment above                    */
        winMmeInputDeviceId = LocalDeviceIndexToWinMmeDeviceId               (
                                this                                         ,
                                inputStreamInfo->devices[i].device         ) ;
        paerror = QueryFormatSupported                                       (
                    inputDeviceInfo                                          ,
                    QueryInputWaveFormatEx                                   ,
                    winMmeInputDeviceId                                      ,
                    inputStreamInfo->devices[i].channelCount                 ,
                    sampleRate                                               ,
                   ((NULL!=inputStreamInfo) ? inputStreamInfo->flags : 0)  ) ;
        if ( NoError != paerror ) return InvalidSampleRate                   ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      if ( inputMultipleDeviceChannelCount != inputChannelCount )            {
        return IncompatibleStreamInfo                         ;
      }                                                                      ;
    } else                                                                   {
      if ( ( NULL != inputStreamInfo                                      ) &&
           ( 0 != (inputStreamInfo->flags & CaMmeUseMultipleDevices   ) ) )  {
        return IncompatibleStreamInfo                         ;
        /* CaUseHostApiSpecificDeviceSpecification was not supplied as the input device */
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      inputDeviceInfo = deviceInfos [ inputParameters->device ]              ;
      /* check that input device can support inputChannelCount              */
      paerror = ((WmmeDeviceInfo *)inputDeviceInfo)->IsInputChannelCountSupported(inputChannelCount) ;
      if ( NoError != paerror ) return paerror                               ;
      /* test for valid sample rate, see comment above                      */
      winMmeInputDeviceId = LocalDeviceIndexToWinMmeDeviceId                 (
                              this                                           ,
                              inputParameters->device                      ) ;
      paerror = QueryFormatSupported                                         (
                  inputDeviceInfo                                            ,
                  QueryInputWaveFormatEx                                     ,
                  winMmeInputDeviceId                                        ,
                  inputChannelCount                                          ,
                  sampleRate                                                 ,
                  ( (NULL!=inputStreamInfo) ? inputStreamInfo->flags : 0 ) ) ;
      if ( NoError != paerror ) return InvalidSampleRate                     ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputChannelCount = outputParameters -> channelCount                    ;
    outputSampleFormat = outputParameters -> sampleFormat                    ;
    outputStreamInfo   = (CaWmmeStreamInfo *) outputParameters -> streamInfo ;
    /* all standard sample formats are supported by the buffer adapter,
       this implementation doesn't support any custom sample formats        */
    if ( 0 != ( outputSampleFormat & cafCustomFormat ) )                     {
      return SampleFormatNotSupported                                        ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( outputParameters->device == CaUseHostApiSpecificDeviceSpecification ) &&
         ( NULL != outputStreamInfo )                                       &&
         ( 0 != ( outputStreamInfo->flags & CaMmeUseMultipleDevices    ) ) ) {
      outputMultipleDeviceChannelCount = 0                                   ;
      for ( i = 0 ; i < outputStreamInfo -> deviceCount ; ++i )              {
        outputMultipleDeviceChannelCount += outputStreamInfo->devices[i].channelCount ;
        outputDeviceInfo = deviceInfos[outputStreamInfo->devices[i].device]  ;
        /* check that output device can support outputChannelCount          */
        if ( outputStreamInfo->devices[i].channelCount < 1 )                 {
          return InvalidChannelCount                                         ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        paerror = ((WmmeDeviceInfo *)outputDeviceInfo)->IsOutputChannelCountSupported(outputStreamInfo->devices[i].channelCount) ;
        if ( NoError != paerror ) return paerror                             ;
        /* test for valid sample rate, see comment above                    */
        winMmeOutputDeviceId = LocalDeviceIndexToWinMmeDeviceId              (
                                 this                                        ,
                                 outputStreamInfo->devices[i].device       ) ;
        paerror = QueryFormatSupported                                       (
                    outputDeviceInfo                                         ,
                    QueryOutputWaveFormatEx                                  ,
                    winMmeOutputDeviceId                                     ,
                    outputStreamInfo->devices[i].channelCount                ,
                    sampleRate                                               ,
                    ((outputStreamInfo) ? outputStreamInfo->flags : 0    ) ) ;
        if ( NoError != paerror ) return InvalidSampleRate                   ;
      }                                                                      ;
      if ( outputMultipleDeviceChannelCount != outputChannelCount )          {
        return IncompatibleStreamInfo                         ;
      }                                                                      ;
    } else                                                                   {
      if ( ( NULL != outputStreamInfo                                     ) &&
           ( 0 != ( outputStreamInfo->flags & CaMmeUseMultipleDevices ) ) )  {
        return IncompatibleStreamInfo                         ;
        /* CaUseHostApiSpecificDeviceSpecification was not supplied as the output device */
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      outputDeviceInfo = deviceInfos[ outputParameters->device ]             ;
      /* check that output device can support outputChannelCount            */
      paerror = ((WmmeDeviceInfo *)outputDeviceInfo)->IsOutputChannelCountSupported(outputChannelCount) ;
      if ( NoError != paerror ) return paerror                               ;
      /* test for valid sample rate, see comment above                      */
      winMmeOutputDeviceId = LocalDeviceIndexToWinMmeDeviceId                (
                               this                                          ,
                               outputParameters->device                    ) ;
      paerror = QueryFormatSupported                                         (
                  outputDeviceInfo                                           ,
                  QueryOutputWaveFormatEx                                    ,
                  winMmeOutputDeviceId                                       ,
                  outputChannelCount                                         ,
                  sampleRate                                                 ,
                  ((outputStreamInfo) ? outputStreamInfo->flags : 0    ) )   ;
      if ( NoError != paerror ) return InvalidSampleRate                     ;
    }                                                                        ;
  }                                                                          ;
  /*
          - if a full duplex stream is requested, check that the combination
              of input and output parameters is supported

          - check that the device supports sampleRate

          for mme all we can do is test that the input and output devices
          support the requested sample rate and number of channels. we
          cannot test for full duplex compatibility.
  */
  return NoError                                                             ;
}

/* updates deviceCount if CaMmeUseMultipleDevices is used */

CaError WmmeHostApi::RetrieveDevicesFromStreamParameters (
          const StreamParameters     * streamParameters  ,
          const CaWmmeStreamInfo     * streamInfo        ,
          CaMmeDeviceAndChannelCount * devices           ,
          unsigned long                deviceCount       )
{
  CaError       result = NoError                                             ;
  unsigned int  i                                                            ;
  int           totalChannelCount                                            ;
  CaDeviceIndex hostApiDevice                                                ;
  if ( ( NULL != streamInfo                          )                      &&
       ( streamInfo->flags & CaMmeUseMultipleDevices )                     ) {
    totalChannelCount = 0                                                    ;
    for ( i = 0 ; i < deviceCount ; ++i )                                    {
      /* validate that the device number is within range                    */
      result = ToHostDeviceIndex(&hostApiDevice,streamInfo->devices[i].device) ;
      if ( NoError != result ) return result                                 ;
      devices [ i ] . device       = hostApiDevice                           ;
      devices [ i ] . channelCount = streamInfo->devices[i].channelCount     ;
      totalChannelCount           += devices[i].channelCount                 ;
    }                                                                        ;
    if ( totalChannelCount != streamParameters->channelCount )               {
      /* channelCount must match total channels specified by multiple devices */
      return InvalidChannelCount                                             ;
      /* REVIEW use of this error code                                      */
    }                                                                        ;
  } else                                                                     {
    devices [ 0 ] . device       = streamParameters -> device                ;
    devices [ 0 ] . channelCount = streamParameters -> channelCount          ;
  }                                                                          ;
  return result                                                              ;
}

CaError WmmeHostApi::ValidateInputChannelCounts    (
          CaMmeDeviceAndChannelCount * devices     ,
          unsigned long                deviceCount )
{
  unsigned int     i                                                         ;
  WmmeDeviceInfo * inputDeviceInfo                                           ;
  CaError          paerror                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < deviceCount ; ++i )                                      {
    if ( devices[i].channelCount < 1 ) return InvalidChannelCount            ;
    inputDeviceInfo = (WmmeDeviceInfo *)deviceInfos[devices[i].device]       ;
    paerror         = inputDeviceInfo->IsInputChannelCountSupported(devices[i].channelCount) ;
    if ( NoError != paerror ) return paerror                                 ;
  }                                                                          ;
  return NoError                                                             ;
}

CaError WmmeHostApi::ValidateOutputChannelCounts   (
          CaMmeDeviceAndChannelCount * devices     ,
          unsigned long                deviceCount )
{
  unsigned int     i                                                         ;
  WmmeDeviceInfo * outputDeviceInfo                                          ;
  CaError          paerror                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < deviceCount ; ++i )                                      {
    if ( devices [ i ] . channelCount < 1 )                                  {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    outputDeviceInfo = (WmmeDeviceInfo *)deviceInfos[devices[i].device]      ;
    paerror          = outputDeviceInfo->IsOutputChannelCountSupported(devices[i].channelCount) ;
    if ( NoError != paerror ) return paerror                                 ;
  }                                                                          ;
  return NoError                                                             ;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
