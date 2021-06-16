/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/22                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CaWaSAPI.hpp"

#if defined(WIN32) || defined(_WIN32)
#else
#error "Windows Aduio Session API works only on Windows platform"
#endif

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

// Availabe from Windows 7
#ifndef AUDCLNT_E_BUFFER_ERROR
  #define AUDCLNT_E_BUFFER_ERROR AUDCLNT_ERR(0x018)
#endif

#ifndef AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED
  #define AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED AUDCLNT_ERR(0x019)
#endif

#ifndef AUDCLNT_E_INVALID_DEVICE_PERIOD
  #define AUDCLNT_E_INVALID_DEVICE_PERIOD AUDCLNT_ERR(0x020)
#endif

#ifndef AUDCLNT_E_BUFFER_ERROR
  #define AUDCLNT_E_BUFFER_ERROR AUDCLNT_ERR(0x018)
#endif

#ifndef AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED
  #define AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED AUDCLNT_ERR(0x019)
#endif

#ifndef AUDCLNT_E_INVALID_DEVICE_PERIOD
  #define AUDCLNT_E_INVALID_DEVICE_PERIOD AUDCLNT_ERR(0x020)
#endif

#define STATIC_ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

#define CA_WASAPI__IS_FULLDUPLEX(STREAM) ((STREAM)->in.clientProc && (STREAM)->out.clientProc)

#ifndef IF_FAILED_JUMP
#define IF_FAILED_JUMP(hr, label) if(FAILED(hr)) goto label;
#endif

#ifndef IF_FAILED_INTERNAL_ERROR_JUMP
#define IF_FAILED_INTERNAL_ERROR_JUMP(hr, error, label) if(FAILED(hr)) { error = InternalError; goto label; }
#endif

#define SAFE_CLOSE(h) if ((h) != NULL) { ::CloseHandle((h)); (h) = NULL; }
#define SAFE_RELEASE(punk) if ((punk) != NULL) { (punk)->Release() ; (punk) = NULL; }

// AVRT is the new "multimedia schedulling stuff"
typedef BOOL   (WINAPI * FAvRtCreateThreadOrderingGroup  ) (PHANDLE,PLARGE_INTEGER,GUID*,PLARGE_INTEGER);
typedef BOOL   (WINAPI * FAvRtDeleteThreadOrderingGroup  ) (HANDLE);
typedef BOOL   (WINAPI * FAvRtWaitOnThreadOrderingGroup  ) (HANDLE);
typedef HANDLE (WINAPI * FAvSetMmThreadCharacteristics   ) (LPCSTR,LPDWORD);
typedef BOOL   (WINAPI * FAvRevertMmThreadCharacteristics) (HANDLE);
typedef BOOL   (WINAPI * FAvSetMmThreadPriority          ) (HANDLE,AVRT_PRIORITY);

static HMODULE hDInputDLL = 0;
FAvRtCreateThreadOrderingGroup   pAvRtCreateThreadOrderingGroup = NULL;
FAvRtDeleteThreadOrderingGroup   pAvRtDeleteThreadOrderingGroup = NULL;
FAvRtWaitOnThreadOrderingGroup   pAvRtWaitOnThreadOrderingGroup = NULL;
FAvSetMmThreadCharacteristics    pAvSetMmThreadCharacteristics = NULL;
FAvRevertMmThreadCharacteristics pAvRevertMmThreadCharacteristics = NULL;
FAvSetMmThreadPriority           pAvSetMmThreadPriority = NULL;

#define _GetProc(fun, type, name)                       { \
           fun = (type) GetProcAddress(hDInputDLL,name) ; \
           if (fun == NULL) return FALSE ;                \
         }

///////////////////////////////////////////////////////////////////////////////

#ifndef GUID_SECT
  #define GUID_SECT
#endif

#define __DEFINE_GUID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const GUID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
#define __DEFINE_IID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const IID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
#define __DEFINE_CLSID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const CLSID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
#define CA_DEFINE_CLSID(className, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    __DEFINE_CLSID(ca_CLSID_##className, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)
#define CA_DEFINE_IID(interfaceName, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    __DEFINE_IID(ca_IID_##interfaceName, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)

// "1CB9AD4C-DBFA-4c32-B178-C2F568A703B2"
CA_DEFINE_IID(IAudioClient,         1cb9ad4c, dbfa, 4c32, b1, 78, c2, f5, 68, a7, 03, b2);
// "1BE09788-6894-4089-8586-9A2A6C265AC5"
CA_DEFINE_IID(IMMEndpoint,          1be09788, 6894, 4089, 85, 86, 9a, 2a, 6c, 26, 5a, c5);
// "A95664D2-9614-4F35-A746-DE8DB63617E6"
CA_DEFINE_IID(IMMDeviceEnumerator,  a95664d2, 9614, 4f35, a7, 46, de, 8d, b6, 36, 17, e6);
// "BCDE0395-E52F-467C-8E3D-C4579291692E"
CA_DEFINE_CLSID(IMMDeviceEnumerator,bcde0395, e52f, 467c, 8e, 3d, c4, 57, 92, 91, 69, 2e);
// "F294ACFC-3146-4483-A7BF-ADDCA7C260E2"
CA_DEFINE_IID(IAudioRenderClient,   f294acfc, 3146, 4483, a7, bf, ad, dc, a7, c2, 60, e2);
// "C8ADBD64-E71E-48a0-A4DE-185C395CD317"
CA_DEFINE_IID(IAudioCaptureClient,  c8adbd64, e71e, 48a0, a4, de, 18, 5c, 39, 5c, d3, 17);
// *2A07407E-6497-4A18-9787-32F79BD0D98F*  Or this??
CA_DEFINE_IID(IDeviceTopology,      2A07407E, 6497, 4A18, 97, 87, 32, f7, 9b, d0, d9, 8f);
// *AE2DE0E4-5BCA-4F2D-AA46-5D13F8FDB3A9*
CA_DEFINE_IID(IPart,                AE2DE0E4, 5BCA, 4F2D, aa, 46, 5d, 13, f8, fd, b3, a9);
// *4509F757-2D46-4637-8E62-CE7DB944F57B*
CA_DEFINE_IID(IKsJackDescription,   4509F757, 2D46, 4637, 8e, 62, ce, 7d, b9, 44, f5, 7b);
CA_DEFINE_IID(IAudioEndpointVolume, 5CDF2C82, 841E, 4546, 97, 22, 0C, F7, 40, 78, 22, 9A);
// Media formats:
__DEFINE_GUID(ca_KSDATAFORMAT_SUBTYPE_PCM,        0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
__DEFINE_GUID(ca_KSDATAFORMAT_SUBTYPE_ADPCM,      0x00000002, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
__DEFINE_GUID(ca_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );

/* use CreateThread for CYGWIN/Windows Mobile, _beginthreadex for all others */
#if !defined(__CYGWIN__) && !defined(_WIN32_WCE)
  #define CREATE_THREAD(PROC) (HANDLE)_beginthreadex( NULL, 0, (PROC), this, 0, &dwThreadId )
#else
  #define CREATE_THREAD(PROC) CreateThread( NULL, 0, (PROC), this, 0, &dwThreadId )
#endif

///////////////////////////////////////////////////////////////////////////////

typedef enum EMixerDir {
  MIX_DIR__1TO2        ,
  MIX_DIR__2TO1        ,
  MIX_DIR__2TO1_L      }
  EMixerDir            ;

typedef enum EWindowsVersion      {
  WINDOWS_UNKNOWN          = 0x00 ,
  WINDOWS_VISTA_SERVER2008 = 0x01 ,
  WINDOWS_7_SERVER2008R2   = 0x02 ,
  WINDOWS_FUTURE           = 0x04 }
  EWindowsVersion                 ;

///////////////////////////////////////////////////////////////////////////////

typedef struct CaWaSapiJackDescription      { /* Stream descriptor. */
  unsigned long              channelMapping ;
  unsigned long              color          ;
  CaWaSapiJackConnectionType connectionType ;
  CaWaSapiJackGeoLocation    geoLocation    ;
  CaWaSapiJackGenLocation    genLocation    ;
  CaWaSapiJackPortConnection portConnection ;
  unsigned int               isConnected    ;
} CaWaSapiJackDescription                   ;

///////////////////////////////////////////////////////////////////////////////

typedef struct ThreadIdleScheduler {
  UINT32 m_idle_microseconds       ; // number of microseconds to sleep
  UINT32 m_next_sleep              ;        //!< next sleep round
  UINT32 m_i                       ;             //!< current round iterator position
  UINT32 m_resolution              ; // resolution in number of milliseconds
} ThreadIdleScheduler              ;

//! Setup scheduler.
static void ThreadIdleScheduler_Setup            (
              ThreadIdleScheduler * sched        ,
              UINT32                resolution   ,
              UINT32                microseconds )
{
//  assert(microseconds != 0);
//  assert(resolution != 0);
//  assert((resolution * 1000) >= microseconds);
  memset ( sched , 0 , sizeof(ThreadIdleScheduler) )                  ;
  sched -> m_idle_microseconds = microseconds                         ;
  sched -> m_resolution        = resolution                           ;
  sched -> m_next_sleep        = ( resolution * 1000 ) / microseconds ;
}

//! Iterate and check if can sleep.
static UINT32 ThreadIdleScheduler_NextSleep(ThreadIdleScheduler * sched)
{ // advance and check if thread can sleep
  if (++ sched->m_i == sched->m_next_sleep) {
    sched -> m_i = 0                        ;
    return sched->m_resolution              ;
  }                                         ;
  return 0                                  ;
}

///////////////////////////////////////////////////////////////////////////////

static double nano100ToSeconds(REFERENCE_TIME ref)
{
  //   1 nano = 0.000000001 seconds
  // 100 nano = 0.0000001   seconds
  // 100 nano = 0.0001      milliseconds
  return ( (double) ref ) * 0.0000001 ;
}

///////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------
static REFERENCE_TIME SecondsTonano100(double ref)
{
  //   1 nano = 0.000000001 seconds
  // 100 nano = 0.0000001   seconds
  // 100 nano = 0.0001      milliseconds
  return (REFERENCE_TIME)( ref / 0.0000001 ) ;
}

///////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------
// Makes Hns period from frames and sample rate
static REFERENCE_TIME MakeHnsPeriod(UINT32 nFrames,DWORD nSamplesPerSec)
{
  return (REFERENCE_TIME) ( ( 10000.0 * 1000 / nSamplesPerSec * nFrames) + 0.5 ) ;
}

///////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------
// Converts Hns period into number of frames
static UINT32 MakeFramesFromHns(REFERENCE_TIME hnsPeriod, UINT32 nSamplesPerSec)
{
  UINT32 nFrames = (UINT32)( // frames =
           1.0 * hnsPeriod * // hns *
           nSamplesPerSec  /  // (frames / s) /
           1000 /            // (ms / s) /
           10000             // (hns / s) /
           + 0.5             // rounding
         )                 ;
  return nFrames           ;
}

///////////////////////////////////////////////////////////////////////////////

typedef UINT32 (*ALIGN_FUNC) (UINT32 v, UINT32 align);

///////////////////////////////////////////////////////////////////////////////

static UINT32 ALIGN_BWD(UINT32 v,UINT32 align)
{
  return ( ( v - ( align ? v % align : 0 ) ) ) ;
}

///////////////////////////////////////////////////////////////////////////////

static UINT32 ALIGN_FWD(UINT32 v,UINT32 align)
{
  UINT32 remainder = ( align ? ( v % align ) : 0 ) ;
  if ( 0 == remainder ) return v                   ;
  return ( v + ( align - remainder ) )             ;
}

///////////////////////////////////////////////////////////////////////////////

UINT32 ALIGN_NEXT_POW2(UINT32 v)
{
  UINT32 v2 = 1                  ;
  while ( v > ( v2 <<= 1 ) ) { ; }
  v = v2                         ;
  return v                       ;
}

///////////////////////////////////////////////////////////////////////////////

static UINT32 AlignFramesPerBuffer        (
                UINT32     nFrames        ,
                UINT32     nSamplesPerSec ,
                UINT32     nBlockAlign    ,
                ALIGN_FUNC pAlignFunc     )
{
  #define HDA_PACKET_SIZE (128)
  long frame_bytes = nFrames * nBlockAlign                         ;
  long packets                                                     ;
  //////////////////////////////////////////////////////////////////
  frame_bytes  = pAlignFunc(frame_bytes, HDA_PACKET_SIZE)          ;
  if (frame_bytes < HDA_PACKET_SIZE) frame_bytes = HDA_PACKET_SIZE ;
  nFrames     = frame_bytes / nBlockAlign                          ;
  packets     = frame_bytes / HDA_PACKET_SIZE                      ;
  frame_bytes = packets     * HDA_PACKET_SIZE                      ;
  nFrames     = frame_bytes / nBlockAlign                          ;
  return nFrames                                                   ;
  #undef HDA_PACKET_SIZE
}

///////////////////////////////////////////////////////////////////////////////

static UINT32 GetFramesSleepTime(UINT32 nFrames, UINT32 nSamplesPerSec)
{
  REFERENCE_TIME nDuration;
  if (nSamplesPerSec == 0) return 0;
  #define REFTIMES_PER_SEC  10000000
  #define REFTIMES_PER_MILLISEC  10000
  nDuration = (REFERENCE_TIME)((double)REFTIMES_PER_SEC * nFrames / nSamplesPerSec);
  return (UINT32)(nDuration/REFTIMES_PER_MILLISEC/2);
}

///////////////////////////////////////////////////////////////////////////////
static UINT32 GetFramesSleepTimeMicroseconds(UINT32 nFrames, UINT32 nSamplesPerSec)
{
  REFERENCE_TIME nDuration;
  if (nSamplesPerSec == 0) return 0;
  #define REFTIMES_PER_SEC  10000000
  #define REFTIMES_PER_MILLISEC  10000
  nDuration = (REFERENCE_TIME)((double)REFTIMES_PER_SEC * nFrames / nSamplesPerSec);
  return (UINT32)(nDuration/10/2);
}

///////////////////////////////////////////////////////////////////////////////

static BOOL SetupAVRT(void)
{
  hDInputDLL = LoadLibraryA ( "avrt.dll" )                                   ;
  if ( NULL == hDInputDLL ) return FALSE                                     ;
  ////////////////////////////////////////////////////////////////////////////
  _GetProc ( pAvRtCreateThreadOrderingGroup   , FAvRtCreateThreadOrderingGroup   , "AvRtCreateThreadOrderingGroup"   ) ;
  _GetProc ( pAvRtDeleteThreadOrderingGroup   , FAvRtDeleteThreadOrderingGroup   , "AvRtDeleteThreadOrderingGroup"   ) ;
  _GetProc ( pAvRtWaitOnThreadOrderingGroup   , FAvRtWaitOnThreadOrderingGroup   , "AvRtWaitOnThreadOrderingGroup"   ) ;
  _GetProc ( pAvSetMmThreadCharacteristics    , FAvSetMmThreadCharacteristics    , "AvSetMmThreadCharacteristicsA"   ) ;
  _GetProc ( pAvRevertMmThreadCharacteristics , FAvRevertMmThreadCharacteristics , "AvRevertMmThreadCharacteristics" ) ;
  _GetProc ( pAvSetMmThreadPriority           , FAvSetMmThreadPriority           , "AvSetMmThreadPriority"           ) ;
  ////////////////////////////////////////////////////////////////////////////
  return pAvRtCreateThreadOrderingGroup   &&
         pAvRtDeleteThreadOrderingGroup   &&
         pAvRtWaitOnThreadOrderingGroup   &&
         pAvSetMmThreadCharacteristics    &&
         pAvRevertMmThreadCharacteristics &&
         pAvSetMmThreadPriority            ;
}

///////////////////////////////////////////////////////////////////////////////

static void CloseAVRT(void)
{
  if ( NULL != hDInputDLL ) FreeLibrary ( hDInputDLL ) ;
  hDInputDLL = NULL                                    ;
}

///////////////////////////////////////////////////////////////////////////////

static BOOL IsWow64(void)
{
  typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL)            ;
  LPFN_ISWOW64PROCESS fnIsWow64Process                                  ;
  BOOL                bIsWow64 = FALSE                                  ;
  fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress               (
                       GetModuleHandleA("kernel32")                     ,
                       "IsWow64Process"                               ) ;
  if (   fnIsWow64Process == NULL                        ) return FALSE ;
  if ( ! fnIsWow64Process(GetCurrentProcess(),&bIsWow64) ) return FALSE ;
  return bIsWow64                                                       ;
}

///////////////////////////////////////////////////////////////////////////////

#define WINDOWS_7_SERVER2008R2_AND_UP (WINDOWS_7_SERVER2008R2|WINDOWS_FUTURE)

static UINT32 GetWindowsVersion(void)
{
  static UINT32 version = WINDOWS_UNKNOWN                                    ;
  ////////////////////////////////////////////////////////////////////////////
  if ( WINDOWS_UNKNOWN == version )                                          {
    DWORD dwVersion      = 0                                                 ;
    DWORD dwMajorVersion = 0                                                 ;
    DWORD dwMinorVersion = 0                                                 ;
    DWORD dwBuild        = 0                                                 ;
    //////////////////////////////////////////////////////////////////////////
    typedef DWORD (WINAPI *LPFN_GETVERSION)(VOID)                            ;
    LPFN_GETVERSION fnGetVersion                                             ;
    //////////////////////////////////////////////////////////////////////////
    fnGetVersion = (LPFN_GETVERSION) GetProcAddress(GetModuleHandleA("kernel32"),"GetVersion") ;
    if ( NULL == fnGetVersion ) return WINDOWS_UNKNOWN                       ;
    dwVersion      = fnGetVersion ( )                                        ;
    dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)))                      ;
    dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)))                      ;
    //////////////////////////////////////////////////////////////////////////
    if ( dwVersion < 0x80000000 ) dwBuild = (DWORD)(HIWORD(dwVersion))       ;
    switch ( dwMajorVersion )                                                {
      case 0                                                                 :
      case 1                                                                 :
      case 2                                                                 :
      case 3                                                                 :
      case 4                                                                 :
      case 5                                                                 :
      break                                                                  ;
      case 6                                                                 :
        switch ( dwMinorVersion )                                            {
          case 0                                                             :
            version |= WINDOWS_VISTA_SERVER2008                              ;
          break                                                              ;
          case 1                                                             :
            version |= WINDOWS_7_SERVER2008R2                                ;
          break                                                              ;
          default                                                            :
            version |= WINDOWS_FUTURE                                        ;
          break                                                              ;
        }                                                                    ;
      break                                                                  ;
      default                                                                :
        version |= WINDOWS_FUTURE                                            ;
      break                                                                  ;
    }                                                                        ;
  }                                                                          ;
  return version                                                             ;
}

///////////////////////////////////////////////////////////////////////////////

static BOOL UseWOW64Workaround(void)
{
  return (IsWow64() && (GetWindowsVersion() & WINDOWS_VISTA_SERVER2008)) ;
}

///////////////////////////////////////////////////////////////////////////////

#define _WASAPI_MONO_TO_STEREO_MIXER_1_TO_2(TYPE)\
    TYPE * __restrict to   = (TYPE *)__to;\
    TYPE * __restrict from = (TYPE *)__from;\
    TYPE * __restrict end  = from + count;\
    while (from != end)\
    {\
        *to ++ = *from;\
        *to ++ = *from;\
        ++ from;\
    }

///////////////////////////////////////////////////////////////////////////////

#define _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_FLT32(TYPE)\
    TYPE * __restrict to   = (TYPE *)__to;\
    TYPE * __restrict from = (TYPE *)__from;\
    TYPE * __restrict end  = to + count;\
    while (to != end)\
    {\
        *to ++ = (TYPE)((float)(from[0] + from[1]) * 0.5f);\
        from += 2;\
    }

///////////////////////////////////////////////////////////////////////////////

#define _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_INT32(TYPE)\
    TYPE * __restrict to   = (TYPE *)__to;\
    TYPE * __restrict from = (TYPE *)__from;\
    TYPE * __restrict end  = to + count;\
    while (to != end)\
    {\
        *to ++ = (TYPE)(((INT32)from[0] + (INT32)from[1]) >> 1);\
        from += 2;\
    }

///////////////////////////////////////////////////////////////////////////////

#define _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_INT64(TYPE)\
    TYPE * __restrict to   = (TYPE *)__to;\
    TYPE * __restrict from = (TYPE *)__from;\
    TYPE * __restrict end  = to + count;\
    while (to != end)\
    {\
        *to ++ = (TYPE)(((INT64)from[0] + (INT64)from[1]) >> 1);\
        from += 2;\
    }

///////////////////////////////////////////////////////////////////////////////

#define _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_L(TYPE)\
    TYPE * __restrict to   = (TYPE *)__to;\
    TYPE * __restrict from = (TYPE *)__from;\
    TYPE * __restrict end  = to + count;\
    while (to != end)\
    {\
        *to ++ = from[0];\
        from += 2;\
    }

///////////////////////////////////////////////////////////////////////////////

static void _MixMonoToStereo_1TO2_8(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_1_TO_2(BYTE); }
static void _MixMonoToStereo_1TO2_16(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_1_TO_2(short); }
static void _MixMonoToStereo_1TO2_24(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_1_TO_2(int); /* !!! int24 data is contained in 32-bit containers*/ }
static void _MixMonoToStereo_1TO2_32(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_1_TO_2(int); }
static void _MixMonoToStereo_1TO2_32f(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_1_TO_2(float); }

///////////////////////////////////////////////////////////////////////////////

static void _MixMonoToStereo_2TO1_8(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_INT32(BYTE); }
static void _MixMonoToStereo_2TO1_16(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_INT32(short); }
static void _MixMonoToStereo_2TO1_24(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_INT32(int); /* !!! int24 data is contained in 32-bit containers*/ }
static void _MixMonoToStereo_2TO1_32(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_INT64(int); }
static void _MixMonoToStereo_2TO1_32f(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_FLT32(float); }

///////////////////////////////////////////////////////////////////////////////

static void _MixMonoToStereo_2TO1_8_L(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_L(BYTE); }
static void _MixMonoToStereo_2TO1_16_L(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_L(short); }
static void _MixMonoToStereo_2TO1_24_L(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_L(int); /* !!! int24 data is contained in 32-bit containers*/ }
static void _MixMonoToStereo_2TO1_32_L(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_L(int); }
static void _MixMonoToStereo_2TO1_32f_L(void *__to, void *__from, UINT32 count) { _WASAPI_MONO_TO_STEREO_MIXER_2_TO_1_L(float); }

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

#define LogHostError(HRES) __LogHostError(HRES, __FUNCTION__, __FILE__, __LINE__)
static HRESULT __LogHostError(HRESULT res, const char *func, const char *file, int line)
{
  const char * text = NULL                      ;
  switch ( res )                                {
    case S_OK                                   : return res;
    case E_POINTER                              : text ="E_POINTER"; break;
    case E_INVALIDARG                           : text ="E_INVALIDARG"; break;
    case AUDCLNT_E_NOT_INITIALIZED              : text ="AUDCLNT_E_NOT_INITIALIZED"; break;
    case AUDCLNT_E_ALREADY_INITIALIZED          : text ="AUDCLNT_E_ALREADY_INITIALIZED"; break;
    case AUDCLNT_E_WRONG_ENDPOINT_TYPE          : text ="AUDCLNT_E_WRONG_ENDPOINT_TYPE"; break;
    case AUDCLNT_E_DEVICE_INVALIDATED           : text ="AUDCLNT_E_DEVICE_INVALIDATED"; break;
    case AUDCLNT_E_NOT_STOPPED                  : text ="AUDCLNT_E_NOT_STOPPED"; break;
    case AUDCLNT_E_BUFFER_TOO_LARGE             : text ="AUDCLNT_E_BUFFER_TOO_LARGE"; break;
    case AUDCLNT_E_OUT_OF_ORDER                 : text ="AUDCLNT_E_OUT_OF_ORDER"; break;
    case AUDCLNT_E_UNSUPPORTED_FORMAT           : text ="AUDCLNT_E_UNSUPPORTED_FORMAT"; break;
    case AUDCLNT_E_INVALID_SIZE                 : text ="AUDCLNT_E_INVALID_SIZE"; break;
    case AUDCLNT_E_DEVICE_IN_USE                : text ="AUDCLNT_E_DEVICE_IN_USE"; break;
    case AUDCLNT_E_BUFFER_OPERATION_PENDING     : text ="AUDCLNT_E_BUFFER_OPERATION_PENDING"; break;
    case AUDCLNT_E_THREAD_NOT_REGISTERED        : text ="AUDCLNT_E_THREAD_NOT_REGISTERED"; break;
    case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED   : text ="AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED"; break;
    case AUDCLNT_E_ENDPOINT_CREATE_FAILED       : text ="AUDCLNT_E_ENDPOINT_CREATE_FAILED"; break;
    case AUDCLNT_E_SERVICE_NOT_RUNNING          : text ="AUDCLNT_E_SERVICE_NOT_RUNNING"; break;
    case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED     : text ="AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED"; break;
    case AUDCLNT_E_EXCLUSIVE_MODE_ONLY          : text ="AUDCLNT_E_EXCLUSIVE_MODE_ONLY"; break;
    case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL : text ="AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL"; break;
    case AUDCLNT_E_EVENTHANDLE_NOT_SET          : text ="AUDCLNT_E_EVENTHANDLE_NOT_SET"; break;
    case AUDCLNT_E_INCORRECT_BUFFER_SIZE        : text ="AUDCLNT_E_INCORRECT_BUFFER_SIZE"; break;
    case AUDCLNT_E_BUFFER_SIZE_ERROR            : text ="AUDCLNT_E_BUFFER_SIZE_ERROR"; break;
    case AUDCLNT_E_CPUUSAGE_EXCEEDED            : text ="AUDCLNT_E_CPUUSAGE_EXCEEDED"; break;
    case AUDCLNT_E_BUFFER_ERROR                 : text ="AUDCLNT_E_BUFFER_ERROR"; break;
    case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED      : text ="AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED"; break;
    case AUDCLNT_E_INVALID_DEVICE_PERIOD        : text ="AUDCLNT_E_INVALID_DEVICE_PERIOD"; break;
    case AUDCLNT_S_BUFFER_EMPTY                 : text ="AUDCLNT_S_BUFFER_EMPTY"; break;
    case AUDCLNT_S_THREAD_ALREADY_REGISTERED    : text ="AUDCLNT_S_THREAD_ALREADY_REGISTERED"; break;
    case AUDCLNT_S_POSITION_STALLED             : text ="AUDCLNT_S_POSITION_STALLED"; break;
    case CO_E_NOTINITIALIZED                    : text ="CO_E_NOTINITIALIZED: you must call CoInitialize() before Pa_OpenStream()"; break;
    default                                     : text = "UNKNOWN ERROR";
  }
  gPrintf(("WASAPI ERROR: 0x%X : %s\n [Function entry: %s FILE: %s {LINE: %d}]\n", res, text, func, file, line)) ;
  return res                                    ;
}

///////////////////////////////////////////////////////////////////////////////

#define LogCaError(CAERR) __LogCaError(CAERR, __FUNCTION__, __FILE__, __LINE__)
static CaError __LogCaError(CaError err,const char * func,const char * file,int line)
{
  if ( NoError == err ) return err ;
  gPrintf(("WASAPI ERROR: %i : %s\n [Function entry: %s FILE: %s {LINE: %d}]\n", err, globalDebugger->Error(err), func, file, line)) ;
  return err;
}

///////////////////////////////////////////////////////////////////////////////

static WORD CaSampleFormatToBitsPerSample(CaSampleFormat format_id)
{
  switch (format_id & ~cafNonInterleaved) {
    case cafFloat64 : return 64           ;
    case cafFloat32 :
    case cafInt32   : return 32           ;
    case cafInt24   : return 24           ;
    case cafInt16   : return 16           ;
    case cafInt8    :
    case cafUint8   : return 8            ;
  }                                       ;
  return 0                                ;
}

///////////////////////////////////////////////////////////////////////////////

static MixMonoToStereoF _GetMonoToStereoMixer    (
                           CaSampleFormat format ,
                           EMixerDir      dir    )
{
  switch ( dir )                                             {
    case MIX_DIR__1TO2                                       :
      switch ( format & ~cafNonInterleaved )                 {
        case cafUint8   : return _MixMonoToStereo_1TO2_8     ;
        case cafInt16   : return _MixMonoToStereo_1TO2_16    ;
        case cafInt24   : return _MixMonoToStereo_1TO2_24    ;
        case cafInt32   : return _MixMonoToStereo_1TO2_32    ;
        case cafFloat32 : return _MixMonoToStereo_1TO2_32f   ;
      }                                                      ;
    break                                                    ;
    case MIX_DIR__2TO1                                       :
      switch ( format & ~cafNonInterleaved )                 {
        case cafUint8   : return _MixMonoToStereo_2TO1_8     ;
        case cafInt16   : return _MixMonoToStereo_2TO1_16    ;
        case cafInt24   : return _MixMonoToStereo_2TO1_24    ;
        case cafInt32   : return _MixMonoToStereo_2TO1_32    ;
        case cafFloat32 : return _MixMonoToStereo_2TO1_32f   ;
      }                                                      ;
    break                                                    ;
    case MIX_DIR__2TO1_L                                     :
      switch ( format & ~cafNonInterleaved )                 {
        case cafUint8   : return _MixMonoToStereo_2TO1_8_L   ;
        case cafInt16   : return _MixMonoToStereo_2TO1_16_L  ;
        case cafInt24   : return _MixMonoToStereo_2TO1_24_L  ;
        case cafInt32   : return _MixMonoToStereo_2TO1_32_L  ;
        case cafFloat32 : return _MixMonoToStereo_2TO1_32f_L ;
      }                                                      ;
     break                                                   ;
  }                                                          ;
  return NULL                                                ;
}

//////////////////////////////////////////////////////////////////////////////

static WaSapiHostApi *_GetHostApi(CaError *_error)
{
  CaError   error                        ;
  HostApi * pApi  = NULL                 ;
  ////////////////////////////////////////
  error = Core::GetHostApi(&pApi,WASAPI) ;
  if ( NoError != error)                 {
    if ( _error != NULL )                {
      (*_error) = error                  ;
    }                                    ;
    return NULL                          ;
  }                                      ;
  ////////////////////////////////////////
  return (WaSapiHostApi *)pApi           ;
}

//////////////////////////////////////////////////////////////////////////////

int GetDeviceDefaultFormat ( void        * pFormat     ,
                             unsigned int  nFormatSize ,
                             CaDeviceIndex nDevice     )
{
  CaError         ret                                                        ;
  WaSapiHostApi * paWasapi                                                   ;
  UINT32          size                                                       ;
  CaDeviceIndex   index                                                      ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL == pFormat  ) return BadBufferPtr                                ;
  if ( nFormatSize <= 0 ) return BufferTooSmall                              ;
  paWasapi = _GetHostApi(&ret)                                               ;
  if ( NULL == paWasapi ) return ret                                         ;
  ret = paWasapi -> ToHostDeviceIndex ( &index , nDevice )                   ;
  if ( NoError != ret ) return ret                                           ;
  if ( (UINT32)index >= paWasapi->deviceCount ) return InvalidDevice         ;
  size = min(nFormatSize, (UINT32)sizeof(paWasapi->devInfo[index].DefaultFormat)) ;
  ::memcpy ( pFormat , &paWasapi -> devInfo [index] . DefaultFormat , size ) ;
  ////////////////////////////////////////////////////////////////////////////
  return size                                                                ;
}

//////////////////////////////////////////////////////////////////////////////

int GetDeviceRole ( CaDeviceIndex nDevice )
{
  CaError       ret                                                          ;
  CaDeviceIndex index                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  WaSapiHostApi * caWasapi = _GetHostApi(&ret)                               ;
  if ( NULL == caWasapi ) return NotInitialized                              ;
  ret = caWasapi -> ToHostDeviceIndex ( &index , nDevice )                   ;
  if ( NoError != ret ) return ret                                           ;
  if ( (UINT32)index >= caWasapi->deviceCount ) return InvalidDevice         ;
  ////////////////////////////////////////////////////////////////////////////
  return caWasapi->devInfo[index].formFactor                                 ;
}

//////////////////////////////////////////////////////////////////////////////

static void LogWAVEFORMATEXTENSIBLE(const WAVEFORMATEXTENSIBLE * in)
{
  if ( NULL == globalDebugger ) return                                       ;
  const WAVEFORMATEX * old = (WAVEFORMATEX *)in                              ;
  switch (old->wFormatTag)                                                   {
    case WAVE_FORMAT_EXTENSIBLE                                            : {
      gPrintf(("wFormatTag      = WAVE_FORMAT_EXTENSIBLE\n"))                  ;
      if (IsEqualGUID(in->SubFormat,ca_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))     {
        gPrintf(("SubFormat       = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT\n"))       ;
      } else
      if (IsEqualGUID(in->SubFormat,ca_KSDATAFORMAT_SUBTYPE_PCM))            {
        gPrintf(("SubFormat       = KSDATAFORMAT_SUBTYPE_PCM\n"))              ;
      } else                                                                 {
        gPrintf(("SubFormat       = CUSTOM GUID{%d:%d:%d:%d%d%d%d%d%d%d%d}\n"  ,
                                        in->SubFormat.Data1                  ,
                                        in->SubFormat.Data2                  ,
                                        in->SubFormat.Data3                  ,
                                   (int)in->SubFormat.Data4[0]               ,
                                   (int)in->SubFormat.Data4[1]               ,
                                   (int)in->SubFormat.Data4[2]               ,
                                   (int)in->SubFormat.Data4[3]               ,
                                   (int)in->SubFormat.Data4[4]               ,
                                   (int)in->SubFormat.Data4[5]               ,
                                   (int)in->SubFormat.Data4[6]               ,
                                   (int)in->SubFormat.Data4[7]           ) ) ;
      }                                                                      ;
      gPrintf(("Samples.wValidBitsPerSample =%d\n",  in->Samples.wValidBitsPerSample));
      gPrintf(("dwChannelMask   = 0x%X\n",in->dwChannelMask))                  ;
     break                                                                   ;
    }                                                                        ;
    case WAVE_FORMAT_PCM:        gPrintf(("wFormatTag      = WAVE_FORMAT_PCM\n")); break;
    case WAVE_FORMAT_IEEE_FLOAT: gPrintf(("wFormatTag      = WAVE_FORMAT_IEEE_FLOAT\n")); break;
    default                                                                  :
      gPrintf(("wFormatTag      = UNKNOWN(%d)\n",old->wFormatTag))             ;
    break                                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "nChannels       = %d\n" , old->nChannels       ) )              ;
  gPrintf ( ( "nSamplesPerSec  = %d\n" , old->nSamplesPerSec  ) )              ;
  gPrintf ( ( "nAvgBytesPerSec = %d\n" , old->nAvgBytesPerSec ) )              ;
  gPrintf ( ( "nBlockAlign     = %d\n" , old->nBlockAlign     ) )              ;
  gPrintf ( ( "wBitsPerSample  = %d\n" , old->wBitsPerSample  ) )              ;
  gPrintf ( ( "cbSize          = %d\n" , old->cbSize          ) )              ;
}

//////////////////////////////////////////////////////////////////////////////

static CaSampleFormat WaveToCaFormat(const WAVEFORMATEXTENSIBLE * in)
{
  const WAVEFORMATEX *old = (WAVEFORMATEX *) in                              ;
  switch ( old -> wFormatTag )                                               {
    case WAVE_FORMAT_EXTENSIBLE                                              :
      if ( IsEqualGUID (in->SubFormat,ca_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) )  {
        if ( in -> Samples . wValidBitsPerSample == 32 ) return cafFloat32   ;
      } else
      if ( IsEqualGUID ( in->SubFormat , ca_KSDATAFORMAT_SUBTYPE_PCM ) )     {
        switch ( old -> wBitsPerSample )                                     {
          case 32: return cafInt32                                           ;
          case 24: return cafInt24                                           ;
          case  8: return cafUint8                                           ;
          case 16: return cafInt16                                           ;
        }                                                                    ;
      }                                                                      ;
    break                                                                    ;
    case WAVE_FORMAT_IEEE_FLOAT                                              :
    return cafFloat32                                                        ;
    case WAVE_FORMAT_PCM                                                     :
      switch ( old -> wBitsPerSample )                                       {
        case 32: return cafInt32                                             ;
        case 24: return cafInt24                                             ;
        case  8: return cafUint8                                             ;
        case 16: return cafInt16                                             ;
      }                                                                      ;
    break                                                                    ;
  }                                                                          ;
  return cafCustomFormat                                                     ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError MakeWaveFormatFromParams              (
                 WAVEFORMATEXTENSIBLE   * wavex      ,
                 const StreamParameters * params     ,
                 double                   sampleRate )
{
  WORD                 bitsPerSample                                         ;
  WAVEFORMATEX       * old                                                   ;
  DWORD                channelMask = 0                                       ;
  CaWaSapiStreamInfo * streamInfo  = (CaWaSapiStreamInfo *)params->streamInfo ;
  ////////////////////////////////////////////////////////////////////////////
  // Get user assigned channel mask
  if ( ( NULL != streamInfo                                               ) &&
       ( streamInfo->flags & CaWaSapiUseChannelMask )                     )  {
    channelMask = streamInfo->channelMask                                    ;
  }                                                                          ;
  // Convert PaSampleFormat to bits per sample
  bitsPerSample = CaSampleFormatToBitsPerSample(params->sampleFormat)        ;
  if ( 0 == bitsPerSample ) return SampleFormatNotSupported                  ;
  ////////////////////////////////////////////////////////////////////////////
  ::memset ( wavex , 0 , sizeof(WAVEFORMATEXTENSIBLE) )                      ;
  ////////////////////////////////////////////////////////////////////////////
  old                   = (WAVEFORMATEX *) wavex                             ;
  old -> nChannels      = (WORD          ) params -> channelCount            ;
  old -> nSamplesPerSec = (DWORD         ) sampleRate                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( old -> wBitsPerSample = bitsPerSample ) > 16 )                      {
    old->wBitsPerSample = 32                                                 ;
    // 20 or 24 bits must go in 32 bit containers (ints)
  }                                                                          ;
  old -> nBlockAlign     = ( old->nChannels      * (old->wBitsPerSample/8) ) ;
  old -> nAvgBytesPerSec = ( old->nSamplesPerSec *  old->nBlockAlign       ) ;
  ////////////////////////////////////////////////////////////////////////////
  // WAVEFORMATEX
  if ( ( params->channelCount <= 2                                        ) &&
       ( ( bitsPerSample == 16 ) || ( bitsPerSample == 8 ) )              )  {
    old -> cbSize     = 0                                                    ;
    old -> wFormatTag = WAVE_FORMAT_PCM                                      ;
  } else                                                                     {
    // WAVEFORMATEXTENSIBLE
    old -> wFormatTag = WAVE_FORMAT_EXTENSIBLE                               ;
    old -> cbSize     = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)  ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( params->sampleFormat & ~cafNonInterleaved ) == cafFloat32 )       {
      wavex -> SubFormat = ca_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT                ;
    } else                                                                   {
      wavex -> SubFormat = ca_KSDATAFORMAT_SUBTYPE_PCM                       ;
    }                                                                        ;
    wavex -> Samples . wValidBitsPerSample = bitsPerSample                   ;
    //////////////////////////////////////////////////////////////////////////
    // Set channel mask
    if ( 0 != channelMask )                                                  {
      wavex -> dwChannelMask = channelMask                                   ;
    } else                                                                   {
      switch ( params -> channelCount )                                      {
        case 1:  wavex->dwChannelMask = KSAUDIO_SPEAKER_MONO         ; break ;
        case 2:  wavex->dwChannelMask = KSAUDIO_SPEAKER_STEREO       ; break ;
        case 3:  wavex->dwChannelMask = KSAUDIO_SPEAKER_STEREO               |
                                        SPEAKER_LOW_FREQUENCY        ; break ;
        case 4:  wavex->dwChannelMask = KSAUDIO_SPEAKER_QUAD         ; break ;
        case 5:  wavex->dwChannelMask = KSAUDIO_SPEAKER_QUAD                 |
                                        SPEAKER_LOW_FREQUENCY        ; break ;
        #ifdef KSAUDIO_SPEAKER_5POINT1_SURROUND
        case 6:  wavex->dwChannelMask = KSAUDIO_SPEAKER_5POINT1_SURROUND ; break ;
        #else
        case 6:  wavex->dwChannelMask = KSAUDIO_SPEAKER_5POINT1      ; break ;
        #endif
        #ifdef KSAUDIO_SPEAKER_5POINT1_SURROUND
        case 7:  wavex->dwChannelMask = KSAUDIO_SPEAKER_5POINT1_SURROUND     |
                                        SPEAKER_BACK_CENTER          ; break ;
        #else
        case 7:  wavex->dwChannelMask = KSAUDIO_SPEAKER_5POINT1              |
                                        SPEAKER_BACK_CENTER          ; break ;
        #endif
        #ifdef KSAUDIO_SPEAKER_7POINT1_SURROUND
        case 8:  wavex->dwChannelMask = KSAUDIO_SPEAKER_7POINT1_SURROUND ; break ;
        #else
        case 8:  wavex->dwChannelMask = KSAUDIO_SPEAKER_7POINT1      ; break ;
        #endif
        default: wavex->dwChannelMask = 0                                    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

static CaUint32 _GetFramesPerHostBuffer        (
                  CaUint32 userFramesPerBuffer ,
                  CaTime   suggestedLatency    ,
                  double   sampleRate          ,
                  CaUint32 TimerJitterMs       )
{
  CaUint32 frames = userFramesPerBuffer                            +
                    max( userFramesPerBuffer                       ,
                         (CaUint32)(suggestedLatency*sampleRate) ) ;
  frames += (CaUint32)((sampleRate * 0.001) * TimerJitterMs)       ;
  return frames                                                    ;
}

//////////////////////////////////////////////////////////////////////////////

static void _RecalculateBuffersCount                   (
               CaWaSapiSubStream * sub                 ,
               UINT32              userFramesPerBuffer ,
               UINT32              framesPerLatency    ,
               BOOL                fullDuplex          )
{
  // Count buffers (must be at least 1)
  sub->buffers = (userFramesPerBuffer ? framesPerLatency / userFramesPerBuffer : 0);
  if ( sub->buffers == 0 ) sub->buffers = 1                                  ;
  // Determine amount of buffers used:
  // - Full-duplex mode will lead to period difference, thus only 1.
  // - Input mode, only 1, as WASAPI allows extraction of only 1 packet.
  // - For Shared mode we use double buffering.
  if ( ( sub->shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE) || fullDuplex )      {
    // Exclusive mode does not allow >1 buffers be used for Event interface, e.g. GetBuffer
    // call must acquire max buffer size and it all must be processed.
    if ( sub->streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK)               {
      sub->userBufferAndHostMatch = 1                                        ;
    }                                                                        ;
    // Use paUtilBoundedHostBufferSize because exclusive mode will starve and produce
    // bad quality of audio
    sub->buffers = 1                                                         ;
  }                                                                          ;
}

//////////////////////////////////////////////////////////////////////////////

static void _CalculateAlignedPeriod                  (
               CaWaSapiSubStream * pSub              ,
               UINT32            * nFramesPerLatency ,
               ALIGN_FUNC          pAlignFunc        )
{
  // Align frames to HD Audio packet size of 128 bytes for Exclusive mode only.
  // Not aligning on Windows Vista will cause Event timeout, although Windows 7 will
  // return AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED error to realign buffer. Aligning is necessary
  // for Exclusive mode only! when audio data is feeded directly to hardware.
  if ( pSub->shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE )                      {
    (*nFramesPerLatency) = AlignFramesPerBuffer                              (
                             (*nFramesPerLatency)                            ,
                             pSub->wavex.Format.nSamplesPerSec               ,
                             pSub->wavex.Format.nBlockAlign                  ,
                             pAlignFunc                                    ) ;
  }                                                                          ;
  // Calculate period
  pSub->period = MakeHnsPeriod                                               (
                   (*nFramesPerLatency)                                      ,
                   pSub->wavex.Format.nSamplesPerSec                       ) ;
}

//////////////////////////////////////////////////////////////////////////////

HRESULT UnmarshalSubStreamComPointers(CaWaSapiSubStream * substream)
{
  HRESULT hResult         = S_OK                                             ;
  HRESULT hFirstBadResult = S_OK                                             ;
  ////////////////////////////////////////////////////////////////////////////
  substream -> clientProc   = NULL                                           ;
  // IAudioClient
  hResult = CoGetInterfaceAndReleaseStream                                   (
              substream->clientStream                                        ,
              ca_IID_IAudioClient                                            ,
              (LPVOID*)&substream->clientProc                              ) ;
  substream -> clientStream = NULL                                           ;
  if ( S_OK != hResult )                                                     {
    hFirstBadResult = (S_OK == hFirstBadResult) ? hResult : hFirstBadResult  ;
  }                                                                          ;
  return hFirstBadResult                                                     ;
}

//////////////////////////////////////////////////////////////////////////////

void ReleaseUnmarshaledSubComPointers(CaWaSapiSubStream * substream)
{
  SAFE_RELEASE ( substream -> clientProc ) ;
}

//////////////////////////////////////////////////////////////////////////////

HRESULT MarshalSubStreamComPointers(CaWaSapiSubStream * substream)
{
  HRESULT hResult                                                            ;
  substream -> clientStream = NULL                                           ;
  // IAudioClient
  hResult = CoMarshalInterThreadInterfaceInStream                            (
              ca_IID_IAudioClient                                            ,
              (LPUNKNOWN)substream->clientParent                             ,
              &substream->clientStream                                     ) ;
  if ( S_OK != hResult ) goto marshal_sub_error                              ;
  return hResult                                                             ;
  // If marshaling error occurred, make sure to release everything.
marshal_sub_error:
  UnmarshalSubStreamComPointers    ( substream )                             ;
  ReleaseUnmarshaledSubComPointers ( substream )                             ;
  return hResult                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

HANDLE MMCSS_activate(const char * name)
{
  DWORD  task_idx = 0                                                        ;
  HANDLE hTask    = pAvSetMmThreadCharacteristics ( name , &task_idx )       ;
  if ( NULL == hTask )                                                       {
    gPrintf(("WASAPI : AvSetMmThreadCharacteristics failed!\n"))             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  int   cur_priority       = GetThreadPriority ( GetCurrentThread  ( ) )     ;
  DWORD cur_priority_class = GetPriorityClass  ( GetCurrentProcess ( ) )     ;
  gPrintf(("WASAPI : thread [ priority-0x%X class-0x%X ]\n", cur_priority, cur_priority_class));
  return hTask                                                               ;
}

//////////////////////////////////////////////////////////////////////////////

void MMCSS_deactivate(HANDLE hTask)
{
  if ( ! hTask ) return                                             ;
  if ( FALSE == pAvRevertMmThreadCharacteristics(hTask) )           {
    gPrintf ( ("WASAPI : AvRevertMmThreadCharacteristics failed!\n") ) ;
  }                                                                 ;
}

//////////////////////////////////////////////////////////////////////////////

static CaWaSapiJackConnectionType ConvertJackConnectionTypeWASAPIToPA(int connType)
{
  switch ( connType )                                                        {
    case eConnTypeUnknown               : return eJackConnTypeUnknown;
    #ifdef _KS_
    case eConnType3Point5mm             : return eJackConnType3Point5mm;
    #else
    case eConnTypeEighth                : return eJackConnType3Point5mm;
    #endif
    case eConnTypeQuarter               : return eJackConnTypeQuarter;
    case eConnTypeAtapiInternal         : return eJackConnTypeAtapiInternal;
    case eConnTypeRCA                   : return eJackConnTypeRCA;
    case eConnTypeOptical               : return eJackConnTypeOptical;
    case eConnTypeOtherDigital          : return eJackConnTypeOtherDigital;
    case eConnTypeOtherAnalog           : return eJackConnTypeOtherAnalog;
    case eConnTypeMultichannelAnalogDIN : return eJackConnTypeMultichannelAnalogDIN;
    case eConnTypeXlrProfessional       : return eJackConnTypeXlrProfessional;
    case eConnTypeRJ11Modem             : return eJackConnTypeRJ11Modem;
    case eConnTypeCombination           : return eJackConnTypeCombination;
  }
  return eJackConnTypeUnknown                                                ;
}

//////////////////////////////////////////////////////////////////////////////

static CaWaSapiJackGeoLocation ConvertJackGeoLocationWASAPIToPA(int geoLoc)
{
  switch ( geoLoc )                                                          {
    case eGeoLocRear             : return eJackGeoLocRear                    ;
    case eGeoLocFront            : return eJackGeoLocFront                   ;
    case eGeoLocLeft             : return eJackGeoLocLeft                    ;
    case eGeoLocRight            : return eJackGeoLocRight                   ;
    case eGeoLocTop              : return eJackGeoLocTop                     ;
    case eGeoLocBottom           : return eJackGeoLocBottom                  ;
    #ifdef _KS_
    case eGeoLocRearPanel        : return eJackGeoLocRearPanel               ;
    #else
    case eGeoLocRearOPanel       :  return eJackGeoLocRearPanel              ;
    #endif
    case eGeoLocRiser            : return eJackGeoLocRiser                   ;
    case eGeoLocInsideMobileLid  : return eJackGeoLocInsideMobileLid         ;
    case eGeoLocDrivebay         : return eJackGeoLocDrivebay                ;
    case eGeoLocHDMI             : return eJackGeoLocHDMI                    ;
    case eGeoLocOutsideMobileLid : return eJackGeoLocOutsideMobileLid        ;
    case eGeoLocATAPI            : return eJackGeoLocATAPI                   ;
  }                                                                          ;
  return eJackGeoLocUnk                                                      ;
}

//////////////////////////////////////////////////////////////////////////////

static CaWaSapiJackGenLocation ConvertJackGenLocationWASAPIToPA(int genLoc)
{
  switch ( genLoc )                                                          {
    case eGenLocPrimaryBox : return eJackGenLocPrimaryBox                    ;
    case eGenLocInternal   : return eJackGenLocInternal                      ;
    #ifdef _KS_
    case eGenLocSeparate   : return eJackGenLocSeparate                      ;
    #else
    case eGenLocSeperate   : return eJackGenLocSeparate                      ;
    #endif
    case eGenLocOther      : return eJackGenLocOther                         ;
  }                                                                          ;
  return eJackGenLocPrimaryBox                                               ;
}

//////////////////////////////////////////////////////////////////////////////

static CaWaSapiJackPortConnection ConvertJackPortConnectionWASAPIToPA(int portConn)
{
  switch (portConn)                                                          {
    case ePortConnJack                  : return eJackPortConnJack           ;
    case ePortConnIntegratedDevice      : return eJackPortConnIntegratedDevice;
    case ePortConnBothIntegratedAndJack : return eJackPortConnBothIntegratedAndJack;
    case ePortConnUnknown               : return eJackPortConnUnknown        ;
  }                                                                          ;
  return eJackPortConnJack                                                   ;
}

//////////////////////////////////////////////////////////////////////////////

CaError GetJackCount ( CaDeviceIndex nDevice , int * jcount )
{
  CaError              ret             = NoError                             ;
  HRESULT              hr              = S_OK                                ;
  CaDeviceIndex        index           = 0                                   ;
  IDeviceTopology    * pDeviceTopology = NULL                                ;
  IConnector         * pConnFrom       = NULL                                ;
  IConnector         * pConnTo         = NULL                                ;
  IPart              * pPart           = NULL                                ;
  IKsJackDescription * pJackDesc       = NULL                                ;
  UINT                 jackCount       = 0                                   ;
  WaSapiHostApi      * paWasapi        = _GetHostApi ( &ret )                ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL    == paWasapi ) return NotInitialized                           ;
  ret = paWasapi -> ToHostDeviceIndex ( &index , nDevice )                   ;
  if ( NoError != ret      ) return ret                                      ;
  if ( ((UINT32)index) >= paWasapi -> deviceCount ) return InvalidDevice     ;
  // Get the endpoint device's IDeviceTopology interface.
  hr = paWasapi -> devInfo [ index ] . device -> Activate                    (
         ca_IID_IDeviceTopology                                              ,
         CLSCTX_INPROC_SERVER                                                ,
         NULL                                                                ,
         (void**)&pDeviceTopology                                          ) ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  // The device topology for an endpoint device always contains just one connector (connector number 0).
  hr = pDeviceTopology->GetConnector ( 0 , &pConnFrom )                      ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  // Step across the connection to the jack on the adapter.
  hr = pConnFrom -> GetConnectedTo ( &pConnTo )                              ;
  if ( HRESULT_FROM_WIN32 ( ERROR_PATH_NOT_FOUND ) == hr )                   {
    // The adapter device is not currently active.
    hr = E_NOINTERFACE                                                       ;
  }                                                                          ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  // Get the connector's IPart interface.
  hr = pConnTo -> QueryInterface ( ca_IID_IPart, (void**)&pPart )            ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  // Activate the connector's IKsJackDescription interface.
  hr = pPart -> Activate                                                     (
         CLSCTX_INPROC_SERVER                                                ,
         ca_IID_IKsJackDescription                                           ,
         (void**)&pJackDesc                                                ) ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  // Return jack count for this device.
  hr = pJackDesc -> GetJackCount ( &jackCount )                              ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  ////////////////////////////////////////////////////////////////////////////
  (*jcount) = jackCount                                                      ;
  ret = NoError                                                              ;
  ////////////////////////////////////////////////////////////////////////////
error:
  SAFE_RELEASE ( pDeviceTopology )                                           ;
  SAFE_RELEASE ( pConnFrom       )                                           ;
  SAFE_RELEASE ( pConnTo         )                                           ;
  SAFE_RELEASE ( pPart           )                                           ;
  SAFE_RELEASE ( pJackDesc       )                                           ;
  LogHostError ( hr              )                                           ;
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

CaError GetJackDescription                           (
          CaDeviceIndex             nDevice          ,
          int                       jindex           ,
          CaWaSapiJackDescription * pJackDescription )
{
  CaError              ret             = NoError                             ;
  HRESULT              hr              = S_OK                                ;
  CaDeviceIndex        index           = 0                                   ;
  IDeviceTopology    * pDeviceTopology = NULL                                ;
  IConnector         * pConnFrom       = NULL                                ;
  IConnector         * pConnTo         = NULL                                ;
  IPart              * pPart           = NULL                                ;
  IKsJackDescription * pJackDesc       = NULL                                ;
  WaSapiHostApi      * paWasapi        = _GetHostApi ( &ret )                ;
  KSJACK_DESCRIPTION   jack = { 0 }                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL == paWasapi ) return NotInitialized                              ;
  ret = paWasapi -> ToHostDeviceIndex ( &index , nDevice )                   ;
  if ( NoError != ret ) return ret                                           ;
  if ( (UINT32)index >= paWasapi->deviceCount ) return InvalidDevice         ;
  // Get the endpoint device's IDeviceTopology interface.
  hr = paWasapi -> devInfo [ index ] . device -> Activate                    (
         ca_IID_IDeviceTopology                                              ,
         CLSCTX_INPROC_SERVER                                                ,
         NULL                                                                ,
         (void**)&pDeviceTopology                                          ) ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  ////////////////////////////////////////////////////////////////////////////
  // The device topology for an endpoint device always contains just one connector (connector number 0).
  hr = pDeviceTopology -> GetConnector ( 0 , &pConnFrom )                    ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  // Step across the connection to the jack on the adapter.
  hr = pConnFrom -> GetConnectedTo ( &pConnTo )                              ;
  if ( HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) == hr )                      {
    // The adapter device is not currently active.
    hr = E_NOINTERFACE                                                       ;
  }                                                                          ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  // Get the connector's IPart interface.
  hr = pConnTo -> QueryInterface ( ca_IID_IPart , (void **) &pPart )         ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  // Activate the connector's IKsJackDescription interface.
  hr = pPart -> Activate                                                     (
         CLSCTX_INPROC_SERVER                                                ,
         ca_IID_IKsJackDescription                                           ,
         (void**)&pJackDesc                                                ) ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  // Test to return jack description struct for index 0.
  hr = pJackDesc -> GetJackDescription ( jindex , &jack )                    ;
  IF_FAILED_JUMP ( hr , error )                                              ;
  ////////////////////////////////////////////////////////////////////////////
  pJackDescription -> channelMapping = jack.ChannelMapping                   ;
  pJackDescription -> color          = jack.Color                            ;
  pJackDescription -> connectionType = ConvertJackConnectionTypeWASAPIToPA(jack.ConnectionType) ;
  pJackDescription -> genLocation    = ConvertJackGenLocationWASAPIToPA   (jack.GenLocation   ) ;
  pJackDescription -> geoLocation    = ConvertJackGeoLocationWASAPIToPA   (jack.GeoLocation   ) ;
  pJackDescription -> isConnected    = jack.IsConnected                      ;
  pJackDescription -> portConnection = ConvertJackPortConnectionWASAPIToPA(jack.PortConnection) ;
  ////////////////////////////////////////////////////////////////////////////
  ret = NoError                                                              ;
error:
  SAFE_RELEASE ( pDeviceTopology )                                           ;
  SAFE_RELEASE ( pConnFrom       )                                           ;
  SAFE_RELEASE ( pConnTo         )                                           ;
  SAFE_RELEASE ( pPart           )                                           ;
  SAFE_RELEASE ( pJackDesc       )                                           ;
  LogHostError ( hr              )                                           ;
  return ret                                                                 ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError GetClosestFormat                      (
                 IAudioClient           * myClient   ,
                 double                   sampleRate ,
                 const StreamParameters * _params    ,
                 AUDCLNT_SHAREMODE        shareMode  ,
                 WAVEFORMATEXTENSIBLE   * outWavex   ,
                 BOOL                     output     )
{
  CaError          answer             = InvalidSampleRate                    ;
  WAVEFORMATEX   * sharedClosestMatch = NULL                                 ;
  HRESULT hr                          = !S_OK                                ;
  StreamParameters params             = (*_params)                           ;
  if ( NULL == myClient ) return BadBufferPtr                                ;
  ////////////////////////////////////////////////////////////////////////////
  /* It was not noticed that 24-bit Input producing no output while device
   * accepts this format. To fix this issue let's ask for 32-bits and let
   * converters convert host 32-bit data to 24-bit for user-space. The bug
   * concerns Vista, if Windows 7 supports 24-bits for Input please report
   * to CIOS Audio Core developers to exclude Windows 7.                    */
  /* if ((params.sampleFormat == paInt24) && (output == FALSE))
       params.sampleFormat = paFloat32;*/ // <<< The silence was due to missing Int32_To_Int24_Dither implementation
  MakeWaveFormatFromParams ( outWavex , &params , sampleRate )               ;
  ////////////////////////////////////////////////////////////////////////////
  hr = myClient->IsFormatSupported                                           (
         shareMode                                                           ,
         &outWavex->Format                                                   ,
         (shareMode==AUDCLNT_SHAREMODE_SHARED ? &sharedClosestMatch : NULL)) ;
  if ( S_OK == hr ) return NoError                                           ;
  if ( NULL != sharedClosestMatch )                                          {
    WORD                   bitsPerSample                                     ;
    WAVEFORMATEXTENSIBLE * ext = (WAVEFORMATEXTENSIBLE *)sharedClosestMatch  ;
    GUID                   subf_guid                                         ;
    subf_guid = GUID_NULL                                                    ;
    if ( sharedClosestMatch -> wFormatTag == WAVE_FORMAT_EXTENSIBLE )        {
      ::memcpy ( outWavex, sharedClosestMatch, sizeof(WAVEFORMATEXTENSIBLE)) ;
      subf_guid = ext->SubFormat                                             ;
    } else                                                                   {
      memcpy(outWavex, sharedClosestMatch, sizeof(WAVEFORMATEX))             ;
    }                                                                        ;
    CoTaskMemFree ( sharedClosestMatch )                                     ;
    answer = NoError                                                         ;
    // Validate SampleRate
    if ( ((DWORD)sampleRate) != outWavex->Format.nSamplesPerSec)             {
      answer = InvalidSampleRate                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    // Validate Channel count
    if ( NoError == answer )                                                 {
      if ( (WORD)params.channelCount != outWavex->Format.nChannels)          {
        // If mono, then driver does not support 1 channel, we use internal workaround
        // of tiny software mixing functionality, e.g. we provide to user buffer 1 channel
        // but then mix into 2 for device buffer
        if ( ( params.channelCount        == 1 )                            &&
             ( outWavex->Format.nChannels == 2 )                           ) {
          return NoError                                                     ;
        } else                                                               {
          answer = InvalidChannelCount                                       ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    // Validate Sample format
    if ( NoError == answer )                                                 {
      bitsPerSample = CaSampleFormatToBitsPerSample ( params.sampleFormat )  ;
      if ( 0 == bitsPerSample ) answer = SampleFormatNotSupported            ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    // Validate Sample format: bit size (WASAPI does not limit 'bit size')
    // if (bitsPerSample != outWavex->Format.wBitsPerSample)
    // return paSampleFormatNotSupported;

    // Validate Sample format: CaFloat32 (WASAPI does not limit 'bit type')
    // if ((params->sampleFormat == paFloat32) && (subf_guid != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
    // return paSampleFormatNotSupported;

    // Validate Sample format: CaInt32 (WASAPI does not limit 'bit type')
    // if ((params->sampleFormat == paInt32) && (subf_guid != KSDATAFORMAT_SUBTYPE_PCM))
    // return paSampleFormatNotSupported;
    if ( NoError == answer ) return NoError                                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  static const int BestToWorst[] = { cafFloat32                              ,
                                     cafInt32                                ,
                                     cafInt24                                ,
                                     cafInt16                                ,
                                     cafUint8                                ,
                                     cafInt8                               } ;
  int              i                                                         ;
  // Try combination stereo and we will use built-in mono-stereo mixer then
  if ( 1 == params.channelCount )                                            {
    WAVEFORMATEXTENSIBLE stereo = { 0 }                                      ;
    StreamParameters stereo_params = params                                  ;
    stereo_params . channelCount = 2                                         ;
    //////////////////////////////////////////////////////////////////////////
    MakeWaveFormatFromParams ( &stereo , &stereo_params , sampleRate )       ;
    hr = myClient -> IsFormatSupported                                       (
           shareMode                                                         ,
           &stereo.Format                                                    ,
           ( ( shareMode == AUDCLNT_SHAREMODE_SHARED )                       ?
             &sharedClosestMatch : NULL)                                   ) ;
    if ( S_OK == hr )                                                        {
      ::memcpy ( outWavex , &stereo , sizeof(WAVEFORMATEXTENSIBLE) )         ;
      CoTaskMemFree ( sharedClosestMatch )                                   ;
      return NoError                                                         ;
    }                                                                        ;
    // Try selecting suitable sample type
    for ( i = 0 ; i < STATIC_ARRAY_SIZE(BestToWorst) ; ++i )                 {
      WAVEFORMATEXTENSIBLE sample = { 0 }                                    ;
      StreamParameters     sample_params = stereo_params                     ;
      sample_params.sampleFormat = (CaSampleFormat) BestToWorst [ i ]        ;
      MakeWaveFormatFromParams ( &sample , &sample_params , sampleRate)      ;
      hr = myClient -> IsFormatSupported                                     (
             shareMode                                                       ,
             &sample . Format                                                ,
             ( ( shareMode == AUDCLNT_SHAREMODE_SHARED )                     ?
               &sharedClosestMatch : NULL                                ) ) ;
      if ( S_OK == hr )                                                      {
        ::memcpy ( outWavex , &sample , sizeof(WAVEFORMATEXTENSIBLE) )       ;
        CoTaskMemFree ( sharedClosestMatch )                                 ;
        return NoError                                                       ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  // Try selecting suitable sample type
  for ( i = 0 ; i < STATIC_ARRAY_SIZE(BestToWorst) ; ++i)                    {
    WAVEFORMATEXTENSIBLE spfmt = { 0 }                                       ;
    StreamParameters     spfmt_params = params                               ;
    spfmt_params.sampleFormat = (CaSampleFormat) BestToWorst [ i ]           ;
    MakeWaveFormatFromParams ( &spfmt , &spfmt_params , sampleRate )         ;
    hr = myClient -> IsFormatSupported                                       (
           shareMode                                                         ,
           &spfmt.Format                                                     ,
           ( ( shareMode == AUDCLNT_SHAREMODE_SHARED )                       ?
             &sharedClosestMatch : NULL )                                  ) ;
    if ( S_OK == hr )                                                        {
      ::memcpy ( outWavex , &spfmt , sizeof(WAVEFORMATEXTENSIBLE) )          ;
      CoTaskMemFree ( sharedClosestMatch )                                   ;
      answer = NoError                                                       ;
      break                                                                  ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  LogHostError ( hr )                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  return answer                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

static void WaSapiHostProcessingLoop (
              void * inputBuffer     ,
              long   inputFrames     ,
              void * outputBuffer    ,
              long   outputFrames    ,
              void * userData        )
{
  WaSapiStream * stream = (WaSapiStream *)userData                           ;
  CaTime         inputBufferAdcTime                                          ;
  CaTime         currentTime                                                 ;
  CaTime         outputBufferDacTime                                         ;
  CaStreamFlags  flags = 0                                                   ;
  int            callbackResult                                              ;
  unsigned long  framesProcessed                                             ;
  HRESULT        hr                                                          ;
  UINT32         pending                                                     ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> cpuLoadMeasurer . Begin ( )                                      ;
  currentTime = stream -> GetTime ( )                                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != stream -> in . clientProc )                                   {
    CaTime pending_time                                                      ;
    hr = stream -> in . clientProc -> GetCurrentPadding ( &pending )         ;
    if ( S_OK == hr )                                                        {
      pending_time = (CaTime) pending                                        /
                     (CaTime) stream -> in . wavex . Format . nSamplesPerSec ;
    } else                                                                   {
      pending_time = (CaTime) stream -> in . latencySeconds                  ;
    }                                                                        ;
    inputBufferAdcTime = currentTime + pending_time                          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != stream -> out . clientProc )                                  {
    CaTime pending_time                                                      ;
    hr = stream -> out . clientProc -> GetCurrentPadding ( &pending )        ;
    if ( S_OK == hr )                                                        {
      pending_time = (CaTime) pending                                        /
                     (CaTime) stream -> out . wavex . Format . nSamplesPerSec;
    } else                                                                   {
      pending_time = (CaTime) stream -> out . latencySeconds                 ;
    }                                                                        ;
    outputBufferDacTime = currentTime + pending_time                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> bufferProcessor . Begin ( stream -> conduit )                    ;
  if ( stream->bufferProcessor.inputChannelCount > 0 )                       {
    stream -> conduit -> Input . AdcDac      = inputBufferAdcTime            ;
    stream -> conduit -> Input . CurrentTime = currentTime                   ;
    stream -> conduit -> Input . StatusFlags = flags                         ;
    if ( stream->doResample && ( NULL != stream->Resampler ) )               {
      #if defined(FFMPEGLIB)
      CaResampler * resampler = (CaResampler *)stream->Resampler             ;
      int           frameSize                                                ;
      frameSize  = inputFrames                                               ;
      frameSize *= stream->conduit->Input . BytesPerSample                   ;
      if ( frameSize > 0 )                                                   {
        ::memcpy ( resampler -> inputBuffer , inputBuffer , frameSize )      ;
      }                                                                      ;
      frameSize  = resampler -> Convert ( inputFrames )                      ;
      stream -> bufferProcessor . setInputFrameCount(0,frameSize)            ;
      stream -> bufferProcessor . setInterleavedInputChannels                (
                  0                                                          ,
                  0                                                          ,
                  resampler->outputBuffer                                    ,
                  0                                                        ) ;
      #else
      stream -> bufferProcessor . setInputFrameCount ( 0 , inputFrames )     ;
      stream -> bufferProcessor . setInterleavedInputChannels(0,0,inputBuffer,0) ;
      #endif
    } else                                                                   {
      stream -> bufferProcessor . setInputFrameCount ( 0 , inputFrames )     ;
      stream -> bufferProcessor . setInterleavedInputChannels(0,0,inputBuffer,0) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( stream->bufferProcessor.outputChannelCount > 0 )                      {
    stream -> conduit -> Output . AdcDac      = outputBufferDacTime          ;
    stream -> conduit -> Output . CurrentTime = currentTime                  ;
    stream -> conduit -> Output . StatusFlags = flags                        ;
    stream -> bufferProcessor . setOutputFrameCount(0,outputFrames)          ;
    if ( stream->doResample && ( NULL != stream->Resampler ) )               {
      #if defined(FFMPEGLIB)
      CaResampler * resampler = (CaResampler *)stream->Resampler             ;
      int           frameSize                                                ;
      stream -> bufferProcessor . setInterleavedOutputChannels               (
                  0                                                          ,
                  0                                                          ,
                  resampler->inputBuffer                                     ,
                  0                                                        ) ;
      frameSize  = resampler -> Convert ( outputFrames )                     ;
      frameSize *= stream->conduit->Output . BytesPerSample                  ;
      if ( frameSize > 0 )                                                   {
        ::memcpy ( outputBuffer , resampler -> outputBuffer , frameSize )    ;
      }                                                                      ;
      #else
      stream -> bufferProcessor . setInterleavedOutputChannels(0,0,outputBuffer,0) ;
      #endif
    } else                                                                   {
      stream -> bufferProcessor . setInterleavedOutputChannels(0,0,outputBuffer,0) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  callbackResult  = Conduit::Continue                                        ;
  framesProcessed = stream -> bufferProcessor . End ( &callbackResult )      ;
  stream -> cpuLoadMeasurer . End ( framesProcessed )                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( Conduit::Continue == callbackResult )                                 {
    /* nothing special to do                                                */
  } else
  if ( Conduit::Postpone == callbackResult )                                 {
  } else
  if ( Conduit::Abort    == callbackResult )                                 {
    // stop stream
    SetEvent ( stream -> hCloseRequest )                                     ;
  } else                                                                     {
    // stop stream
    SetEvent ( stream -> hCloseRequest )                                     ;
  }                                                                          ;
}

//////////////////////////////////////////////////////////////////////////////

CaError ThreadPriorityBoost(void ** hTask,CaWaSapiThreadPriority nPriorityClass)
{
  static const char * mmcs_name[] =                                          {
    NULL                                                                     ,
    "Audio"                                                                  ,
    "Capture"                                                                ,
    "Distribution"                                                           ,
    "Games"                                                                  ,
    "Playback"                                                               ,
    "Pro Audio"                                                              ,
    "Window Manager"                                                       } ;
  HANDLE task                                                                ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL == hTask ) return UnanticipatedHostError                         ;
  if ( (UINT32)nPriorityClass >= STATIC_ARRAY_SIZE(mmcs_name))               {
    return UnanticipatedHostError                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  task = MMCSS_activate ( mmcs_name [ nPriorityClass ] )                     ;
  if ( NULL == task ) return UnanticipatedHostError                          ;
  (*hTask) = task                                                            ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

CaError ThreadPriorityRevert(void * hTask)
{
  if ( NULL == hTask ) return UnanticipatedHostError ;
  MMCSS_deactivate ( (HANDLE) hTask )                ;
  return NoError                                     ;
}

//////////////////////////////////////////////////////////////////////////////

CA_THREAD_FUNC ProcThreadEvent(void * param)
{
  WaSapiStream * stream                = (WaSapiStream *)param               ;
  HRESULT        hr                    = 0                                   ;
  DWORD          dwResult              = 0                                   ;
  BOOL           set_event[S_COUNT]    = { FALSE , FALSE }                   ;
  BOOL           bWaitAllEvents        = FALSE                               ;
  BOOL           bThreadComInitialized = FALSE                               ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "Windows Audio Session API Thread Event started\n" ) )         ;
  /* If COM is already initialized CoInitialize will either return
     FALSE, or RPC_E_CHANGED_MODE if it was initialized in a different
     threading mode. In either case we shouldn't consider it an error
     but we need to be careful to not call CoUninitialize() if
     RPC_E_CHANGED_MODE was returned.                                       */
  hr = CoInitializeEx ( NULL , COINIT_APARTMENTTHREADED)                     ;
  if ( FAILED(hr) && ( hr != RPC_E_CHANGED_MODE ) )                          {
    gPrintf ( ( "WASAPI : failed ProcThreadEvent CoInitialize" ) )           ;
    return UnanticipatedHostError                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( RPC_E_CHANGED_MODE != hr ) bThreadComInitialized = TRUE               ;
  // Unmarshal stream pointers for safe COM operation
  hr = stream -> UnmarshalStreamComPointers ( )                              ;
  if ( S_OK != hr )                                                          {
    gPrintf(("Error unmarshaling stream COM pointers. HRESULT: %i\n", hr))     ;
    goto thread_end                                                          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Waiting on all events in case of Full-Duplex/Exclusive mode.
  if ((stream->in.clientProc != NULL) && (stream->out.clientProc != NULL))   {
    bWaitAllEvents = (stream->in.shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE ) &&
                     (stream->out.shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE)  ;
  }                                                                          ;
  // Boost thread priority
  ThreadPriorityBoost ( (void **) &stream->hAvTask,stream->nThreadPriority ) ;
  ////////////////////////////////////////////////////////////////////////////
  // Create events
  if ( stream -> event [ S_OUTPUT ] == NULL )                                {
    stream->event [S_OUTPUT] = CreateEvent ( NULL , FALSE , FALSE , NULL )   ;
    set_event     [S_OUTPUT] = TRUE                                          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( stream -> event [ S_INPUT  ] == NULL )                                {
    stream->event [S_INPUT ] = CreateEvent ( NULL , FALSE , FALSE , NULL )   ;
    set_event     [S_INPUT ] = TRUE                                          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ((stream->event[S_OUTPUT] == NULL) || (stream->event[S_INPUT] == NULL)) {
    gPrintf(("WASAPI Thread: failed creating Input/Output event handle\n"))    ;
    goto thread_error                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Initialize event & start INPUT stream
  if ( NULL != stream->in.clientProc)                                        {
    // Create & set handle
    if ( set_event[S_INPUT] )                                                {
      hr = stream->in.clientProc->SetEventHandle(stream->event[S_INPUT])     ;
      if ( S_OK != hr )                                                      {
        LogHostError ( hr )                                                  ;
        goto thread_error                                                    ;
      }                                                                      ;
    }                                                                        ;
    // Start
    hr = stream -> in . clientProc -> Start ( )                              ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      goto thread_error                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Initialize event & start OUTPUT stream
  if ( NULL != stream->out.clientProc )                                      {
    // Create & set handle
    if ( set_event[S_OUTPUT] )                                               {
      hr = stream->out.clientProc->SetEventHandle(stream->event[S_OUTPUT])   ;
      if ( S_OK != hr )                                                      {
        LogHostError ( hr )                                                  ;
        goto thread_error                                                    ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    // Preload buffer before start
    hr = stream -> ProcessOutputBuffer ( stream -> out . framesPerBuffer )   ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      goto thread_error                                                      ;
    }                                                                        ;
    // Start
    hr = stream->out.clientProc->Start()                                     ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      goto thread_error                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Signal: stream running
  stream -> running = TRUE                                                   ;
  // Notify: thread started
  SetEvent ( stream -> hThreadStart )                                        ;
  ////////////////////////////////////////////////////////////////////////////
    // Processing Loop
  for ( ; ; )                                                                {
    // 10 sec timeout (on timeout stream will auto-stop when processed by WAIT_TIMEOUT case)
    dwResult = ::WaitForMultipleObjects                                      (
                   S_COUNT                                                   ,
                   stream->event                                             ,
                   bWaitAllEvents                                            ,
                   10 * 1000                                               ) ;
    // Check for close event (after wait for buffers to avoid any calls to user
    // callback when hCloseRequest was set)
    if ( WAIT_TIMEOUT != ::WaitForSingleObject(stream->hCloseRequest,0) )    {
      break                                                                  ;
    }                                                                        ;
    // Process S_INPUT/S_OUTPUT
    switch ( dwResult )                                                      {
      case WAIT_TIMEOUT                                                      :
        gPrintf(("WASAPI Thread: WAIT_TIMEOUT - probably bad audio driver or Vista x64 bug: use paWinWasapiPolling instead\n"));
        goto thread_end                                                      ;
      break                                                                  ;
      // Input stream
      case WAIT_OBJECT_0 + S_INPUT                                           :
        if ( NULL == stream->captureClient ) break                           ;
        hr = stream -> ProcessInputBuffer ( )                                ;
        if ( S_OK != hr )                                                    {
          LogHostError ( hr )                                                ;
          goto thread_error                                                  ;
        }                                                                    ;
      break                                                                  ;
      // Output stream
      case WAIT_OBJECT_0 + S_OUTPUT                                          :
        if ( NULL == stream->renderClient ) break                            ;
        hr = stream -> ProcessOutputBuffer ( stream->out.framesPerBuffer )   ;
        if ( S_OK != hr )                                                    {
          LogHostError ( hr )                                                ;
          goto thread_error                                                  ;
        }                                                                    ;
      break                                                                  ;
    }                                                                        ;
  }                                                                          ;
thread_end                                                                   :
  stream -> StreamOnStop ( )                                                 ;
  stream -> ReleaseUnmarshaledComPointers ( )                                ;
  if ( TRUE == bThreadComInitialized ) CoUninitialize ( )                    ;
  stream -> running = FALSE                                                  ;
  SetEvent ( stream -> hThreadExit )                                         ;
  return 0                                                                   ;
thread_error:
  SetEvent ( stream -> hThreadStart )                                        ;
  goto thread_end                                                            ;
  return 0                                                                   ;
}

//////////////////////////////////////////////////////////////////////////////

CA_THREAD_FUNC ProcThreadPoll(void * param)
{
  HRESULT             hr                    = 0                              ;
  WaSapiStream      * stream                = (WaSapiStream *) param         ;
  INT32               i                     = 0                              ;
  DWORD               sleep_ms              = 0                              ;
  DWORD               sleep_ms_in           = 0                              ;
  DWORD               sleep_ms_out          = 0                              ;
  BOOL                bThreadComInitialized = FALSE                          ;
  ThreadIdleScheduler scheduler                                              ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "Windows Audio Session API Thread Poll started\n" ) )          ;
  // Calculate the actual duration of the allocated buffer.
  /* If COM is already initialized CoInitialize will either return
     FALSE, or RPC_E_CHANGED_MODE if it was initialized in a different
     threading mode. In either case we shouldn't consider it an error
     but we need to be careful to not call CoUninitialize() if
     RPC_E_CHANGED_MODE was returned.                                       */
  hr = CoInitializeEx ( NULL , COINIT_APARTMENTTHREADED )                    ;
  if ( FAILED(hr) && ( hr != RPC_E_CHANGED_MODE ) )                          {
    gPrintf ( ( "WASAPI : failed ProcThreadPoll CoInitialize" ) )            ;
    return UnanticipatedHostError                                            ;
  }                                                                          ;
  if ( RPC_E_CHANGED_MODE != hr ) bThreadComInitialized = TRUE               ;
  // Unmarshal stream pointers for safe COM operation
  hr = stream -> UnmarshalStreamComPointers ( )                              ;
  if ( S_OK != hr )                                                          {
    gPrintf(("Error unmarshaling stream COM pointers. HRESULT: %i\n", hr))   ;
    return 0                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Calculate timeout for next polling attempt.
  sleep_ms_in  = GetFramesSleepTime                                          (
                   stream->in.framesPerHostCallback                          /
                   WASAPI_PACKETS_PER_INPUT_BUFFER                           ,
                   stream->in.wavex.Format.nSamplesPerSec                  ) ;
  sleep_ms_out = GetFramesSleepTime                                          (
                   stream->out.framesPerBuffer                               ,
                   stream->out.wavex.Format.nSamplesPerSec                 ) ;
  // WASAPI Input packets tend to expire very easily, let's limit sleep time to 2 milliseconds
  // for all cases. Please propose better solution if any.
  if ( sleep_ms_in > 2 ) sleep_ms_in = 2                                     ;
  // Adjust polling time for non-paUtilFixedHostBufferSize. Input stream is not adjustable as it is being
  // polled according its packet length.
  if ( stream->bufferMode != cabFixedHostBufferSize )                        {
    //sleep_ms_in = GetFramesSleepTime(stream->bufferProcessor.framesPerUserBuffer, stream->in.wavex.Format.nSamplesPerSec);
    sleep_ms_out = GetFramesSleepTime                                        (
                     stream->bufferProcessor.framesPerUserBuffer             ,
                     stream->out.wavex.Format.nSamplesPerSec               ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Choose smallest
  if ( ( sleep_ms_in != 0 ) && ( sleep_ms_out != 0 ) )                       {
    sleep_ms = min ( sleep_ms_in , sleep_ms_out )                            ;
  } else                                                                     {
    sleep_ms = ( sleep_ms_in ? sleep_ms_in : sleep_ms_out )                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Make sure not 0, othervise use ThreadIdleScheduler
  if ( 0 == sleep_ms )                                                       {
    sleep_ms_in  = GetFramesSleepTimeMicroseconds                            (
                     stream->in.framesPerHostCallback                        /
                     WASAPI_PACKETS_PER_INPUT_BUFFER                         ,
                     stream->in.wavex.Format.nSamplesPerSec                ) ;
    sleep_ms_out = GetFramesSleepTimeMicroseconds                            (
                     stream->bufferProcessor.framesPerUserBuffer             ,
                     stream->out.wavex.Format.nSamplesPerSec               ) ;
    // Choose smallest
    if ( ( sleep_ms_in != 0 ) && ( sleep_ms_out != 0 ) )                     {
      sleep_ms = min ( sleep_ms_in , sleep_ms_out )                          ;
    } else                                                                   {
      sleep_ms = ( sleep_ms_in ? sleep_ms_in : sleep_ms_out )                ;
    }                                                                        ;
    // Setup thread sleep scheduler
    ThreadIdleScheduler_Setup ( &scheduler , 1 , sleep_ms )                  ;
    sleep_ms = 0                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Boost thread priority
  ThreadPriorityBoost((void **)&stream->hAvTask,stream->nThreadPriority)     ;
  // Initialize event & start INPUT stream
  if ( NULL != stream->in.clientProc )                                       {
    hr = stream -> in . clientProc -> Start ( )                              ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      goto thread_error                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Initialize event & start OUTPUT stream
  if ( NULL != stream -> out . clientProc )                                  {
    // Preload buffer (obligatory, othervise ->Start() will fail), avoid processing
    // when in full-duplex mode as it requires input processing as well
    if ( ! CA_WASAPI__IS_FULLDUPLEX(stream) )                                {
      UINT32 frames = 0                                                      ;
      hr = stream -> PollGetOutputFramesAvailable ( &frames )                ;
      if ( S_OK == hr )                                                      {
        if ( stream->bufferMode == cabFixedHostBufferSize )                  {
          if ( frames >= stream->out.framesPerBuffer )                       {
            frames = stream -> out.framesPerBuffer                           ;
            hr     = stream -> ProcessOutputBuffer ( frames )                ;
            if ( S_OK != hr )                                                {
              // not fatal, just log
              LogHostError ( hr )                                            ;
            }                                                                ;
          }                                                                  ;
        } else                                                               {
          if ( 0 != frames )                                                 {
            hr = stream -> ProcessOutputBuffer ( frames )                    ;
            if ( S_OK != hr )                                                {
             // not fatal, just log
              LogHostError ( hr )                                            ;
            }                                                                ;
          }                                                                  ;
        }                                                                    ;
      } else                                                                 {
        // not fatal, just log
        LogHostError ( hr )                                                  ;
      }                                                                      ;
    }                                                                        ;
    // Start
    hr = stream -> out . clientProc -> Start ( )                             ;
    if ( S_OK != hr)                                                         {
      LogHostError ( hr )                                                    ;
      goto thread_error                                                      ;
    }                                                                        ;
  }                                                                          ;
  // Signal: stream running
  stream -> running = TRUE                                                   ;
  // Notify: thread started
  SetEvent ( stream -> hThreadStart )                                        ;
  if ( ! CA_WASAPI__IS_FULLDUPLEX(stream) )                                  {
    // Processing Loop
    UINT32 next_sleep = sleep_ms                                             ;
    while ( WAIT_TIMEOUT == ::WaitForSingleObject                            (
                              stream->hCloseRequest                          ,
                              next_sleep                                 ) ) {
      // Get next sleep time
      if ( 0 == sleep_ms )                                                   {
        next_sleep = ThreadIdleScheduler_NextSleep ( &scheduler )            ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      for ( i = 0 ; i < S_COUNT ; ++i )                                      {
        // Process S_INPUT/S_OUTPUT
        switch ( i )                                                         {
          case S_INPUT                                                       :
            // Input stream
            if ( NULL == stream->captureClient ) break                       ;
            hr = stream -> ProcessInputBuffer ( )                            ;
            if ( S_OK != hr )                                                {
              LogHostError ( hr )                                            ;
              goto thread_error                                              ;
            }                                                                ;
          break                                                              ;
          case S_OUTPUT                                                    : {
            // Output stream
            UINT32 frames                                                    ;
            if ( NULL == stream -> renderClient ) break                      ;
            // get available frames
            hr = stream -> PollGetOutputFramesAvailable ( &frames )          ;
            if ( S_OK != hr )                                                {
              LogHostError ( hr )                                            ;
              goto thread_error                                              ;
            }                                                                ;
            // output
            if ( stream->bufferMode == cabFixedHostBufferSize )              {
               while ( frames >= stream->out.framesPerBuffer )               {
                 hr = stream->ProcessOutputBuffer(stream->out.framesPerBuffer) ;
                 if ( S_OK != hr )                                           {
                   LogHostError ( hr )                                       ;
                   goto thread_error                                         ;
                 }                                                           ;
                 /////////////////////////////////////////////////////////////
                 frames -= stream -> out . framesPerBuffer                   ;
               }                                                             ;
             } else                                                          {
               if ( 0 != frames )                                            {
                 hr = stream -> ProcessOutputBuffer ( frames )               ;
                 if ( S_OK != hr )                                           {
                   LogHostError ( hr )                                       ;
                   goto thread_error                                         ;
                 }                                                           ;
              }                                                              ;
            }                                                                ;
          }                                                                  ;
          break                                                              ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
  } else                                                                     {
    // Processing Loop
    UINT32 next_sleep = sleep_ms                                             ;
    while ( WAIT_TIMEOUT == ::WaitForSingleObject                            (
                                stream->hCloseRequest                        ,
                                next_sleep                               ) ) {
      UINT32 i_frames    = 0                                                 ;
      UINT32 i_processed = 0                                                 ;
      BYTE * i_data      = NULL                                              ;
      BYTE * o_data      = NULL                                              ;
      BYTE * o_data_host = NULL                                              ;
      DWORD  i_flags     = 0                                                 ;
      UINT32 o_frames    = 0                                                 ;
      ////////////////////////////////////////////////////////////////////////
      // Get next sleep time
      if ( 0 == sleep_ms )                                                   {
        next_sleep = ThreadIdleScheduler_NextSleep ( &scheduler )            ;
      }                                                                      ;
      // get available frames
      hr = stream -> PollGetOutputFramesAvailable ( &o_frames )              ;
      if ( S_OK != hr )                                                      {
        LogHostError ( hr )                                                  ;
        break                                                                ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      while ( 0 != o_frames )                                                {
        // get host input buffer
        hr = stream -> captureClient -> GetBuffer                            (
               &i_data                                                       ,
               &i_frames                                                     ,
               &i_flags                                                      ,
               NULL                                                          ,
               NULL                                                        ) ;
        if ( S_OK != hr )                                                    {
          // no data in capture buffer
          if ( AUDCLNT_S_BUFFER_EMPTY == hr ) break                          ;
          LogHostError ( hr )                                                ;
          break                                                              ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        // process equal ammount of frames
        if ( o_frames >= i_frames )                                          {
          // process input ammount of frames
          UINT32 o_processed = i_frames                                      ;
          // get host output buffer
          hr = stream -> renderClient -> GetBuffer ( o_processed , &o_data ) ;
          if ( S_OK == hr )                                                  {
            // processed amount of i_frames
            i_processed = i_frames                                           ;
            o_data_host = o_data                                             ;
            // convert output mono
            if ( NULL != stream -> out . monoMixer )                         {
              UINT32 mono_frames_size                                        ;
              mono_frames_size = o_processed                                 *
                                (stream->out.wavex.Format.wBitsPerSample/8)  ;
              // expand buffer
              if ( mono_frames_size > stream->out.monoBufferSize )           {
                stream -> out . monoBuffer = realloc                         (
                  stream->out.monoBuffer                                     ,
                  (stream->out.monoBufferSize = mono_frames_size)          ) ;
                if ( NULL == stream -> out . monoBuffer )                    {
                  // release input buffer
                  stream -> captureClient -> ReleaseBuffer ( 0     )         ;
                  // release output buffer
                  stream -> renderClient  -> ReleaseBuffer ( 0 , 0 )         ;
                  LogCaError ( InsufficientMemory )                          ;
                  goto thread_error                                          ;
                }                                                            ;
              }                                                              ;
              // replace buffer pointer
              o_data = (BYTE *) stream -> out . monoBuffer                   ;
            }                                                                ;
            //////////////////////////////////////////////////////////////////
            // convert input mono
            if ( NULL != stream->in.monoMixer )                              {
              UINT32 mono_frames_size                                        ;
              mono_frames_size = i_processed                                 *
                                 (stream->in.wavex.Format.wBitsPerSample/8)  ;
              // expand buffer
              if ( mono_frames_size > stream -> in . monoBufferSize )        {
                stream -> in . monoBuffer = realloc                          (
                  stream->in.monoBuffer                                      ,
                  ( stream->in.monoBufferSize = mono_frames_size )         ) ;
                if ( NULL == stream -> in . monoBuffer )                     {
                  // release input buffer
                  stream -> captureClient -> ReleaseBuffer ( 0     )         ;
                  // release output buffer
                  stream -> renderClient  -> ReleaseBuffer ( 0 , 0 )         ;
                  LogCaError ( InsufficientMemory )                          ;
                  goto thread_error                                          ;
                }                                                            ;
              }                                                              ;
              // mix 2 to 1 input channels
              stream->in.monoMixer(stream->in.monoBuffer,i_data,i_processed) ;
              // replace buffer pointer
              i_data = (BYTE *) stream -> in . monoBuffer                    ;
            }                                                                ;
            // process
            WaSapiHostProcessingLoop                                         (
              i_data                                                         ,
              i_processed                                                    ,
              o_data                                                         ,
              o_processed                                                    ,
              stream                                                       ) ;
            // mix 1 to 2 output channels
            if ( NULL != stream->out.monoBuffer )                            {
              stream -> out . monoMixer                                      (
                o_data_host                                                  ,
                stream->out.monoBuffer                                       ,
                o_processed                                                ) ;
            }                                                                ;
            // release host output buffer
            hr = stream -> renderClient -> ReleaseBuffer ( o_processed , 0 ) ;
            if ( S_OK != hr ) LogHostError ( hr )                            ;
            o_frames -= o_processed                                          ;
          } else                                                             {
            if ( stream -> out . shareMode != AUDCLNT_SHAREMODE_SHARED )     {
              // be silent in shared mode, try again next time
              LogHostError ( hr )                                            ;
            }                                                                ;
          }                                                                  ;
        } else                                                               {
          i_processed = 0                                                    ;
          goto fd_release_buffer_in                                          ;
        }                                                                    ;
fd_release_buffer_in:
        // release host input buffer
        hr = stream -> captureClient -> ReleaseBuffer ( i_processed )        ;
        if ( S_OK != hr )                                                    {
          LogHostError ( hr )                                                ;
          break                                                              ;
        }                                                                    ;
        // break processing, input hasn't been accumulated yet
        if ( 0 == i_processed ) break                                        ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
thread_end:
  // Process stop
  stream -> StreamOnStop ( )                                                 ;
  // Release unmarshaled COM pointers
  stream -> ReleaseUnmarshaledComPointers ( )                                ;
  // Cleanup COM for this thread
  if ( TRUE == bThreadComInitialized ) CoUninitialize ( )                    ;
  // Notify: not running
  stream -> running = FALSE                                                  ;
  // Notify: thread exited
  SetEvent ( stream -> hThreadExit  )                                        ;
  return 0                                                                   ;
thread_error:
  // Prevent deadlocking in Pa_StreamStart
  SetEvent ( stream -> hThreadStart )                                        ;
  // Exit
  goto thread_end                                                            ;
  return 0                                                                   ;
}

///////////////////////////////////////////////////////////////////////////////

CaError WaSapiInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError               result     = NoError                                 ;
  WaSapiHostApi       * paWasapi                                             ;
  DeviceInfo          * deviceInfoArray                                      ;
  HRESULT               hr         = S_OK                                    ;
  IMMDeviceCollection * pEndPoints = NULL                                    ;
  UINT                  i                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ! SetupAVRT ( ) )                                                     {
    gPrintf ( ( "WASAPI : No AVRT! (not VISTA?)" ) )                         ;
    return NoError                                                           ;
  }                                                                          ;
  paWasapi = new WaSapiHostApi ( )                                           ;
  if ( NULL == paWasapi )                                                    {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  result = CaComInitialize ( WASAPI , & paWasapi->comInitializationResult )  ;
  if ( NoError != result ) goto error                                        ;
  paWasapi->allocations = new AllocationGroup ( )                            ;
  if ( NULL == paWasapi->allocations )                                       {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *hostApi                                 = (HostApi *) paWasapi            ;
  (*hostApi) -> info . structVersion       = 1                               ;
  (*hostApi) -> info . type                = WASAPI                          ;
  (*hostApi) -> info . index               = hostApiIndex                    ;
  (*hostApi) -> info . name                = "Windows WASAPI"                ;
  (*hostApi) -> info . deviceCount         = 0                               ;
  (*hostApi) -> info . defaultInputDevice  = CaNoDevice                      ;
  (*hostApi) -> info . defaultOutputDevice = CaNoDevice                      ;
  ////////////////////////////////////////////////////////////////////////////
  paWasapi -> enumerator = NULL                                              ;
  hr = CoCreateInstance                                                      (
         ca_CLSID_IMMDeviceEnumerator                                        ,
         NULL                                                                ,
         CLSCTX_INPROC_SERVER                                                ,
         ca_IID_IMMDeviceEnumerator                                          ,
         (void **)&paWasapi->enumerator                                    ) ;
  // We need to set the result to a value otherwise we will return paNoError
  // [IF_FAILED_JUMP(hResult, error);]
  IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                      ;
  // getting default device ids in the eMultimedia "role"
  {
    IMMDevice * defaultRenderer = NULL                                       ;
    hr = paWasapi->enumerator->GetDefaultAudioEndpoint                       (
           eRender                                                           ,
           eMultimedia                                                       ,
           &defaultRenderer                                                ) ;
    if ( S_OK != hr )                                                        {
      if ( E_NOTFOUND != hr )                                                {
        // We need to set the result to a value otherwise we will return NoError
        // [IF_FAILED_JUMP(hResult, error);]
        IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                ;
      }                                                                      ;
    } else                                                                   {
      WCHAR * pszDeviceId = NULL                                             ;
      hr = defaultRenderer -> GetId ( &pszDeviceId )                         ;
      // We need to set the result to a value otherwise we will return NoError
      // [IF_FAILED_JUMP(hResult, error);]
      IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                  ;
      ::wcsncpy ( paWasapi->defaultRenderer , pszDeviceId , MAX_STR_LEN-1 )  ;
      CoTaskMemFree ( pszDeviceId )                                          ;
      defaultRenderer -> Release ( )                                         ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  {
    IMMDevice * defaultCapturer = NULL                                       ;
     hr = paWasapi -> enumerator -> GetDefaultAudioEndpoint                  (
            eCapture                                                         ,
            eMultimedia                                                      ,
            &defaultCapturer                                               ) ;
     if ( S_OK != hr )                                                       {
       if ( E_NOTFOUND != hr )                                               {
         // We need to set the result to a value otherwise we will return NoError
         // [IF_FAILED_JUMP(hResult, error);]
         IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )               ;
       }                                                                     ;
     } else                                                                  {
       WCHAR *pszDeviceId = NULL                                             ;
       hr = defaultCapturer -> GetId ( &pszDeviceId )                        ;
       // We need to set the result to a value otherwise we will return NoError
       // [IF_FAILED_JUMP(hResult, error);]
       IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                 ;
       ::wcsncpy ( paWasapi->defaultCapturer , pszDeviceId , MAX_STR_LEN-1 ) ;
       CoTaskMemFree ( pszDeviceId )                                         ;
       defaultCapturer -> Release ( )                                        ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  hr = paWasapi -> enumerator -> EnumAudioEndpoints                          (
         eAll                                                                ,
         DEVICE_STATE_ACTIVE                                                 ,
         &pEndPoints                                                       ) ;
  // We need to set the result to a value otherwise we will return NoError
  // [IF_FAILED_JUMP(hResult, error);]
  IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                      ;
  hr = pEndPoints -> GetCount ( & paWasapi -> deviceCount )                  ;
  // We need to set the result to a value otherwise we will return paNoError
  // [IF_FAILED_JUMP(hResult, error);]
  IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                      ;
  ////////////////////////////////////////////////////////////////////////////
  paWasapi->devInfo = new WaSapiDeviceInfo [ paWasapi->deviceCount ]         ;
  for (i = 0; i < paWasapi->deviceCount; ++i)                                {
    paWasapi -> devInfo [ i ] . Initialize ( )                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( paWasapi -> deviceCount > 0 )                                         {
    (*hostApi)->deviceInfos = new DeviceInfo * [ paWasapi->deviceCount ]     ;
    if ( NULL == (*hostApi)->deviceInfos )                                   {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    /* allocate all device info structs in a contiguous block               */
    deviceInfoArray = new DeviceInfo [ paWasapi->deviceCount ]               ;
    if ( NULL == deviceInfoArray )                                           {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    for ( i = 0 ; i < paWasapi -> deviceCount ; ++i )                        {
      DWORD        state          = 0                                        ;
      DeviceInfo * deviceInfo     = & deviceInfoArray [ i ]                  ;
      ////////////////////////////////////////////////////////////////////////
      deviceInfo -> structVersion = 2                                        ;
      deviceInfo -> hostApi       = hostApiIndex                             ;
      deviceInfo -> hostType      = WASAPI                                   ;
      gPrintf ( ( "WASAPI : device idx: %02d\n" , i ) )                      ;
      gPrintf ( ( "WASAPI : ---------------\n"      ) )                      ;
      ////////////////////////////////////////////////////////////////////////
      hr = pEndPoints -> Item ( i , &paWasapi->devInfo[i].device )           ;
      // We need to set the result to a value otherwise we will return paNoError
      // [IF_FAILED_JUMP(hResult, error);]
      IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                  ;
      { // getting ID
        WCHAR * pszDeviceId = NULL                                           ;
        hr = paWasapi->devInfo[i].device->GetId ( &pszDeviceId )             ;
        // We need to set the result to a value otherwise we will return paNoError
        // [IF_FAILED_JUMP(hr, error);]
        IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                ;
        ::wcsncpy(paWasapi->devInfo[i].szDeviceID,pszDeviceId,MAX_STR_LEN-1) ;
        CoTaskMemFree ( pszDeviceId )                                        ;
        if ( lstrcmpW ( paWasapi->devInfo[i].szDeviceID                      ,
                        paWasapi->defaultCapturer                    ) == 0) {
          // we found the default input!
          (*hostApi)->info.defaultInputDevice = (*hostApi)->info.deviceCount ;
        }                                                                    ;
        if ( lstrcmpW ( paWasapi->devInfo[i].szDeviceID                      ,
                        paWasapi->defaultRenderer) == 0                    ) {
          // we found the default output!
          (*hostApi)->info.defaultOutputDevice = (*hostApi)->info.deviceCount;
        }                                                                    ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      hr = paWasapi->devInfo[i].device->GetState(&paWasapi->devInfo[i].state);
      // We need to set the result to a value otherwise we will return paNoError
      // [IF_FAILED_JUMP(hResult, error);]
      IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                  ;
      if ( paWasapi->devInfo[i].state != DEVICE_STATE_ACTIVE )               {
        gPrintf(("WASAPI device: %d is not currently available (state:%d)\n",i,state));
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      {
        IPropertyStore * pProperty                                           ;
        hr = paWasapi -> devInfo [ i ] . device -> OpenPropertyStore         (
               STGM_READ                                                     ,
               &pProperty                                                  ) ;
        // We need to set the result to a value otherwise we will return NoError
        // [IF_FAILED_JUMP(hResult, error);]
        IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                ;
        { // "Friendly" Name
          char      * deviceName                                             ;
          PROPVARIANT value                                                  ;
          PropVariantInit ( &value )                                         ;
          hr = pProperty -> GetValue ( PKEY_Device_FriendlyName , &value )   ;
          // We need to set the result to a value otherwise we will return NoError
          // [IF_FAILED_JUMP(hResult, error);]
          IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )              ;
          deviceInfo -> name = NULL                                          ;
          deviceName         = (char *)paWasapi->allocations->alloc          (
                                         MAX_STR_LEN + 1                   ) ;
          if ( NULL == deviceName )                                          {
            result = InsufficientMemory                                      ;
            goto error                                                       ;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          if ( NULL != value.pwszVal )                                       {
            ::WideCharToMultiByte                                            (
                CP_UTF8                                                      ,
                0                                                            ,
                value.pwszVal                                                ,
                (int)wcslen(value.pwszVal)                                   ,
                deviceName                                                   ,
                MAX_STR_LEN-1                                                ,
                0                                                            ,
                0                                                          ) ;
          } else                                                             {
            _snprintf ( deviceName , MAX_STR_LEN - 1 , "baddev%d" , i)       ;
          }                                                                  ;
          ////////////////////////////////////////////////////////////////////
          deviceInfo -> name = deviceName                                    ;
          PropVariantClear ( &value )                                        ;
          gPrintf ( ( "WASAPI : %d | name[%s]\n" , i , deviceInfo->name ) )  ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        { // Default format
          PROPVARIANT value                                                  ;
          PropVariantInit ( &value )                                         ;
          hr = pProperty -> GetValue ( PKEY_AudioEngine_DeviceFormat,&value) ;
          // We need to set the result to a value otherwise we will return paNoError
          // [IF_FAILED_JUMP(hResult, error);]
          IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )              ;
          ::memcpy ( &paWasapi->devInfo[i].DefaultFormat                     ,
                     value.blob.pBlobData                                    ,
                     min( sizeof(paWasapi->devInfo[i].DefaultFormat)         ,
                          value.blob.cbSize                              ) ) ;
          // cleanup
          PropVariantClear ( &value )                                        ;
        }
        //////////////////////////////////////////////////////////////////////
        { // Formfactor
          PROPVARIANT value                                                  ;
          PropVariantInit ( &value )                                         ;
          hr = pProperty -> GetValue ( PKEY_AudioEndpoint_FormFactor,&value) ;
          // We need to set the result to a value otherwise we will return paNoError
          // [IF_FAILED_JUMP(hResult, error);]
          IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )              ;
          // set
          #if defined(DUMMYUNIONNAME) && defined(NONAMELESSUNION)
            // avoid breaking strict-aliasing rules in such line: (EndpointFormFactor)(*((UINT *)(((WORD *)&value.wReserved3)+1)));
            UINT v;
            ::memcpy(&v, (((WORD *)&value.wReserved3)+1), sizeof(v))         ;
            paWasapi->devInfo[i].formFactor = (EndpointFormFactor)v          ;
          #else
            paWasapi->devInfo[i].formFactor = (EndpointFormFactor)value.uintVal ;
          #endif
          gPrintf ( ( "WASAPI : %d | form-factor[%d]\n", i, paWasapi->devInfo[i].formFactor ) ) ;
          // cleanup
          PropVariantClear ( &value )                                        ;
        }
        SAFE_RELEASE ( pProperty )                                           ;
      }
      ////////////////////////////////////////////////////////////////////////
      { // Endpoint data
        IMMEndpoint * endpoint = NULL                                        ;
        hr = paWasapi -> devInfo [ i ] . device -> QueryInterface            (
               ca_IID_IMMEndpoint                                            ,
               (void **)&endpoint                                          ) ;
        if ( SUCCEEDED ( hr ) )                                              {
          hr = endpoint -> GetDataFlow ( &paWasapi -> devInfo [ i ] . flow ) ;
          SAFE_RELEASE ( endpoint )                                          ;
        }                                                                    ;
      }
      // Getting a temporary IAudioClient for more fields
      // we make sure NOT to call Initialize yet!
      {
        IAudioClient *tmpClient = NULL;
        hr = paWasapi -> devInfo [ i ] . device -> Activate                  (
               ca_IID_IAudioClient                                           ,
               CLSCTX_INPROC_SERVER                                          ,
               NULL                                                          ,
               (void **)&tmpClient                                         ) ;
        // We need to set the result to a value otherwise we will return paNoError
        // [IF_FAILED_JUMP(hResult, error);]
        IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                ;
        hr = tmpClient -> GetDevicePeriod                                    (
               & paWasapi -> devInfo [ i ] . DefaultDevicePeriod             ,
               & paWasapi -> devInfo [ i ] . MinimumDevicePeriod           ) ;
        // We need to set the result to a value otherwise we will return paNoError
        // [IF_FAILED_JUMP(hResult, error);]
        IF_FAILED_INTERNAL_ERROR_JUMP ( hr , result , error )                ;
        // hr = tmpClient->GetMixFormat(&paWasapi->devInfo[i].MixFormat) ;
        // Release client
        SAFE_RELEASE ( tmpClient )                                           ;
        if ( S_OK != hr )                                                    {
          // davidv: this happened with my hardware, previously for that same device in DirectSound:
          // Digital Output (Realtek AC'97 Audio)'s GUID: {0x38f2cf50,0x7b4c,0x4740,0x86,0xeb,0xd4,0x38,0x66,0xd8,0xc8, 0x9f}
          // so something must be _really_ wrong with this device, TODO handle this better. We kind of need GetMixFormat
          LogHostError ( hr )                                                ;
          // We need to set the result to a value otherwise we will return paNoError
          result = InternalError                                             ;
          goto error                                                         ;
        }                                                                    ;
      }                                                                      ;
      // we can now fill in portaudio device data
      deviceInfo -> maxInputChannels  = 0                                    ;
      deviceInfo -> maxOutputChannels = 0                                    ;
      deviceInfo -> defaultSampleRate = paWasapi->devInfo[i].DefaultFormat.Format.nSamplesPerSec ;
      switch ( paWasapi -> devInfo [ i ] . flow )                            {
        case eRender                                                         :
          deviceInfo -> maxOutputChannels        = paWasapi->devInfo[i].DefaultFormat.Format.nChannels ;
          deviceInfo -> defaultHighOutputLatency = nano100ToSeconds(paWasapi->devInfo[i].DefaultDevicePeriod) ;
          deviceInfo -> defaultLowOutputLatency  = nano100ToSeconds(paWasapi->devInfo[i].MinimumDevicePeriod) ;
          gPrintf                                                          ( (
              "WASAPI : %d| def.SR[%d] max.CH[%d] latency{hi[%f] lo[%f]}\n"  ,
              i                                                              ,
              (UINT32)deviceInfo->defaultSampleRate                          ,
              deviceInfo->maxOutputChannels                                  ,
              (float)deviceInfo->defaultHighOutputLatency                    ,
              (float)deviceInfo->defaultLowOutputLatency                 ) ) ;
        break                                                                ;
        case eCapture                                                        :
          deviceInfo -> maxInputChannels		= paWasapi->devInfo[i].DefaultFormat.Format.nChannels;
          deviceInfo -> defaultHighInputLatency = nano100ToSeconds(paWasapi->devInfo[i].DefaultDevicePeriod);
          deviceInfo -> defaultLowInputLatency  = nano100ToSeconds(paWasapi->devInfo[i].MinimumDevicePeriod);
          gPrintf                                                          ( (
              "WASAPI : %d| def.SR[%d] max.CH[%d] latency{hi[%f] lo[%f]}\n"  ,
              i                                                              ,
              (UINT32)deviceInfo->defaultSampleRate                          ,
              deviceInfo->maxInputChannels                                   ,
              (float)deviceInfo->defaultHighInputLatency                     ,
              (float)deviceInfo->defaultLowInputLatency                  ) ) ;
        break                                                                ;
        default                                                              :
          gPrintf ( ( "WASAPI : %d| bad Data Flow!\n" , i ) )                ;
          result = InternalError                                             ;
          //continue; // do not skip from list, allow to initialize
        break                                                                ;
      }                                                                      ;
      (*hostApi)->deviceInfos[i] = deviceInfo                                ;
      ++(*hostApi)->info.deviceCount                                         ;
    }                                                                        ;
  }                                                                          ;
  // findout if platform workaround is required
  paWasapi->useWOW64Workaround = UseWOW64Workaround ( )                      ;
  SAFE_RELEASE ( pEndPoints )                                                ;
  gPrintf ( ( "WASAPI : initialized ok\n" ) )                                ;
  return NoError                                                             ;
error:
  gPrintf ( ( "WASAPI : failed %s error[%d|%s]\n"                            ,
              __FUNCTION__                                                   ,
              result                                                         ,
              globalDebugger->Error(result)                              ) ) ;
  SAFE_RELEASE ( pEndPoints )                                                ;
  paWasapi -> Terminate ( )                                                  ;
  if ( NoError == result ) result = InternalError                            ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

WaSapiStream:: WaSapiStream (void)
             : Stream       (    )
{
  ::memset ( event , 0 , sizeof(HANDLE) * S_COUNT  ) ;
  captureClientParent = NULL                         ;
  captureClientStream = NULL                         ;
  captureClient       = NULL                         ;
  inVol               = NULL                         ;
  renderClientParent  = NULL                         ;
  renderClientStream  = NULL                         ;
  renderClient        = NULL                         ;
  outVol              = NULL                         ;
  Resampler           = NULL                         ;
  bufferMode          = cabFixedHostBufferSize       ;
  running             = 0                            ;
  dwThreadId          = 0                            ;
  hThread             = 0                            ;
  hCloseRequest       = 0                            ;
  hThreadStart        = 0                            ;
  hThreadExit         = 0                            ;
  hBlockingOpStreamRD = 0                            ;
  hBlockingOpStreamWR = 0                            ;
  hAvTask             = 0                            ;
  bBlocking           = 0                            ;
  nThreadPriority     = eThreadPriorityNone          ;
  doResample          = false                        ;
  NearestSampleRate   = -1                           ;
  ////////////////////////////////////////////////////
  InitializeSubStream ( &in  )                       ;
  InitializeSubStream ( &out )                       ;
}

WaSapiStream::~WaSapiStream(void)
{
  #if defined(FFMPEGLIB)
  if ( doResample && ( NULL != Resampler) )            {
    CaResampler * resampler = (CaResampler *)Resampler ;
    delete resampler                                   ;
  }                                                    ;
  #endif
}

void WaSapiStream::InitializeSubStream(CaWaSapiSubStream * s)
{
  s->clientParent           = NULL                                       ;
  s->clientStream           = NULL                                       ;
  s->clientProc             = NULL                                       ;
  s->monoBuffer             = NULL                                       ;
  s->tailBuffer             = NULL                                       ;
  s->tailBufferMemory       = NULL                                       ;
  s->monoMixer              = NULL                                       ;
  s->bufferSize             = 0                                          ;
  s->latencySeconds         = 0                                          ;
  s->framesPerHostCallback  = 0                                          ;
  s->streamFlags            = 0                                          ;
  s->flags                  = 0                                          ;
  s->buffers                = 0                                          ;
  s->framesPerBuffer        = 0                                          ;
  s->userBufferAndHostMatch = FALSE                                      ;
  s->monoBufferSize         = 0                                          ;
  ////////////////////////////////////////////////////////////////////////
  memset ( &(s->deviceLatency) , 0 , sizeof(REFERENCE_TIME           ) ) ;
  memset ( &(s->period       ) , 0 , sizeof(REFERENCE_TIME           ) ) ;
  memset ( &(s->shareMode    ) , 0 , sizeof(AUDCLNT_SHAREMODE        ) ) ;
  memset ( &(s->wavex        ) , 0 , sizeof(AUDCLNT_SHAREMODE        ) ) ;
  memset ( &(s->params       ) , 0 , sizeof(CaWaSapiAudioClientParams) ) ;
}

bool WaSapiStream::setResampling       (
       double         inputSampleRate  ,
       double         outputSampleRate ,
       int            channels         ,
       CaSampleFormat format           )
{
  #if defined(FFMPEGLIB)
  CaResampler * resampler = new CaResampler() ;
  if ( NULL != resampler )                    {
    if (resampler -> Setup ( inputSampleRate  ,
                             channels         ,
                             format           ,
                             outputSampleRate ,
                             channels         ,
                             format       ) ) {
      doResample = true                       ;
      Resampler  = (void *)resampler          ;
      return true                             ;
    } else                                    {
      delete resampler                        ;
    }                                         ;
  }                                           ;
  #endif
  return false                                ;
}

CaError WaSapiStream::Start(void)
{
  CaError result = NoError                                                   ;
  HRESULT hr                                                                 ;
  // check if stream is active already
  if ( IsActive ( ) ) return StreamIsNotStopped                              ;
  bufferProcessor . Reset ( )                                                ;
  // Cleanup handles (may be necessary if stream was stopped by itself due to error)
  Cleanup                 ( )                                                ;
  ////////////////////////////////////////////////////////////////////////////
  // Create close event
  hCloseRequest = CreateEvent ( NULL , TRUE , FALSE , NULL )                 ;
  if ( NULL == hCloseRequest )                                               {
    result = InsufficientMemory                                              ;
    goto start_error                                                         ;
  }                                                                          ;
  // Create thread
  if ( ! bBlocking )                                                         {
    // Create thread events
    hThreadStart = CreateEvent ( NULL , TRUE , FALSE , NULL )                ;
    hThreadExit  = CreateEvent ( NULL , TRUE , FALSE , NULL )                ;
    if ( ( hThreadStart == NULL ) || ( hThreadExit == NULL ) )               {
      result = InsufficientMemory                                            ;
      goto start_error                                                       ;
    }                                                                        ;
    // Marshal WASAPI interface pointers for safe use in thread created below.
    hr = MarshalStreamComPointers ( )                                        ;
    if ( S_OK != hr )                                                        {
      dPrintf ( ( "Failed marshaling stream COM pointers." ) )               ;
      result = UnanticipatedHostError                                        ;
      goto nonblocking_start_error                                           ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( in  . clientParent && ( in  . streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK)) ||
         ( out . clientParent && ( out . streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK))) {
      if ( ( hThread = CREATE_THREAD(ProcThreadEvent) ) == NULL)             {
        dPrintf ( ( "Failed creating thread: ProcThreadEvent." ) )           ;
        result = UnanticipatedHostError                                      ;
        goto nonblocking_start_error                                         ;
      }                                                                      ;
    } else                                                                   {
      if ( ( hThread = CREATE_THREAD(ProcThreadPoll ) ) == NULL)             {
        dPrintf ( ( "Failed creating thread: ProcThreadPoll." ) )            ;
        result = UnanticipatedHostError                                      ;
        goto nonblocking_start_error                                         ;
      }                                                                      ;
    }                                                                        ;
    // Wait for thread to start
    if ( WaitForSingleObject(hThreadStart,60*1000) == WAIT_TIMEOUT )         {
      dPrintf ( ( "Failed starting thread: timeout." ) )                     ;
      result = UnanticipatedHostError                                        ;
      goto nonblocking_start_error                                           ;
    }                                                                        ;
  } else                                                                     {
    // Create blocking operation events (non-signaled event means - blocking operation is pending)
    if ( out . clientParent != NULL )                                        {
      hBlockingOpStreamWR = CreateEvent ( NULL , TRUE , TRUE , NULL )        ;
      if ( NULL == hBlockingOpStreamWR )                                     {
        result = InsufficientMemory                                          ;
        goto start_error                                                     ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( NULL != in.clientParent )                                           {
      hBlockingOpStreamRD = CreateEvent ( NULL , TRUE , TRUE , NULL )        ;
      if ( NULL == hBlockingOpStreamRD )                                     {
        result = InsufficientMemory                                          ;
        goto start_error                                                     ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    // Initialize event & start INPUT stream
    if ( NULL != in . clientParent )                                         {
      hr = in . clientParent -> Start ( )                                    ;
      if ( S_OK != hr )                                                      {
        LogHostError ( hr )                                                  ;
        result = UnanticipatedHostError                                      ;
        goto start_error                                                     ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    // Initialize event & start OUTPUT stream
    if ( NULL != out . clientParent )                                        {
      // Start
      hr = out . clientParent -> Start ( )                                   ;
      if ( S_OK != hr )                                                      {
        LogHostError ( hr )                                                  ;
        result = UnanticipatedHostError                                      ;
        goto start_error                                                     ;
      }                                                                      ;
    }                                                                        ;
    // Set parent to working pointers to use shared functions.
    captureClient    = captureClientParent                                   ;
    renderClient     = renderClientParent                                    ;
    in  . clientProc = in  . clientParent                                    ;
    out . clientProc = out . clientParent                                    ;
    // Signal: stream running.
    running = TRUE                                                           ;
  }                                                                          ;
  return result                                                              ;
nonblocking_start_error                                                      :
  // Set hThreadExit event to prevent blocking during cleanup
  SetEvent                      ( hThreadExit )                              ;
  UnmarshalStreamComPointers    (             )                              ;
  ReleaseUnmarshaledComPointers (             )                              ;
start_error                                                                  :
  Stop ( )                                                                   ;
  return result                                                              ;
}

CaError WaSapiStream::Stop(void)
{
  Finish ( )     ;
  return NoError ;
}

CaError WaSapiStream::Close(void)
{
  CaError result = NoError                                                   ;
  if ( IsActive ( ) )  result = Abort ( )                                    ;
  ////////////////////////////////////////////////////////////////////////////
  SAFE_RELEASE ( captureClientParent )                                       ;
  SAFE_RELEASE ( renderClientParent  )                                       ;
  SAFE_RELEASE ( out.clientParent    )                                       ;
  SAFE_RELEASE ( in.clientParent     )                                       ;
  SAFE_RELEASE ( inVol               )                                       ;
  SAFE_RELEASE ( outVol              )                                       ;
  ////////////////////////////////////////////////////////////////////////////
  CloseHandle  ( event [ S_INPUT  ]  )                                       ;
  CloseHandle  ( event [ S_OUTPUT ]  )                                       ;
  Cleanup      (                     )                                       ;
  ////////////////////////////////////////////////////////////////////////////
  delete in  . tailBuffer                                                    ;
  delete out . tailBuffer                                                    ;
  Allocator :: free ( in  . monoBuffer       )                               ;
  Allocator :: free ( out . monoBuffer       )                               ;
  Allocator :: free ( in  . tailBufferMemory )                               ;
  Allocator :: free ( out . tailBufferMemory )                               ;
  in  . tailBuffer       = NULL                                              ;
  out . tailBuffer       = NULL                                              ;
  in  . tailBufferMemory = NULL                                              ;
  out . tailBufferMemory = NULL                                              ;
  in  . monoBuffer       = NULL                                              ;
  out . monoBuffer       = NULL                                              ;
  ////////////////////////////////////////////////////////////////////////////
  bufferProcessor . Terminate ( )                                            ;
  Terminate                   ( )                                            ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

CaError WaSapiStream::Abort(void)
{
  Finish ( )     ;
  return NoError ;
}

CaError WaSapiStream::IsStopped(void)
{
  return ! running ;
}

CaError WaSapiStream::IsActive(void)
{
  return   running ;
}

CaTime WaSapiStream::GetTime(void)
{
  return Timer :: Time ( ) ;
}

double WaSapiStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer . Value ( ) ;
}

CaUint32 WaSapiStream::GetOutputFrameCount(void)
{
  return bufferProcessor . hostOutputFrameCount [ 0 ] ;
}

CaInt32 WaSapiStream::ReadAvailable(void)
{
  UINT32  available = 0                               ;
  HRESULT hr                                          ;
  if ( ! running             ) return StreamIsStopped ;
  if ( NULL == captureClient ) return BadStreamPtr    ;
  hr = PollGetInputFramesAvailable ( &available )     ;
  if ( S_OK != hr )                                   {
    LogHostError ( hr )                               ;
    return UnanticipatedHostError                     ;
  }                                                   ;
  available += in.tailBuffer->ReadAvailable()         ;
  return available                                    ;
}

CaInt32 WaSapiStream::WriteAvailable(void)
{
  UINT32  available = 0                              ;
  HRESULT hr                                         ;
  if ( ! running            ) return StreamIsStopped ;
  if ( NULL == renderClient ) return BadStreamPtr    ;
  hr = PollGetOutputFramesAvailable ( &available )   ;
  if ( S_OK != hr )                                  {
    LogHostError ( hr )                              ;
    return UnanticipatedHostError                    ;
  }                                                  ;
  return available                                   ;
}

CaError WaSapiStream::Read(void * buffer,unsigned long frames)
{
  HRESULT             hr            = S_OK                                   ;
  BYTE             *  user_buffer   = (BYTE *)buffer                         ;
  BYTE             *  wasapi_buffer = NULL                                   ;
  DWORD               flags         = 0                                      ;
  UINT32              i             = 0                                      ;
  UINT32              available     = 0                                      ;
  UINT32              sleep         = 0                                      ;
  unsigned long       processed     = 0                                      ;
  ThreadIdleScheduler sched                                                  ;
  // validate
  if ( ! running             ) return StreamIsStopped                        ;
  if ( NULL == captureClient ) return BadStreamPtr                           ;
  // Notify blocking op has begun
  ResetEvent ( hBlockingOpStreamRD )                                         ;
  // Use thread scheduling for 500 microseconds (emulated) when wait time for frames is less than
  // 1 milliseconds, emulation helps to normalize CPU consumption and avoids too busy waiting
  ThreadIdleScheduler_Setup ( &sched , 1 , 250 /* microseconds */ )          ;
  // Make a local copy of the user buffer pointer(s), this is necessary
  // because PaUtil_CopyOutput() advances these pointers every time it is called
  if ( ! bufferProcessor . userInputIsInterleaved )                          {
    user_buffer = (BYTE *)alloca(sizeof(BYTE *) * bufferProcessor . inputChannelCount ) ;
    if ( user_buffer == NULL ) return InsufficientMemory                     ;
     for ( i = 0 ; i < bufferProcessor . inputChannelCount ; ++i )           {
       ((BYTE **)user_buffer)[i] = ((BYTE **)buffer)[i]                      ;
     }                                                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Findout if there are tail frames, flush them all before reading hardware
  available = in . tailBuffer -> ReadAvailable ( )                           ;
  if ( 0 != available )                                                      {
    CaInt32 buf1_size = 0, buf2_size = 0, read, desired                      ;
    void *buf1 = NULL, *buf2 = NULL                                          ;
    // Limit desired to amount of requested frames
    desired = available                                                      ;
    if ( (UINT32)desired > frames ) desired = frames                         ;
    // Get pointers to read regions
    read = in . tailBuffer -> ReadRegions                                    (
             desired                                                         ,
             &buf1                                                           ,
             &buf1_size                                                      ,
             &buf2                                                           ,
             &buf2_size                                                    ) ;
    if ( buf1 != NULL )                                                      {
      // Register available frames to processor
      bufferProcessor . setInputFrameCount ( 0 , buf1_size )                 ;
      // Register host buffer pointer to processor
      bufferProcessor . setInterleavedInputChannels                          (
        0                                                                    ,
        0                                                                    ,
        buf1                                                                 ,
        bufferProcessor . inputChannelCount                                ) ;
      // Copy user data to host buffer (with conversion if applicable)
      processed = bufferProcessor.CopyInput((void **)&user_buffer,buf1_size) ;
      frames -= processed                                                    ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( buf2 != NULL )                                                      {
      // Register available frames to processor
      bufferProcessor . setInputFrameCount ( 0 , buf2_size )                 ;
      // Register host buffer pointer to processor
      bufferProcessor . setInterleavedInputChannels                          (
        0                                                                    ,
        0                                                                    ,
        buf2                                                                 ,
        bufferProcessor . inputChannelCount                                ) ;
        // Copy user data to host buffer (with conversion if applicable)
      processed = bufferProcessor.CopyInput((void **)&user_buffer,buf2_size) ;
      frames -= processed                                                    ;
    }                                                                        ;
    // Advance
    in . tailBuffer -> AdvanceReadIndex ( read )                             ;
  }                                                                          ;
  // Read hardware
  while ( 0 != frames )                                                      {
    // Check if blocking call must be interrupted
    if ( WaitForSingleObject(hCloseRequest,sleep) != WAIT_TIMEOUT) break     ;
    // Get available frames (must be finding out available frames before call to IAudioCaptureClient_GetBuffer
    // othervise audio glitches will occur inExclusive mode as it seems that WASAPI has some scheduling/
    // processing problems when such busy polling with IAudioCaptureClient_GetBuffer occurs)
    hr = PollGetInputFramesAvailable(&available)                             ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      return UnanticipatedHostError                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    // Wait for more frames to become available
    if ( 0 == available )                                                    {
      // Exclusive mode may require latency of 1 millisecond, thus we shall sleep
      // around 500 microseconds (emulated) to collect packets in time
      if ( in . shareMode != AUDCLNT_SHAREMODE_EXCLUSIVE )                   {
        UINT32 sleep_frames = ( frames < in.framesPerHostCallback            ?
                                frames : in.framesPerHostCallback          ) ;
        sleep  = GetFramesSleepTime                                          (
                   sleep_frames                                              ,
                   in.wavex.Format.nSamplesPerSec                          ) ;
        // wait only for 1/4 of the buffer
        sleep /= 4                                                           ;
        // WASAPI input provides packets, thus expiring packet will result in bad audio
        // limit waiting time to 2 seconds (will always work for smallest buffer in Shared)
        if ( sleep > 2 ) sleep = 2                                           ;
        // Avoid busy waiting, schedule next 1 millesecond wait
        if ( sleep == 0 ) sleep = ThreadIdleScheduler_NextSleep ( &sched )   ;
      } else                                                                 {
        sleep = ThreadIdleScheduler_NextSleep(&sched)                        ;
        if ( sleep != 0 )                                                    {
          ::Sleep ( sleep )                                                  ;
          sleep = 0                                                          ;
        }                                                                    ;
      }                                                                      ;
      continue                                                               ;
    }                                                                        ;
    // Get the available data in the shared buffer.
    hr = captureClient -> GetBuffer                                          (
           &wasapi_buffer                                                    ,
           &available                                                        ,
           &flags                                                            ,
           NULL                                                              ,
           NULL                                                            ) ;
    if ( S_OK != hr )                                                        {
      // Buffer size is too small, waiting
      if ( hr != AUDCLNT_S_BUFFER_EMPTY )                                    {
        LogHostError ( hr )                                                  ;
        goto end                                                             ;
      }                                                                      ;
      continue                                                               ;
    }                                                                        ;
    // Register available frames to processor
    bufferProcessor . setInputFrameCount ( 0 , available )                   ;
    // Register host buffer pointer to processor
    bufferProcessor . setInterleavedInputChannels                            (
      0                                                                      ,
      0                                                                      ,
      wasapi_buffer                                                          ,
      bufferProcessor . inputChannelCount                                  ) ;
    // Copy user data to host buffer (with conversion if applicable)
    processed = bufferProcessor.CopyInput((void **)&user_buffer, frames)     ;
    frames   -= processed                                                    ;
    // Save tail into buffer
    if ( ( frames == 0 ) && ( available > processed ) )                      {
      UINT32 bytes_processed = processed * in.wavex.Format.nBlockAlign       ;
      UINT32 frames_to_save  = available - processed                         ;
      in.tailBuffer->Write(wasapi_buffer + bytes_processed , frames_to_save) ;
    }                                                                        ;
    // Release host buffer
    hr = captureClient -> ReleaseBuffer ( available )                        ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      goto end                                                               ;
    }                                                                        ;
  }                                                                          ;
end:
  // Notify blocking op has ended
  SetEvent ( hBlockingOpStreamRD )                                           ;
  return ( hr != S_OK ? UnanticipatedHostError : NoError )                   ;
}

CaError WaSapiStream::Write(const void * buffer,unsigned long frames)
{
  // UINT32 frames;
  const BYTE        * user_buffer = (const BYTE *)buffer                     ;
  BYTE              * wasapi_buffer                                          ;
  HRESULT             hr          = S_OK                                     ;
  UINT32              i                                                      ;
  UINT32              available                                              ;
  UINT32              sleep       = 0                                        ;
  unsigned long       processed                                              ;
  ThreadIdleScheduler sched                                                  ;
  // validate
  if ( ! running            ) return StreamIsStopped                         ;
  if ( renderClient == NULL ) return BadStreamPtr                            ;
  // Notify blocking op has begun
  ResetEvent ( hBlockingOpStreamWR )                                         ;
  // Use thread scheduling for 500 microseconds (emulated) when wait time for frames is less than
  // 1 milliseconds, emulation helps to normalize CPU consumption and avoids too busy waiting
  ThreadIdleScheduler_Setup ( &sched , 1 , 500/* microseconds */ )           ;
  // Make a local copy of the user buffer pointer(s), this is necessary
  // because PaUtil_CopyOutput() advances these pointers every time it is called
  if ( ! bufferProcessor . userOutputIsInterleaved )                         {
    user_buffer = (const BYTE *)alloca(sizeof(const BYTE *) * bufferProcessor.outputChannelCount) ;
    if ( user_buffer == NULL ) return InsufficientMemory                     ;
    for ( i = 0; i < bufferProcessor . outputChannelCount ; ++i )            {
      ((const BYTE **)user_buffer)[i] = ((const BYTE **)buffer)[i]           ;
    }                                                                        ;
  }                                                                          ;
  // Blocking (potentially, untill 'frames' are consumed) loop
  while ( frames != 0 )                                                      {
    // Check if blocking call must be interrupted
    if ( WaitForSingleObject ( hCloseRequest,sleep ) != WAIT_TIMEOUT) break  ;
    // Get frames available
    hr = PollGetOutputFramesAvailable ( &available )                         ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      goto end                                                               ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    // Wait for more frames to become available
    if ( available == 0 )                                                    {
      UINT32 sleep_frames = ( frames < out . framesPerHostCallback           ?
                              frames : out . framesPerHostCallback         ) ;
      sleep  = GetFramesSleepTime(sleep_frames,out.wavex.Format.nSamplesPerSec ) ;
      // wait only for half of the buffer
      sleep /= 2                                                             ;
      // Avoid busy waiting, schedule next 1 millesecond wait
      if ( sleep == 0 ) sleep = ThreadIdleScheduler_NextSleep ( &sched )     ;
      continue                                                               ;
    }                                                                        ;
    // Keep in 'frmaes' range
    if ( available > frames ) available = frames                             ;
    // Get pointer to host buffer
    hr = renderClient -> GetBuffer ( available, &wasapi_buffer )             ;
    if ( S_OK != hr)                                                         {
      // Buffer size is too big, waiting
      if ( hr == AUDCLNT_E_BUFFER_TOO_LARGE ) continue                       ;
        LogHostError(hr)                                                     ;
        goto end                                                             ;
      }                                                                      ;
      // Keep waiting again (on Vista it was noticed that WASAPI could SOMETIMES return NULL pointer
      // to buffer without returning AUDCLNT_E_BUFFER_TOO_LARGE instead)
      if ( wasapi_buffer == NULL ) continue                                  ;
      // Register available frames to processor
      bufferProcessor . setOutputFrameCount ( 0 , available )                ;
      // Register host buffer pointer to processor
      bufferProcessor . setInterleavedOutputChannels                         (
        0                                                                    ,
        0                                                                    ,
        wasapi_buffer                                                        ,
        bufferProcessor . outputChannelCount                               ) ;
    // Copy user data to host buffer (with conversion if applicable), this call will advance
    // pointer 'user_buffer' to consumed portion of data
    processed = bufferProcessor . CopyOutput                                 (
                  (const void **)&user_buffer                                ,
                  frames                                                   ) ;
    frames -= processed                                                      ;
    // Release host buffer
    hr = renderClient -> ReleaseBuffer ( available , 0 )                     ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      goto end                                                               ;
    }                                                                        ;
  }                                                                          ;
end:
  // Notify blocking op has ended
  SetEvent ( hBlockingOpStreamWR )                                           ;
  return ( hr != S_OK ? UnanticipatedHostError : NoError )                   ;
}

bool WaSapiStream::hasVolume(void)
{
  return CaOR ( CaNotNull ( inVol ) , CaNotNull ( outVol ) ) ;
}

CaVolume WaSapiStream::MinVolume(void)
{
  return 0.0 ;
}

CaVolume WaSapiStream::MaxVolume(void)
{
  return 10000.0 ;
}

CaVolume WaSapiStream::Volume(int atChannel)
{
  CaVolume v = 10000.0                                       ;
  float    s = 1.0                                           ;
  HRESULT  hr                                                ;
  if ( CaNotNull(inVol ) )                                   {
    UINT channels = 0                                        ;
    hr = inVol  -> GetChannelCount ( &channels )             ;
    if ( CaNotEqual ( hr , S_OK ) ) return v                 ;
    if ( ( atChannel <  0                                )  ||
         ( atChannel >= channels                         ) ) {
      hr = inVol->GetMasterVolumeLevelScalar(&s)             ;
      if ( CaNotEqual ( hr , S_OK ) ) return v               ;
      s *= 10000.0                                           ;
      v  = s                                                 ;
    } else                                                   {
      hr = inVol->GetChannelVolumeLevelScalar(atChannel,&s)  ;
      if ( CaNotEqual ( hr , S_OK ) ) return v               ;
      s *= 10000.0                                           ;
      v  = s                                                 ;
    }                                                        ;
  }                                                          ;
  if ( CaNotNull(outVol) )                                   {
    UINT channels = 0                                        ;
    hr = outVol -> GetChannelCount ( &channels )             ;
    if ( CaNotEqual ( hr , S_OK ) ) return v                 ;
    if ( ( atChannel <  0                                )  ||
         ( atChannel >= channels                         ) ) {
      hr = outVol->GetMasterVolumeLevelScalar(&s) ;
      if ( CaNotEqual ( hr , S_OK ) ) return v               ;
      s *= 10000.0                                           ;
      v  = s                                                 ;
    } else                                                   {
      hr = outVol->GetChannelVolumeLevelScalar(atChannel,&s) ;
      if ( CaNotEqual ( hr , S_OK ) ) return v               ;
      s *= 10000.0                                           ;
      v  = s                                                 ;
    }                                                        ;
  }                                                          ;
  return v                                                   ;
}

CaVolume WaSapiStream::setVolume(CaVolume volume,int atChannel)
{
  CaVolume v = 10000.0                                           ;
  float    s = 1.0                                               ;
  HRESULT  hr                                                    ;
  if ( CaNotNull(inVol ) )                                       {
    UINT channels = 0                                            ;
    hr = inVol  -> GetChannelCount ( &channels )                 ;
    if ( CaNotEqual ( hr , S_OK ) ) return v                     ;
    s  = volume                                                  ;
    v  = s                                                       ;
    s /= 10000.0                                                 ;
    if ( ( atChannel <  0                                    )  ||
         ( atChannel >= channels                             ) ) {
      hr = inVol->SetMasterVolumeLevelScalar(s,NULL)             ;
      if ( CaNotEqual ( hr , S_OK ) ) return v                   ;
    } else                                                       {
      hr = inVol->SetChannelVolumeLevelScalar(atChannel,s,NULL)  ;
      if ( CaNotEqual ( hr , S_OK ) ) return v                   ;
    }                                                            ;
  }                                                              ;
  if ( CaNotNull(outVol) )                                       {
    UINT channels = 0                                            ;
    hr = outVol -> GetChannelCount ( &channels )                 ;
    if ( CaNotEqual ( hr , S_OK ) ) return v                     ;
    s  = volume                                                  ;
    v  = s                                                       ;
    s /= 10000.0                                                 ;
    if ( ( atChannel <  0                                    )  ||
         ( atChannel >= channels                             ) ) {
      hr = outVol->SetMasterVolumeLevelScalar(s,NULL)            ;
      if ( CaNotEqual ( hr , S_OK ) ) return v                   ;
    } else                                                       {
      hr = outVol->SetChannelVolumeLevelScalar(atChannel,s,NULL) ;
      if ( CaNotEqual ( hr , S_OK ) ) return v                   ;
    }                                                            ;
  }                                                              ;
  return v                                                       ;
}

CaError WaSapiStream::GetFramesPerHostBuffer (
          unsigned int * nInput              ,
          unsigned int * nOutput             )
{
  if ( NULL != nInput  )                   {
    (*nInput ) = in.framesPerHostCallback  ;
  }                                        ;
  if ( NULL != nOutput )                   {
    (*nOutput) = out.framesPerHostCallback ;
  }                                        ;
  return NoError                           ;
}

HRESULT WaSapiStream::CreateAudioClient (
          CaWaSapiSubStream * pSub      ,
          BOOL                output    ,
          CaError           * pa_error  )
{
  CaError                  error               = NoError                     ;
  HRESULT                  hr                  = 0                           ;
  const WaSapiDeviceInfo * pInfo               = pSub->params.device_info    ;
  const StreamParameters * params              = &(pSub->params.stream_params)  ;
  UINT32                   framesPerLatency    = pSub->params.frames_per_buffer ;
  double                   SampleRate          = pSub->params.sample_rate    ;
  BOOL                     blocking            = pSub->params.blocking       ;
  BOOL                     fullDuplex          = pSub->params.full_duplex    ;
  const UINT32             userFramesPerBuffer = framesPerLatency            ;
  IAudioClient           * audioClient         = NULL                        ;
  if ( doResample && ( NearestSampleRate > 0 ) )                             {
    dPrintf ( ( "Replace sample rate from %d to %d\n"                        ,
                (int)SampleRate                                              ,
                (int)NearestSampleRate                                   ) ) ;
    SampleRate = NearestSampleRate                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Assume default failure due to some reason
  (*pa_error) = InvalidDevice                                                ;
  // Validate parameters
  if ( !pSub || !pInfo || !params )                                          {
    (*pa_error) = BadStreamPtr                                               ;
    return E_POINTER                                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == (UINT32)SampleRate )                                             {
    (*pa_error) = InvalidSampleRate                                          ;
    return E_INVALIDARG                                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Get the audio client
  hr = pInfo->device->Activate                                               (
         ca_IID_IAudioClient                                                 ,
         CLSCTX_ALL                                                          ,
         NULL                                                                ,
         (void **)&audioClient                                             ) ;
  if ( hr != S_OK )                                                          {
    (*pa_error) = InsufficientMemory                                         ;
    LogHostError ( hr )                                                      ;
    goto done                                                                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Get closest format
  error = GetClosestFormat                                                   (
            audioClient                                                      ,
            SampleRate                                                       ,
            params                                                           ,
            pSub  -> shareMode                                               ,
            &pSub -> wavex                                                   ,
            output                                                         ) ;
  if ( NoError != error )                                                    {
    (*pa_error) = error                                                      ;
    LogHostError ( hr = AUDCLNT_E_UNSUPPORTED_FORMAT )                       ;
    // fail, format not supported
    goto done                                                                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Check for Mono <<>> Stereo workaround
  if ( ( params -> channelCount           == 1 )                            &&
       ( pSub   -> wavex.Format.nChannels == 2 )                           ) {
    pSub->monoMixer = _GetMonoToStereoMixer                                  (
                       WaveToCaFormat ( &pSub->wavex )                       ,
                      ( pInfo->flow == eRender                               ?
                        MIX_DIR__1TO2                                        :
                        MIX_DIR__2TO1_L                                  ) ) ;
    if ( pSub->monoMixer == NULL )                                           {
      (*pa_error) = InvalidChannelCount                                      ;
      LogHostError ( hr = AUDCLNT_E_UNSUPPORTED_FORMAT)                      ;
      // fail, no mixer for format
      goto done                                                              ;
    }                                                                        ;
  }                                                                          ;
  // Calculate host buffer size
  if ( (   pSub->shareMode   != AUDCLNT_SHAREMODE_EXCLUSIVE )               &&
       ( ! pSub->streamFlags                                                ||
         ((pSub->streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK) == 0 ) ) ) {
    framesPerLatency = _GetFramesPerHostBuffer                               (
                         userFramesPerBuffer                                 ,
                         params->suggestedLatency                            ,
                         pSub->wavex.Format.nSamplesPerSec                   ,
                         0
    /*,(pSub->streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK ? 0 : 1)*/   ) ;
  } else                                                                     {
    REFERENCE_TIME overall                                                   ;
    // Work 1:1 with user buffer (only polling allows to use >1)
    framesPerLatency += MakeFramesFromHns                                    (
                          SecondsTonano100(params->suggestedLatency)         ,
                          pSub->wavex.Format.nSamplesPerSec                ) ;
    // Use Polling if overall latency is > 5ms as it allows to use 100% CPU in a callback,
    // or user specified latency parameter
    overall = MakeHnsPeriod ( framesPerLatency                               ,
                              pSub->wavex.Format.nSamplesPerSec            ) ;
    if ( ( overall >= (106667*2)/*21.33ms*/)                                ||
         ((INT32)(params->suggestedLatency*100000.0) != 0 /*0.01 msec granularity*/ ) ) {
      framesPerLatency = _GetFramesPerHostBuffer                             (
                           userFramesPerBuffer                               ,
                           params->suggestedLatency                          ,
                           pSub->wavex.Format.nSamplesPerSec                 ,
                           0
           /*, (streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK ? 0 : 1)*/ ) ;
      // Use Polling interface
      pSub -> streamFlags &= ~AUDCLNT_STREAMFLAGS_EVENTCALLBACK              ;
    }                                                                        ;
  }                                                                          ;
  // For full-duplex output resize buffer to be the same as for input
  if ( output && fullDuplex )                                                {
    framesPerLatency = in . framesPerHostCallback                            ;
  }                                                                          ;
  // Avoid 0 frames
  if ( framesPerLatency == 0 )                                               {
    framesPerLatency = MakeFramesFromHns                                     (
                         pInfo->DefaultDevicePeriod                          ,
                         pSub->wavex.Format.nSamplesPerSec                 ) ;
  }                                                                          ;
  // Exclusive Input stream renders data in 6 packets, we must set then the size of
  // single packet, total buffer size, e.g. required latency will be PacketSize * 6
  if ( !output && (pSub->shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE) )         {
    // Do it only for Polling mode
    if ( (pSub->streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK) == 0)       {
      framesPerLatency /= WASAPI_PACKETS_PER_INPUT_BUFFER                    ;
    }                                                                        ;
  }                                                                          ;
  // Calculate aligned period
  _CalculateAlignedPeriod ( pSub , &framesPerLatency , ALIGN_BWD )           ;
  /* Enforce min/max period for device in Shared mode to avoid bad audio quality.
     Avoid doing so for Exclusive mode as alignment will suffer.          */
  if ( pSub->shareMode == AUDCLNT_SHAREMODE_SHARED )                         {
    if (pSub->period < pInfo->DefaultDevicePeriod)                           {
      pSub -> period = pInfo -> DefaultDevicePeriod                          ;
      // Recalculate aligned period
      framesPerLatency = MakeFramesFromHns                                   (
                           pSub->period                                      ,
                           pSub->wavex.Format.nSamplesPerSec               ) ;
      _CalculateAlignedPeriod ( pSub , &framesPerLatency , ALIGN_BWD )       ;
    }                                                                        ;
  } else                                                                     {
    if ( pSub->period < pInfo->MinimumDevicePeriod )                         {
      pSub->period = pInfo->MinimumDevicePeriod                              ;
      // Recalculate aligned period
      framesPerLatency = MakeFramesFromHns                                   (
                           pSub->period                                      ,
                           pSub->wavex.Format.nSamplesPerSec               ) ;
      _CalculateAlignedPeriod ( pSub , &framesPerLatency , ALIGN_FWD )       ;
    }                                                                        ;
  }                                                                          ;
  /* Windows 7 does not allow to set latency lower than minimal device period and will
     return error: AUDCLNT_E_INVALID_DEVICE_PERIOD. Under Vista we enforce the same behavior
     manually for unified behavior on all platforms.                      */
  {
    /* AUDCLNT_E_BUFFER_SIZE_ERROR: Applies to Windows 7 and later.
       Indicates that the buffer duration value requested by an exclusive-mode client is
       out of range. The requested duration value for pull mode must not be greater than
       500 milliseconds; for push mode the duration value must not be greater than 2 seconds.
     */
    if ( pSub->shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE )                    {
      static const REFERENCE_TIME MAX_BUFFER_EVENT_DURATION = 500  * 10000   ;
      static const REFERENCE_TIME MAX_BUFFER_POLL_DURATION  = 2000 * 10000   ;
      if ( pSub->streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK )           {
        // pull mode, max 500ms
        if ( pSub->period > MAX_BUFFER_EVENT_DURATION )                      {
          pSub->period = MAX_BUFFER_EVENT_DURATION                           ;
          // Recalculate aligned period
          framesPerLatency = MakeFramesFromHns                               (
                               pSub->period                                  ,
                               pSub->wavex.Format.nSamplesPerSec         )   ;
          _CalculateAlignedPeriod ( pSub , &framesPerLatency , ALIGN_BWD )   ;
        }                                                                    ;
      } else                                                                 {
        // push mode, max 2000ms
        if ( pSub->period > MAX_BUFFER_POLL_DURATION )                       {
          pSub->period = MAX_BUFFER_POLL_DURATION                            ;
          // Recalculate aligned period
          framesPerLatency = MakeFramesFromHns                               (
                               pSub->period                                  ,
                               pSub->wavex.Format.nSamplesPerSec         )   ;
          _CalculateAlignedPeriod ( pSub , &framesPerLatency , ALIGN_BWD )   ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  // Open the stream and associate it with an audio session
  hr = audioClient -> Initialize                                             (
         pSub ->shareMode                                                    ,
         pSub ->streamFlags                                                  ,
         pSub ->period                                                       ,
         (pSub->shareMode==AUDCLNT_SHAREMODE_EXCLUSIVE ? pSub->period : 0)   ,
         &pSub->wavex.Format                                                 ,
         NULL                                                              ) ;
  /* WASAPI is tricky on large device buffer, sometimes 2000ms can be allocated sometimes
     less. There is no known guaranteed level thus we make subsequent tries by decreasing
     buffer by 100ms per try.                                            */
  while ( ( hr == E_OUTOFMEMORY ) && (pSub->period > (100 * 10000)))         {
    dPrintf ( ( "WASAPI : CreateAudioClient: decreasing buffer size to %d milliseconds\n",
                (pSub->period / 10000) )                                   ) ;
    // Decrease by 100ms and try again
    pSub->period -= (100 * 10000)                                            ;
    // Recalculate aligned period
    framesPerLatency = MakeFramesFromHns                                     (
                         pSub->period                                        ,
                         pSub->wavex.Format.nSamplesPerSec                 ) ;
    _CalculateAlignedPeriod ( pSub , &framesPerLatency , ALIGN_BWD )         ;
    // Release the previous allocations
    SAFE_RELEASE ( audioClient )                                             ;
    // Create a new audio client
    hr = pInfo->device->Activate                                             (
           ca_IID_IAudioClient                                               ,
           CLSCTX_ALL                                                        ,
           NULL                                                              ,
           (void**)&audioClient                                            ) ;
    if ( hr != S_OK )                                                        {
      (*pa_error) = InsufficientMemory                                       ;
       LogHostError ( hr )                                                   ;
       goto done                                                             ;
    }                                                                        ;
    // Open the stream and associate it with an audio session
    hr = audioClient->Initialize                                             (
           pSub -> shareMode                                                 ,
           pSub -> streamFlags                                               ,
           pSub -> period                                                    ,
          (pSub->shareMode==AUDCLNT_SHAREMODE_EXCLUSIVE ? pSub->period : 0)  ,
          &pSub->wavex.Format                                                ,
           NULL                                                            ) ;
  }                                                                          ;
  /*! WASAPI buffer size failure. Fallback to using default size.           */
  if ( AUDCLNT_E_BUFFER_SIZE_ERROR == hr )                                   {
    // Use default
    pSub->period = pInfo->DefaultDevicePeriod                                ;
    dPrintf ( ( "WASAPI : CreateAudioClient: correcting buffer size to device default\n" ) ) ;
    // Release the previous allocations
    SAFE_RELEASE ( audioClient )                                             ;
    // Create a new audio client
    hr = pInfo->device->Activate                                             (
           ca_IID_IAudioClient                                               ,
           CLSCTX_ALL                                                        ,
           NULL                                                              ,
           (void**)&audioClient                                            ) ;
    if ( hr != S_OK )                                                        {
      (*pa_error) = InsufficientMemory                                       ;
      LogHostError ( hr )                                                    ;
      goto done                                                              ;
    }                                                                        ;
    // Open the stream and associate it with an audio session
    hr = audioClient->Initialize                                             (
           pSub->shareMode                                                   ,
           pSub->streamFlags                                                 ,
           pSub->period                                                      ,
          (pSub->shareMode==AUDCLNT_SHAREMODE_EXCLUSIVE ? pSub->period : 0)  ,
          &pSub->wavex.Format                                                ,
           NULL                                                            ) ;
  }                                                                          ;
  /* If the requested buffer size is not aligned. Can be triggered by Windows 7 and up.
      Should not be be triggered ever as we do align buffers always with _CalculateAlignedPeriod. */
  if ( hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED )                             {
    UINT32 frames = 0                                                        ;
    // Get the next aligned frame
    hr = audioClient -> GetBufferSize ( &frames )                            ;
    if ( hr != S_OK )                                                        {
      (*pa_error) = InvalidDevice                                            ;
      LogHostError ( hr )                                                    ;
      goto done                                                              ;
    }                                                                        ;
    dPrintf ( ( "WASAPI : CreateAudioClient: aligning buffer size to % frames\n" , frames ) ) ;
    // Release the previous allocations
    SAFE_RELEASE ( audioClient )                                             ;
    // Create a new audio client
    hr = pInfo->device->Activate                                             (
           ca_IID_IAudioClient                                               ,
           CLSCTX_ALL                                                        ,
           NULL                                                              ,
           (void**)&audioClient                                            ) ;
    if ( hr != S_OK )                                                        {
      (*pa_error) = InsufficientMemory                                       ;
      LogHostError ( hr )                                                    ;
      goto done                                                              ;
    }                                                                        ;
    // Get closest format
    error = GetClosestFormat                                                 (
              audioClient                                                    ,
              SampleRate                                                     ,
              params                                                         ,
              pSub->shareMode                                                ,
              &pSub->wavex                                                   ,
              output                                                       ) ;
    if ( NoError != error )                                                  {
      (*pa_error) = error                                                    ;
      // fail, format not supported
      LogHostError ( hr = AUDCLNT_E_UNSUPPORTED_FORMAT )                     ;
      goto done                                                              ;
    }                                                                        ;
    // Check for Mono >> Stereo workaround
    if ( ( params -> channelCount               == 1 )                      &&
         ( pSub   -> wavex . Format . nChannels == 2 )                     ) {
      // Select mixer
      pSub->monoMixer = _GetMonoToStereoMixer                                (
                          WaveToCaFormat(&pSub->wavex)                       ,
                         (pInfo->flow == eRender ? MIX_DIR__1TO2             :
                                                   MIX_DIR__2TO1_L       ) ) ;
      if ( pSub->monoMixer == NULL )                                         {
        (*pa_error) = InvalidChannelCount                                    ;
        LogHostError ( hr = AUDCLNT_E_UNSUPPORTED_FORMAT )                   ;
        // fail, no mixer for format
        goto done                                                            ;
      }                                                                      ;
    }                                                                        ;
    // Calculate period
    pSub->period = MakeHnsPeriod(frames, pSub->wavex.Format.nSamplesPerSec)  ;
    // Open the stream and associate it with an audio session
    hr = audioClient->Initialize                                             (
            pSub->shareMode                                                  ,
            pSub->streamFlags                                                ,
            pSub->period                                                     ,
           (pSub->shareMode==AUDCLNT_SHAREMODE_EXCLUSIVE ? pSub->period : 0) ,
           &pSub->wavex.Format                                               ,
            NULL                                                           ) ;
    if ( hr != S_OK )                                                        {
      (*pa_error) = InvalidDevice                                            ;
      LogHostError ( hr )                                                    ;
      goto done                                                              ;
    }                                                                        ;
  } else
  if ( hr != S_OK )                                                          {
    (*pa_error) = InvalidDevice                                              ;
    LogHostError ( hr )                                                      ;
    goto done                                                                ;
  }                                                                          ;
  // Set client
  pSub -> clientParent  = audioClient                                        ;
  pSub -> clientParent -> AddRef ( )                                         ;
  // Recalculate buffers count
  _RecalculateBuffersCount                                                   (
    pSub                                                                     ,
    userFramesPerBuffer                                                      ,
    MakeFramesFromHns(pSub->period, pSub->wavex.Format.nSamplesPerSec)       ,
    fullDuplex                                                             ) ;
  // No error, client is succesfully created
  (*pa_error) = NoError                                                      ;
done:
  // Clean up
  SAFE_RELEASE(audioClient)                                                  ;
  return hr                                                                  ;
}

CaError WaSapiStream::ActivateAudioClientOutput(void)
{
  HRESULT hr              = 0                                                ;
  CaError result          = NoError                                          ;
  UINT32  maxBufferSize   = 0                                                ;
  CaTime  buffer_latency  = 0                                                ;
  UINT32  framesPerBuffer = out.params.frames_per_buffer                     ;
  ////////////////////////////////////////////////////////////////////////////
  hr = CreateAudioClient ( &out , TRUE , &result )                           ;
  if ( hr != S_OK )                                                          {
    LogCaError ( result )                                                    ;
    return result                                                            ;
  }                                                                          ;
  LogWAVEFORMATEXTENSIBLE ( & out . wavex )                                  ;
  // Activate volume
  outVol = NULL                                                              ;
  // Get the output audio end point volume
  hr = out.params.device_info->device->Activate                               (
         ca_IID_IAudioEndpointVolume                                         ,
         CLSCTX_ALL                                                          ,
         NULL                                                                ,
         (void **)&outVol                                                  ) ;
  if ( hr != S_OK ) outVol = NULL                                            ;
  // Get max possible buffer size to check if it is not less than that we request
  hr = out . clientParent -> GetBufferSize ( &maxBufferSize )                ;
  if ( hr != S_OK )                                                          {
    LogHostError ( hr                     )                                  ;
    LogCaError   ( result = InvalidDevice )                                  ;
    return result                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Correct buffer to max size if it maxed out result of GetBufferSize
  out . bufferSize = maxBufferSize                                           ;
  // Get interface latency (actually uneeded as we calculate latency from the size
  // of maxBufferSize).
  hr = out . clientParent -> GetStreamLatency ( & out . deviceLatency )      ;
  if ( hr != S_OK )                                                          {
    LogHostError ( hr                     )                                  ;
    LogCaError   ( result = InvalidDevice )                                  ;
    return result                                                            ;
  }                                                                          ;
  //stream->out.latencySeconds = nano100ToSeconds(stream->out.deviceLatency);
  // Number of frames that are required at each period
  // Calculate frames per single buffer, if buffers > 1 then always framesPerBuffer
  out . framesPerHostCallback = maxBufferSize                                ;
  out . framesPerBuffer       = ( out . userBufferAndHostMatch               ?
                                  out . framesPerHostCallback                :
                                  framesPerBuffer                          ) ;
  // Calculate buffer latency
  buffer_latency = (CaTime)maxBufferSize / out.wavex.Format.nSamplesPerSec   ;
  // Append buffer latency to interface latency in shared mode (see GetStreamLatency notes)
  out.latencySeconds = buffer_latency                                        ;
  dPrintf ( ( "%s\n"
              "  framesPerUser [ %d ]\n"
              "  framesPerHost [ %d ]\n"
              "  latency       [ %.02fms ]\n"
              "  exclusive     [ %s ]\n"
              "  wow64_fix     [ %s ]\n"
              "  mode          [ %s ]\n"                                     ,
              __FUNCTION__                                                   ,
              (UINT32)framesPerBuffer                                        ,
              (UINT32)out.framesPerHostCallback                              ,
              (float)(out.latencySeconds*1000.0f)                            ,
              (out.shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE ? "YES" : "NO")  ,
              (out.params.wow64_workaround ? "YES" : "NO")                   ,
              (out.streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK ? "EVENT" : "POLL")));
  return NoError                                                             ;
}

CaError WaSapiStream::ActivateAudioClientInput(void)
{
  HRESULT hr              = 0                                                ;
  CaError result          = NoError                                          ;
  UINT32  maxBufferSize   = 0                                                ;
  CaTime  buffer_latency  = 0                                                ;
  UINT32  framesPerBuffer = in.params.frames_per_buffer                      ;
  ////////////////////////////////////////////////////////////////////////////
  // Create Audio client
  hr = CreateAudioClient ( &in , FALSE , &result )                           ;
  if ( hr != S_OK )                                                          {
    LogCaError ( result )                                                    ;
    goto error                                                               ;
  }                                                                          ;
  LogWAVEFORMATEXTENSIBLE ( &in . wavex )                                    ;
  ////////////////////////////////////////////////////////////////////////////
  // Create volume mgr
  inVol = NULL                                                               ;
  // Get the input audio end point volume
  hr = in.params.device_info->device->Activate                               (
         ca_IID_IAudioEndpointVolume                                         ,
         CLSCTX_ALL                                                          ,
         NULL                                                                ,
         (void **)&inVol                                                   ) ;
  if ( hr != S_OK ) inVol = NULL                                             ;
  // Get max possible buffer size to check if it is not less than that we request
  hr = in . clientParent -> GetBufferSize ( &maxBufferSize )                 ;
  if ( hr != S_OK )                                                          {
    LogHostError ( hr                     )                                  ;
    LogCaError   ( result = InvalidDevice )                                  ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Correct buffer to max size if it maxed out result of GetBufferSize
  in . bufferSize = maxBufferSize                                            ;
  // Get interface latency (actually uneeded as we calculate latency from the size
  // of maxBufferSize).
  hr = in . clientParent -> GetStreamLatency ( & in . deviceLatency )        ;
  if ( hr != S_OK )                                                          {
    LogHostError ( hr                     )                                  ;
    LogCaError   ( result = InvalidDevice )                                  ;
    goto error                                                               ;
  }                                                                          ;
  //stream->in.latencySeconds = nano100ToSeconds(stream->in.deviceLatency);
  // Number of frames that are required at each period
  in.framesPerHostCallback = maxBufferSize                                   ;
  // Calculate frames per single buffer, if buffers > 1 then always framesPerBuffer
  in.framesPerBuffer = (in.userBufferAndHostMatch ? in.framesPerHostCallback : framesPerBuffer) ;
  // Calculate buffer latency
  buffer_latency     = (CaTime) maxBufferSize/in.wavex.Format.nSamplesPerSec ;
  // Append buffer latency to interface latency in shared mode (see GetStreamLatency notes)
  in.latencySeconds  = buffer_latency                                        ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "%s\n"
              "\tframesPerUser [ %d ]\n"
              "\tframesPerHost [ %d ]\n"
              "\tlatency       [ %.02fms ]\n"
              "\texclusive     [ %s ]\n"
              "\twow64_fix     [ %s ]\n"
              "\tmode          [ %s ]\n"                                     ,
              __FUNCTION__                                                   ,
              (UINT32)framesPerBuffer                                        ,
              (UINT32)in.framesPerHostCallback                               ,
              (float)(in.latencySeconds*1000.0f)                             ,
              (in.shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE ? "YES" : "NO")   ,
              (in.params.wow64_workaround ? "YES" : "NO")                    ,
              (in.streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK ? "EVENT" : "POLL" ) ) ) ;
  return NoError                                                             ;
error:
  return result                                                              ;
}

HRESULT WaSapiStream::UnmarshalStreamComPointers(void)
{
  HRESULT hResult         = S_OK                                             ;
  HRESULT hFirstBadResult = S_OK                                             ;
  ////////////////////////////////////////////////////////////////////////////
  captureClient    = NULL                                                    ;
  renderClient     = NULL                                                    ;
  in  . clientProc = NULL                                                    ;
  out . clientProc = NULL                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != in . clientParent )                                           {
    // SubStream pointers
    hResult = UnmarshalSubStreamComPointers ( &in )                          ;
    if ( hResult != S_OK)                                                    {
      hFirstBadResult = (hFirstBadResult==S_OK) ? hResult : hFirstBadResult  ;
    }                                                                        ;
    // IAudioCaptureClient
    hResult = CoGetInterfaceAndReleaseStream                                 (
                captureClientStream                                          ,
                ca_IID_IAudioCaptureClient                                   ,
                (LPVOID *) &captureClient                                  ) ;
    captureClientStream = NULL                                               ;
    if ( hResult != S_OK )                                                   {
      hFirstBadResult = (hFirstBadResult==S_OK) ? hResult : hFirstBadResult  ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != out . clientParent )                                          {
    // SubStream pointers
    hResult = UnmarshalSubStreamComPointers ( &out )                         ;
    if ( hResult != S_OK )                                                   {
      hFirstBadResult = (hFirstBadResult==S_OK) ? hResult : hFirstBadResult  ;
    }                                                                        ;
    // IAudioRenderClient
    hResult = CoGetInterfaceAndReleaseStream                                 (
                renderClientStream                                           ,
                ca_IID_IAudioRenderClient                                    ,
                (LPVOID *) &renderClient                                   ) ;
    renderClientStream = NULL                                                ;
    if ( hResult != S_OK )                                                   {
      hFirstBadResult = (hFirstBadResult==S_OK) ? hResult : hFirstBadResult  ;
    }                                                                        ;
  }                                                                          ;
  return hFirstBadResult                                                     ;
}

void WaSapiStream::ReleaseUnmarshaledComPointers(void)
{
  // Release AudioClient services first
  SAFE_RELEASE ( captureClient )            ;
  SAFE_RELEASE ( renderClient  )            ;
  // Release AudioClients
  ReleaseUnmarshaledSubComPointers ( &in  ) ;
  ReleaseUnmarshaledSubComPointers ( &out ) ;
}

HRESULT WaSapiStream::PollGetOutputFramesAvailable(UINT32 * available)
{
  HRESULT hr      = 0                                     ;
  UINT32  frames  = out . framesPerHostCallback           ;
  UINT32  padding = 0                                     ;
  /////////////////////////////////////////////////////////
  (*available) = 0                                        ;
  hr = out . clientProc -> GetCurrentPadding ( &padding ) ;
  // get read position
  if ( S_OK != hr ) return LogHostError ( hr )            ;
  // get available
  frames -= padding                                       ;
  // set
  (*available) = frames                                   ;
  return hr                                               ;
}

///////////////////////////////////////////////////////////////////////////////

HRESULT WaSapiStream::PollGetInputFramesAvailable(UINT32 * available)
{
  HRESULT hr                                                 ;
  (*available) = 0                                           ;
  // GetCurrentPadding() has opposite meaning to Output stream
  hr = in . clientProc->GetCurrentPadding(available)         ;
  if ( S_OK != hr ) return LogHostError ( hr )               ;
  return hr                                                  ;
}

///////////////////////////////////////////////////////////////////////////////

HRESULT WaSapiStream::ProcessOutputBuffer(UINT32 frames)
{
  HRESULT hr                                                                 ;
  BYTE  * data       = NULL                                                  ;
  UINT32  realFrames = frames                                                ;
  ////////////////////////////////////////////////////////////////////////////
  if ( doResample && ( NULL != Resampler ) )                                 {
    #if defined(FFMPEGLIB)
    CaResampler * resampler = (CaResampler *)Resampler                       ;
    realFrames = resampler->ToFrames(frames)                                 ;
    #endif
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  hr = renderClient -> GetBuffer ( realFrames , &data )                      ;
  if ( S_OK != hr )                                                          {
    if ( out . shareMode == AUDCLNT_SHAREMODE_SHARED )                       {
      if ( hr == AUDCLNT_E_BUFFER_TOO_LARGE ) return S_OK                    ;
    } else return LogHostError ( hr )                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // Process data
  if ( NULL != out . monoMixer )                                             {
    // expand buffer
    UINT32 mono_frames_size = realFrames                                     *
                              ( out.wavex.Format.wBitsPerSample / 8 )        ;
    if ( mono_frames_size > out . monoBufferSize )                           {
      out.monoBuffer = realloc ( out . monoBuffer                            ,
                                (out.monoBufferSize = mono_frames_size)    ) ;
    }                                                                        ;
    // process
    WaSapiHostProcessingLoop                                                 (
      NULL                                                                   ,
      0                                                                      ,
      out . monoBuffer                                                       ,
      frames                                                                 ,
      this                                                                 ) ;
    // mix 1 to 2 channels
    out . monoMixer ( data , out . monoBuffer , realFrames )                 ;
  } else                                                                     {
    WaSapiHostProcessingLoop                                                 (
      NULL                                                                   ,
      0                                                                      ,
      data                                                                   ,
      frames                                                                 ,
      this                                                                 ) ;
  }                                                                          ;
  // Release buffer
  hr = renderClient -> ReleaseBuffer ( realFrames , 0 )                      ;
  if ( S_OK != hr) LogHostError ( hr )                                       ;
  return hr                                                                  ;
}

//////////////////////////////////////////////////////////////////////////////

HRESULT WaSapiStream::ProcessInputBuffer(void)
{
  HRESULT hr         = S_OK                                                  ;
  UINT32  frames     = 0                                                     ;
  BYTE  * data       = NULL                                                  ;
  DWORD   flags      = 0                                                     ;
  ////////////////////////////////////////////////////////////////////////////
  for ( ; ; )                                                                {
    // Check if blocking call must be interrupted
    if ( WaitForSingleObject(hCloseRequest,0) != WAIT_TIMEOUT ) break        ;
    // Findout if any frames available
    frames = 0                                                               ;
    hr     = PollGetInputFramesAvailable ( &frames )                         ;
    if ( S_OK != hr ) return hr                                              ;
    // Empty/consumed buffer
    if ( 0 == frames ) break                                                 ;
    // Get the available data in the shared buffer.
    hr = captureClient -> GetBuffer ( &data,&frames,&flags,NULL,NULL )       ;
    if ( S_OK != hr )                                                        {
      if ( hr == AUDCLNT_S_BUFFER_EMPTY )                                    {
        hr = S_OK                                                            ;
        break                                                                ;
        // Empty/consumed buffer
      }                                                                      ;
      return LogHostError ( hr )                                             ;
    }                                                                        ;
    // Detect silence
    // if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
    // data = NULL;
    // Process data
    if ( NULL != in . monoMixer )                                            {
      // expand buffer
      UINT32 mono_frames_size = frames                                       *
                                ( in.wavex.Format.wBitsPerSample / 8)        ;
      if ( mono_frames_size > in . monoBufferSize )                          {
        in . monoBuffer = realloc ( in . monoBuffer                          ,
                                   (in . monoBufferSize=mono_frames_size ) ) ;
      }                                                                      ;
      // mix 1 to 2 channels
      in . monoMixer ( in . monoBuffer , data , frames )                     ;
      // process
      WaSapiHostProcessingLoop                                               (
        (BYTE *) in . monoBuffer                                             ,
        frames                                                               ,
        NULL                                                                 ,
        0                                                                    ,
        this                                                               ) ;
    } else                                                                   {
      WaSapiHostProcessingLoop                                               (
        data                                                                 ,
        frames                                                               ,
        NULL                                                                 ,
        0                                                                    ,
        this                                                               ) ;
    }                                                                        ;
    // Release buffer
    hr = captureClient -> ReleaseBuffer ( frames )                           ;
    if ( S_OK != hr ) return LogHostError ( hr )                             ;
//    break                                                                    ;
  }                                                                          ;
  return hr                                                                  ;
}

///////////////////////////////////////////////////////////////////////////////

void WaSapiStream::StreamOnStop(void)
{
  // Stop INPUT/OUTPUT clients
  if ( 0 == bBlocking )                                                      {
    if ( NULL != in  . clientProc   ) in  . clientProc   -> Stop ( )         ;
    if ( NULL != out . clientProc   ) out . clientProc   -> Stop ( )         ;
  } else                                                                     {
    if ( NULL != in  . clientParent ) in  . clientParent -> Stop ( )         ;
    if ( NULL != out . clientParent ) out . clientParent -> Stop ( )         ;
  }                                                                          ;
  // Restore thread priority
  if ( NULL != hAvTask )                                                     {
    ThreadPriorityRevert ( hAvTask )                                         ;
    hAvTask = NULL                                                           ;
  }                                                                          ;
  // Notify
  if ( NULL != conduit )                                                     {
    if ( 0 == bBlocking )                                                    {
      if ( NULL != in  . clientProc   )                                      {
        conduit -> finish ( Conduit::InputDirection  , Conduit::Correct )    ;
      }                                                                      ;
      if ( NULL != out . clientProc   )                                      {
        conduit -> finish ( Conduit::OutputDirection , Conduit::Correct )    ;
      }                                                                      ;
    } else                                                                   {
      if ( NULL != in  . clientParent )                                      {
        conduit -> finish ( Conduit::InputDirection  , Conduit::Correct )    ;
      }                                                                      ;
      if ( NULL != out . clientParent )                                      {
        conduit -> finish ( Conduit::OutputDirection , Conduit::Correct )    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
}

///////////////////////////////////////////////////////////////////////////////

HRESULT WaSapiStream::MarshalStreamComPointers(void)
{
  HRESULT hResult     = S_OK                                                 ;
  captureClientStream = NULL                                                 ;
  renderClientStream  = NULL                                                 ;
  in  . clientStream  = NULL                                                 ;
  out . clientStream  = NULL                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != in . clientParent )                                           {
    // SubStream pointers
    hResult = MarshalSubStreamComPointers ( &in )                            ;
    if ( hResult != S_OK ) goto marshal_error                                ;
    // IAudioCaptureClient
    hResult = CoMarshalInterThreadInterfaceInStream                          (
                ca_IID_IAudioCaptureClient                                   ,
                (LPUNKNOWN)captureClientParent                               ,
                &captureClientStream                                       ) ;
    if (hResult != S_OK) goto marshal_error                                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != out . clientParent )                                          {
    // SubStream pointers
    hResult = MarshalSubStreamComPointers(&out)                              ;
    if ( hResult != S_OK ) goto marshal_error                                ;
    // IAudioRenderClient
    hResult = CoMarshalInterThreadInterfaceInStream                          (
                ca_IID_IAudioRenderClient                                    ,
                (LPUNKNOWN)renderClientParent                                ,
                &renderClientStream                                        ) ;
    if ( hResult != S_OK ) goto marshal_error                                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return hResult                                                             ;
  // If marshaling error occurred, make sure to release everything.
marshal_error:
  UnmarshalStreamComPointers    ( )                                          ;
  ReleaseUnmarshaledComPointers ( )                                          ;
  return hResult                                                             ;
}

///////////////////////////////////////////////////////////////////////////////

void WaSapiStream::Finish(void)
{ // Issue command to thread to stop processing and wait for thread exit
  if ( ! bBlocking )                                                         {
    ::SignalObjectAndWait                                                    (
        hCloseRequest                                                        ,
        hThreadExit                                                          ,
        INFINITE                                                             ,
        FALSE                                                              ) ;
  } else                                                                     {
    // Blocking mode does not own thread
    // Signal close event and wait for each of 2 blocking operations to complete
    if ( NULL != out . clientParent )                                        {
      ::SignalObjectAndWait                                                  (
          hCloseRequest                                                      ,
          hBlockingOpStreamWR                                                ,
          INFINITE                                                           ,
          TRUE                                                             ) ;
    }                                                                        ;
    if ( NULL != out . clientParent )                                        {
      ::SignalObjectAndWait                                                  (
          hCloseRequest                                                      ,
          hBlockingOpStreamRD                                                ,
          INFINITE                                                           ,
          TRUE                                                             ) ;
    }                                                                        ;
    // Process stop
    StreamOnStop ( )                                                         ;
  }                                                                          ;
  // Cleanup handles
  Cleanup ( )                                                                ;
  running = FALSE                                                            ;
}

void WaSapiStream::Cleanup(void)
{ // Close thread handles to allow restart
  SAFE_CLOSE ( hThread             ) ;
  SAFE_CLOSE ( hThreadStart        ) ;
  SAFE_CLOSE ( hThreadExit         ) ;
  SAFE_CLOSE ( hCloseRequest       ) ;
  SAFE_CLOSE ( hBlockingOpStreamRD ) ;
  SAFE_CLOSE ( hBlockingOpStreamWR ) ;
}

///////////////////////////////////////////////////////////////////////////////

WaSapiDeviceInfo:: WaSapiDeviceInfo (void)
                 : DeviceInfo       (    )
{
  Initialize ( ) ;
}

WaSapiDeviceInfo::~WaSapiDeviceInfo (void)
{
}

void WaSapiDeviceInfo::Initialize(void)
{
  device              = NULL                                   ;
  state               = 0                                      ;
  DefaultDevicePeriod = 0                                      ;
  MinimumDevicePeriod = 0                                      ;
  memset ( szDeviceID     , 0 , sizeof(WCHAR) * MAX_STR_LEN  ) ;
  memset ( &flow          , 0 , sizeof(EDataFlow           ) ) ;
  memset ( &DefaultFormat , 0 , sizeof(WAVEFORMATEXTENSIBLE) ) ;
  memset ( &formFactor    , 0 , sizeof(EndpointFormFactor  ) ) ;
}

///////////////////////////////////////////////////////////////////////////////

WaSapiHostApiInfo:: WaSapiHostApiInfo (void)
                  : HostApiInfo       (    )
{
}

WaSapiHostApiInfo::~WaSapiHostApiInfo (void)
{
}

///////////////////////////////////////////////////////////////////////////////

WaSapiHostApi:: WaSapiHostApi (void)
              : HostApi       (    )
{
  ::memset ( &comInitializationResult , 0 , sizeof(CaComInitializationResult) ) ;
  ::memset ( defaultRenderer          , 0 , sizeof(WCHAR)*MAX_STR_LEN         ) ;
  ::memset ( defaultCapturer          , 0 , sizeof(WCHAR)*MAX_STR_LEN         ) ;
  ///////////////////////////////////////////////////////////////////////////////
  allocations             = NULL                                             ;
  enumerator              = NULL                                             ;
  devInfo                 = NULL                                             ;
  deviceCount             = 0                                                ;
  useWOW64Workaround      = 0                                                ;
  ////////////////////////////////////////////////////////////////////////////
  #if defined(FFMPEGLIB)
  CanDoResample           = true                                             ;
  #else
  CanDoResample           = false                                            ;
  #endif
}

WaSapiHostApi::~WaSapiHostApi(void)
{
}

HostApi::Encoding WaSapiHostApi::encoding(void) const
{
  return UTF8 ;
}

bool WaSapiHostApi::hasDuplex(void) const
{
  return true ;
}

CaError WaSapiHostApi::Open                          (
          Stream                ** s                 ,
          const StreamParameters * inputParameters   ,
          const StreamParameters * outputParameters  ,
          double                   SampleRate        ,
          CaUint32                 framesPerCallback ,
          CaStreamFlags            streamFlags       ,
          Conduit                * conduit           )
{
  CaError              result = NoError                                      ;
  HRESULT              hr     = 0                                            ;
  WaSapiStream       * stream = NULL                                         ;
  int                  inputChannelCount                                     ;
  int                  outputChannelCount                                    ;
  CaSampleFormat       inputSampleFormat                                     ;
  CaSampleFormat       outputSampleFormat                                    ;
  CaSampleFormat       hostInputSampleFormat                                 ;
  CaSampleFormat       hostOutputSampleFormat                                ;
  CaWaSapiStreamInfo * inputStreamInfo  = NULL                               ;
  CaWaSapiStreamInfo * outputStreamInfo = NULL                               ;
  WaSapiDeviceInfo   * info             = NULL                               ;
  ULONG                framesPerHostCallback                                 ;
  CaHostBufferSizeMode bufferMode                                            ;
  const BOOL           fullDuplex = ( ( inputParameters  != NULL )          &&
                                      ( outputParameters != NULL )         ) ;
  ////////////////////////////////////////////////////////////////////////////
  result = isValid ( inputParameters , outputParameters , SampleRate )       ;
  if ( NoError != result ) return LogCaError ( result )                      ;
  // Validate platform specific flags
  if ( ( streamFlags & csfPlatformSpecificFlags) != 0)                       {
    /* unexpected platform specific flag                                    */
    LogCaError ( result = InvalidFlag )                                      ;
    goto error                                                               ;
  }                                                                          ;
  // Allocate memory for PaWasapiStream
  stream  = new WaSapiStream ( )                                             ;
  stream -> debugger = debugger                                              ;
  CaExprCorrect ( CaIsNull ( stream ) , InsufficientMemory )                 ;
  // Default thread priority is Audio: for exclusive mode we will use Pro Audio.
  stream->nThreadPriority = eThreadPriorityAudio                             ;
  // Set default number of frames: paFramesPerBufferUnspecified
  if ( framesPerCallback == Stream::FramesPerBufferUnspecified )             {
    UINT32 framesPerBufferIn  = 0, framesPerBufferOut = 0                    ;
    if ( inputParameters != NULL )                                           {
      info = &devInfo [ inputParameters->device ]                            ;
      framesPerBufferIn = MakeFramesFromHns(info->DefaultDevicePeriod, (UINT32)SampleRate);
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( outputParameters != NULL )                                          {
      info = & devInfo [ outputParameters->device ]                          ;
      framesPerBufferOut = MakeFramesFromHns                                 (
                             info->DefaultDevicePeriod                       ,
                             (UINT32)SampleRate                            ) ;
    }                                                                        ;
    // choosing maximum default size
    framesPerCallback = max ( framesPerBufferIn , framesPerBufferOut )       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( framesPerCallback == 0 )                                              {
    framesPerCallback = ((UINT32)SampleRate / 100) * 2                       ;
  }                                                                          ;
  // Try create device: Input
  if ( inputParameters != NULL )                                             {
    inputChannelCount = inputParameters->channelCount                        ;
    inputSampleFormat = inputParameters->sampleFormat                        ;
    inputStreamInfo   = (CaWaSapiStreamInfo *)inputParameters->streamInfo ;
    info              = &devInfo [ inputParameters -> device ]               ;
    stream->in.flags  = ( inputStreamInfo ? inputStreamInfo->flags : 0 )     ;
    // Select Exclusive/Shared mode
    stream->in.shareMode = AUDCLNT_SHAREMODE_SHARED                          ;
    if ( ( inputStreamInfo != NULL                                        ) &&
         ( inputStreamInfo -> flags & CaWaSapiExclusive )                 )  {
      // Boost thread priority
      stream->nThreadPriority = eThreadPriorityProAudio                      ;
      // Make Exclusive
      stream->in.shareMode = AUDCLNT_SHAREMODE_EXCLUSIVE                     ;
    }                                                                        ;
    // If user provided explicit thread priority level, use it
    if ( (inputStreamInfo != NULL                                         ) &&
         (inputStreamInfo->flags & CaWaSapiThread                       ) )  {
      if ((inputStreamInfo->threadPriority >  eThreadPriorityNone         ) &&
          (inputStreamInfo->threadPriority <= eThreadPriorityWindowManager)) {
        stream->nThreadPriority = inputStreamInfo->threadPriority            ;
      }                                                                      ;
    }                                                                        ;
    // Choose processing mode
    stream->in.streamFlags = (stream->in.shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE ?
                              AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0        ) ;
    if ( useWOW64Workaround )                                                {
      // polling interface
      stream->in.streamFlags = 0                                             ;
    } else
    if ( conduit == NULL    )                                                {
       // polling interface
      stream->in.streamFlags = 0                                             ;
    } else
    if ( ( inputStreamInfo != NULL                                        ) &&
         ( inputStreamInfo->flags & CaWaSapiPolling                     ) )  {
      // polling interface
      stream->in.streamFlags = 0                                             ;
    } else
    if ( fullDuplex )                                                        {
      // polling interface is implemented for full-duplex mode also
      stream->in.streamFlags = 0                                             ;
    }                                                                        ;
    // Fill parameters for Audio Client creation
    stream->in.params.device_info       = info                               ;
    stream->in.params.stream_params     = (*inputParameters)                 ;
    if ( inputStreamInfo != NULL )                                           {
      stream->in.params.wasapi_params   = (*inputStreamInfo)                 ;
      stream->in.params.stream_params.streamInfo = &stream->in.params.wasapi_params;
    }                                                                        ;
    stream->in.params.frames_per_buffer = framesPerCallback                  ;
    stream->in.params.sample_rate       = SampleRate                         ;
    stream->in.params.blocking          = ( conduit == NULL )                ;
    stream->in.params.full_duplex       = fullDuplex                         ;
    stream->in.params.wow64_workaround  = useWOW64Workaround                 ;
    // Create and activate audio client
    if ( CanDoResample )                                                     {
      double nearest = Nearest(NULL,inputParameters,SampleRate)              ;
      if ( ((int)nearest) != ((int)SampleRate) )                             {
        dPrintf ( ( "A resampling from %d to %d is needed.\n"                ,
                    (int)nearest                                             ,
                    (int)SampleRate                                      ) ) ;
        if ( stream -> setResampling                                         (
                         nearest                                             ,
                         SampleRate                                          ,
                         inputChannelCount                                   ,
                         inputSampleFormat                               ) ) {
          stream->NearestSampleRate = nearest                                ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
    // Create and activate audio client
    hr = stream -> ActivateAudioClientInput ( )                              ;
    if ( hr != S_OK )                                                        {
      LogCaError ( result = InvalidDevice )                                  ;
      goto error                                                             ;
    }                                                                        ;
    // Get closest format
    hostInputSampleFormat = ClosestFormat                                    (
                              WaveToCaFormat ( &stream->in.wavex )           ,
                              inputSampleFormat                            ) ;
    // Set user-side custom host processor
    if ( ( inputStreamInfo != NULL                                        ) &&
         ( inputStreamInfo->flags & CaWaSapiRedirectHostProcessor ) )        {
      // New implementation cancelled Host Processor Override
    }                                                                        ;
    // Only get IAudioCaptureClient input once here instead of getting it at multiple places based on the use
    hr = stream -> in . clientParent -> GetService                           (
           ca_IID_IAudioCaptureClient                                        ,
           (void **)&stream->captureClientParent                           ) ;
    if ( hr != S_OK )                                                        {
      LogHostError ( hr )                                                    ;
      LogCaError ( result = UnanticipatedHostError )                         ;
      goto error                                                             ;
    }                                                                        ;
    // Create ring buffer for blocking mode (It is needed because we fetch Input packets, not frames,
    // and thus we have to save partial packet if such remains unread)
    if ( stream -> in . params . blocking == TRUE )                          {
      UINT32 bufferFrames = ALIGN_NEXT_POW2((stream->in.framesPerHostCallback /
                                             WASAPI_PACKETS_PER_INPUT_BUFFER  ) * 2 ) ;
      UINT32 frameSize    = stream->in.wavex.Format.nBlockAlign              ;
      // buffer
      stream->in.tailBuffer = new RingBuffer ( )                             ;
      if ( NULL == stream->in.tailBuffer )                                   {
        LogCaError ( result = InsufficientMemory )                           ;
        goto error                                                           ;
      }                                                                      ;
      // buffer memory region
      stream->in.tailBufferMemory = Allocator::allocate(frameSize*bufferFrames) ;
      if ( stream->in.tailBufferMemory == NULL )                             {
        LogCaError ( result = InsufficientMemory )                           ;
        goto error                                                           ;
      }                                                                      ;
      // initialize
      if (stream->in.tailBuffer->Initialize(frameSize,bufferFrames,stream->in.tailBufferMemory) != 0) {
        LogCaError ( result = InternalError )                                ;
        goto error                                                           ;
      }                                                                      ;
    }                                                                        ;
    if ( NULL != conduit )                                                   {
      conduit->Input . BytesPerSample  = Core::SampleSize(inputSampleFormat) ;
      conduit->Input . BytesPerSample *= inputChannelCount                   ;
    }                                                                        ;
  } else                                                                     {
    inputChannelCount = 0                                                    ;
    inputSampleFormat = hostInputSampleFormat = cafInt16                     ;
    /* Surpress 'uninitialised var' warnings.                               */
  }                                                                          ;
  // Try create device: Output
  if (outputParameters != NULL)                                              {
    outputChannelCount = outputParameters->channelCount                      ;
    outputSampleFormat = outputParameters->sampleFormat                      ;
    outputStreamInfo   = (CaWaSapiStreamInfo *)outputParameters->streamInfo ;
    info               = &devInfo[outputParameters->device]                  ;
    stream->out.flags  = (outputStreamInfo ? outputStreamInfo->flags : 0)    ;
    // Select Exclusive/Shared mode
    stream->out.shareMode = AUDCLNT_SHAREMODE_SHARED                         ;
    if ( ( outputStreamInfo != NULL                                       ) &&
         ( outputStreamInfo->flags & CaWaSapiExclusive                  ) )  {
      // Boost thread priority
      stream -> nThreadPriority = eThreadPriorityProAudio                    ;
      // Make Exclusive
      stream -> out.shareMode   = AUDCLNT_SHAREMODE_EXCLUSIVE                ;
    }                                                                        ;
    // If user provided explicit thread priority level, use it
    if ( ( outputStreamInfo != NULL                                       ) &&
         ( outputStreamInfo -> flags & CaWaSapiThread                   ) )  {
      if ( ( outputStreamInfo->threadPriority >  eThreadPriorityNone      ) &&
           ( outputStreamInfo->threadPriority <= eThreadPriorityWindowManager ) ) {
        stream->nThreadPriority = outputStreamInfo->threadPriority           ;
      }                                                                      ;
    }                                                                        ;
    // Choose processing mode
    stream->out.streamFlags = ( stream -> out . shareMode == AUDCLNT_SHAREMODE_EXCLUSIVE ?
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK : 0      ) ;
    if ( useWOW64Workaround )                                                {
      // polling interface
      stream->out.streamFlags = 0                                            ;
    } else
    if ( conduit == NULL )                                                   {
      // polling interface
      stream->out.streamFlags = 0                                            ;
    }  else
    if ( ( outputStreamInfo != NULL                                       ) &&
         ( outputStreamInfo -> flags & CaWaSapiPolling                  ) )  {
      // polling interface
      stream->out.streamFlags = 0                                            ;
    }  else
    if (fullDuplex)                                                          {
      // polling interface is implemented for full-duplex mode also
      stream->out.streamFlags = 0                                            ;
    }                                                                        ;
    // Fill parameters for Audio Client creation
    stream -> out.params.device_info       = info                            ;
    stream -> out.params.stream_params     = (*outputParameters)             ;
    if ( inputStreamInfo != NULL )                                           {
      stream->out.params.wasapi_params = (*outputStreamInfo);
      stream->out.params.stream_params.streamInfo = &stream->out.params.wasapi_params;
    }                                                                        ;
    stream->out.params.frames_per_buffer = framesPerCallback                 ;
    stream->out.params.sample_rate       = SampleRate                        ;
    stream->out.params.blocking          = ( conduit == NULL )               ;
    stream->out.params.full_duplex       = fullDuplex                        ;
    stream->out.params.wow64_workaround  = useWOW64Workaround                ;
    // Create and activate audio client
    if ( CanDoResample )                                                     {
      double nearest = Nearest(NULL,outputParameters,SampleRate)             ;
      if ( ((int)nearest) != ((int)SampleRate) )                             {
        dPrintf ( ( "A resampling from %d to %d is needed.\n"                ,
                    (int)SampleRate                                          ,
                    (int)nearest                                         ) ) ;
        if ( stream -> setResampling                                         (
                         SampleRate                                          ,
                         nearest                                             ,
                         outputChannelCount                                  ,
                         outputSampleFormat                              ) ) {
          stream->NearestSampleRate = nearest                                ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
    hr = stream -> ActivateAudioClientOutput ( )                             ;
    if ( hr != S_OK )                                                        {
      LogCaError ( result = InvalidDevice )                                  ;
      goto error                                                             ;
    }                                                                        ;
    // Get closest format
    hostOutputSampleFormat = ClosestFormat                                   (
                               WaveToCaFormat(&stream->out.wavex)            ,
                               outputSampleFormat                          ) ;
    // Set user-side custom host processor
    if ( ( outputStreamInfo != NULL                                       ) &&
         ( outputStreamInfo -> flags & CaWaSapiRedirectHostProcessor )    )  {
      // New implementation cancelled Host Processor Override
    }
    // Only get IAudioCaptureClient output once here instead of getting it at multiple places based on the use
    hr = stream -> out . clientParent -> GetService                          (
           ca_IID_IAudioRenderClient                                         ,
           (void **)&stream->renderClientParent                            ) ;
    if ( hr != S_OK )                                                        {
      LogHostError ( hr                              )                       ;
      LogCaError   ( result = UnanticipatedHostError )                       ;
      goto error                                                             ;
    }                                                                        ;
    if ( NULL != conduit )                                                   {
      conduit->Output . BytesPerSample  = Core::SampleSize(outputSampleFormat) ;
      conduit->Output . BytesPerSample *= outputChannelCount                 ;
    }                                                                        ;
  } else                                                                     {
    outputChannelCount = 0                                                   ;
    outputSampleFormat = hostOutputSampleFormat = cafInt16                   ;
    /* Surpress 'uninitialized var' warnings.                               */
  }                                                                          ;
  // log full-duplex
  if ( fullDuplex ) dPrintf ( ( "WASAPI ::OpenStream: full-duplex mode\n" ) ) ;
  // CaWasapiPolling must be on/or not on both streams
  if ((inputParameters != NULL) && (outputParameters != NULL))               {
    if ((inputStreamInfo != NULL) && (outputStreamInfo != NULL))             {
      if ( ( ( inputStreamInfo  -> flags & CaWaSapiPolling )                &&
            !( outputStreamInfo -> flags & CaWaSapiPolling )              ) ||
           (!( inputStreamInfo  -> flags & CaWaSapiPolling )                &&
             ( outputStreamInfo -> flags & CaWaSapiPolling )            ) )  {
        LogCaError ( result = InvalidFlag )                                  ;
        goto error                                                           ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  // Initialize stream representation
  if ( NULL != conduit )                                                     {
    stream -> bBlocking = FALSE                                              ;
    stream -> conduit   = conduit                                            ;
  } else {
    stream -> bBlocking = TRUE                                               ;
    stream -> conduit   = conduit                                            ;
  }                                                                          ;
  // Initialize CPU measurer
  stream -> cpuLoadMeasurer . Initialize ( SampleRate )                      ;
  if ( ( NULL != outputParameters ) && ( NULL != inputParameters ) )         {
    if ( stream -> in . period != stream -> out . period )                   {
      dPrintf ( ( "WASAPI : OpenStream: period discrepancy\n" ) )            ;
      LogCaError ( result = BadIODeviceCombination )                         ;
      goto error                                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
  }                                                                          ;
  // Calculate frames per host for processor
  framesPerHostCallback = ( outputParameters                                 ?
                            stream -> out . framesPerBuffer                  :
                            stream -> in  . framesPerBuffer                ) ;
  // Choose correct mode of buffer processing:
  // Exclusive/Shared non paWinWasapiPolling mode: paUtilFixedHostBufferSize - always fixed
  // Exclusive/Shared paWinWasapiPolling mode: paUtilBoundedHostBufferSize - may vary for Exclusive or Full-duplex
  bufferMode = cabFixedHostBufferSize                                        ;
  if ( NULL != inputParameters  )                                            {
      // !!! WASAPI IAudioCaptureClient::GetBuffer extracts not number of frames but 1 packet, thus we always must adapt
    bufferMode = cabBoundedHostBufferSize                                    ;
  } else
  if ( NULL != outputParameters )                                            {
    if ( ( stream -> out . buffers == 1                                   ) &&
         (!stream -> out . streamFlags                                      ||
         ((stream -> out . streamFlags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK) == 0 ) ) ) {
      bufferMode = cabBoundedHostBufferSize                                  ;
    }                                                                        ;
  }                                                                          ;
  stream->bufferMode = bufferMode                                            ;
  // Initialize buffer processor
  result = stream -> bufferProcessor . Initialize                            (
             inputChannelCount                                               ,
             inputSampleFormat                                               ,
             hostInputSampleFormat                                           ,
             outputChannelCount                                              ,
             outputSampleFormat                                              ,
             hostOutputSampleFormat                                          ,
             SampleRate                                                      ,
             streamFlags                                                     ,
             framesPerCallback                                               ,
             framesPerHostCallback                                           ,
             bufferMode                                                      ,
             conduit                                                       ) ;
  if ( result != NoError )                                                   {
    LogCaError ( result )                                                    ;
    goto error                                                               ;
  }                                                                          ;
  // Set Input latency
  stream -> inputLatency                                                     =
          ( (double)stream->bufferProcessor.InputLatencyFrames()/SampleRate  )
          + ( ( NULL != inputParameters ) ? stream->in.latencySeconds : 0  ) ;
  // Set Output latency
  stream -> outputLatency                                                    =
          ((double)stream->bufferProcessor.OutputLatencyFrames()/ SampleRate )
          + ( ( NULL != outputParameters) ? stream->out.latencySeconds : 0 ) ;
  // Set SR
  stream -> sampleRate = SampleRate                                          ;
  (*s) = (Stream *)stream                                                    ;
  return result                                                              ;
error                                                                        :
  if ( NULL != stream )                                                      {
    stream -> Close ( )                                                      ;
    delete stream                                                            ;
    stream = NULL                                                            ;
  }                                                                          ;
  return result                                                              ;
}

void WaSapiHostApi::Terminate(void)
{
  UINT i                                                                     ;
  // Release IMMDeviceEnumerator
  SAFE_RELEASE ( enumerator )                                                ;
  for ( i = 0; i < deviceCount ; ++i)                                        {
    WaSapiDeviceInfo * info = & devInfo [ i ]                                ;
    SAFE_RELEASE ( info -> device )                                          ;
    // if (info->MixFormat) CoTaskMemFree(info->MixFormat)                      ;
  }                                                                          ;
  if ( CaNotNull ( devInfo ) )                                               {
    delete [] devInfo                                                        ;
    devInfo = NULL                                                           ;
  }                                                                          ;
  CaEraseAllocation                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  CaComUninitialize ( WASAPI , &comInitializationResult )                    ;
  CloseAVRT ( )                                                              ;
}

CaError WaSapiHostApi::isSupported                  (
          const StreamParameters * inputParameters  ,
          const StreamParameters * outputParameters ,
          double                   sampleRate       )
{
  CaError              error            = NoError                            ;
  IAudioClient       * tmpClient        = NULL                               ;
  CaWaSapiStreamInfo * inputStreamInfo  = NULL                               ;
  CaWaSapiStreamInfo * outputStreamInfo = NULL                               ;
  ////////////////////////////////////////////////////////////////////////////
  // Validate PaStreamParameters
  error = isValid ( inputParameters , outputParameters , sampleRate )        ;
  if ( NoError != error ) return error                                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    WAVEFORMATEXTENSIBLE wavex                                               ;
    HRESULT              hr                                                  ;
    CaError              answer                                              ;
    AUDCLNT_SHAREMODE    shareMode = AUDCLNT_SHAREMODE_SHARED                ;
    inputStreamInfo = (CaWaSapiStreamInfo *)inputParameters->streamInfo ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( NULL != inputStreamInfo                                        ) &&
         ( inputStreamInfo->flags & CaWaSapiExclusive                   ) )  {
      shareMode  = AUDCLNT_SHAREMODE_EXCLUSIVE                               ;
    }                                                                        ;
    hr = devInfo [ inputParameters -> device ] . device -> Activate          (
           ca_IID_IAudioClient                                               ,
           CLSCTX_INPROC_SERVER                                              ,
           NULL                                                              ,
           (void **)&tmpClient                                             ) ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      return InvalidDevice                                                   ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    answer = GetClosestFormat                                                (
               tmpClient                                                     ,
               sampleRate                                                    ,
               inputParameters                                               ,
               shareMode                                                     ,
               &wavex                                                        ,
               FALSE                                                       ) ;
    SAFE_RELEASE ( tmpClient )                                               ;
    if ( NoError != answer ) return answer                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    HRESULT              hr                                                  ;
    WAVEFORMATEXTENSIBLE wavex                                               ;
    CaError              answer                                              ;
    AUDCLNT_SHAREMODE    shareMode = AUDCLNT_SHAREMODE_SHARED                ;
    //////////////////////////////////////////////////////////////////////////
    outputStreamInfo = (CaWaSapiStreamInfo *)outputParameters->streamInfo ;
    if ( ( NULL != outputStreamInfo                                       ) &&
         ( outputStreamInfo->flags & CaWaSapiExclusive                  ) )  {
      shareMode  = AUDCLNT_SHAREMODE_EXCLUSIVE                               ;
    }                                                                        ;
    hr = devInfo[outputParameters->device].device->Activate                  (
          ca_IID_IAudioClient                                                ,
          CLSCTX_INPROC_SERVER                                               ,
          NULL                                                               ,
          (void **)&tmpClient                                              ) ;
    if ( S_OK != hr )                                                        {
      LogHostError ( hr )                                                    ;
      return InvalidDevice                                                   ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    answer = GetClosestFormat                                                (
               tmpClient                                                     ,
               sampleRate                                                    ,
               outputParameters                                              ,
               shareMode                                                     ,
               &wavex                                                        ,
               TRUE                                                        ) ;
    SAFE_RELEASE ( tmpClient )                                               ;
    if ( NoError != answer ) return answer                                   ;
  }                                                                          ;
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

CaError WaSapiHostApi::isValid                      (
          const StreamParameters * inputParameters  ,
          const StreamParameters * outputParameters ,
          double                   SampleRate       )
{
  if ( 0 == (UINT32)SampleRate ) return InvalidSampleRate                    ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    /* all standard sample formats are supported by the buffer adapter,
       this implementation doesn't support any custom sample formats        */
    if ( inputParameters->sampleFormat & cafCustomFormat )                   {
      return SampleFormatNotSupported                                        ;
    }                                                                        ;
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    if (inputParameters->device == CaUseHostApiSpecificDeviceSpecification)  {
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that input device can support inputChannelCount                */
    if (inputParameters->channelCount>deviceInfos[inputParameters->device]->maxInputChannels) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate inputStreamInfo                                             */
    if (inputParameters->streamInfo)                          {
      CaWaSapiStreamInfo * inputStreamInfo = (CaWaSapiStreamInfo *)inputParameters->streamInfo ;
      if ( ( inputStreamInfo -> size        != sizeof(CaWaSapiStreamInfo) ) ||
           ( inputStreamInfo -> version     != 1                          ) ||
           ( inputStreamInfo -> hostApiType != WASAPI                   ) )  {
        return IncompatibleStreamInfo                         ;
      }                                                                      ;
    }                                                                        ;
    return NoError                                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    /* all standard sample formats are supported by the buffer adapter,
       this implementation doesn't support any custom sample formats        */
    if ( outputParameters->sampleFormat & cafCustomFormat)                   {
      return SampleFormatNotSupported                                        ;
    }                                                                        ;
    /* unless alternate device specification is supported, reject the use of
       paUseHostApiSpecificDeviceSpecification                              */
    if ( outputParameters->device==CaUseHostApiSpecificDeviceSpecification)  {
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that output device can support outputChannelCount              */
    if (outputParameters->channelCount>deviceInfos[outputParameters->device]->maxOutputChannels) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate outputStreamInfo                                            */
    if ( outputParameters->streamInfo )                       {
      CaWaSapiStreamInfo *outputStreamInfo = (CaWaSapiStreamInfo *)outputParameters->streamInfo ;
      if ( ( outputStreamInfo->size    != sizeof(CaWaSapiStreamInfo)      ) ||
           ( outputStreamInfo->version != 1                               ) ||
           ( outputStreamInfo->hostApiType != WASAPI                    ) )  {
        return IncompatibleStreamInfo                         ;
      }                                                                      ;
    }                                                                        ;
    return NoError                                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return ( ( inputParameters || outputParameters ) ? NoError : InternalError ) ;
}

double WaSapiHostApi::Nearest                      (
         const StreamParameters * inputParameters  ,
         const StreamParameters * outputParameters ,
         double                   SampleRate       )
{
  CaRETURN ( ( CaAND ( CaNotNull ( inputParameters  )                        ,
                       CaNotNull ( outputParameters )                    ) ) ,
             0                                                             ) ;
  ////////////////////////////////////////////////////////////////////////////
  HRESULT              hr                                                    ;
  WAVEFORMATEXTENSIBLE wavex                                                 ;
  IAudioClient       * client             = NULL                             ;
  WaSapiDeviceInfo   * dev                = NULL                             ;
  WAVEFORMATEX       * sharedClosestMatch = NULL                             ;
  StreamParameters   * params             = NULL                             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( inputParameters  ) )                                      {
    params = (StreamParameters *) inputParameters                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( outputParameters ) )                                      {
    params = (StreamParameters *) outputParameters                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  CaRETURN ( ( params->device  >= deviceCount ) , 0 )                        ;
  dev = &devInfo[params->device]                                             ;
  hr = dev->device->Activate                                                 (
         ca_IID_IAudioClient                                                 ,
         CLSCTX_ALL                                                          ,
         NULL                                                                ,
         (void **)&client                                                  ) ;
  if ( CaAND ( CaIsEqual ( hr , S_OK ) , CaNotNull ( client ) ) )            {
    MakeWaveFormatFromParams ( &wavex , params  , SampleRate )               ;
    hr = client -> IsFormatSupported                                         (
           AUDCLNT_SHAREMODE_SHARED                                          ,
           (WAVEFORMATEX *)&wavex                                            ,
           &sharedClosestMatch                                             ) ;
    if ( CaIsEqual ( hr , S_OK ) )                                           {
      SAFE_RELEASE ( client )                                                ;
      return SampleRate                                                      ;
    }                                                                        ;
  } else return SampleRate                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  bool   WaSupported [15]                                                    ;
  double WaSample    [15]                                                    ;
  int    WaID      = 0                                                       ;
  int    Supported = 0                                                       ;
  ////////////////////////////////////////////////////////////////////////////
  while ( CaIsGreater ( AllSamplingRates [ WaID ] , 0 ) )                    {
    WaSupported [ WaID ] = false                                             ;
    WaSample    [ WaID ] = -1                                                ;
    MakeWaveFormatFromParams ( &wavex                                        ,
                               params                                        ,
                               AllSamplingRates [ WaID ]                   ) ;
    hr = client -> IsFormatSupported                                         (
           AUDCLNT_SHAREMODE_SHARED                                          ,
           (WAVEFORMATEX *)&wavex                                            ,
           &sharedClosestMatch                                             ) ;
    if ( CaIsEqual ( hr , S_OK ) )                                           {
      dPrintf ( ( "%d is supported\n"    ,(int)AllSamplingRates [ WaID ] ) ) ;
      WaSupported [ WaID      ] = true                                       ;
      WaSample    [ Supported ] = AllSamplingRates [ WaID ]                  ;
      Supported ++                                                           ;
    } else                                                                   {
      dPrintf ( ( "%d is not supported\n",(int)AllSamplingRates [ WaID ] ) ) ;
    }                                                                        ;
    WaID++                                                                   ;
    WaSample [ WaID ] = -1                                                   ;
  }                                                                          ;
  if ( Supported <= 0 ) return SampleRate                                    ;
  if ( CaIsEqual ( Supported , 1 ) )                                         {
    SAFE_RELEASE(client)                                                     ;
    return WaSample [ 0 ]                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "%d formats are supported.\n" , Supported ) )                  ;
  double nearest = NearestSampleRate ( SampleRate , 15 , WaSample )          ;
  ////////////////////////////////////////////////////////////////////////////
  SAFE_RELEASE ( client                                )                     ;
  CaRETURN     ( CaIsGreater ( nearest , 0 ) , nearest )                     ;
  return SampleRate                                                          ;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
