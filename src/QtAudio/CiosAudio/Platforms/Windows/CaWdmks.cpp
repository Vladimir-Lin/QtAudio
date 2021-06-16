/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/23                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CaWdmks.hpp"

#if defined(WIN32) || defined(_WIN32)
#else
#error "Windows Driver Model - Kernal Streaming works only on Windows platform"
#endif

//////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif

/* A define that selects whether the resulting pin names are chosen from pin category
instead of the available pin names, who sometimes can be quite cheesy, like "Volume control".
Default is to use the pin category.
*/
#ifndef CA_WDMKS_USE_CATEGORY_FOR_PIN_NAMES
#define CA_WDMKS_USE_CATEGORY_FOR_PIN_NAMES  1
#endif

#ifdef __GNUC__
/* These defines are set in order to allow the WIndows DirectX
* headers to compile with a GCC compiler such as MinGW
* NOTE: The headers may generate a few warning in GCC, but
* they should compile */
#define _INC_MMSYSTEM
#define _INC_MMREG
#define _NTRTL_ /* Turn off default definition of DEFINE_GUIDEX */
#define DEFINE_GUID_THUNK(name,guid) DEFINE_GUID(name,guid)
#define DEFINE_GUIDEX(n) DEFINE_GUID_THUNK( n, STATIC_##n )
#if !defined( DEFINE_WAVEFORMATEX_GUID )
#define DEFINE_WAVEFORMATEX_GUID(x) (USHORT)(x), 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
#endif
#define  WAVE_FORMAT_ADPCM      0x0002
#define  WAVE_FORMAT_IEEE_FLOAT 0x0003
#define  WAVE_FORMAT_ALAW       0x0006
#define  WAVE_FORMAT_MULAW      0x0007
#define  WAVE_FORMAT_MPEG       0x0050
#define  WAVE_FORMAT_DRM        0x0009
#define DYNAMIC_GUID_THUNK(l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
#define DYNAMIC_GUID(data) DYNAMIC_GUID_THUNK(data)
#endif

/* use CreateThread for CYGWIN/Windows Mobile, _beginthreadex for all others */
#if !defined(__CYGWIN__) && !defined(_WIN32_WCE)
#define CREATE_THREAD_FUNCTION (HANDLE)_beginthreadex
#define CA_THREAD_FUNC static unsigned WINAPI
#else
#define CREATE_THREAD_FUNCTION CreateThread
#define CA_THREAD_FUNC static DWORD WINAPI
#endif

#ifdef _MSC_VER
#define NOMMIDS
#define DYNAMIC_GUID(data) {data}
#define _NTRTL_ /* Turn off default definition of DEFINE_GUIDEX */
#undef DEFINE_GUID
#define DEFINE_GUID(n,data) EXTERN_C const GUID n = {data}
#define DEFINE_GUID_THUNK(n,data) DEFINE_GUID(n,data)
//#define DEFINE_GUIDEX(n) DEFINE_GUID_THUNK(n, STATIC_##n)
#endif

/* An unspecified channel count (-1) is not treated correctly, so we replace it with
* an arbitrarily large number */
#define MAXIMUM_NUMBER_OF_CHANNELS 256

//////////////////////////////////////////////////////////////////////////////

/* These next definitions allow the use of the KSUSER DLL */
typedef /*KSDDKAPI*/ DWORD WINAPI KSCREATEPIN(HANDLE, PKSPIN_CONNECT, ACCESS_MASK, PHANDLE);

static const unsigned cPacketsArrayMask = 3;

HMODULE       DllKsUser = NULL;
KSCREATEPIN * FunctionKsCreatePin = NULL;

/* These definitions allows the use of AVRT.DLL on Vista and later OSs */
typedef enum _PA_AVRT_PRIORITY
{
    PA_AVRT_PRIORITY_LOW = -1,
    PA_AVRT_PRIORITY_NORMAL,
    PA_AVRT_PRIORITY_HIGH,
    PA_AVRT_PRIORITY_CRITICAL
} PA_AVRT_PRIORITY, *PPA_AVRT_PRIORITY;

typedef struct
{
    HINSTANCE hInstance;

    HANDLE  (WINAPI *AvSetMmThreadCharacteristics) (LPCSTR, LPDWORD);
    BOOL    (WINAPI *AvRevertMmThreadCharacteristics) (HANDLE);
    BOOL    (WINAPI *AvSetMmThreadPriority) (HANDLE, PA_AVRT_PRIORITY);
} CaWdmKsAvRtEntryPoints ;

static CaWdmKsAvRtEntryPoints caWdmKsAvRtEntryPoints = {0};

//////////////////////////////////////////////////////////////////////////////

/* Note: Somewhat different order compared to WMME implementation, as we want
 * to focus on fidelity first                                               */
static const int defaultSampleRateSearchOrder[] = {
                    44100                         ,
                    48000                         ,
                    88200                         ,
                    96000                         ,
                   192000                         ,
                    32000                         ,
                    24000                         ,
                    22050                         ,
                    16000                         ,
                    12000                         ,
                    11025                         ,
                     9600                         ,
                     8000                       } ;

static const int          defaultSampleRateSearchOrderCount   =
                 sizeof ( defaultSampleRateSearchOrder      ) /
                 sizeof ( defaultSampleRateSearchOrder[0]   ) ;

//////////////////////////////////////////////////////////////////////////////

/* Copy the first interleaved channel of 16 bit data to the other channels  */
static void DuplicateFirstChannelInt16(void* buffer, int channels, int samples)
{
  unsigned short * data = (unsigned short *)buffer ;
  int              channel                         ;
  unsigned short   sourceSample                    ;
  while ( samples-- )                              {
    sourceSample = *data++                         ;
    channel      = channels-1                      ;
    while ( channel-- )                            {
      *data++ = sourceSample                       ;
    }                                              ;
  }                                                ;
}

//////////////////////////////////////////////////////////////////////////////

/* Copy the first interleaved channel of 24 bit data to the other channels  */
static void DuplicateFirstChannelInt24(void * buffer, int channels, int samples)
{
  unsigned char * data = (unsigned char *) buffer ;
  int             channel                         ;
  unsigned char   sourceSample[3]                 ;
  while ( samples-- )                             {
    sourceSample[0] = data[0]                     ;
    sourceSample[1] = data[1]                     ;
    sourceSample[2] = data[2]                     ;
    data           += 3                           ;
    channel         = channels - 1                ;
    while ( channel-- )                           {
      data[0] = sourceSample[0]                   ;
      data[1] = sourceSample[1]                   ;
      data[2] = sourceSample[2]                   ;
      data   += 3                                 ;
    }                                             ;
  }                                               ;
}

//////////////////////////////////////////////////////////////////////////////

/* Copy the first interleaved channel of 32 bit data to the other channels  */
static void DuplicateFirstChannelInt32(void* buffer, int channels, int samples)
{
  unsigned long * data = (unsigned long *)buffer ;
  int             channel                        ;
  unsigned long   sourceSample                   ;
  while ( samples-- )                            {
    sourceSample = *data++                       ;
    channel      = channels - 1                  ;
    while ( channel-- )                          {
      *data++ = sourceSample                     ;
    }                                            ;
  }                                              ;
}

//////////////////////////////////////////////////////////////////////////////

static unsigned long GetWfexSize(const WAVEFORMATEX * wfex)
{
  if ( WAVE_FORMAT_PCM == wfex -> wFormatTag ) {
    return sizeof(WAVEFORMATEX)                ;
  }                                            ;
  return (sizeof(WAVEFORMATEX) + wfex->cbSize) ;
}

//////////////////////////////////////////////////////////////////////////////

static unsigned NextPowerOf2(unsigned val)
{
  val--                   ;
  val = (val >>  1) | val ;
  val = (val >>  2) | val ;
  val = (val >>  4) | val ;
  val = (val >>  8) | val ;
  val = (val >> 16) | val ;
  return ++ val           ;
}

//////////////////////////////////////////////////////////////////////////////

/* greatest common divisor - PGCD in French */

static unsigned long WdmGCD( unsigned long a , unsigned long b )
{
  return ( 0 == b ) ? a : WdmGCD ( b , a % b ) ;
}

//////////////////////////////////////////////////////////////////////////////

static void MemoryBarrierDummy(void)
{
  /* Do nothing */
}

//////////////////////////////////////////////////////////////////////////////

static void MemoryBarrierRead(void)
{
  CaReadMemoryBarrier ( ) ;
}

//////////////////////////////////////////////////////////////////////////////

static void MemoryBarrierWrite(void)
{
  CaWriteMemoryBarrier ( ) ;
}

//////////////////////////////////////////////////////////////////////////////

typedef struct __CaUsbTerminalGUIDToName
{
    USHORT     usbGUID;
    wchar_t    name[64];
} CaUsbTerminalGUIDToName;

static const CaUsbTerminalGUIDToName kNames[] =
{
    /* Types copied from: http://msdn.microsoft.com/en-us/library/ff537742(v=vs.85).aspx */
    /* Input terminal types */
    { 0x0201, L"Microphone" },
    { 0x0202, L"Desktop Microphone" },
    { 0x0203, L"Personal Microphone" },
    { 0x0204, L"Omni Directional Microphone" },
    { 0x0205, L"Microphone Array" },
    { 0x0206, L"Processing Microphone Array" },
    /* Output terminal types */
    { 0x0301, L"Speakers" },
    { 0x0302, L"Headphones" },
    { 0x0303, L"Head Mounted Display Audio" },
    { 0x0304, L"Desktop Speaker" },
    { 0x0305, L"Room Speaker" },
    { 0x0306, L"Communication Speaker" },
    { 0x0307, L"LFE Speakers" },
    /* External terminal types */
    { 0x0601, L"Analog" },
    { 0x0602, L"Digital" },
    { 0x0603, L"Line" },
    { 0x0604, L"Audio" },
    { 0x0605, L"SPDIF" },
};

static const unsigned kNamesCnt = sizeof(kNames)/sizeof(CaUsbTerminalGUIDToName);

static int CaUsbTerminalGUIDToNameCmp(const void* lhs,const void* rhs)
{
    const CaUsbTerminalGUIDToName* pL = (const CaUsbTerminalGUIDToName*)lhs;
    const CaUsbTerminalGUIDToName* pR = (const CaUsbTerminalGUIDToName*)rhs;
    return ((int)(pL->usbGUID) - (int)(pR->usbGUID));
}

//////////////////////////////////////////////////////////////////////////////

static const wchar_t kUsbPrefix[] = L"\\\\?\\USB";

static BOOL IsUSBDevice(const wchar_t* devicePath)
{
    /* Alex Lessard pointed out that different devices might present the device path with
       lower case letters. */
    return (_wcsnicmp(devicePath, kUsbPrefix, sizeof(kUsbPrefix)/sizeof(kUsbPrefix[0]) ) == 0);
}

/* This should make it more language tolerant, I hope... */
static const wchar_t kUsbNamePrefix[] = L"USB Audio";

static BOOL IsNameUSBAudioDevice(const wchar_t* friendlyName)
{
    return (_wcsnicmp(friendlyName, kUsbNamePrefix, sizeof(kUsbNamePrefix)/sizeof(kUsbNamePrefix[0])) == 0);
}

typedef enum _tag_EAlias
{
    Alias_kRender   = (1<<0),
    Alias_kCapture  = (1<<1),
    Alias_kRealtime = (1<<2),
} EAlias;

/* Trim whitespace from string */
static void TrimString(wchar_t* str, size_t length)
{
    wchar_t* s = str;
    wchar_t* e = 0;

    /* Find start of string */
    while (iswspace(*s)) ++s;
    e=s+min(length,wcslen(s))-1;

    /* Find end of string */
    while(e>s && iswspace(*e)) --e;
    ++e;

    length = e - s;
    memmove(str, s, length * sizeof(wchar_t));
    str[length] = 0;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif /* __cplusplus */

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

//////////////////////////////////////////////////////////////////////////////

static void WdmSetLastErrorInfo(long errCode,const char * fmt, ...)
{
  va_list list;
  char buffer[1024];
  va_start(list, fmt);
  _vsnprintf(buffer, 1023, fmt, list);
  va_end(list);
  SetLastHostErrorInfo(WDMKS, errCode, buffer);
}

//////////////////////////////////////////////////////////////////////////////

/* Low level pin/filter access functions */

static CaError WdmSyncIoctl                     (
                 HANDLE          handle         ,
                 unsigned long   ioctlNumber    ,
                 void          * inBuffer       ,
                 unsigned long   inBufferCount  ,
                 void          * outBuffer      ,
                 unsigned long   outBufferCount ,
                 unsigned long * bytesReturned  )
{
  CaError       result             = NoError                                 ;
  unsigned long dummyBytesReturned = 0                                       ;
  BOOL          bRes                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ! bytesReturned )                                                     {
    /* Use a dummy as the caller hasn't supplied one                        */
    bytesReturned = &dummyBytesReturned                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  bRes = ::DeviceIoControl                                                   (
             handle                                                          ,
             ioctlNumber                                                     ,
             inBuffer                                                        ,
             inBufferCount                                                   ,
             outBuffer                                                       ,
             outBufferCount                                                  ,
             bytesReturned                                                   ,
             NULL                                                          ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( bRes ) return result                                                  ;
  unsigned long error = GetLastError ( )                                     ;
  if ( ! ( ( ( error          == ERROR_INSUFFICIENT_BUFFER )                ||
             ( error          == ERROR_MORE_DATA           )              ) &&
             ( ioctlNumber    == IOCTL_KS_PROPERTY                        ) &&
             ( outBufferCount == 0                                     ) ) ) {
    KSPROPERTY * ksProperty = (KSPROPERTY *)inBuffer                         ;
    WdmSetLastErrorInfo                                                      (
      result                                                                 ,
      "WdmSyncIoctl: DeviceIoControl GLE = 0x%08X (prop_set = {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}, prop_id = %u)",
      error                                                                  ,
      ksProperty->Set.Data1, ksProperty->Set.Data2, ksProperty->Set.Data3    ,
      ksProperty->Set.Data4[0], ksProperty->Set.Data4[1]                     ,
      ksProperty->Set.Data4[2], ksProperty->Set.Data4[3]                     ,
      ksProperty->Set.Data4[4], ksProperty->Set.Data4[5]                     ,
      ksProperty->Set.Data4[6], ksProperty->Set.Data4[7]                     ,
      ksProperty->Id                                                       ) ;
    result = UnanticipatedHostError                                          ;
  }                                                                          ;
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError WdmGetPropertySimple            (
                 HANDLE        handle          ,
                 const GUID  * guidPropertySet ,
                 unsigned long property        ,
                 void        * value           ,
                 unsigned long valueCount      )
{
  CaError    result                        ;
  KSPROPERTY ksProperty                    ;
  //////////////////////////////////////////
  ksProperty . Set   = *guidPropertySet    ;
  ksProperty . Id    = property            ;
  ksProperty . Flags = KSPROPERTY_TYPE_GET ;
  //////////////////////////////////////////
  result = WdmSyncIoctl                    (
             handle                        ,
             IOCTL_KS_PROPERTY             ,
             &ksProperty                   ,
             sizeof(KSPROPERTY)            ,
             value                         ,
             valueCount                    ,
             NULL                        ) ;
  //////////////////////////////////////////
  return result                            ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError WdmSetPropertySimple            (
                 HANDLE        handle          ,
                 const GUID  * guidPropertySet ,
                 unsigned long property        ,
                 void        * value           ,
                 unsigned long valueCount      ,
                 void        * instance        ,
                 unsigned long instanceCount   )
{
  CaError       result                                                       ;
  KSPROPERTY  * ksProperty                                                   ;
  unsigned long propertyCount  = 0                                           ;
  ////////////////////////////////////////////////////////////////////////////
  propertyCount = sizeof(KSPROPERTY) + instanceCount                         ;
  ksProperty    = (KSPROPERTY*)_alloca(propertyCount)                        ;
  if ( !ksProperty ) return InsufficientMemory                               ;
  ////////////////////////////////////////////////////////////////////////////
  ksProperty -> Set   = *guidPropertySet                                     ;
  ksProperty -> Id    = property                                             ;
  ksProperty -> Flags = KSPROPERTY_TYPE_SET                                  ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != instance )                                                    {
    ::memcpy ( (void*)((char*)ksProperty + sizeof(KSPROPERTY))               ,
               instance                                                      ,
               instanceCount                                               ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  result = WdmSyncIoctl                                                      (
             handle                                                          ,
             IOCTL_KS_PROPERTY                                               ,
             ksProperty                                                      ,
             propertyCount                                                   ,
             value                                                           ,
             valueCount                                                      ,
             NULL                                                          ) ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError WdmGetPinPropertySimple           (
                 HANDLE          handle          ,
                 unsigned long   pinId           ,
                 const GUID    * guidPropertySet ,
                 unsigned long   property        ,
                 void          * value           ,
                 unsigned long   valueCount      ,
                 unsigned long * byteCount       )
{
  CaError result                                                             ;
  KSP_PIN ksPProp                                                            ;
  ////////////////////////////////////////////////////////////////////////////
  ksPProp . Property . Set   = *guidPropertySet                              ;
  ksPProp . Property . Id    = property                                      ;
  ksPProp . Property . Flags = KSPROPERTY_TYPE_GET                           ;
  ksPProp . PinId            = pinId                                         ;
  ksPProp . Reserved         = 0                                             ;
  ////////////////////////////////////////////////////////////////////////////
  result = WdmSyncIoctl                                                      (
             handle                                                          ,
             IOCTL_KS_PROPERTY                                               ,
             &ksPProp                                                        ,
             sizeof(KSP_PIN)                                                 ,
             value                                                           ,
             valueCount                                                      ,
             byteCount                                                     ) ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError WdmGetPinPropertyMulti               (
                 HANDLE             handle          ,
                 unsigned long      pinId           ,
                 const GUID      *  guidPropertySet ,
                 unsigned long      property        ,
                 KSMULTIPLE_ITEM ** ksMultipleItem  )
{
  CaError       result                                                       ;
  unsigned long multipleItemSize = 0                                         ;
  KSP_PIN       ksPProp                                                      ;
  ////////////////////////////////////////////////////////////////////////////
  ksPProp . Property . Set   = *guidPropertySet                              ;
  ksPProp . Property . Id    = property                                      ;
  ksPProp . Property . Flags = KSPROPERTY_TYPE_GET                           ;
  ksPProp . PinId            = pinId                                         ;
  ksPProp . Reserved         = 0                                             ;
  ////////////////////////////////////////////////////////////////////////////
  result = WdmSyncIoctl                                                      (
             handle                                                          ,
             IOCTL_KS_PROPERTY                                               ,
             &ksPProp.Property                                               ,
             sizeof(KSP_PIN)                                                 ,
             NULL                                                            ,
             0                                                               ,
             &multipleItemSize                                             ) ;
  if ( result != NoError ) return result                                     ;
  ////////////////////////////////////////////////////////////////////////////
  *ksMultipleItem = (KSMULTIPLE_ITEM *)Allocator::allocate(multipleItemSize) ;
  if ( !(*ksMultipleItem) ) return InsufficientMemory                        ;
  ////////////////////////////////////////////////////////////////////////////
  result = WdmSyncIoctl                                                      (
             handle                                                          ,
             IOCTL_KS_PROPERTY                                               ,
             &ksPProp                                                        ,
             sizeof(KSP_PIN)                                                 ,
             (void *)*ksMultipleItem                                         ,
             multipleItemSize                                                ,
             NULL                                                          ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( result != NoError ) free ( ksMultipleItem )                           ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError WdmGetPropertyMulti                  (
                 HANDLE             handle          ,
                 const GUID      *  guidPropertySet ,
                 unsigned long      property        ,
                 KSMULTIPLE_ITEM ** ksMultipleItem  )
{
  CaError       result                                                       ;
  unsigned long multipleItemSize = 0                                         ;
  KSPROPERTY    ksProp                                                       ;
  ////////////////////////////////////////////////////////////////////////////
  ksProp . Set   = *guidPropertySet                                          ;
  ksProp . Id    = property                                                  ;
  ksProp . Flags = KSPROPERTY_TYPE_GET                                       ;
  ////////////////////////////////////////////////////////////////////////////
  result = WdmSyncIoctl                                                      (
             handle                                                          ,
             IOCTL_KS_PROPERTY                                               ,
             &ksProp                                                         ,
             sizeof(KSPROPERTY)                                              ,
             NULL                                                            ,
             0                                                               ,
             &multipleItemSize                                             ) ;
  if ( result != NoError ) return result                                     ;
  ////////////////////////////////////////////////////////////////////////////
  *ksMultipleItem = (KSMULTIPLE_ITEM *)Allocator::allocate(multipleItemSize) ;
  if ( !(*ksMultipleItem) ) return InsufficientMemory                        ;
  ////////////////////////////////////////////////////////////////////////////
  result = WdmSyncIoctl                                                      (
             handle                                                          ,
             IOCTL_KS_PROPERTY                                               ,
             &ksProp                                                         ,
             sizeof(KSPROPERTY)                                              ,
             (void*)*ksMultipleItem                                          ,
             multipleItemSize                                                ,
             NULL                                                          ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( result != NoError ) free ( ksMultipleItem )                           ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError WdmSetMuxNodeProperty (
                 HANDLE handle       ,
                 ULONG  nodeId       ,
                 ULONG  pinId        )
{
  KSNODEPROPERTY prop                                                      ;
  CaError        result   = NoError                                        ;
  prop . Property . Set   = KSPROPSETID_Audio                              ;
  prop . Property . Id    = KSPROPERTY_AUDIO_MUX_SOURCE                    ;
  prop . Property . Flags = KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY ;
  prop . NodeId           = nodeId                                         ;
  prop . Reserved         = 0                                              ;
  result                  = WdmSyncIoctl                                   (
                              handle                                       ,
                              IOCTL_KS_PROPERTY                            ,
                              &prop                                        ,
                              sizeof(KSNODEPROPERTY)                       ,
                              &pinId                                       ,
                              sizeof(ULONG)                                ,
                              NULL                                       ) ;
  return result                                                            ;
}

//////////////////////////////////////////////////////////////////////////////

static BOOL IsDeviceTheSame                 (
              const WdmKsDeviceInfo * pDev1 ,
              const WdmKsDeviceInfo * pDev2 )
{
  if ( pDev1 == NULL  ) return FALSE                       ;
  if ( pDev2 == NULL  ) return FALSE                       ;
  if ( pDev1 == pDev2 ) return TRUE                        ;
  if ( ::strcmp ( pDev1->compositeName                     ,
                  pDev2->compositeName ) == 0) return TRUE ;
  return FALSE                                             ;
}

//////////////////////////////////////////////////////////////////////////////

/* Used when traversing topology for outputs */
static const KSTOPOLOGY_CONNECTION * GetConnectionTo (
               const KSTOPOLOGY_CONNECTION * pFrom   ,
               CaWdmFilter                 * filter  ,
               int                           muxIdx  )
{
  unsigned                      i                                            ;
  const KSTOPOLOGY_CONNECTION * retval     = NULL                            ;
  const KSTOPOLOGY_CONNECTION * connections = (const KSTOPOLOGY_CONNECTION*)(filter->connections + 1) ;
  gPrintf ( ("GetConnectionTo: Checking %u connections... (pFrom = %p)", filter->connections->Count, pFrom ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < filter -> connections -> Count ; ++i )                   {
    const KSTOPOLOGY_CONNECTION * pConn = connections + i                    ;
    if ( pConn == pFrom ) continue                                           ;
    if ( pConn->FromNode == pFrom->ToNode )                                  {
      retval = pConn                                                         ;
      break                                                                  ;
    }                                                                        ;
  }                                                                          ;
  gPrintf(("GetConnectionTo: Returning %p\n", retval) )                      ;
  return retval                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

/* Used when traversing topology for inputs */

static const KSTOPOLOGY_CONNECTION * GetConnectionFrom (
               const KSTOPOLOGY_CONNECTION * pTo       ,
               CaWdmFilter                 * filter    ,
               int                           muxIdx    )
{
  unsigned                      i                                            ;
  const KSTOPOLOGY_CONNECTION * retval      = NULL                           ;
  const KSTOPOLOGY_CONNECTION * connections = (const KSTOPOLOGY_CONNECTION*)(filter->connections + 1);
  int                           muxCntr     = 0                              ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "GetConnectionFrom: Checking %u connections... (pTo = %p)\n"   ,
              filter->connections->Count , pTo                           ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < filter -> connections -> Count ; ++i )                   {
    const KSTOPOLOGY_CONNECTION * pConn = connections + i                    ;
    if ( pConn == pTo ) continue                                             ;
    if ( pConn -> ToNode == pTo -> FromNode )                                {
      if ( muxIdx >= 0 )                                                     {
        if ( muxCntr < muxIdx )                                              {
          ++muxCntr                                                          ;
          continue                                                           ;
        }                                                                    ;
      }                                                                      ;
      retval = pConn                                                         ;
      break                                                                  ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "GetConnectionFrom: Returning %p\n" , retval ) )               ;
  return retval                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

static ULONG GetNumberOfConnectionsTo               (
               const KSTOPOLOGY_CONNECTION * pTo    ,
               CaWdmFilter                 * filter )
{
  ULONG                         retval = 0                                   ;
  unsigned                      i                                            ;
  const KSTOPOLOGY_CONNECTION * connections = (const KSTOPOLOGY_CONNECTION *)(filter->connections + 1) ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf(("GetNumberOfConnectionsTo: Checking %u connections...", filter->connections->Count)) ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < filter -> connections -> Count ; ++i )                   {
    const KSTOPOLOGY_CONNECTION * pConn = connections + i                    ;
    if ( (   pConn -> ToNode    == pTo->FromNode                          ) &&
         ( ( pTo   -> FromNode  != KSFILTER_NODE                          ) ||
           ( pConn -> ToNodePin == pTo->FromNodePin                   ) ) )  {
      ++retval                                                               ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return retval                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

typedef const KSTOPOLOGY_CONNECTION *(*TFnGetConnection)(const KSTOPOLOGY_CONNECTION*, CaWdmFilter*, int) ;

static const KSTOPOLOGY_CONNECTION * FindStartConnectionFrom(ULONG startPin, CaWdmFilter * filter)
{
  unsigned i ;
  const KSTOPOLOGY_CONNECTION* connections = (const KSTOPOLOGY_CONNECTION*)(filter->connections + 1);
  ////////////////////////////////////////////////////////////////////////////
  gPrintf(("FindStartConnectionFrom: Checking %u connections...", filter->connections->Count)) ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < filter -> connections -> Count ; ++i )                   {
    const KSTOPOLOGY_CONNECTION* pConn = connections + i;
    if (pConn->ToNode == KSFILTER_NODE && pConn->ToNodePin == startPin)      {
            return pConn;
        }
    }
//  assert(FALSE);
  return 0;
}

//////////////////////////////////////////////////////////////////////////////

static const KSTOPOLOGY_CONNECTION* FindStartConnectionTo(ULONG startPin, CaWdmFilter* filter)
{
  unsigned i;
  const KSTOPOLOGY_CONNECTION* connections = (const KSTOPOLOGY_CONNECTION*)(filter->connections + 1);
  ////////////////////////////////////////////////////////////////////////////
  gPrintf(("FindStartConnectionTo: Checking %u connections...", filter->connections->Count));
  ////////////////////////////////////////////////////////////////////////////
  for (i = 0; i < filter->connections->Count; ++i)   {
    const KSTOPOLOGY_CONNECTION* pConn = connections + i;
    if (pConn->FromNode == KSFILTER_NODE && pConn->FromNodePin == startPin) {
      return pConn;
    }
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////////

static ULONG GetConnectedPin(ULONG startPin, BOOL forward, CaWdmFilter* filter, int muxPosition, ULONG *muxInputPinId, ULONG *muxNodeId)
{
  const KSTOPOLOGY_CONNECTION *conn = NULL;
  TFnGetConnection fnGetConnection = forward ? GetConnectionTo : GetConnectionFrom ;
  while (1)
    {
        if (conn == NULL)
        {
            conn = forward ? FindStartConnectionTo(startPin, filter) : FindStartConnectionFrom(startPin, filter);
        }
        else
        {
            conn = fnGetConnection(conn, filter, -1);
        }

        /* Handling case of erroneous connection list */
        if (conn == NULL)
        {
            break;
        }

        if (forward ? conn->ToNode == KSFILTER_NODE : conn->FromNode == KSFILTER_NODE)
        {
            return forward ? conn->ToNodePin : conn->FromNodePin;
        }
        else
        {
            gPrintf(("GetConnectedPin: count=%d, forward=%d, muxPosition=%d\n", filter->nodes->Count, forward, muxPosition)) ;
            if (filter->nodes->Count > 0 && !forward && muxPosition >= 0)
            {
                const GUID* nodes = (const GUID*)(filter->nodes + 1);
                if (IsEqualGUID(nodes[conn->FromNode], KSNODETYPE_MUX))
                {
                    ULONG nConn = GetNumberOfConnectionsTo(conn, filter);
                    conn = fnGetConnection(conn, filter, muxPosition);
                    if (conn == NULL)
                    {
                        break;
                    }
                    if (muxInputPinId != 0)
                    {
                        *muxInputPinId = conn->ToNodePin;
                    }
                    if (muxNodeId != 0)
                    {
                        *muxNodeId = conn->ToNode;
                    }
                }
            }
        }
    }
    return KSFILTER_NODE;
}

//////////////////////////////////////////////////////////////////////////////

static void DumpConnectionsAndNodes(CaWdmFilter* filter)
{
    unsigned i;
    const KSTOPOLOGY_CONNECTION* connections = (const KSTOPOLOGY_CONNECTION*)(filter->connections + 1);
    const GUID* nodes = (const GUID*)(filter->nodes + 1);

    gPrintf(("DumpConnectionsAndNodes: connections=%d, nodes=%d\n", filter->connections->Count, filter->nodes->Count)) ;

    for (i=0; i < filter->connections->Count; ++i)
    {
        const KSTOPOLOGY_CONNECTION* pConn = connections + i;
        gPrintf(("  Connection: %u - FromNode=%u,FromPin=%u -> ToNode=%u,ToPin=%u\n",
                               i,
                               pConn->FromNode, pConn->FromNodePin,
                               pConn->ToNode, pConn->ToNodePin
                    ) )                                                 ;
    }

    for (i=0; i < filter->nodes->Count; ++i)
    {
        const GUID* pConn = nodes + i;
          gPrintf(("  Node: %d - {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
                      i,
                      pConn->Data1, pConn->Data2, pConn->Data3,
                      pConn->Data4[0], pConn->Data4[1],
                      pConn->Data4[2], pConn->Data4[3],
                      pConn->Data4[4], pConn->Data4[5],
                      pConn->Data4[6], pConn->Data4[7]
                  ) )                                                    ;
    }

}

//////////////////////////////////////////////////////////////////////////////

static CaError GetNameFromCategory(const GUID* pGUID, BOOL input, wchar_t* name, unsigned length)
{
    CaError result = UnanticipatedHostError;
    USHORT usbTerminalGUID = (USHORT)(pGUID->Data1 - 0xDFF219E0);

    if (input && usbTerminalGUID >= 0x301 && usbTerminalGUID < 0x400)
    {
        /* Output terminal name for an input !? Set it to Line! */
        usbTerminalGUID = 0x603;
    }
    if (!input && usbTerminalGUID >= 0x201 && usbTerminalGUID < 0x300)
    {
        /* Input terminal name for an output !? Set it to Line! */
        usbTerminalGUID = 0x603;
    }
    if (usbTerminalGUID >= 0x201 && usbTerminalGUID < 0x713)
    {
        CaUsbTerminalGUIDToName s = { usbTerminalGUID };
        const CaUsbTerminalGUIDToName* ptr = (const CaUsbTerminalGUIDToName*)
        bsearch(
            &s,
            kNames,
            kNamesCnt,
            sizeof(CaUsbTerminalGUIDToName),
            CaUsbTerminalGUIDToNameCmp
            );
        if (ptr != 0)
        {
              gPrintf(("GetNameFromCategory: USB GUID %04X -> '%S'\n", usbTerminalGUID, ptr->name)) ;

            if (name != NULL && length > 0)
            {
                int n = _snwprintf(name, length, L"%s", ptr->name);
                if (usbTerminalGUID >= 0x601 && usbTerminalGUID < 0x700)
                {
                    _snwprintf(name + n, length - n, L" %s", (input ? L"In":L"Out"));
                }
            }
            result = NoError;
        }
    }
    else
    {
        WdmSetLastErrorInfo(result, "GetNameFromCategory: usbTerminalGUID = %04X ", usbTerminalGUID) ;
    }
    return result;
}

//////////////////////////////////////////////////////////////////////////////


static BOOL IsFrequencyWithinRange(const KSDATARANGE_AUDIO* range, int frequency)
{
    if (frequency < (int)range->MinimumSampleFrequency)
        return FALSE;
    if (frequency > (int)range->MaximumSampleFrequency)
        return FALSE;
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

static BOOL IsBitsWithinRange(const KSDATARANGE_AUDIO* range, int noOfBits)
{
    if (noOfBits < (int)range->MinimumBitsPerSample)
        return FALSE;
    if (noOfBits > (int)range->MaximumBitsPerSample)
        return FALSE;
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

static int DefaultSampleFrequencyIndex(const KSDATARANGE_AUDIO* range)
{
    int i;

    for(i=0; i < defaultSampleRateSearchOrderCount; ++i)
    {
        int currentFrequency = defaultSampleRateSearchOrder[i];

        if (IsFrequencyWithinRange(range, currentFrequency))
        {
            return i;
        }
    }

    return -1;
}

void PinFree(CaWdmPin * pin) ;
CaError FilterInitializePins(CaWdmFilter * filter) ;

/* Reopen the filter handle if necessary so it can be used */

static CaError FilterUse(CaWdmFilter* filter)
{
  if ( filter->handle == NULL )  {
        /* Open the filter */
        filter->handle = CreateFileW(
            filter->devInfo.filterPath,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
            NULL);

        if( filter->handle == NULL )
        {
            return DeviceUnavailable ;
        }
  }
  filter->usageCount++;
  return NoError;
}

/* Release the filter handle if nobody is using it */

static void FilterRelease(CaWdmFilter * filter)
{
  /* Check first topology filter, if used */
  if ( CaAND ( CaNotNull(filter->topologyFilter             ) ,
               CaNotNull(filter->topologyFilter->handle ) ) ) {
    FilterRelease ( filter -> topologyFilter )                ;
  }                                                           ;
  filter -> usageCount--                                      ;
  if ( CaIsEqual ( filter->usageCount , 0 ) )                 {
    if ( CaNotNull ( filter->handle ) )                       {
      ::CloseHandle( filter->handle )                         ;
      filter->handle = NULL                                   ;
    }                                                         ;
  }                                                           ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError PinQueryNotificationSupport(CaWdmPin * pPin, BOOL* pbResult)
{
  CaError    result = NoError                              ;
  KSPROPERTY propIn                                        ;
  //////////////////////////////////////////////////////////
  propIn . Set   = KSPROPSETID_RtAudio                     ;
  propIn . Id    = 8                   ; /* = KSPROPERTY_RTAUDIO_QUERY_NOTIFICATION_SUPPORT */
  propIn . Flags = KSPROPERTY_TYPE_GET                     ;
  //////////////////////////////////////////////////////////
  result = WdmSyncIoctl                                    (
             pPin->handle                                  ,
             IOCTL_KS_PROPERTY                             ,
             &propIn                                       ,
             sizeof(KSPROPERTY)                            ,
             pbResult                                      ,
             sizeof(BOOL)                                  ,
             NULL                                        ) ;
  if ( CaIsWrong ( result ) )                              {
    gPrintf ( ( "Failed PinQueryNotificationSupport\n" ) ) ;
  }                                                        ;
  return result                                            ;
}

/* Free a previously created filter */

static void FilterFree(CaWdmFilter * filter)
{
  if ( CaIsNull ( filter ) ) return                                          ;
  --filter->filterRefCount                                                   ;
  if ( CaIsGreater ( filter->filterRefCount , 0 ) ) return                   ;
  if ( CaNotNull   ( filter->topologyFilter     ) )                          {
    FilterFree ( filter -> topologyFilter )                                  ;
    filter -> topologyFilter = NULL                                          ;
  }                                                                          ;
  if ( CaNotNull ( filter->pins ) )                                          {
    for ( int pinId = 0 ; pinId < filter->pinCount ; pinId++ )               {
      if ( CaNotNull ( filter -> pins [ pinId ] ) )                          {
        PinFree ( filter -> pins [ pinId ] )                                 ;
      }                                                                      ;
    }                                                                        ;
    Allocator :: free ( filter->pins )                                       ;
    filter -> pinCount    = 0                                                ;
    filter -> pins        = NULL                                             ;
  }                                                                          ;
  if ( CaNotNull ( filter -> connections ) )                                 {
    Allocator :: free ( filter -> connections )                              ;
    filter -> connections = NULL                                             ;
  }                                                                          ;
  if ( CaNotNull ( filter -> nodes ) )                                       {
    Allocator :: free ( filter -> nodes )                                    ;
    filter -> nodes       = NULL                                             ;
  }                                                                          ;
  if ( CaNotEqual ( filter->handle , 0 ) ) ::CloseHandle ( filter->handle )  ;
  filter -> handle = NULL                                                    ;
  Allocator :: free ( filter )                                               ;
}

/* Create a new filter object. */
static CaWdmFilter     * FilterNew    (
         CaWdmKsType     type         ,
         DWORD           devNode      ,
         const wchar_t * filterName   ,
         const wchar_t * friendlyName ,
         CaError       * error        )
{
  CaWdmFilter * filter = 0                                                   ;
  CaError       result                                                       ;
  /* Allocate the new filter object                                         */
  filter = (CaWdmFilter *)Allocator::allocate(sizeof(CaWdmFilter))           ;
  if ( CaIsNull ( filter ) )                                                 {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ::memset ( filter , 0 , sizeof(CaWdmFilter) )                              ;
  filter -> devInfo . Initialize ( )                                         ;
  gPrintf ( ( "FilterNew: Creating filter '%S'\n" , friendlyName ) )         ;
  filter -> devInfo . streamingType = type                                   ;
  filter -> deviceNode              = devNode                                ;
  ::wcsncpy ( filter->devInfo.filterPath , filterName   , MAX_PATH )         ;
  ::wcsncpy ( filter->friendlyName       , friendlyName , MAX_PATH )         ;
  gPrintf ( ( "FilterNew: Opening filter...\n"    , friendlyName ) )         ;
  /* Open the filter handle                                                 */
  result = FilterUse ( filter )                                              ;
  if ( CaIsWrong ( result ) ) goto error                                     ;
  /* Get pin count                                                          */
  result = WdmGetPinPropertySimple                                           (
             filter->handle                                                  ,
             0                                                               ,
             &KSPROPSETID_Pin                                                ,
             KSPROPERTY_PIN_CTYPES                                           ,
             &(filter->pinCount)                                             ,
             sizeof(filter->pinCount)                                        ,
             NULL                                                          ) ;
  if ( CaIsWrong ( result ) ) goto error                                     ;
  /* Get connections & nodes for filter                                     */
  result = WdmGetPropertyMulti                                               (
             filter->handle                                                  ,
             &KSPROPSETID_Topology                                           ,
             KSPROPERTY_TOPOLOGY_CONNECTIONS                                 ,
             &(filter->connections)                                        ) ;
  if ( CaIsWrong ( result ) ) goto error                                     ;
  result = WdmGetPropertyMulti                                               (
             filter->handle                                                  ,
             &KSPROPSETID_Topology                                           ,
             KSPROPERTY_TOPOLOGY_NODES                                       ,
             &filter->nodes                                                ) ;
  if ( CaIsWrong ( result ) ) goto error                                     ;
  /* For debugging purposes                                                 */
  DumpConnectionsAndNodes ( filter )                                         ;
  /* Get product GUID (it might not be supported)                           */
  {
    KSCOMPONENTID compId                                                     ;
    if ( CaIsCorrect                                                         (
           WdmGetPropertySimple                                              (
             filter->handle                                                  ,
             &KSPROPSETID_General                                            ,
             KSPROPERTY_GENERAL_COMPONENTID                                  ,
             &compId                                                         ,
             sizeof(KSCOMPONENTID)                                     ) ) ) {
      filter -> devInfo . deviceProductGuid = compId . Product               ;
    }                                                                        ;
  }
  /* This section is not executed for topology filters                      */
  if ( CaNotEqual ( type , Type_kNotUsed ) )                                 {
    /* Initialize the pins                                                  */
    result = FilterInitializePins ( filter )                                 ;
    if ( CaIsWrong ( result ) ) goto error                                   ;
  }                                                                          ;
  /* Close the filter handle for now. It will be opened later when needed   */
  FilterRelease ( filter )                                                   ;
  *error = NoError                                                           ;
  return filter                                                              ;
error                                                                        :
  gPrintf    ( ( "FilterNew: Error %d\n", result ) )                         ;
  FilterFree ( filter                              )                         ;
  *error = result                                                            ;
  return NULL                                                                ;
}

//////////////////////////////////////////////////////////////////////////////

static CaWdmPin * PinNew(CaWdmFilter* parentFilter, unsigned long pinId, CaError* error)
{
  CaWdmPin        * pin         = NULL                                       ;
  CaError           result                                                   ;
  unsigned long     i                                                        ;
  KSMULTIPLE_ITEM * item        = NULL                                       ;
  KSIDENTIFIER    * identifier                                               ;
  KSDATARANGE     * dataRange                                                ;
  const ULONG       streamingId = (parentFilter->devInfo.streamingType == Type_kWaveRT) ? KSINTERFACE_STANDARD_LOOPED_STREAMING : KSINTERFACE_STANDARD_STREAMING ;
  int               defaultSampleRateIndex = defaultSampleRateSearchOrderCount;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "PinNew: Creating pin %d:\n" , pinId ) )                       ;
  /* Allocate the new PIN object                                            */
  pin = (CaWdmPin *)Allocator::allocate(sizeof(CaWdmPin) )                   ;
  if ( CaIsNull ( pin ) )                                                    {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  /* Zero the pin object                                                    */
  ::memset( (void *)pin , 0 , sizeof(CaWdmPin) )                             ;
  pin -> parentFilter = parentFilter                                         ;
  pin -> pinId        = pinId                                                ;
  /* Allocate a connect structure                                           */
  pin->pinConnectSize = sizeof(KSPIN_CONNECT) + sizeof(KSDATAFORMAT_WAVEFORMATEX);
  pin->pinConnect = (KSPIN_CONNECT *)Allocator::allocate(pin->pinConnectSize) ;
  if ( !pin->pinConnect )                                                    {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  /* Configure the connect structure with default values                    */
  pin->pinConnect->Interface.Set               = KSINTERFACESETID_Standard   ;
  pin->pinConnect->Interface.Id                = streamingId                 ;
  pin->pinConnect->Interface.Flags             = 0                           ;
  pin->pinConnect->Medium.Set                  = KSMEDIUMSETID_Standard      ;
  pin->pinConnect->Medium.Id                   = KSMEDIUM_TYPE_ANYINSTANCE   ;
  pin->pinConnect->Medium.Flags                = 0                           ;
  pin->pinConnect->PinId                       = pinId                       ;
  pin->pinConnect->PinToHandle                 = NULL                        ;
  pin->pinConnect->Priority.PriorityClass      = KSPRIORITY_NORMAL           ;
  pin->pinConnect->Priority.PrioritySubClass   = 1                           ;
  pin->ksDataFormatWfx = (KSDATAFORMAT_WAVEFORMATEX*)(pin->pinConnect + 1)   ;
  pin->ksDataFormatWfx->DataFormat.FormatSize  = sizeof(KSDATAFORMAT_WAVEFORMATEX);
  pin->ksDataFormatWfx->DataFormat.Flags       = 0                           ;
  pin->ksDataFormatWfx->DataFormat.Reserved    = 0                           ;
  pin->ksDataFormatWfx->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO     ;
  pin->ksDataFormatWfx->DataFormat.SubFormat   = KSDATAFORMAT_SUBTYPE_PCM    ;
  pin->ksDataFormatWfx->DataFormat.Specifier   = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
  pin->frameSize                               = 0                           ;
  /* Unknown until we instantiate pin */
  /* Get the COMMUNICATION property */
    result = WdmGetPinPropertySimple(
        parentFilter->handle,
        pinId,
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_COMMUNICATION,
        &pin->communication,
        sizeof(KSPIN_COMMUNICATION),
        NULL);
    if( result != NoError ) goto error;
    if( /*(pin->communication != KSPIN_COMMUNICATION_SOURCE) &&*/
        (pin->communication != KSPIN_COMMUNICATION_SINK) &&
        (pin->communication != KSPIN_COMMUNICATION_BOTH) )
    {
        gPrintf(("PinNew: Not source/sink\n"));
        result = InvalidDevice ;
        goto error;
    }
    /* Get dataflow information */
    result = WdmGetPinPropertySimple(
        parentFilter->handle,
        pinId,
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_DATAFLOW,
        &pin->dataFlow,
        sizeof(KSPIN_DATAFLOW),
        NULL);
    if( result != NoError ) goto error;
    /* Get the INTERFACE property list */
    result = WdmGetPinPropertyMulti(
        parentFilter->handle,
        pinId,
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_INTERFACES,
        &item);
    if( result != NoError ) goto error;
    identifier = (KSIDENTIFIER*)(item+1);
    /* Check that at least one interface is STANDARD_STREAMING */
    result = UnanticipatedHostError ;
    for( i = 0; i < item->Count; i++ ) {
        if( IsEqualGUID(identifier[i].Set,KSINTERFACESETID_Standard) && ( identifier[i].Id == streamingId ) )
        {
            result = NoError ;
            break;
        }
    }

    if( result != NoError )
    {
        gPrintf(("PinNew: No %s streaming\n", streamingId==KSINTERFACE_STANDARD_LOOPED_STREAMING?"looped":"standard"));
        goto error;
    }
    /* Don't need interfaces any more */
    Allocator::free ( item ) ;
    item = NULL;
    /* Get the MEDIUM properties list */
    result = WdmGetPinPropertyMulti(
        parentFilter->handle,
        pinId,
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_MEDIUMS,
        &item);
    if ( result != NoError ) goto error ;
    identifier = (KSIDENTIFIER*)(item+1); /* Not actually necessary... */
    /* Check that at least one medium is STANDARD_DEVIO */
    result = UnanticipatedHostError;
    for( i = 0; i < item->Count; i++ )
    {
        if( IsEqualGUID(identifier[i].Set,KSMEDIUMSETID_Standard) && ( identifier[i].Id == KSMEDIUM_STANDARD_DEVIO ) )
        {
            result = NoError ;
            break;
        }
    }
    if( result != NoError )
    {
        gPrintf(("No standard devio\n")) ;
        goto error;
    }
    /* Don't need mediums any more */
    Allocator::free ( item ) ;
    item = NULL;
    /* Get DATARANGES */
    result = WdmGetPinPropertyMulti(
        parentFilter->handle,
        pinId,
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_DATARANGES,
        &pin->dataRangesItem);

    if ( result != NoError ) goto error ;
    pin->dataRanges = (KSDATARANGE*)(pin->dataRangesItem +1);
    /* Check that at least one datarange supports audio */
    result                 = UnanticipatedHostError;
    dataRange              = pin->dataRanges;
    pin->maxChannels       = 0;
    pin->defaultSampleRate = 0;
    pin->formats           = 0;
    gPrintf(("PinNew: Checking %u no of dataranges...\n", pin->dataRangesItem->Count)) ;
    for ( i = 0; i < pin->dataRangesItem->Count; i++) {
        gPrintf(("PinNew: DR major format %x\n",*(unsigned long*)(&(dataRange->MajorFormat)))) ;
        /* Check that subformat is WAVEFORMATEX, PCM or WILDCARD */
        if( IS_VALID_WAVEFORMATEX_GUID(&dataRange->SubFormat) ||
            IsEqualGUID(dataRange->SubFormat  ,KSDATAFORMAT_SUBTYPE_PCM) ||
            IsEqualGUID(dataRange->SubFormat  ,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) ||
            IsEqualGUID(dataRange->SubFormat  ,KSDATAFORMAT_SUBTYPE_WILDCARD) ||
            IsEqualGUID(dataRange->MajorFormat,KSDATAFORMAT_TYPE_AUDIO) )
        {
            int defaultIndex ;
            result = NoError ;
            /* Record the maximum possible channels with this pin */
            if( ((KSDATARANGE_AUDIO*)dataRange)->MaximumChannels == (ULONG) -1 )
            {
                pin->maxChannels = MAXIMUM_NUMBER_OF_CHANNELS;
            }
            else if( (int) ((KSDATARANGE_AUDIO*)dataRange)->MaximumChannels > pin->maxChannels )
            {
                pin->maxChannels = (int) ((KSDATARANGE_AUDIO*)dataRange)->MaximumChannels;
            }
            gPrintf(("PinNew: MaxChannel: %d\n",pin->maxChannels)) ;
            /* Record the formats (bit depths) that are supported */
            if( IsBitsWithinRange((KSDATARANGE_AUDIO*)dataRange, 8) )
            {
                pin->formats |= cafInt8;
                gPrintf(("PinNew: Format PCM 8 bit supported\n"));
            }
            if( IsBitsWithinRange((KSDATARANGE_AUDIO*)dataRange, 16) )
            {
                pin->formats |= cafInt16;
                gPrintf(("PinNew: Format PCM 16 bit supported\n"));
            }
            if( IsBitsWithinRange((KSDATARANGE_AUDIO*)dataRange, 24) )
            {
                pin->formats |= cafInt24;
                gPrintf(("PinNew: Format PCM 24 bit supported\n"));
            }
            if( IsBitsWithinRange((KSDATARANGE_AUDIO*)dataRange, 32) )
            {
                if (IsEqualGUID(dataRange->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
                {
                    pin->formats |= cafFloat32;
                    gPrintf(("PinNew: Format IEEE float 32 bit supported\n"));
                }
                else
                {
                    pin->formats |= cafInt32;
                    gPrintf(("PinNew: Format PCM 32 bit supported\n"));
                }
            }
            defaultIndex = DefaultSampleFrequencyIndex((KSDATARANGE_AUDIO*)dataRange);
            if (defaultIndex >= 0 && defaultIndex < defaultSampleRateIndex)
            {
                defaultSampleRateIndex = defaultIndex;
            }
        }
        dataRange = (KSDATARANGE*)( ((char*)dataRange) + dataRange->FormatSize);
    }

    if ( result != NoError ) goto error;

    /* If none of the frequencies searched for are present, there's something seriously wrong */
    if (defaultSampleRateIndex == defaultSampleRateSearchOrderCount)
    {
        gPrintf(("PinNew: No default sample rate found, skipping pin!\n"));
        WdmSetLastErrorInfo(UnanticipatedHostError, "PinNew: No default sample rate found");
        result = UnanticipatedHostError;
        goto error;
    }

    /* Set the default sample rate */
    pin->defaultSampleRate = defaultSampleRateSearchOrder[defaultSampleRateIndex];
    gPrintf(("PinNew: Default sample rate = %d Hz\n", pin->defaultSampleRate)) ;

    /* Get instance information */
    result = WdmGetPinPropertySimple(
        parentFilter->handle,
        pinId,
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_CINSTANCES,
        &pin->instances,
        sizeof(KSPIN_CINSTANCES),
        NULL);

    if ( result != NoError ) goto error;

    /* If WaveRT, check if pin supports notification mode */
    if (parentFilter->devInfo.streamingType == Type_kWaveRT)
    {
        BOOL bSupportsNotification = FALSE;
        if (PinQueryNotificationSupport(pin, &bSupportsNotification) == NoError)
        {
            pin->pinKsSubType = bSupportsNotification ? SubType_kNotification : SubType_kPolled;
        }
    }

    /* Query pin name (which means we need to traverse to non IRP pin, via physical connection to topology filter pin, through
    its nodes to the endpoint pin, and get that ones name... phew...) */
    gPrintf(("PinNew: Finding topology pin...\n"));

    {
        ULONG topoPinId = GetConnectedPin(pinId, (pin->dataFlow == KSPIN_DATAFLOW_IN), parentFilter, -1, NULL, NULL);
        const wchar_t kInputName[] = L"Input";
        const wchar_t kOutputName[] = L"Output";
        if (topoPinId != KSFILTER_NODE) {
            /* Get physical connection for topo pin */
            unsigned long cbBytes = 0;
            gPrintf(("PinNew: Getting physical connection...\n"));
            result = WdmGetPinPropertySimple(parentFilter->handle,
                topoPinId,
                &KSPROPSETID_Pin,
                KSPROPERTY_PIN_PHYSICALCONNECTION,
                0,
                0,
                &cbBytes
                );
            if (result != NoError) {
                /* No physical connection -> there is no topology filter! So we get the name of the pin! */
                gPrintf(("PinNew: No physical connection! Getting the pin name\n")) ;
                result = WdmGetPinPropertySimple(parentFilter->handle,
                    topoPinId,
                    &KSPROPSETID_Pin,
                    KSPROPERTY_PIN_NAME,
                    pin->friendlyName,
                    MAX_PATH,
                    NULL);
                if (result != NoError) {
                    GUID category = {0};

                    /* Get pin category information */
                    result = WdmGetPinPropertySimple(parentFilter->handle,
                        topoPinId,
                        &KSPROPSETID_Pin,
                        KSPROPERTY_PIN_CATEGORY,
                        &category,
                        sizeof(GUID),
                        NULL);

                    if (result == NoError)
                    {
                        result = GetNameFromCategory(&category, (pin->dataFlow == KSPIN_DATAFLOW_OUT), pin->friendlyName, MAX_PATH) ;
                    }
                }
                /* Make sure pin gets a name here... */
                if (wcslen(pin->friendlyName) == 0)
                {
                    wcscpy(pin->friendlyName, (pin->dataFlow == KSPIN_DATAFLOW_IN) ? kOutputName : kInputName) ;
#ifdef UNICODE
                    gPrintf(("PinNew: Setting pin friendly name to '%s'\n", pin->friendlyName)) ;
#else
                    gPrintf(("PinNew: Setting pin friendly name to '%S'\n", pin->friendlyName)) ;
#endif
                }
                /* This is then == the endpoint pin */
                pin->endpointPinId = (pin->dataFlow == KSPIN_DATAFLOW_IN) ? pinId : topoPinId;
            } else {
                KSPIN_PHYSICALCONNECTION* pc = (KSPIN_PHYSICALCONNECTION*)Allocator::allocate(cbBytes + 2) ;
                gPrintf(("PinNew: Physical connection found!\n"));
                if (pc == NULL) {
                    result = InsufficientMemory ;
                    goto error;
                }
                result = WdmGetPinPropertySimple(parentFilter->handle,
                    topoPinId,
                    &KSPROPSETID_Pin,
                    KSPROPERTY_PIN_PHYSICALCONNECTION,
                    pc,
                    cbBytes,
                    NULL
                    );
                if (result == NoError)
                {
                    wchar_t symbLinkName[MAX_PATH];
                    wcsncpy(symbLinkName, pc->SymbolicLinkName, MAX_PATH);
                    if (symbLinkName[1] == TEXT('?'))
                    {
                        symbLinkName[1] = TEXT('\\');
                    }
                    if (pin->parentFilter->topologyFilter == NULL)
                    {
                        gPrintf(("PinNew: Creating topology filter '%S'\n", symbLinkName)) ;

                        pin->parentFilter->topologyFilter = FilterNew(Type_kNotUsed, 0, symbLinkName, L"", &result) ;
                        if (pin->parentFilter->topologyFilter == NULL)
                        {
                            gPrintf(("PinNew: Failed creating topology filter\n"));
                            result = UnanticipatedHostError;
                            WdmSetLastErrorInfo(result, "Failed to create topology filter '%S'", symbLinkName) ;
                            goto error;
                        }

                        /* Copy info so we have it in device info */
                        wcsncpy(pin->parentFilter->devInfo.topologyPath, symbLinkName, MAX_PATH);
                    }
                    else
                    {
                        /* Must be the same */
                    }

                    gPrintf(("PinNew: Opening topology filter...")) ;

                    result = FilterUse(pin->parentFilter->topologyFilter);

                    if (result == NoError)
                    {
                        unsigned long endpointPinId;

                        if (pin->dataFlow == KSPIN_DATAFLOW_IN)
                        {
                            /* The "endpointPinId" is what WASAPI looks at for pin names */
                            GUID category = {0};

                            gPrintf(("PinNew: Checking for output endpoint pin id...\n"));

                            endpointPinId = GetConnectedPin(pc->Pin, TRUE, pin->parentFilter->topologyFilter, -1, NULL, NULL);

                            if (endpointPinId == KSFILTER_NODE)
                            {
                                result = UnanticipatedHostError;
                                WdmSetLastErrorInfo(result, "Failed to get endpoint pin ID on topology filter!") ;
                                goto error;
                            }

                            gPrintf(("PinNew: Found endpoint pin id %u\n", endpointPinId));

                            /* Get pin category information */
                            result = WdmGetPinPropertySimple(pin->parentFilter->topologyFilter->handle,
                                endpointPinId,
                                &KSPROPSETID_Pin,
                                KSPROPERTY_PIN_CATEGORY,
                                &category,
                                sizeof(GUID),
                                NULL);
                            if (result == NoError) {
                                result = GetNameFromCategory(&category, (pin->dataFlow == KSPIN_DATAFLOW_OUT), pin->friendlyName, MAX_PATH) ;

                                if (wcslen(pin->friendlyName) == 0)
                                {
                                    wcscpy(pin->friendlyName, L"Output");
                                }
#ifdef UNICODE
                                gPrintf(("PinNew: Pin name '%s'\n", pin->friendlyName));
#else
                                gPrintf(("PinNew: Pin name '%S'\n", pin->friendlyName));
#endif
                            }

                            /* Set endpoint pin ID (this is the topology INPUT pin, since portmixer will always traverse the
                            filter in audio streaming direction, see http://msdn.microsoft.com/en-us/library/windows/hardware/ff536331(v=vs.85).aspx
                            for more information)
                            */
                            pin->endpointPinId = pc->Pin;
                        } else {
                            unsigned muxCount = 0;
                            int muxPos = 0;
                            /* Max 64 multiplexer inputs... sanity check :) */
                            for (i = 0; i < 64; ++i) {
                                ULONG muxNodeIdTest = (unsigned)-1;
                                gPrintf(("PinNew: Checking for input endpoint pin id (%d)...\n", i)) ;

                                endpointPinId = GetConnectedPin(pc->Pin,
                                    FALSE,
                                    pin->parentFilter->topologyFilter,
                                    (int)i,
                                    NULL,
                                    &muxNodeIdTest);

                                if (endpointPinId == KSFILTER_NODE)
                                {
                                    /* We're done */
                                    gPrintf(("PinNew: Done with inputs.\n", endpointPinId));
                                    break;
                                } else {
                                    /* The "endpointPinId" is what WASAPI looks at for pin names */
                                    GUID category = {0};

                                    gPrintf(("PinNew: Found endpoint pin id %u\n", endpointPinId)) ;

                                    /* Get pin category information */
                                    result = WdmGetPinPropertySimple(pin->parentFilter->topologyFilter->handle,
                                        endpointPinId,
                                        &KSPROPSETID_Pin,
                                        KSPROPERTY_PIN_CATEGORY,
                                        &category,
                                        sizeof(GUID),
                                        NULL);

                                    if (result == NoError)
                                    {
                                        if (muxNodeIdTest == (unsigned)-1)
                                        {
                                            /* Ok, try pin name, and favor that if available */
                                            result = WdmGetPinPropertySimple(pin->parentFilter->topologyFilter->handle,
                                                endpointPinId,
                                                &KSPROPSETID_Pin,
                                                KSPROPERTY_PIN_NAME,
                                                pin->friendlyName,
                                                MAX_PATH,
                                                NULL);

                                            if (result != NoError)
                                            {
                                                result = GetNameFromCategory(&category, TRUE, pin->friendlyName, MAX_PATH);
                                            }
                                            break;
                                        } else {
                                            result = GetNameFromCategory(&category, TRUE, NULL, 0);

                                            if (result == NoError)
                                            {
                                                ++muxCount;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        gPrintf(("PinNew: Failed to get pin category"));
                                    }
                                }
                            }

                            if (muxCount == 0)
                            {
                                pin->endpointPinId = endpointPinId;
                                /* Make sure we get a name for the pin */
                                if (wcslen(pin->friendlyName) == 0)
                                {
                                    wcscpy(pin->friendlyName, kInputName);
                                }
#ifdef UNICODE
                                gPrintf(("PinNew: Input friendly name '%s'\n", pin->friendlyName));
#else
                                gPrintf(("PinNew: Input friendly name '%S'\n", pin->friendlyName));
#endif
                            } else { // muxCount > 0
                                gPrintf(("PinNew: Setting up %u inputs\n", muxCount));

                                /* Now we redo the operation once known how many multiplexer positions there are */
                                pin->inputs = (CaWdmMuxedInput **)Allocator::allocate(muxCount * sizeof(CaWdmMuxedInput*)) ;
                                if (pin->inputs == NULL)
                                {
                                    FilterRelease(pin->parentFilter->topologyFilter);
                                    result = InsufficientMemory ;
                                    goto error;
                                }
                                pin->inputCount = muxCount;

                                for (i = 0; i < muxCount; ++muxPos)
                                {
                                    gPrintf(("PinNew: Setting up input %u...\n", i));

                                    if (pin->inputs[i] == NULL)
                                    {
                                        pin->inputs[i] = (CaWdmMuxedInput *)Allocator::allocate(sizeof(CaWdmMuxedInput)) ;
                                        if (pin->inputs[i] == NULL)
                                        {
                                            FilterRelease(pin->parentFilter->topologyFilter);
                                            result = InsufficientMemory ;
                                            goto error;
                                        }
                                    }

                                    endpointPinId = GetConnectedPin(pc->Pin,
                                        FALSE,
                                        pin->parentFilter->topologyFilter,
                                        muxPos,
                                        &pin->inputs[i]->muxPinId,
                                        &pin->inputs[i]->muxNodeId);

                                    if (endpointPinId != KSFILTER_NODE)
                                    {
                                        /* The "endpointPinId" is what WASAPI looks at for pin names */
                                        GUID category = {0};

                                        /* Set input endpoint ID */
                                        pin->inputs[i]->endpointPinId = endpointPinId;

                                        /* Get pin category information */
                                        result = WdmGetPinPropertySimple(pin->parentFilter->topologyFilter->handle,
                                            endpointPinId,
                                            &KSPROPSETID_Pin,
                                            KSPROPERTY_PIN_CATEGORY,
                                            &category,
                                            sizeof(GUID),
                                            NULL);

                                        if (result == NoError)
                                        {
                                            /* Try pin name first, and if that is not defined, use category instead */
                                            result = WdmGetPinPropertySimple(pin->parentFilter->topologyFilter->handle,
                                                endpointPinId,
                                                &KSPROPSETID_Pin,
                                                KSPROPERTY_PIN_NAME,
                                                pin->inputs[i]->friendlyName,
                                                MAX_PATH,
                                                NULL);

                                            if (result != NoError)
                                            {
                                                result = GetNameFromCategory(&category, TRUE, pin->inputs[i]->friendlyName, MAX_PATH);
                                                if (result != NoError)
                                                {
                                                    /* Only specify name, let name hash in ScanDeviceInfos fix postfix enumerators */
                                                    wcscpy(pin->inputs[i]->friendlyName, kInputName);
                                                }
                                            }
#ifdef UNICODE
                                            gPrintf(("PinNew: Input (%u) friendly name '%s'\n", i, pin->inputs[i]->friendlyName));
#else
                                            gPrintf(("PinNew: Input (%u) friendly name '%S'\n", i, pin->inputs[i]->friendlyName));
#endif
                                            ++i;
                                        }
                      } else {
                                        /* Should never come here! */
                      }
                    }
                  }
                }
              }
            }
            Allocator::free(pc) ;
          }
        } else {
          gPrintf(("PinNew: No topology pin id found. Bad...\n"));
          /* No TOPO pin id ??? This is bad. Ok, so we just say it is an input or output... */
          wcscpy(pin->friendlyName, (pin->dataFlow == KSPIN_DATAFLOW_IN) ? kOutputName : kInputName);
        }
    }
    /* Release topology filter if it has been used */
    if (pin->parentFilter->topologyFilter && pin->parentFilter->topologyFilter->handle != NULL)
    {
        gPrintf(("PinNew: Releasing topology filter...\n")) ;
        FilterRelease(pin->parentFilter->topologyFilter);
    }
    /* Success */
    *error = NoError;
    gPrintf(("Pin created successfully\n"));
    return pin;

error:
    gPrintf(("PinNew: Error %d\n", result));
    /* Error cleanup */

    Allocator::free( item );
    if( pin )
    {
        if (pin->parentFilter->topologyFilter && pin->parentFilter->topologyFilter->handle != NULL)
        {
            FilterRelease(pin->parentFilter->topologyFilter);
        }

        Allocator::free( pin->pinConnect ) ;
        Allocator::free( pin->dataRangesItem ) ;
        Allocator::free( pin ) ;
    }
    *error = result;
    return NULL;
}

/* Set the state of this (instantiated) pin */

static CaError PinSetState(CaWdmPin * pin, KSSTATE state)
{
  CaError    result = NoError                           ;
  KSPROPERTY prop                                       ;
  ///////////////////////////////////////////////////////
  prop . Set   = KSPROPSETID_Connection                 ;
  prop . Id    = KSPROPERTY_CONNECTION_STATE            ;
  prop . Flags = KSPROPERTY_TYPE_SET                    ;
  ///////////////////////////////////////////////////////
  CaRETURN ( CaIsNull ( pin         ) , InternalError ) ;
  CaRETURN ( CaIsNull ( pin->handle ) , InternalError ) ;
  ///////////////////////////////////////////////////////
  result = WdmSyncIoctl                                 (
             pin->handle                                ,
             IOCTL_KS_PROPERTY                          ,
             &prop                                      ,
             sizeof(KSPROPERTY)                         ,
             &state                                     ,
             sizeof(KSSTATE)                            ,
             NULL                                     ) ;
  ///////////////////////////////////////////////////////
  return result                                         ;
}

/* If the pin handle is open, close it */
static void PinClose(CaWdmPin * pin)
{
  if ( CaIsNull ( pin             ) ) return ;
  if ( CaIsEqual( pin->handle , 0 ) ) return ;
  PinSetState   ( pin ,  KSSTATE_PAUSE )     ;
  PinSetState   ( pin ,  KSSTATE_STOP  )     ;
  ::CloseHandle ( pin -> handle        )     ;
  pin->handle = NULL                         ;
  FilterRelease ( pin -> parentFilter )      ;
}

/* Safely free all resources associated with the pin */

static void PinFree(CaWdmPin * pin)
{
  if ( CaIsNull ( pin ) ) return                                             ;
  PinClose ( pin )                                                           ;
  if ( NULL != pin->pinConnect     ) Allocator::free ( pin->pinConnect     ) ;
  if ( NULL != pin->dataRangesItem ) Allocator::free ( pin->dataRangesItem ) ;
  if ( NULL != pin->inputs         )                                         {
    for (int i = 0 ; i < pin -> inputCount ; ++i )                           {
      Allocator::free ( pin->inputs[i] )                                     ;
    }                                                                        ;
    Allocator::free ( pin->inputs )                                          ;
  }                                                                          ;
  Allocator::free ( pin )                                                    ;
}

static CaError PinInstantiate(CaWdmPin* pin)
{
  CaError                result                                              ;
  unsigned long          createResult                                        ;
  KSALLOCATOR_FRAMING    ksaf                                                ;
  KSALLOCATOR_FRAMING_EX ksafex                                              ;
  ////////////////////////////////////////////////////////////////////////////
  if (  pin == NULL       ) return InternalError                             ;
  if ( !pin -> pinConnect ) return InternalError                             ;
  FilterUse ( pin -> parentFilter )                                          ;
  ////////////////////////////////////////////////////////////////////////////
  createResult = FunctionKsCreatePin                                         (
                   pin->parentFilter->handle                                 ,
                   pin->pinConnect                                           ,
                   GENERIC_WRITE | GENERIC_READ                              ,
                   &pin->handle                                            ) ;
  gPrintf ( ( "Pin create result = 0x%08x\n" , createResult ) )              ;
  ////////////////////////////////////////////////////////////////////////////
  if ( createResult != ERROR_SUCCESS )                                       {
    FilterRelease ( pin -> parentFilter )                                    ;
    pin -> handle = NULL                                                     ;
    switch ( createResult )                                                  {
      case ERROR_INVALID_PARAMETER                                           :
        /* First case when pin actually don't support the format            */
      return SampleFormatNotSupported                                        ;
      case ERROR_BAD_COMMAND                                                 :
        /* Case when pin is occupied (by another application)               */
      return DeviceUnavailable                                               ;
      default                                                                :
        /* All other cases                                                  */
      return InvalidDevice                                                   ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( pin->parentFilter->devInfo.streamingType == Type_kWaveCyclic )        {
    /* Framing size query only valid for WaveCyclic devices                 */
    result = WdmGetPropertySimple                                            (
               pin->handle                                                   ,
               &KSPROPSETID_Connection                                       ,
               KSPROPERTY_CONNECTION_ALLOCATORFRAMING                        ,
               &ksaf                                                         ,
               sizeof(ksaf)                                                ) ;
    if ( NoError != result )                                                 {
      result = WdmGetPropertySimple                                          (
                 pin->handle                                                 ,
                 &KSPROPSETID_Connection                                     ,
                 KSPROPERTY_CONNECTION_ALLOCATORFRAMING_EX                   ,
                 &ksafex                                                     ,
                 sizeof(ksafex)                                            ) ;
      if ( NoError == result )                                               {
        pin->frameSize = ksafex.FramingItem[0].FramingRange.Range.MinFrameSize;
      }                                                                      ;
    } else                                                                   {
      pin -> frameSize = ksaf . FrameSize                                    ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

static CaError PinSetFormat(CaWdmPin * pin,const WAVEFORMATEX * format)
{
  unsigned long size                                                         ;
  void        * newConnect                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL == pin    ) return InternalError                                 ;
  if ( NULL == format ) return InternalError                                 ;
  ////////////////////////////////////////////////////////////////////////////
  size = GetWfexSize(format)                                                 +
         sizeof(KSPIN_CONNECT)                                               +
         sizeof(KSDATAFORMAT_WAVEFORMATEX)                                   -
         sizeof(WAVEFORMATEX)                                                ;
  ////////////////////////////////////////////////////////////////////////////
  if ( pin->pinConnectSize != size )                                         {
    newConnect = Allocator::allocate( size )                                 ;
    if ( newConnect == NULL ) return InsufficientMemory                      ;
    memcpy ( newConnect                                                      ,
             (void*)pin->pinConnect                                          ,
             min(pin->pinConnectSize,size)                                 ) ;
    Allocator :: free ( pin->pinConnect )                                    ;
    pin -> pinConnect      = (KSPIN_CONNECT *) newConnect                    ;
    pin -> pinConnectSize  = size                                            ;
    pin -> ksDataFormatWfx = (KSDATAFORMAT_WAVEFORMATEX*)((KSPIN_CONNECT*)newConnect + 1) ;
    pin -> ksDataFormatWfx -> DataFormat.FormatSize = size - sizeof(KSPIN_CONNECT);
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  memcpy ( (void*)&(pin->ksDataFormatWfx->WaveFormatEx)                      ,
           format                                                            ,
           GetWfexSize(format)                                             ) ;
  pin->ksDataFormatWfx->DataFormat.SampleSize                                =
    (unsigned short)(format->nChannels * (format->wBitsPerSample / 8))       ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

static CaError PinIsFormatSupported          (
                 CaWdmPin           * pin    ,
                 const WAVEFORMATEX * format )
{
  CaError                      result = InvalidDevice                        ;
  KSDATARANGE_AUDIO          * dataRange                                     ;
  unsigned long                count                                         ;
  GUID                         guid = DYNAMIC_GUID( DEFINE_WAVEFORMATEX_GUID(format->wFormatTag) );
  const WAVEFORMATEXTENSIBLE * pFormatExt = (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) ? (const WAVEFORMATEXTENSIBLE*)format : 0;
  ////////////////////////////////////////////////////////////////////////////
  if ( pFormatExt != 0 )                                                     {
    guid = pFormatExt->SubFormat                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  dataRange = (KSDATARANGE_AUDIO*)pin->dataRanges                            ;
  for ( count = 0                                                            ;
        count<pin->dataRangesItem->Count                                     ;
        count++                                                              ,
        dataRange = (KSDATARANGE_AUDIO*)( ((char*)dataRange) + dataRange->DataRange.FormatSize)) /* Need to update dataRange here, due to 'continue' !! */
  { /* Check major format                                                   */
    if (!(IsEqualGUID((dataRange->DataRange.MajorFormat), KSDATAFORMAT_TYPE_AUDIO) ||
          IsEqualGUID((dataRange->DataRange.MajorFormat), KSDATAFORMAT_TYPE_WILDCARD))) {
      continue                                                               ;
    }                                                                        ;
    /* This is an audio or wildcard datarange...                            */
    if (! (IsEqualGUID((dataRange->DataRange.SubFormat), KSDATAFORMAT_SUBTYPE_WILDCARD) ||
           IsEqualGUID((dataRange->DataRange.SubFormat), KSDATAFORMAT_SUBTYPE_PCM) ||
           IsEqualGUID((dataRange->DataRange.SubFormat), guid)           ) ) {
      continue                                                               ;
    }                                                                        ;
    /* Check specifier...                                                   */
    if (! (IsEqualGUID((dataRange->DataRange.Specifier), KSDATAFORMAT_SPECIFIER_WILDCARD) ||
           IsEqualGUID((dataRange->DataRange.Specifier), KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) ) {
      continue                                                               ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    gPrintf ( ( "Pin:%x\n"             , (void*)pin                      ) ) ;
    gPrintf ( ( "\tDataRange:%d\n"     , count                           ) ) ;
    gPrintf ( ( "\tFormatSize:%d\n"    , dataRange->DataRange.FormatSize ) ) ;
    gPrintf ( ( "\tSampleSize:%d\n"    , dataRange->DataRange.SampleSize ) ) ;
    gPrintf ( ( "\tMaxChannels:%d\n"   , dataRange->MaximumChannels      ) ) ;
    gPrintf ( ( "\tBits:%d-%d\n"       , dataRange->MinimumBitsPerSample     ,
                                         dataRange->MaximumBitsPerSample ) ) ;
    gPrintf ( ( "\tSampleRate:%d-%d\n" , dataRange->MinimumSampleFrequency   ,
                                         dataRange->MaximumSampleFrequency)) ;
    //////////////////////////////////////////////////////////////////////////
    if ( dataRange->MaximumChannels != (ULONG)-1 &&
         dataRange->MaximumChannels < format->nChannels )                    {
      result = InvalidChannelCount                                           ;
      continue                                                               ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( CaNotEqual ( pFormatExt , 0 ) )                                     {
      if ( CaIsGreater ( dataRange->MinimumBitsPerSample                     ,
                         pFormatExt->Samples.wValidBitsPerSample         ) ) {
        result = SampleFormatNotSupported                                    ;
        continue                                                             ;
      }                                                                      ;
      if ( CaIsLess ( dataRange  -> MaximumBitsPerSample                     ,
                      pFormatExt -> Samples.wValidBitsPerSample          ) ) {
        result = SampleFormatNotSupported                                    ;
        continue                                                             ;
      }                                                                      ;
    } else                                                                   {
      if ( CaIsGreater ( dataRange->MinimumBitsPerSample                     ,
                         format->wBitsPerSample                          ) ) {
        result = SampleFormatNotSupported                                    ;
        continue                                                             ;
      }                                                                      ;
      if ( CaIsLess ( dataRange -> MaximumBitsPerSample                      ,
                      format    -> wBitsPerSample                        ) ) {
        result = SampleFormatNotSupported                                    ;
        continue                                                             ;
      }                                                                      ;
    }                                                                        ;
    if ( CaIsGreater ( dataRange -> MinimumSampleFrequency                   ,
                       format    -> nSamplesPerSec                       ) ) {
      result = InvalidSampleRate                                             ;
      continue                                                               ;
    }                                                                        ;
    if ( CaIsLess ( dataRange -> MaximumSampleFrequency                      ,
                    format    -> nSamplesPerSec                          ) ) {
      result = InvalidSampleRate                                             ;
      continue                                                               ;
    }                                                                        ;
    /* Success!                                                             */
    result = NoError                                                         ;
    break                                                                    ;
  }                                                                          ;
  return result                                                              ;
}

static CaError PinGetBufferWithNotification(CaWdmPin* pPin, void** pBuffer, DWORD* pRequestedBufSize, BOOL* pbCallMemBarrier)
{
  CaError result = NoError;
    KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION propIn;
    KSRTAUDIO_BUFFER propOut;

    propIn.BaseAddress = 0;
    propIn.NotificationCount = 2;
    propIn.RequestedBufferSize = *pRequestedBufSize;
    propIn.Property.Set = KSPROPSETID_RtAudio;
    propIn.Property.Id = KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION;
    propIn.Property.Flags = KSPROPERTY_TYPE_GET;

    result = WdmSyncIoctl(pPin->handle, IOCTL_KS_PROPERTY,
        &propIn,
        sizeof(KSRTAUDIO_BUFFER_PROPERTY_WITH_NOTIFICATION),
        &propOut,
        sizeof(KSRTAUDIO_BUFFER),
        NULL);

    if ( result == NoError ) {
        *pBuffer = propOut.BufferAddress;
        *pRequestedBufSize = propOut.ActualBufferSize;
        *pbCallMemBarrier = propOut.CallMemoryBarrier;
    }
    else
    {
        gPrintf(("Failed to get buffer with notification\n")) ;
    }
    return result;
}

static CaError PinGetBufferWithoutNotification(CaWdmPin* pPin, void** pBuffer, DWORD* pRequestedBufSize, BOOL* pbCallMemBarrier)
{
  CaError result = NoError;
  KSRTAUDIO_BUFFER_PROPERTY propIn;
  KSRTAUDIO_BUFFER propOut;

    propIn.BaseAddress = NULL;
    propIn.RequestedBufferSize = *pRequestedBufSize;
    propIn.Property.Set = KSPROPSETID_RtAudio;
    propIn.Property.Id = KSPROPERTY_RTAUDIO_BUFFER;
    propIn.Property.Flags = KSPROPERTY_TYPE_GET;

    result = WdmSyncIoctl(pPin->handle, IOCTL_KS_PROPERTY,
        &propIn,
        sizeof(KSRTAUDIO_BUFFER_PROPERTY),
        &propOut,
        sizeof(KSRTAUDIO_BUFFER),
        NULL);

    if (result == NoError)
    {
        *pBuffer = propOut.BufferAddress;
        *pRequestedBufSize = propOut.ActualBufferSize;
        *pbCallMemBarrier = propOut.CallMemoryBarrier;
    }
    else
    {
        gPrintf(("Failed to get buffer without notification\n")) ;
    }
    return result;
}

/* This function will handle getting the cyclic buffer from a WaveRT driver. Certain WaveRT drivers needs to have
requested buffer size on multiples of 128 bytes:

*/
static CaError PinGetBuffer(CaWdmPin* pPin, void** pBuffer, DWORD* pRequestedBufSize, BOOL* pbCallMemBarrier)
{
  CaError result = NoError;

  while ( 1 )                                                                {
        if (pPin->pinKsSubType != SubType_kPolled)
        {
            /* In case of unknown (or notification), we try both modes */
            result = PinGetBufferWithNotification(pPin, pBuffer, pRequestedBufSize, pbCallMemBarrier);
            if (result == NoError)
            {
                gPrintf(("PinGetBuffer: SubType_kNotification\n")) ;
                pPin->pinKsSubType = SubType_kNotification;
                break;
            }
        }

        result = PinGetBufferWithoutNotification(pPin, pBuffer, pRequestedBufSize, pbCallMemBarrier) ;
        if (result == NoError)
        {
            gPrintf(("PinGetBuffer: SubType_kPolled\n")) ;
            pPin->pinKsSubType = SubType_kPolled;
            break;
        }

        /* Check if requested size is on a 128 byte boundary */
        if (((*pRequestedBufSize) % 128UL) == 0)
        {
            gPrintf(("Buffer size on 128 byte boundary, still fails :(\n"));
            /* Ok, can't do much more */
            break;
        } else {
            /* Compute LCM so we know which sizes are on a 128 byte boundary */
            const unsigned gcd = WdmGCD(128UL, pPin->ksDataFormatWfx->WaveFormatEx.nBlockAlign) ;
            const unsigned lcm = (128UL * pPin->ksDataFormatWfx->WaveFormatEx.nBlockAlign) / gcd;
            DWORD dwOldSize = *pRequestedBufSize;

            /* Align size to (next larger) LCM byte boundary, and then we try again. Note that LCM is not necessarily a
            power of 2. */
            *pRequestedBufSize = ((*pRequestedBufSize + lcm - 1) / lcm) * lcm;

            gPrintf(("Adjusting buffer size from %u to %u bytes (128 byte boundary, LCM=%u)\n", dwOldSize, *pRequestedBufSize, lcm));
        }
    }
    return result;
}

static CaError PinRegisterPositionRegister(CaWdmPin* pPin)
{
  CaError result = NoError;
  KSRTAUDIO_HWREGISTER_PROPERTY propIn;
  KSRTAUDIO_HWREGISTER propOut;

    propIn.BaseAddress = NULL;
    propIn.Property.Set = KSPROPSETID_RtAudio;
    propIn.Property.Id = KSPROPERTY_RTAUDIO_POSITIONREGISTER;
    propIn.Property.Flags = KSPROPERTY_TYPE_GET;

    result = WdmSyncIoctl(pPin->handle, IOCTL_KS_PROPERTY,
        &propIn,
        sizeof(KSRTAUDIO_HWREGISTER_PROPERTY),
        &propOut,
        sizeof(KSRTAUDIO_HWREGISTER),
        NULL);

    if (result == NoError)
    {
        pPin->positionRegister = (ULONG*)propOut.Register;
    }
    else
    {
        gPrintf(("Failed to register position register\n")) ;
    }

    return result;
}

static CaError PinRegisterNotificationHandle(CaWdmPin* pPin, HANDLE handle)
{
  CaError result = NoError;
    KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY prop;

    prop.NotificationEvent = handle;
    prop.Property.Set = KSPROPSETID_RtAudio;
    prop.Property.Id = KSPROPERTY_RTAUDIO_REGISTER_NOTIFICATION_EVENT;
    prop.Property.Flags = KSPROPERTY_TYPE_GET;

    result = WdmSyncIoctl(pPin->handle,
        IOCTL_KS_PROPERTY,
        &prop,
        sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY),
        &prop,
        sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY),
        NULL);

    if (result != NoError) {
        gPrintf(("Failed to register notification handle 0x%08X\n", handle)) ;
    }

    return result;
}

static CaError PinUnregisterNotificationHandle(CaWdmPin* pPin, HANDLE handle)
{
  CaError result = NoError;
    KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY prop;


    if (handle != NULL)
    {
        prop.NotificationEvent = handle;
        prop.Property.Set = KSPROPSETID_RtAudio;
        prop.Property.Id = KSPROPERTY_RTAUDIO_UNREGISTER_NOTIFICATION_EVENT;
        prop.Property.Flags = KSPROPERTY_TYPE_GET;

        result = WdmSyncIoctl(pPin->handle,
            IOCTL_KS_PROPERTY,
            &prop,
            sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY),
            &prop,
            sizeof(KSRTAUDIO_NOTIFICATION_EVENT_PROPERTY),
            NULL);

        if (result != NoError) {
            gPrintf(("Failed to unregister notification handle 0x%08X\n", handle));
        }
    }
    return result;
}

static CaError PinGetHwLatency(CaWdmPin* pPin, ULONG* pFifoSize, ULONG* pChipsetDelay, ULONG* pCodecDelay)
{
  CaError             result = NoError;
  KSPROPERTY          propIn;
  KSRTAUDIO_HWLATENCY propOut;

    propIn.Set = KSPROPSETID_RtAudio;
    propIn.Id = KSPROPERTY_RTAUDIO_HWLATENCY;
    propIn.Flags = KSPROPERTY_TYPE_GET;

    result = WdmSyncIoctl(pPin->handle, IOCTL_KS_PROPERTY,
        &propIn,
        sizeof(KSPROPERTY),
        &propOut,
        sizeof(KSRTAUDIO_HWLATENCY),
        NULL);

    if (result == NoError)
    {
        *pFifoSize = propOut.FifoSize;
        *pChipsetDelay = propOut.ChipsetDelay;
        *pCodecDelay = propOut.CodecDelay;
    }
    else
    {
        gPrintf(("Failed to retrieve hardware FIFO size!\n"));
    }

    return result;
}

/* This one is used for WaveRT */
static CaError PinGetAudioPositionDirect(CaWdmPin* pPin, ULONG* pPosition)
{
  *pPosition = (*pPin->positionRegister);
  return NoError;
}

/* This one also, but in case the driver hasn't implemented memory mapped access to the position register */
static CaError PinGetAudioPositionViaIOCTL(CaWdmPin* pPin, ULONG* pPosition)
{
  CaError          result = NoError                                          ;
  KSPROPERTY       propIn                                                    ;
  KSAUDIO_POSITION propOut                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  propIn . Set   = KSPROPSETID_Audio                                         ;
  propIn . Id    = KSPROPERTY_AUDIO_POSITION                                 ;
  propIn . Flags = KSPROPERTY_TYPE_GET                                       ;
  ////////////////////////////////////////////////////////////////////////////
  result = WdmSyncIoctl                                                      (
             pPin->handle                                                    ,
             IOCTL_KS_PROPERTY                                               ,
             &propIn                                                         ,
             sizeof(KSPROPERTY)                                              ,
             &propOut                                                        ,
             sizeof(KSAUDIO_POSITION)                                        ,
             NULL                                                          ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaIsCorrect ( result ) )                                              {
    *pPosition = (ULONG) ( propOut . PlayOffset )                            ;
  } else                                                                     {
    gPrintf ( ( "Failed to get audio position!\n" ) )                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

/* Initialize the pins of the filter. This is separated from FilterNew because
 * this might fail if there is another process using the pin(s).            */

CaError FilterInitializePins(CaWdmFilter * filter)
{
  CaError result = NoError;
  int     pinId;

  if (filter->devInfo.streamingType == Type_kNotUsed) return NoError;
  if (filter->pins != NULL) return NoError;
  /* Allocate pointer array to hold the pins */
  filter->pins = (CaWdmPin **)Allocator::allocate(sizeof(CaWdmPin*) * filter->pinCount ) ;
  CaExprCorrect ( !filter->pins , InsufficientMemory ) ;
  /* Create all the pins we can */
    for(pinId = 0; pinId < filter->pinCount; pinId++)
    {
        /* Create the pin with this Id */
        CaWdmPin* newPin;
        newPin = PinNew(filter, pinId, &result);
        if( result == InsufficientMemory )
            goto error;
        if( newPin != NULL )
        {
            filter->pins[pinId] = newPin;
            ++filter->validPinCount;
        }
    }

    if (filter->validPinCount == 0)
    {
        result = DeviceUnavailable;
        goto error;
    }

    return NoError ;

error:

    if (filter->pins)
    {
        for (pinId = 0; pinId < filter->pinCount; ++pinId)
        {
            if (filter->pins[pinId])
            {
                Allocator::free (filter->pins[pinId]);
                filter->pins[pinId] = 0;
            }
        }
        Allocator::free ( filter->pins ) ;
        filter->pins = 0;
    }

    return result;
}

/* Add reference to filter */
static void FilterAddRef(CaWdmFilter * filter)
{
  if ( CaNotEqual ( filter , 0 ) ) filter -> filterRefCount++ ;
}

/* Create a render or playback pin using the supplied format */
static CaWdmPin * FilterCreatePin               (
                    CaWdmFilter        * filter ,
                    int                  pinId  ,
                    const WAVEFORMATEX * wfex   ,
                    CaError            * error  )
{
  CaError    result = NoError                          ;
  CaWdmPin * pin    = NULL                             ;
  pin = filter->pins[pinId]                            ;
  result = PinSetFormat ( pin , wfex )                 ;
  if ( CaIsCorrect ( result ) )                        {
    result = PinInstantiate ( pin )                    ;
  }                                                    ;
  *error = result                                      ;
  return CaAnswer ( CaIsCorrect ( result ) , pin , 0 ) ;
}

CaWdmFilter ** BuildFilterList            (
                 int     * pFilterCount   ,
                 int     * pNoOfPaDevices ,
                 CaError * pResult        )
{
  CaWdmFilter           ** ppFilters = NULL                                  ;
  HDEVINFO                 handle    = NULL                                  ;
  int                      device                                            ;
  int                      invalidDevices                                    ;
  int                      slot                                              ;
  SP_DEVICE_INTERFACE_DATA interfaceData                                     ;
  SP_DEVICE_INTERFACE_DATA aliasData                                         ;
  SP_DEVINFO_DATA          devInfoData                                       ;
  int                      noError                                           ;
  const int sizeInterface = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + (MAX_PATH * sizeof(WCHAR));
  unsigned char interfaceDetailsArray[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + (MAX_PATH * sizeof(WCHAR))];
  SP_DEVICE_INTERFACE_DETAIL_DATA_W* devInterfaceDetails = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)interfaceDetailsArray;
  const GUID * category          = (const GUID*)&KSCATEGORY_AUDIO            ;
  const GUID * alias_render      = (const GUID*)&KSCATEGORY_RENDER           ;
  const GUID * alias_capture     = (const GUID*)&KSCATEGORY_CAPTURE          ;
  const GUID * category_realtime = (const GUID*)&KSCATEGORY_REALTIME         ;
  DWORD        aliasFlags                                                    ;
  CaWdmKsType  streamingType                                                 ;
  int          filterCount       = 0                                         ;
  int          noOfPaDevices     = 0                                         ;
  ////////////////////////////////////////////////////////////////////////////
  devInterfaceDetails -> cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)  ;
  *pFilterCount                 = 0                                          ;
  *pNoOfPaDevices               = 0                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Open a handle to search for devices (filters)                          */
  handle = SetupDiGetClassDevs ( category                                    ,
                                 NULL                                        ,
                                 NULL                                        ,
                                 DIGCF_PRESENT | DIGCF_DEVICEINTERFACE     ) ;
  if ( handle == INVALID_HANDLE_VALUE )                                      {
    *pResult = UnanticipatedHostError                                        ;
    return NULL                                                              ;
  }                                                                          ;
  /* First let's count the number of devices so we can allocate a list      */
  invalidDevices = 0                                                         ;
    for( device = 0;;device++ ) {
        interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        interfaceData.Reserved = 0;
        aliasData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        aliasData.Reserved = 0;
        noError = SetupDiEnumDeviceInterfaces(handle,NULL,category,device,&interfaceData);
        if( !noError )
            break; /* No more devices */

        /* Check this one has the render or capture alias */
        aliasFlags = 0;
        noError = SetupDiGetDeviceInterfaceAlias(handle,&interfaceData,alias_render,&aliasData);
        if(noError)
        {
            if(aliasData.Flags && (!(aliasData.Flags & SPINT_REMOVED)))
            {
                gPrintf(("Device %d has render alias\n",device));
                aliasFlags |= Alias_kRender; /* Has render alias */
            }
            else
            {
                gPrintf(("Device %d has no render alias\n",device));
            }
        }
        noError = SetupDiGetDeviceInterfaceAlias(handle,&interfaceData,alias_capture,&aliasData);
        if(noError)
        {
            if(aliasData.Flags && (!(aliasData.Flags & SPINT_REMOVED)))
            {
                gPrintf(("Device %d has capture alias\n",device));
                aliasFlags |= Alias_kCapture; /* Has capture alias */
            }
            else
            {
                gPrintf(("Device %d has no capture alias\n",device));
            }
        }
        if(!aliasFlags)
            invalidDevices++; /* This was not a valid capture or render audio device */
    }
    /* Remember how many there are */
    filterCount = device-invalidDevices;
    gPrintf(("Interfaces found: %d\n",device-invalidDevices)) ;
    /* Now allocate the list of pointers to devices */
    ppFilters  = (CaWdmFilter **)Allocator::allocate(sizeof(CaWdmFilter*) * filterCount) ;
    if ( ppFilters == 0 ) {
        if (handle != NULL) SetupDiDestroyDeviceInfoList(handle);
        *pResult = InsufficientMemory ;
        return NULL;
    }
    /* Now create filter objects for each interface found */
    slot = 0;
    for( device = 0;;device++ )
    {
        interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        interfaceData.Reserved = 0;
        aliasData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        aliasData.Reserved = 0;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        devInfoData.Reserved = 0;
        streamingType = Type_kWaveCyclic;

        noError = SetupDiEnumDeviceInterfaces(handle,NULL,category,device,&interfaceData);
        if( !noError )
            break; /* No more devices */

        /* Check this one has the render or capture alias */
        aliasFlags = 0;
        noError = SetupDiGetDeviceInterfaceAlias(handle,&interfaceData,alias_render,&aliasData);
        if(noError)
        {
            if(aliasData.Flags && (!(aliasData.Flags & SPINT_REMOVED)))
            {
                gPrintf(("Device %d has render alias\n",device));
                aliasFlags |= Alias_kRender; /* Has render alias */
            }
        }
        noError = SetupDiGetDeviceInterfaceAlias(handle,&interfaceData,alias_capture,&aliasData);
        if(noError)
        {
            if(aliasData.Flags && (!(aliasData.Flags & SPINT_REMOVED)))
            {
                gPrintf(("Device %d has capture alias\n",device));
                aliasFlags |= Alias_kCapture; /* Has capture alias */
            }
        }
        if(!aliasFlags)
        {
            continue; /* This was not a valid capture or render audio device */
        }
        else
        {
            /* Check if filter is WaveRT, if not it is a WaveCyclic */
            noError = SetupDiGetDeviceInterfaceAlias(handle,&interfaceData,category_realtime,&aliasData);
            if (noError)
            {
                gPrintf(("Device %d has realtime alias\n",device));
                aliasFlags |= Alias_kRealtime;
                streamingType = Type_kWaveRT;
            }
        }

        noError = SetupDiGetDeviceInterfaceDetailW(handle,&interfaceData,devInterfaceDetails,sizeInterface,NULL,&devInfoData);
        if( noError )
        {
            DWORD type;
            WCHAR friendlyName[MAX_PATH] = {0};
            DWORD sizeFriendlyName;
            CaWdmFilter* newFilter = 0;

            CaError result = NoError;
            /* Try to get the "friendly name" for this interface */
            sizeFriendlyName = sizeof(friendlyName);

            if (IsEarlierThanVista() && IsUSBDevice(devInterfaceDetails->DevicePath))
            {
                /* XP and USB audio device needs to look elsewhere, otherwise it'll only be a "USB Audio Device". Not
                very literate. */
                if (!SetupDiGetDeviceRegistryPropertyW(handle,
                    &devInfoData,
                    SPDRP_LOCATION_INFORMATION,
                    &type,
                    (BYTE*)friendlyName,
                    sizeof(friendlyName),
                    NULL))
                {
                    friendlyName[0] = 0;
                }
            }

            if (friendlyName[0] == 0 || IsNameUSBAudioDevice(friendlyName))
            {
                /* Fix contributed by Ben Allison
                * Removed KEY_SET_VALUE from flags on following call
                * as its causes failure when running without admin rights
                * and it was not required */
                HKEY hkey=SetupDiOpenDeviceInterfaceRegKey(handle,&interfaceData,0,KEY_QUERY_VALUE);
                if(hkey!=INVALID_HANDLE_VALUE)
                {
                    noError = RegQueryValueExW(hkey,L"FriendlyName",0,&type,(BYTE*)friendlyName,&sizeFriendlyName);
                    if( noError == ERROR_SUCCESS )
                    {
                        gPrintf(("Interface %d, Name: %s\n",device,friendlyName));
                        RegCloseKey(hkey);
                    }
                    else
                    {
                        friendlyName[0] = 0;
                    }
                }
            }

            TrimString(friendlyName, sizeFriendlyName);

            newFilter = FilterNew(streamingType,
                devInfoData.DevInst,
                devInterfaceDetails->DevicePath,
                friendlyName,
                &result);

            if( result == NoError )
            {
                int pin;
                unsigned filterIOs = 0;

                /* Increment number of "devices" */
                for (pin = 0; pin < newFilter->pinCount; ++pin)
                {
                    CaWdmPin* pPin = newFilter->pins[pin];
                    if (pPin == NULL)
                        continue;

                    filterIOs += max(1, pPin->inputCount);
                }
                noOfPaDevices += filterIOs;
                gPrintf(("Filter (%s) created with %d valid pins (total I/Os: %u)\n", ((newFilter->devInfo.streamingType==Type_kWaveRT)?"WaveRT":"WaveCyclic"), newFilter->validPinCount, filterIOs));
                ppFilters[slot] = newFilter;
                slot++;
            } else {
                gPrintf(("Filter NOT created\n"));
                /* As there are now less filters than we initially thought
                * we must reduce the count by one */
                filterCount--;
            }
        }
    }
    /* Clean up */
    if (handle != NULL) SetupDiDestroyDeviceInfoList(handle);
    *pFilterCount = filterCount;
    *pNoOfPaDevices = noOfPaDevices;
    return ppFilters;
}

typedef struct CaNameHashIndex   {
  unsigned                 index ;
  unsigned                 count ;
  ULONG                    hash  ;
  struct CaNameHashIndex * next  ;
}              CaNameHashIndex   ;

typedef struct CaNameHashObject  {
  CaNameHashIndex * list         ;
  AllocationGroup * allocGroup   ;
}              CaNameHashObject  ;

static ULONG GetNameHash(const wchar_t* str, const BOOL input)
{
    /* This is to make sure that a name that exists as both input & output won't get the same hash value */
    const ULONG fnv_prime = (input ? 0x811C9DD7 : 0x811FEB0B);
    ULONG hash = 0;
    for(; *str != 0; str++)
    {
        hash *= fnv_prime;
        hash ^= (*str);
    }
//    assert(hash != 0);
    return hash;
}

static CaError CreateHashEntry(CaNameHashObject* obj, const wchar_t* name, const BOOL input)
{
    ULONG hash = GetNameHash(name, input);
    CaNameHashIndex * pLast = NULL;
    CaNameHashIndex * p = obj->list;
    while (p != 0)
    {
        if (p->hash == hash)
        {
            break;
        }
        pLast = p;
        p = p->next;
    }
    if (p == NULL)
    {
        p = (CaNameHashIndex *)obj->allocGroup->alloc(sizeof(CaNameHashIndex)) ;
        if (p == NULL)
        {
            return InsufficientMemory;
        }
        p->hash = hash;
        p->count = 1;
        if (pLast != 0)
        {
            pLast->next = p;
        }
        if (obj->list == 0)
        {
            obj->list = p;
        }
    } else {
        ++p->count;
    }
    return NoError;
}

static CaError InitNameHashObject(CaNameHashObject* obj, CaWdmFilter* pFilter)
{
    int i;

    obj->allocGroup = new AllocationGroup ( ) ;
    if (obj->allocGroup == NULL)
    {
        return InsufficientMemory;
    }

    for (i = 0; i < pFilter->pinCount; ++i)
    {
        unsigned m;
        CaWdmPin* pin = pFilter->pins[i];

        if (pin == NULL) continue ;

        for (m = 0; m < max(1, pin->inputCount); ++m)
        {
            const BOOL isInput = (pin->dataFlow == KSPIN_DATAFLOW_OUT);
            const wchar_t* name = (pin->inputs == NULL) ? pin->friendlyName : pin->inputs[m]->friendlyName ;

            CaError result = CreateHashEntry(obj, name, isInput);

            if (result != NoError)
            {
                return result;
            }
        }
    }
    return NoError;
}

static void DeinitNameHashObject(CaNameHashObject* obj)
{
  if ( NULL == obj ) return ;
  obj->allocGroup->release();
  delete obj->allocGroup ;
  obj->allocGroup = NULL ;
  memset(obj, 0, sizeof(CaNameHashObject));
}

static unsigned GetNameIndex(CaNameHashObject* obj, const wchar_t* name, const BOOL input)
{
    ULONG hash = GetNameHash(name, input);
    CaNameHashIndex * p = obj->list;
    while ( p != NULL ) {
        if (p->hash == hash)
        {
            if (p->count > 1)
            {
                return (++p->index);
            }
            else
            {
                return 0;
            }
        }

        p = p->next;
    }
    // Should never get here!!
    return 0;
}

static CaError PinWrite(HANDLE h, DATAPACKET* p)
{
    CaError result = NoError;
    unsigned long cbReturned = 0;
    BOOL fRes = DeviceIoControl(h,
        IOCTL_KS_WRITE_STREAM,
        NULL,
        0,
        &p->Header,
        p->Header.Size,
        &cbReturned,
        &p->Signal);
    if (!fRes)
    {
        unsigned long error = GetLastError();
        if (error != ERROR_IO_PENDING)
        {
            result = InternalError;
        }
    }
    return result;
}

/*
Read to the supplied packet from the pin
Asynchronous
Should return NoError on success
*/
static CaError PinRead(HANDLE h, DATAPACKET* p)
{
    CaError result = NoError;
    unsigned long cbReturned = 0;
    BOOL fRes = DeviceIoControl(h,
        IOCTL_KS_READ_STREAM,
        NULL,
        0,
        &p->Header,
        p->Header.Size,
        &cbReturned,
        &p->Signal);
    if (!fRes)
    {
        unsigned long error = GetLastError();
        if (error != ERROR_IO_PENDING)
        {
            result = InternalError ;
        }
    }
    return result;
}

/* Increase the priority of the calling thread to RT */
static HANDLE BumpThreadPriority(void)
{
    HANDLE hThread = GetCurrentThread();
    DWORD dwTask = 0;
    HANDLE hAVRT = NULL;
    /* If we have access to AVRT.DLL (Vista and later), use it */
    if (caWdmKsAvRtEntryPoints.AvSetMmThreadCharacteristics != NULL)
    {
        hAVRT = caWdmKsAvRtEntryPoints.AvSetMmThreadCharacteristics("Pro Audio", &dwTask);
        if (hAVRT != NULL && hAVRT != INVALID_HANDLE_VALUE)
        {
            BOOL bret = caWdmKsAvRtEntryPoints.AvSetMmThreadPriority(hAVRT, PA_AVRT_PRIORITY_CRITICAL);
            if (!bret)
            {
                gPrintf(("Set mm thread prio to critical failed!\n"));
            }
            else
            {
                return hAVRT;
            }
        }
        else
        {
            gPrintf(("Set mm thread characteristic to 'Pro Audio' failed, reverting to SetThreadPriority\n")) ;
        }
    }

    /* For XP and earlier, or if AvSetMmThreadCharacteristics fails (MMCSS disabled ?) */
    if (timeBeginPeriod(1) != TIMERR_NOERROR) {
      gPrintf(("timeBeginPeriod(1) failed!\n"));
    }

    if (!SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL)) {
      gPrintf(("SetThreadPriority failed!\n"));
    }
    return hAVRT;
}

/* Decrease the priority of the calling thread to normal */

static void DropThreadPriority(HANDLE hAVRT)
{
    HANDLE hThread = GetCurrentThread();

    if (hAVRT != NULL)
    {
        caWdmKsAvRtEntryPoints.AvSetMmThreadPriority(hAVRT, PA_AVRT_PRIORITY_NORMAL);
        caWdmKsAvRtEntryPoints.AvRevertMmThreadCharacteristics(hAVRT);
        return;
    }

    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);
    timeEndPeriod(1);
}

static CaError PreparePinForStart(CaWdmPin* pin)
{
    CaError result;
    result = PinSetState(pin, KSSTATE_ACQUIRE);
    if (result != NoError)
    {
        goto error;
    }
    result = PinSetState(pin, KSSTATE_PAUSE);
    if (result != NoError)
    {
        goto error;
    }
    return result;

error:
    PinSetState(pin, KSSTATE_STOP);
    return result;
}

static CaError PreparePinsForStart(CaProcessThreadInfo * pInfo)
{
    CaError result = NoError;
    /* Submit buffers */
    if (pInfo->stream->capture.pPin)
    {
        if ((result = PreparePinForStart(pInfo->stream->capture.pPin)) != NoError)
        {
            goto error;
        }

        if (pInfo->stream->capture.pPin->parentFilter->devInfo.streamingType == Type_kWaveCyclic)
        {
            unsigned i;
            for(i=0; i < pInfo->stream->capture.noOfPackets; ++i)
            {
                if ((result = PinRead(pInfo->stream->capture.pPin->handle, pInfo->stream->capture.packets + i)) != NoError)
                {
                    goto error;
                }
                ++pInfo->pending;
            }
        }
        else
        {
            pInfo->pending = 2;
        }
    }

    if(pInfo->stream->render.pPin)
    {
        if ((result = PreparePinForStart(pInfo->stream->render.pPin)) != NoError)
        {
            goto error;
        }

        pInfo->priming += pInfo->stream->render.noOfPackets;
        ++pInfo->pending;
        SetEvent(pInfo->stream->render.events[0]);
        if (pInfo->stream->render.pPin->parentFilter->devInfo.streamingType == Type_kWaveCyclic)
        {
            unsigned i;
            for(i=1; i < pInfo->stream->render.noOfPackets; ++i)
            {
                SetEvent(pInfo->stream->render.events[i]);
                ++pInfo->pending;
            }
        }
    }
error:
    gPrintf(("PreparePinsForStart = %d\n", result)) ;
    return result;
}

static CaError StartPin(CaWdmPin * pin)
{
  return PinSetState ( pin , KSSTATE_RUN ) ;
}

static CaError StartPins(CaProcessThreadInfo* pInfo)
{
  CaError result = NoError                         ;
  /* Start the pins as synced as possible         */
  if (pInfo->stream->capture.pPin)                 {
    result = StartPin(pInfo->stream->capture.pPin) ;
  }                                                ;
  if (pInfo->stream->render.pPin)                  {
    result = StartPin(pInfo->stream->render.pPin)  ;
  }                                                ;
  gPrintf(("StartPins = %d\n", result))            ;
  return result                                    ;
}

static CaError StopPin(CaWdmPin * pin)
{
  PinSetState ( pin , KSSTATE_PAUSE ) ;
  PinSetState ( pin , KSSTATE_STOP  ) ;
  return NoError                      ;
}

static CaError StopPins(CaProcessThreadInfo* pInfo)
{
  CaError result = NoError               ;
  if ( pInfo->stream->render .pPin )     {
    StopPin(pInfo->stream->render .pPin) ;
  }                                      ;
  if ( pInfo->stream->capture.pPin )     {
    StopPin(pInfo->stream->capture.pPin) ;
  }                                      ;
  return result                          ;
}

static CaError CaDoProcessing(CaProcessThreadInfo * pInfo)
{
  CaError result = NoError                                                   ;
  int i, framesProcessed = 0, doChannelCopy = 0                              ;
  CaInt32 inputFramesAvailable = pInfo->stream->ringBuffer.ReadAvailable()   ;
  /* Do necessary buffer processing (which will invoke user callback if necessary) */
  if ( ( ( pInfo -> cbResult   == Conduit::Continue                       ) ||
         ( pInfo -> cbResult   == Conduit::Postpone                     ) ) &&
         ( pInfo -> renderHead != pInfo->renderTail                         ||
           inputFramesAvailable                                          ) ) {
    unsigned processFullDuplex = pInfo->stream->capture.pPin && pInfo->stream->render.pPin && (!pInfo->priming) ;
    pInfo->stream->cpuLoadMeasurer.Begin()                                   ;
    pInfo->CurrentTime = pInfo->stream->GetTime()                            ;
    pInfo->stream->bufferProcessor.Begin(pInfo->stream->conduit)             ;
    pInfo->underover = 0; /* Reset the (under|over)flow status              */
    if ( pInfo->renderTail != pInfo->renderHead )                            {
      DATAPACKET * packet = pInfo->renderPackets[pInfo->renderTail & cPacketsArrayMask].packet;
      pInfo->stream->bufferProcessor.setOutputFrameCount(0,pInfo->stream->render.framesPerBuffer);
      for (i=0;i<pInfo->stream->userOutputChannels;i++)                      {
        /* Only write the user output channels. Leave the rest blank        */
        pInfo->stream->bufferProcessor.setOutputChannel                      (
          0                                                                  ,
          i                                                                  ,
         ((unsigned char*)(packet->Header.Data))+(i*pInfo->stream->render.bytesPerSample),
           pInfo->stream->deviceOutputChannels                             ) ;
      }                                                                      ;
      /* We will do a copy to the other channels after the data has been written */
      doChannelCopy = ( pInfo->stream->userOutputChannels == 1 )             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( inputFramesAvailable                                               &&
         ( ! pInfo->stream->userOutputChannels                              ||
           inputFramesAvailable >= (int)pInfo->stream->render.framesPerBuffer) ) {
      unsigned wrapCntr   = 0                                                ;
      void   * data [ 2 ] = { NULL , NULL }                                  ;
      CaInt32  size [ 2 ] = {    0 ,    0 }                                  ;
      /* If full-duplex, we just extract output buffer number of frames     */
      if ( pInfo->stream->userOutputChannels )                               {
        inputFramesAvailable = min(inputFramesAvailable, (int)pInfo->stream->render.framesPerBuffer);
      }                                                                      ;
      inputFramesAvailable = pInfo->stream->ringBuffer.ReadRegions           (
        inputFramesAvailable                                                 ,
        &data[0]                                                             ,
        &size[0]                                                             ,
        &data[1]                                                             ,
        &size[1]                                                           ) ;
      for ( wrapCntr = 0 ; wrapCntr < 2 ; ++wrapCntr )                       {
        if ( size [ wrapCntr ] == 0) break                                   ;
        pInfo -> stream -> bufferProcessor . setInputFrameCount              (
          wrapCntr                                                           ,
          size[wrapCntr]                                                   ) ;
        for ( i = 0 ; i < pInfo->stream->userInputChannels ; i++ )           {
          pInfo -> stream -> bufferProcessor .setInputChannel                (
            wrapCntr                                                         ,
            i                                                                ,
            ((unsigned char*)(data[wrapCntr]))+(i*pInfo->stream->capture.bytesPerSample),
            pInfo->stream->deviceInputChannels                             ) ;
        }                                                                    ;
      }                                                                      ;
    } else                                                                   {
      /* We haven't consumed anything from the ring buffer...               */
      inputFramesAvailable = 0                                               ;
      /* If we have full-duplex, this is at startup, so mark no-input!      */
      if ( ( pInfo -> stream -> userOutputChannels > 0 )                    &&
           ( pInfo -> stream -> userInputChannels  > 0 )                   ) {
        pInfo -> stream -> bufferProcessor . setNoInput ( )                  ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( processFullDuplex )                                                 {
      /* full duplex                                                        */
      /* Only call the EndBufferProcessing function when the total input
       * frames == total output frames                                      */
      const unsigned long totalInputFrameCount = pInfo->stream->bufferProcessor.hostInputFrameCount[0] + pInfo->stream->bufferProcessor.hostInputFrameCount[1];
      const unsigned long totalOutputFrameCount = pInfo->stream->bufferProcessor.hostOutputFrameCount[0] + pInfo->stream->bufferProcessor.hostOutputFrameCount[1];
      if (totalInputFrameCount == totalOutputFrameCount && totalOutputFrameCount != 0 ) {
        framesProcessed = pInfo->stream->bufferProcessor.End(&pInfo->cbResult) ;
      } else                                                                 {
        framesProcessed = 0                                                  ;
      }                                                                      ;
    } else                                                                   {
      framesProcessed = pInfo->stream->bufferProcessor.End(&pInfo->cbResult) ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( doChannelCopy )                                                     {
      DATAPACKET * packet                                                    ;
      packet = pInfo->renderPackets[pInfo->renderTail & cPacketsArrayMask].packet ;
      /* Copy the first output channel to the other channels                */
      switch ( pInfo->stream->render.bytesPerSample )                        {
        case 2                                                               :
          DuplicateFirstChannelInt16                                         (
            packet -> Header.Data                                            ,
            pInfo  -> stream->deviceOutputChannels                           ,
            pInfo  -> stream->render.framesPerBuffer                       ) ;
        break                                                                ;
        case 3                                                               :
          DuplicateFirstChannelInt24                                         (
            packet -> Header.Data                                            ,
            pInfo  -> stream->deviceOutputChannels                           ,
            pInfo  -> stream->render.framesPerBuffer                       ) ;
        break                                                                ;
        case 4                                                               :
          DuplicateFirstChannelInt32                                         (
            packet -> Header.Data                                            ,
            pInfo  -> stream->deviceOutputChannels                           ,
            pInfo  -> stream->render.framesPerBuffer                       ) ;
        break                                                                ;
        default                                                              :
        break                                                                ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    pInfo->stream->cpuLoadMeasurer.End(framesProcessed)                      ;
    //////////////////////////////////////////////////////////////////////////
    if (inputFramesAvailable)                                                {
      pInfo->stream->ringBuffer.AdvanceReadIndex(inputFramesAvailable)       ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if (pInfo->renderTail != pInfo->renderHead)                              {
      if (!pInfo->stream->streamStop)                                        {
        result = pInfo->stream->render.pPin->fnSubmitHandler(pInfo, pInfo->renderTail);
        if ( CaIsWrong ( result ) ) return result                            ;
      }                                                                      ;
      pInfo->renderTail++                                                    ;
      if ( ( ! pInfo->pinsStarted ) && ( pInfo->priming == 0) )              {
        /* We start the pins here to allow "prime time"                     */
        if ( ( result = StartPins(pInfo) ) == NoError )                      {
          pInfo->pinsStarted = 1                                             ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

static VOID CALLBACK TimerAPCWaveRTPolledMode(
    LPVOID lpArgToCompletionRoutine,
    DWORD dwTimerLowValue,
    DWORD dwTimerHighValue)
{
    HANDLE* pHandles = (HANDLE*)lpArgToCompletionRoutine;
    if (pHandles[0]) SetEvent(pHandles[0]);
    if (pHandles[1]) SetEvent(pHandles[1]);
}

static DWORD GetCurrentTimeInMillisecs(void)
{
  return timeGetTime ( ) ;
}

CA_THREAD_FUNC ProcessingThread(void * pParam)
{
  CaError result = NoError;
  HANDLE hAVRT = NULL;
  HANDLE hTimer = NULL;
  HANDLE *handleArray = NULL;
  HANDLE timerEventHandles[2] = {0};
  unsigned noOfHandles = 0;
  unsigned captureEvents = 0;
  unsigned renderEvents = 0;
  unsigned timerPeriod = 0;
  DWORD timeStamp[2] = {0};
  CaProcessThreadInfo info;
  memset(&info, 0, sizeof(CaProcessThreadInfo));
  info.stream = (WdmKsStream *)pParam ;
  info.stream->threadResult = NoError;
  info.InputBufferAdcTime = 0.0;
  info.CurrentTime = 0.0;
  info.OutputBufferDacTime = 0.0;
  gPrintf(("In  buffer len: %.3f ms\n",(2000*info.stream->capture.framesPerBuffer) / info.stream->sampleRate )) ;
  gPrintf(("Out buffer len: %.3f ms\n",(2000*info.stream->render.framesPerBuffer) / info.stream ->sampleRate )) ;
  info.timeout = (DWORD)max(
    (2000*info.stream->render .framesPerBuffer/info.stream->sampleRate + 0.5)   ,
    (2000*info.stream->capture.framesPerBuffer/info.stream->sampleRate + 0.5) ) ;
  info.timeout = max(info.timeout*8, 100);
  timerPeriod = info.timeout;
  gPrintf(("Timeout = %ld ms\n",info.timeout)) ;
  /* Allocate handle array */
  handleArray = (HANDLE *)Allocator::allocate((info.stream->capture.noOfPackets + info.stream->render.noOfPackets + 1) * sizeof(HANDLE)) ;
  /* Setup handle array for WFMO */
    if (info.stream->capture.pPin != 0)
    {
        handleArray[noOfHandles++] = info.stream->capture.events[0];
        if (info.stream->capture.pPin->parentFilter->devInfo.streamingType == Type_kWaveCyclic)
        {
            unsigned i;
            for(i=1; i < info.stream->capture.noOfPackets; ++i)
            {
                handleArray[noOfHandles++] = info.stream->capture.events[i];
            }
        }
        captureEvents = noOfHandles;
        renderEvents = noOfHandles;
    }
    ///////////////////////////////////////////////////////////////////////////
    if (info.stream->render.pPin != 0)
    {
        handleArray[noOfHandles++] = info.stream->render.events[0];
        if (info.stream->render.pPin->parentFilter->devInfo.streamingType == Type_kWaveCyclic)
        {
            unsigned i;
            for(i=1; i < info.stream->render.noOfPackets; ++i)
            {
                handleArray[noOfHandles++] = info.stream->render.events[i];
            }
        }
        renderEvents = noOfHandles;
    }
    handleArray[noOfHandles++] = info.stream->eventAbort;
    /* Prepare render and capture pins */
    if ((result = PreparePinsForStart(&info)) != NoError)
    {
        gPrintf(("Failed to prepare device(s)!\n")) ;
        goto error;
    }
    /* Heighten priority here */
    hAVRT = BumpThreadPriority();

    /* If input only, we start the pins immediately */
    if (info.stream->render.pPin == 0)
    {
        if ((result = StartPins(&info)) != NoError)
        {
            gPrintf(("Failed to start device(s)!\n"));
            goto error;
        }
        info.pinsStarted = 1;
    }
    /* Handle WaveRT polled mode */
    {
        const unsigned fs = (unsigned)info.stream->sampleRate;
        if (info.stream->capture.pPin != 0 && info.stream->capture.pPin->pinKsSubType == SubType_kPolled)
        {
            timerEventHandles[0] = info.stream->capture.events[0];
            timerPeriod = min(timerPeriod, (1000*info.stream->capture.framesPerBuffer)/fs);
        }

        if (info.stream->render.pPin != 0 && info.stream->render.pPin->pinKsSubType == SubType_kPolled)
        {
            timerEventHandles[1] = info.stream->render.events[0];
            timerPeriod = min(timerPeriod, (1000*info.stream->render.framesPerBuffer)/fs);
        }

        if (timerEventHandles[0] || timerEventHandles[1])
        {
            LARGE_INTEGER dueTime = {0};

            timerPeriod=max(timerPeriod/5,1);
            gPrintf(("Timer event handles=0x%04X,0x%04X period=%u ms", timerEventHandles[0], timerEventHandles[1], timerPeriod)) ;
            hTimer = CreateWaitableTimer(0, FALSE, NULL);
            if (hTimer == NULL)
            {
                result = UnanticipatedHostError;
                goto error;
            }
            /* invoke first timeout immediately */
            if (!SetWaitableTimer(hTimer, &dueTime, timerPeriod, TimerAPCWaveRTPolledMode, timerEventHandles, FALSE))
            {
                result = UnanticipatedHostError;
                goto error;
            }
            gPrintf(("Waitable timer started, period = %u ms\n", timerPeriod));
        }
    }
    /* Mark stream as active */
    info.stream->streamActive = 1;
    info.stream->threadResult = NoError;

    /* Up and running... */
    SetEvent(info.stream->eventStreamStart[StreamStart_kOk]);

    /* Take timestamp here */
    timeStamp[0] = timeStamp[1] = GetCurrentTimeInMillisecs();

    while ( ! info . stream -> streamAbort )                                 {
        unsigned doProcessing = 1;
        unsigned wait = WaitForMultipleObjects(noOfHandles, handleArray, FALSE, 0);
        unsigned eventSignalled = wait - WAIT_OBJECT_0;
        DWORD dwCurrentTime = 0;

        if (wait == WAIT_FAILED) {
          gPrintf(("Wait failed = %ld! \n",wait)) ;
          break;
        }
        if ( wait == WAIT_TIMEOUT ) {
          wait = WaitForMultipleObjectsEx(noOfHandles, handleArray, FALSE, 50, TRUE);
          eventSignalled = wait - WAIT_OBJECT_0 ;
        } else {
        if ( eventSignalled < captureEvents ) {
          if ( info.stream->ringBuffer.WriteAvailable() == 0 ) {
            info.underover |= StreamIO::InputOverflow ;
          }
        } else
        if (eventSignalled < renderEvents) {
         if (!info.priming && info.renderHead - info.renderTail > 1) {
           info.underover |= StreamIO::OutputUnderflow ;
         }
       }
     }
        /* Get event time */
        dwCurrentTime = GetCurrentTimeInMillisecs();
        /* Since we can mix capture/render devices between WaveCyclic, WaveRT polled and WaveRT notification (3x3 combinations),
        we can't rely on the timeout of WFMO to check for device timeouts, we need to keep tally. */
        if (info.stream->capture.pPin && (dwCurrentTime - timeStamp[0]) >= info.timeout)
        {
            gPrintf(("Timeout for capture device (%u ms)!", info.timeout, (dwCurrentTime - timeStamp[0])));
            result = TimedOut;
            break;
        }
        if (info.stream->render.pPin && (dwCurrentTime - timeStamp[1]) >= info.timeout)
        {
            gPrintf(("Timeout for render device (%u ms)!", info.timeout, (dwCurrentTime - timeStamp[1])));
            result = TimedOut;
            break;
        }

        if (wait == WAIT_IO_COMPLETION)
        {
            /* Waitable timer has fired! */
            continue;
        }

        if ( wait == WAIT_TIMEOUT ) {
            continue;
        } else {
            if (eventSignalled < captureEvents)
            {
                if (info.stream->capture.pPin->fnEventHandler(&info, eventSignalled) == NoError)
                {
                    timeStamp[0] = dwCurrentTime;

                    /* Since we use the ring buffer, we can submit the buffers directly */
                    if (!info.stream->streamStop)
                    {
                        result = info.stream->capture.pPin->fnSubmitHandler(&info, info.captureTail);
                        if (result != NoError)
                        {
                            break;
                        }
                    }
                    ++info.captureTail;
                    /* If full-duplex, let _only_ render event trigger processing. We still need the stream stop
                    handling working, so let that be processed anyways... */
                    if (info.stream->userOutputChannels > 0)
                    {
                        doProcessing = 0;
                    }
                }
            }
            else if (eventSignalled < renderEvents)
            {
                timeStamp[1] = dwCurrentTime;
                eventSignalled -= captureEvents;
                info.stream->render.pPin->fnEventHandler(&info, eventSignalled);
            }
            else
            {
                continue;
            }
        }
    /* Handle processing                                                    */
    if ( doProcessing )                                                      {
      result = CaDoProcessing ( &info )                                      ;
      if ( CaIsWrong ( result ) ) break                                      ;
    }                                                                        ;
    if ( ( info.stream->streamStop            )                             &&
         ( info.cbResult != Conduit::Postpone )                             &&
         ( info.cbResult != Conduit::Complete ) )                            {
      info.cbResult = Conduit::Complete                                      ;
      /* Stop, but play remaining buffers                                   */
    }                                                                        ;
    if ( info.pending <= 0 ) break                                           ;
    if ( ( ! info . stream   -> render.pPin       )                         &&
         (   info . cbResult != Conduit::Continue )                         &&
         (   info . cbResult != Conduit::Postpone )                        ) {
      break                                                                  ;
    }                                                                        ;
  }                                                                          ;
  gPrintf ( ( "Finished processing loop\n" ) )                               ;
  info . stream -> threadResult = result                                     ;
  goto bailout                                                               ;
error                                                                        :
  gPrintf(("Error starting processing thread\n"));
  /* Set the "error" event together with result */
  info.stream->threadResult = result;
  SetEvent(info.stream->eventStreamStart[StreamStart_kFailed]);
bailout                                                                      :
  if ( hTimer )                                                              {
    gPrintf ( ( "Waitable timer stopped\n" , timerPeriod ) )                 ;
    CancelWaitableTimer ( hTimer )                                           ;
    CloseHandle         ( hTimer )                                           ;
    hTimer = 0                                                               ;
  }                                                                          ;
  if (info.pinsStarted)                                                      {
    StopPins(&info)                                                          ;
  }                                                                          ;
  /* Lower prio here                                                        */
  DropThreadPriority ( hAVRT )                                               ;
  if ( CaNotNull ( handleArray ) ) Allocator :: free ( handleArray )         ;
  info . stream -> streamActive = 0                                          ;
  if ( ( ! info.stream->streamStop ) && ( ! info.stream->streamAbort ) )     {
    /* Invoke the user stream finished callback                             */
    /* Only do it from here if not being stopped/aborted by user            */
    if ( info.stream->conduit != NULL )                                      {
      if (info.stream->userInputChannels>0)                                  {
        info . stream -> conduit -> finish ( Conduit::InputDirection         ,
                                             Conduit::Correct              ) ;
      }                                                                      ;
      if (info.stream->userOutputChannels>0)                                 {
        info . stream -> conduit -> finish ( Conduit::OutputDirection        ,
                                             Conduit::Correct              ) ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  info . stream -> streamStop  = 0                                           ;
  info . stream -> streamAbort = 0                                           ;
  return 0                                                                   ;
}

/***************************************************************************************/
/* Event and submit handlers for WaveCyclic                                            */
/***************************************************************************************/

static CaError CaPinCaptureEventHandler_WaveCyclic(CaProcessThreadInfo * pInfo, unsigned eventIndex)
{
  CaError      result = NoError                                              ;
  CaInt32      frameCount                                                    ;
  DATAPACKET * packet = pInfo->stream->capture.packets + eventIndex          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( packet->Header.DataUsed == 0 )                                        {
    ResetEvent ( packet->Signal.hEvent )                                     ;
    result = -1                                                              ;
    /* Only need this to be NOT NoError                                     */
  } else                                                                     {
    pInfo->capturePackets[pInfo->captureHead & cPacketsArrayMask].packet = packet;
    frameCount = pInfo->stream->ringBuffer.Write(packet->Header.Data, pInfo->stream->capture.framesPerBuffer) ;
    ++pInfo->captureHead                                                     ;
  }                                                                          ;
  --pInfo->pending                                                           ;
  /* This needs to be done in either case                                   */
  return result                                                              ;
}

static CaError CaPinCaptureSubmitHandler_WaveCyclic(CaProcessThreadInfo* pInfo, unsigned eventIndex)
{
    CaError result = NoError;
    DATAPACKET* packet = pInfo->capturePackets[pInfo->captureTail & cPacketsArrayMask].packet;
    pInfo->capturePackets[pInfo->captureTail & cPacketsArrayMask].packet = 0;
    packet->Header.DataUsed = 0; /* Reset for reuse */
    ResetEvent(packet->Signal.hEvent);
    result = PinRead(pInfo->stream->capture.pPin->handle, packet);
    ++pInfo->pending;
    return result;
}

static CaError CaPinRenderEventHandler_WaveCyclic(CaProcessThreadInfo* pInfo, unsigned eventIndex)
{
  pInfo->renderPackets[pInfo->renderHead & cPacketsArrayMask].packet = pInfo->stream->render.packets + eventIndex;
  ++pInfo->renderHead;
  --pInfo->pending;
  return NoError;
}

static CaError CaPinRenderSubmitHandler_WaveCyclic(CaProcessThreadInfo* pInfo, unsigned eventIndex)
{
  CaError result = NoError;
  DATAPACKET* packet = pInfo->renderPackets[pInfo->renderTail & cPacketsArrayMask].packet;
  pInfo->renderPackets[pInfo->renderTail & cPacketsArrayMask].packet = 0;
  ResetEvent(packet->Signal.hEvent);
  result = PinWrite(pInfo->stream->render.pPin->handle, packet);
  /* Reset event, just in case we have an analogous situation to capture (see CaPinCaptureSubmitHandler_WaveCyclic) */
  ++pInfo->pending;
  if (pInfo->priming) --pInfo->priming;
  return result;
}

/***************************************************************************************/
/* Event and submit handlers for WaveRT                                                */
/***************************************************************************************/

static CaError CaPinCaptureEventHandler_WaveRTEvent(CaProcessThreadInfo* pInfo, unsigned eventIndex)
{
    unsigned long pos;
    unsigned realInBuf;
    unsigned frameCount;
    CaWdmIOInfo* pCapture = &pInfo->stream->capture;
    const unsigned halfInputBuffer = pCapture->hostBufferSize >> 1;
    CaWdmPin* pin = pCapture->pPin;
    DATAPACKET* packet = 0;

    /* Get hold of current ADC position */
    pin->fnAudioPosition(pin, &pos);
    /* Wrap it (robi: why not use hw latency compensation here ?? because pos then gets _way_ off from
    where it should be, i.e. at beginning or half buffer position. Why? No idea.)  */

    pos %= pCapture->hostBufferSize;
    /* Then realInBuf will point to "other" half of double buffer */
    realInBuf = pos < halfInputBuffer ? 1U : 0U;

    packet = pInfo->stream->capture.packets + realInBuf;

    /* Call barrier (or dummy) */
    pin->fnMemBarrier();

    /* Put it in queue */
    frameCount = pInfo->stream->ringBuffer.Write(packet->Header.Data, pCapture->framesPerBuffer) ;

    pInfo->capturePackets[pInfo->captureHead & cPacketsArrayMask].packet = packet;

    ++pInfo->captureHead;
    --pInfo->pending;
    return NoError;
}

static CaError CaPinCaptureEventHandler_WaveRTPolled(CaProcessThreadInfo* pInfo, unsigned eventIndex)
{
    unsigned long pos;
    unsigned bytesToRead;
    CaWdmIOInfo* pCapture = &pInfo->stream->capture;
    const unsigned halfInputBuffer = pCapture->hostBufferSize>>1;
    CaWdmPin* pin = pInfo->stream->capture.pPin;

    /* Get hold of current ADC position */
    pin->fnAudioPosition(pin, &pos);
    /* Wrap it (robi: why not use hw latency compensation here ?? because pos then gets _way_ off from
    where it should be, i.e. at beginning or half buffer position. Why? No idea.)  */
    /* Compensate for HW FIFO to get to last read buffer position */
    pos += pin->hwLatency;
    pos %= pCapture->hostBufferSize;
    /* Need to align position on frame boundary */
    pos &= ~(pCapture->bytesPerFrame - 1);

    /* Call barrier (or dummy) */
    pin->fnMemBarrier();

    /* Put it in "queue" */
    bytesToRead = (pCapture->hostBufferSize + pos - pCapture->lastPosition) % pCapture->hostBufferSize;
    if ( bytesToRead > 0) {
        unsigned frameCount = pInfo->stream->ringBuffer.Write(
            pCapture->hostBuffer + pCapture->lastPosition,
            bytesToRead / pCapture->bytesPerFrame);

        pCapture->lastPosition = (pCapture->lastPosition + frameCount * pCapture->bytesPerFrame) % pCapture->hostBufferSize ;

        ++pInfo->captureHead;
        --pInfo->pending;
    }
    return NoError;
}

static CaError CaPinCaptureSubmitHandler_WaveRTEvent(CaProcessThreadInfo * pInfo,unsigned eventIndex)
{
  pInfo->capturePackets[pInfo->captureTail & cPacketsArrayMask].packet = 0   ;
  ++pInfo->pending                                                           ;
  return NoError                                                             ;
}

static CaError CaPinCaptureSubmitHandler_WaveRTPolled(CaProcessThreadInfo * pInfo,unsigned eventIndex)
{
  pInfo->capturePackets[pInfo->captureTail & cPacketsArrayMask].packet = 0   ;
  ++pInfo->pending                                                           ;
  return NoError                                                             ;
}

static CaError CaPinRenderEventHandler_WaveRTEvent(CaProcessThreadInfo* pInfo, unsigned eventIndex)
{
    unsigned long pos;
    unsigned realOutBuf;
    CaWdmIOInfo* pRender = &pInfo->stream->render;
    const unsigned halfOutputBuffer = pRender->hostBufferSize >> 1;
    CaWdmPin* pin = pInfo->stream->render.pPin;
    CaIOPacket* ioPacket = &pInfo->renderPackets[pInfo->renderHead & cPacketsArrayMask];

    /* Get hold of current DAC position */
    pin->fnAudioPosition(pin, &pos);
    /* Compensate for HW FIFO to get to last read buffer position */
    pos += pin->hwLatency;
    /* Wrap it */
    pos %= pRender->hostBufferSize;
    /* And align it, not sure its really needed though */
    pos &= ~(pRender->bytesPerFrame - 1);
    /* Then realOutBuf will point to "other" half of double buffer */
    realOutBuf = pos < halfOutputBuffer ? 1U : 0U;

    if (pInfo->priming)
    {
        realOutBuf = pInfo->renderHead & 0x1;
    }
    ioPacket->packet = pInfo->stream->render.packets + realOutBuf;
    ioPacket->startByte = realOutBuf * halfOutputBuffer;
    ioPacket->lengthBytes = halfOutputBuffer;

    ++pInfo->renderHead;
    --pInfo->pending;
    return NoError ;
}

static CaError CaPinRenderEventHandler_WaveRTPolled(CaProcessThreadInfo* pInfo, unsigned eventIndex)
{
    unsigned long  pos;
    unsigned       realOutBuf;
    unsigned       bytesToWrite;
    CaWdmIOInfo  * pRender = &pInfo->stream->render;
    const unsigned halfOutputBuffer = pRender->hostBufferSize >> 1;
    CaWdmPin     * pin = pInfo->stream->render.pPin;
    CaIOPacket   * ioPacket = &pInfo->renderPackets[pInfo->renderHead & cPacketsArrayMask];

    /* Get hold of current DAC position */
    pin->fnAudioPosition(pin, &pos);
    /* Compensate for HW FIFO to get to last read buffer position */
    pos += pin->hwLatency;
    /* Wrap it */
    pos %= pRender->hostBufferSize;
    /* And align it, not sure its really needed though */
    pos &= ~(pRender->bytesPerFrame - 1);

    if (pInfo->priming)
    {
        realOutBuf = pInfo->renderHead & 0x1;
        ioPacket->packet = pInfo->stream->render.packets + realOutBuf;
        ioPacket->startByte = realOutBuf * halfOutputBuffer;
        ioPacket->lengthBytes = halfOutputBuffer;
        ++pInfo->renderHead;
        --pInfo->pending;
    } else {
        bytesToWrite = (pRender->hostBufferSize + pos - pRender->lastPosition) % pRender->hostBufferSize ;
        ++pRender->pollCntr;
        if (bytesToWrite >= halfOutputBuffer)
        {
            realOutBuf = (pos < halfOutputBuffer) ? 1U : 0U;
            ioPacket->packet = pInfo->stream->render.packets + realOutBuf;
            pRender->lastPosition = realOutBuf ? 0U : halfOutputBuffer;
            ioPacket->startByte = realOutBuf * halfOutputBuffer;
            ioPacket->lengthBytes = halfOutputBuffer;
            ++pInfo->renderHead;
            --pInfo->pending;
            pRender->pollCntr = 0;
        }
    }
    return NoError;
}

static CaError CaPinRenderSubmitHandler_WaveRTEvent(CaProcessThreadInfo * pInfo, unsigned eventIndex)
{
    CaWdmPin* pin = pInfo->stream->render.pPin;
    pInfo->renderPackets[pInfo->renderTail & cPacketsArrayMask].packet = 0;
    /* Call barrier (if needed) */
    pin->fnMemBarrier();
    ++pInfo->pending;
    if (pInfo->priming)
    {
        --pInfo->priming;
        if (pInfo->priming)
        {
            SetEvent(pInfo->stream->render.events[0]);
        }
    }
    return NoError;
}

static CaError CaPinRenderSubmitHandler_WaveRTPolled(CaProcessThreadInfo* pInfo, unsigned eventIndex)
{
  CaWdmPin * pin = pInfo->stream->render.pPin                                ;
  pInfo->renderPackets[pInfo->renderTail & cPacketsArrayMask].packet = 0     ;
  pin->fnMemBarrier()                                                        ;
  ++pInfo->pending                                                           ;
  if (pInfo->priming)                                                        {
    --pInfo->priming                                                         ;
    if (pInfo->priming) SetEvent(pInfo->stream->render.events[0])            ;
  }                                                                          ;
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

CaError WdmKsInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError        result      = NoError                                       ;
  int            deviceCount = 0                                             ;
  void         * scanResults = NULL                                          ;
  WdmKsHostApi * wdmHostApi  = NULL                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Attempt to load the KSUSER.DLL without which we cannot create pins
     We will unload this on termination                                     */
  if ( DllKsUser == NULL )                                                   {
    DllKsUser = LoadLibrary(TEXT("ksuser.dll"))                              ;
    if ( DllKsUser == NULL ) goto error                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  FunctionKsCreatePin = (KSCREATEPIN *)GetProcAddress(DllKsUser, "KsCreatePin") ;
  if ( FunctionKsCreatePin == NULL ) goto error                              ;
  /* Attempt to load AVRT.DLL, if we can't, then we'll just use time critical prio instead... */
  if (caWdmKsAvRtEntryPoints.hInstance == NULL)                              {
    caWdmKsAvRtEntryPoints.hInstance = LoadLibrary(TEXT("avrt.dll"))         ;
    if (caWdmKsAvRtEntryPoints.hInstance != NULL)                            {
      caWdmKsAvRtEntryPoints.AvSetMmThreadCharacteristics =
      (HANDLE(WINAPI*)(LPCSTR,LPDWORD))GetProcAddress(caWdmKsAvRtEntryPoints.hInstance,"AvSetMmThreadCharacteristicsA") ;
      caWdmKsAvRtEntryPoints.AvRevertMmThreadCharacteristics =
      (BOOL(WINAPI*)(HANDLE))GetProcAddress(caWdmKsAvRtEntryPoints.hInstance, "AvRevertMmThreadCharacteristics") ;
      caWdmKsAvRtEntryPoints.AvSetMmThreadPriority =
      (BOOL(WINAPI*)(HANDLE,PA_AVRT_PRIORITY))GetProcAddress(caWdmKsAvRtEntryPoints.hInstance, "AvSetMmThreadPriority") ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  wdmHostApi = new WdmKsHostApi ( )                                          ;
  if ( !wdmHostApi )                                                         {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  wdmHostApi->allocations = new AllocationGroup ( )                          ;
  if ( NULL == wdmHostApi->allocations )                                     {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *hostApi                             = (HostApi *)wdmHostApi               ;
  (*hostApi)->info.structVersion       = 1                                   ;
  (*hostApi)->info.type                = WDMKS                               ;
  (*hostApi)->info.index               = hostApiIndex                        ;
  (*hostApi)->info.name                = "Windows WDM-KS"                    ;
  /* these are all updated by CommitDeviceInfos()                           */
  (*hostApi)->info.deviceCount         = 0                                   ;
  (*hostApi)->info.defaultInputDevice  = CaNoDevice                          ;
  (*hostApi)->info.defaultOutputDevice = CaNoDevice                          ;
  (*hostApi)->deviceInfos              = NULL                                ;
  ////////////////////////////////////////////////////////////////////////////
  result = wdmHostApi->ScanDeviceInfos(hostApiIndex,&scanResults,&deviceCount) ;
  if ( result != NoError) goto error                                         ;
  ////////////////////////////////////////////////////////////////////////////
  wdmHostApi->CommitDeviceInfos(hostApiIndex,scanResults,deviceCount)        ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
error:
  if ( NULL != wdmHostApi ) wdmHostApi->Terminate()                          ;
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

WdmKsStream:: WdmKsStream (void)
            : Stream      (    )
{
  ::memset ( &hostApiStreamInfo , 0 , sizeof(CaWdmKsSpecificStreamInfo) ) ;
  ::memset ( &capture           , 0 , sizeof(CaWdmIOInfo              ) ) ;
  ::memset ( &render            , 0 , sizeof(CaWdmIOInfo              ) ) ;
  ::memset (  eventStreamStart  , 0 , sizeof(HANDLE) * StreamStart_kCnt ) ;
  /////////////////////////////////////////////////////////////////////////
  allocGroup           = NULL                                             ;
  ringBufferData       = NULL                                             ;
  /////////////////////////////////////////////////////////////////////////
  streamStarted        = 0                                                ;
  streamActive         = 0                                                ;
  streamStop           = 0                                                ;
  streamAbort          = 0                                                ;
  oldProcessPriority   = 0                                                ;
  streamThread         = 0                                                ;
  eventAbort           = 0                                                ;
  threadResult         = 0                                                ;
  streamFlags          = 0                                                ;
  userInputChannels    = 0                                                ;
  deviceInputChannels  = 0                                                ;
  userOutputChannels   = 0                                                ;
  deviceOutputChannels = 0                                                ;
  /////////////////////////////////////////////////////////////////////////
  doResample           = false                                            ;
  Resampler            = NULL                                             ;
  NearestSampleRate    = -1                                               ;
}

WdmKsStream::~WdmKsStream(void)
{
}

CaError WdmKsStream::Start(void)
{
  CaError result = NoError                                                   ;

  if ( streamThread != NULL) return StreamIsNotStopped ;

  streamStop = 0;
  streamAbort = 0;

  ResetEvents();

  bufferProcessor.Reset();

  oldProcessPriority = GetPriorityClass(GetCurrentProcess()) ;
  /* Uncomment the following line to enable dynamic boosting of the process
  * priority to real time for best low latency support
  * Disabled by default because RT processes can easily block the OS */
  /*ret = SetPriorityClass(GetCurrentProcess(),REALTIME_PRIORITY_CLASS);
  gPrintf(("Class ret = %d;",ret));*/

  streamThread = CREATE_THREAD_FUNCTION (NULL, 0, ProcessingThread, this, CREATE_SUSPENDED, NULL) ;
  if (streamThread == NULL)
  {
      result = InsufficientMemory;
      goto end;
  }

  ResumeThread(streamThread) ;

  switch (WaitForMultipleObjects(2,eventStreamStart, FALSE, 5000)) {
  case WAIT_OBJECT_0 + StreamStart_kOk:
      dPrintf(("Windows Driver Model - Kernel Streaming started!\n"));
      result = NoError;
      /* streamActive is set in processing thread */
      streamStarted = 1;
      break;
  case WAIT_OBJECT_0 + StreamStart_kFailed:
      dPrintf(("Windows Driver Model - Kernel Streaming start failed! (result=%d)\n",threadResult));
      result = threadResult;
      /* Wait for the stream to really exit */
      WaitForSingleObject(streamThread, 200);
      CloseHandle(streamThread);
      streamThread = 0;
      break;
  case WAIT_TIMEOUT:
  default:
      result = TimedOut ;
      WdmSetLastErrorInfo(result, "Failed to start Windows Driver Model - Kernel Streaming (timeout)!") ;
      break;
  }

end:

  return result;
}

CaError WdmKsStream::Stop(void)
{
  CaError result = NoError                                                   ;
  BOOL doCb = FALSE;

  if (streamActive)
  {
      DWORD dwExitCode;
      doCb = TRUE;
      streamStop = 1;
      if (GetExitCodeThread(streamThread, &dwExitCode) && dwExitCode == STILL_ACTIVE)
      {
          if (WaitForSingleObject(streamThread, INFINITE) != WAIT_OBJECT_0)
          {
              dPrintf(("StopStream: stream thread terminated\n"));
              TerminateThread(streamThread, -1);
              result = TimedOut;
          }
      }
      else
      {
          dPrintf(("StopStream: GECT says not active, but streamActive is not false ??"));
          result = UnanticipatedHostError;
          WdmSetLastErrorInfo(result, "StopStream: GECT says not active, but streamActive = %d",streamActive);
      }
  }
  else
  {
      if (threadResult != NoError)
      {
          dPrintf(("StopStream: Stream not active (%d)\n", threadResult));
          result = threadResult;
          threadResult = NoError;
      }
  }

  if (streamThread != NULL)
  {
      CloseHandle(streamThread);
      streamThread = 0;
  }
  streamStarted = 0;
  streamActive = 0;

  if(doCb)
  {
      /* Do user callback now after all state has been reset */
      /* This means it should be safe for the called function */
      /* to invoke e.g. StartStream */
      if ( conduit != NULL ) conduit->finish();
  }

  return result;
}

CaError WdmKsStream::Close(void)
{
  CaError result = NoError                                                   ;

  bufferProcessor.Terminate();
  Terminate();
  CloseEvents();

  if (allocGroup)    {
    allocGroup->release();
    delete allocGroup ;
    allocGroup = NULL ;
    }

    if(render.pPin)
    {
        PinClose(render.pPin);
    }
    if(capture.pPin)
    {
        PinClose(capture.pPin);
    }

    if (render.pPin)
    {
        FilterFree(render.pPin->parentFilter);
    }

    if (capture.pPin)
    {
        FilterFree(capture.pPin->parentFilter);
    }

  return result                                                              ;
}

CaError WdmKsStream::Abort(void)
{
  CaError result = NoError                                                   ;
  int     doCb   = 0                                                         ;

  if (streamActive) {
      doCb = 1;
      streamAbort = 1;
      SetEvent(eventAbort); /* Signal immediately */
      if (WaitForSingleObject(streamThread, 10000) != WAIT_OBJECT_0)
      {
          TerminateThread(streamThread, -1);
          result = TimedOut;

          dPrintf(("AbortStream: stream thread terminated\n"));
      }
  }
  CloseHandle(streamThread);
  streamThread = NULL;
  streamStarted = 0;

  if ( doCb ) {
      /* Do user callback now after all state has been reset */
      /* This means it should be safe for the called function */
      /* to invoke e.g. StartStream */
      if ( conduit != NULL) conduit->finish();
  }
  ////////////////////////////////////////////////////////////////////////////
  streamActive  = 0                                                          ;
  streamStarted = 0                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

CaError WdmKsStream::IsStopped(void)
{
  return ( ! streamStarted ) ;
}

CaError WdmKsStream::IsActive(void)
{
  return streamActive ;
}

CaTime WdmKsStream::GetTime(void)
{
  return Timer :: Time ( ) ;
}

double WdmKsStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer . Value ( ) ;
}

CaInt32 WdmKsStream::ReadAvailable(void)
{
  return 0 ;
}

CaInt32 WdmKsStream::WriteAvailable(void)
{
  return 0 ;
}

CaError WdmKsStream::Read(void * buffer,unsigned long frames)
{
  return InternalError ;
}

CaError WdmKsStream::Write(const void * buffer,unsigned long frames)
{
  return InternalError ;
}

bool WdmKsStream::hasVolume(void)
{
  // KSNODETYPE_VOLUME
  // KSPROPERTY_AUDIO_VOLUMELEVEL
  return false ;
}

CaVolume WdmKsStream::MinVolume(void)
{
  return 0.0 ;
}

CaVolume WdmKsStream::MaxVolume(void)
{
  return 10000.0 ;
}

CaVolume WdmKsStream::Volume(int atChannel)
{
  return 10000.0 ;
}

CaVolume WdmKsStream::setVolume(CaVolume volume,int atChannel)
{
  return 10000.0 ;
}

void WdmKsStream::ResetEvents(void)
{
    unsigned i;
    ResetEvent(eventAbort);
    ResetEvent(eventStreamStart[StreamStart_kOk]);
    ResetEvent(eventStreamStart[StreamStart_kFailed]);

    for (i=0; i<capture.noOfPackets; ++i)
    {
        if (capture.events && capture.events[i])
        {
            ResetEvent(capture.events[i]);
        }
    }

    for (i=0; i<render.noOfPackets; ++i)
    {
        if (render.events && render.events[i])
        {
            ResetEvent(render.events[i]);
        }
    }
}

void WdmKsStream::CloseEvents(void)
{
    unsigned i;
    CaWdmIOInfo* ios[2] = { &capture, &render };

    if (eventAbort)
    {
        CloseHandle(eventAbort);
        eventAbort = 0;
    }
    if (eventStreamStart[StreamStart_kOk])
    {
        CloseHandle(eventStreamStart[StreamStart_kOk]);
    }
    if (eventStreamStart[StreamStart_kFailed])
    {
        CloseHandle(eventStreamStart[StreamStart_kFailed]);
    }

    for (i = 0; i < 2; ++i)
    {
        unsigned j;
        /* Unregister notification handles for WaveRT */
        if (ios[i]->pPin && ios[i]->pPin->parentFilter->devInfo.streamingType == Type_kWaveRT &&
            ios[i]->pPin->pinKsSubType == SubType_kNotification &&
            ios[i]->events != 0)
        {
            PinUnregisterNotificationHandle(ios[i]->pPin, ios[i]->events[0]);
        }

        for (j=0; j < ios[i]->noOfPackets; ++j)
        {
            if (ios[i]->events && ios[i]->events[j])
            {
                CloseHandle(ios[i]->events[j]);
                ios[i]->events[j] = 0;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

WdmKsDeviceInfo:: WdmKsDeviceInfo (void)
                : DeviceInfo      (    )
{
  Initialize ( ) ;
}

WdmKsDeviceInfo::~WdmKsDeviceInfo (void)
{
}

void WdmKsDeviceInfo::Initialize(void)
{
  memset ( compositeName , 0 , MAX_PATH                   ) ;
  memset ( filterPath    , 0 , MAX_PATH * sizeof(wchar_t) ) ;
  memset ( topologyPath  , 0 , MAX_PATH * sizeof(wchar_t) ) ;
  filter            = NULL                                  ;
  pin               = 0                                     ;
  muxPosition       = 0                                     ;
  endpointPinId     = 0                                     ;
  streamingType     = Type_kNotUsed                         ;
  deviceProductGuid = GUID_NULL                             ;
}

//////////////////////////////////////////////////////////////////////////////

WdmKsHostApiInfo:: WdmKsHostApiInfo (void)
                 : HostApiInfo      (    )
{
}

WdmKsHostApiInfo::~WdmKsHostApiInfo (void)
{
}

//////////////////////////////////////////////////////////////////////////////

WdmKsHostApi:: WdmKsHostApi (void)
             : HostApi      (    )
{
  allocations   = NULL  ;
  deviceCount   = 0     ;
  /////////////////////////////////
  #if defined(FFMPEGLIB)
  CanDoResample = true  ;
  #else
  CanDoResample = false ;
  #endif
}

WdmKsHostApi::~WdmKsHostApi(void)
{
  CaEraseAllocation ;
}

HostApi::Encoding WdmKsHostApi::encoding(void) const
{
  return UTF8 ;
}

bool WdmKsHostApi::hasDuplex(void) const
{
  return true ;
}

CaError WdmKsHostApi::Open                           (
          Stream                ** s                 ,
          const StreamParameters * inputParameters   ,
          const StreamParameters * outputParameters  ,
          double                   sampleRate        ,
          CaUint32                 framesPerCallback ,
          CaStreamFlags            streamFlags       ,
          Conduit                * conduit           )
{
  CaError              result                 = NoError                      ;
  WdmKsStream        * stream                 = NULL                         ;
  CaSampleFormat       inputSampleFormat      = cafNothing                   ;
  CaSampleFormat       outputSampleFormat     = cafNothing                   ;
  CaSampleFormat       hostInputSampleFormat  = cafNothing                   ;
  CaSampleFormat       hostOutputSampleFormat = cafNothing                   ;
  int                  userInputChannels      = 0                            ;
  int                  userOutputChannels     = 0                            ;
  WAVEFORMATEXTENSIBLE wfx                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "Open::sampleRate = %f\n"       , sampleRate        ) )        ;
  dPrintf ( ( "Open::framesPerBuffer = %lu\n" , framesPerCallback ) )        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( inputParameters ) )                                       {
    dPrintf ( ( "Having input parameters\n" ) )                              ;
    userInputChannels = inputParameters->channelCount                        ;
    inputSampleFormat = inputParameters->sampleFormat                        ;
    /* unless alternate device specification is supported, reject the use of
      CaUseHostApiSpecificDeviceSpecification                               */
    if ( inputParameters->device==CaUseHostApiSpecificDeviceSpecification )  {
      WdmSetLastErrorInfo ( InvalidDevice                                    ,
               "CaUseHostApiSpecificDeviceSpecification(in) not supported" ) ;
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that input device can support stream->userInputChannels        */
    if ( userInputChannels > deviceInfos[ inputParameters->device ]->maxInputChannels ) {
      WdmSetLastErrorInfo(InvalidChannelCount, "Invalid input channel count");
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate inputStreamInfo                                             */
    result = ValidateSpecificStreamParameters                                (
               inputParameters                                               ,
               (CaWdmKsInfo *)inputParameters->streamInfo   ) ;
    if ( NoError != result )                                                 {
      WdmSetLastErrorInfo(result, "Host API stream info not supported (in)") ;
      return result                                                          ;
      /* this implementation doesn't use custom stream info                 */
    }                                                                        ;
  } else                                                                     {
    userInputChannels     = 0                                                ;
    inputSampleFormat     = cafInt16                                         ;
    hostInputSampleFormat = cafInt16                                         ;
    /* Supress 'uninitialised var' warnings.                                */
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( outputParameters ) )                                      {
    dPrintf ( ( "Having output parameters\n" ) )                             ;
    userOutputChannels = outputParameters->channelCount                      ;
    outputSampleFormat = outputParameters->sampleFormat                      ;
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    if ( CaIsEqual ( outputParameters->device                                ,
                     CaUseHostApiSpecificDeviceSpecification             ) ) {
      WdmSetLastErrorInfo                                                    (
        InvalidDevice                                                        ,
        "paUseHostApiSpecificDeviceSpecification(out) not supported"       ) ;
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that output device can support stream->userInputChannels       */
    if ( CaIsGreater                                                         (
           userOutputChannels                                                ,
           deviceInfos [ outputParameters->device ] -> maxOutputChannels ) ) {
      WdmSetLastErrorInfo                                                    (
        InvalidChannelCount                                                  ,
        "Invalid output channel count"                                     ) ;
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate outputStreamInfo                                            */
    result = ValidateSpecificStreamParameters                                (
               outputParameters                                              ,
               (CaWdmKsInfo *)outputParameters->streamInfo  ) ;
    if ( CaIsWrong ( result ) )                                              {
      WdmSetLastErrorInfo                                                    (
        result                                                               ,
        "Host API stream info not supported (out)"                         ) ;
      return result                                                          ;
      /* this implementation doesn't use custom stream info                 */
    }                                                                        ;
  } else                                                                     {
    userOutputChannels     = 0                                               ;
    outputSampleFormat     = cafInt16                                        ;
    hostOutputSampleFormat = cafInt16                                        ;
    /* Supress 'uninitialized var' warnings.                                */
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* validate platform specific flags                                       */
  if ( CaNotEqual ( (streamFlags & csfPlatformSpecificFlags) , 0 ) )         {
    WdmSetLastErrorInfo ( InvalidFlag , "Invalid flag supplied" )            ;
    return InvalidFlag; /* unexpected platform specific flag                */
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "Create WdmKsStream instance\n" ) )                            ;
  stream  = new WdmKsStream ( )                                              ;
  stream -> debugger = debugger                                              ;
  if ( !stream )                                                             {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "Create allocation group\n" ) )                                ;
  stream -> allocGroup = new AllocationGroup ( )                             ;
  if ( !stream->allocGroup )                                                 {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  if ( CaNotNull ( conduit ) )                                               {
    stream->conduit = conduit                                                ;
  } else                                                                     {
    dPrintf ( ( "Blocking API not supported yet!\n" ) )                      ;
    WdmSetLastErrorInfo ( UnanticipatedHostError                             ,
                         "Blocking API not supported yet"                  ) ;
    result = UnanticipatedHostError                                          ;
    goto error                                                               ;
  }                                                                          ;
  stream -> cpuLoadMeasurer . Initialize ( sampleRate )                      ;
  /* Instantiate the input pin if necessary                                 */
  if ( userInputChannels > 0 )                                               {
    dPrintf ( ( "Instantiate the input pin\n" ) )                            ;
    CaWdmFilter           * pFilter            = NULL                        ;
    WdmKsDeviceInfo       * pDeviceInfo        = NULL                        ;
    CaWdmPin              * pPin               = NULL                        ;
    unsigned                validBitsPerSample = 0                           ;
    CaWaveFormatChannelMask channelMask        = 0                           ;
    //////////////////////////////////////////////////////////////////////////
    channelMask = CaDefaultChannelMask( userInputChannels )                  ;
    result      = SampleFormatNotSupported                                   ;
    pDeviceInfo = (WdmKsDeviceInfo *) deviceInfos[inputParameters->device]   ;
    pFilter     = pDeviceInfo->filter                                        ;
    pPin        = pFilter->pins[pDeviceInfo->pin]                            ;
    //////////////////////////////////////////////////////////////////////////
    stream->userInputChannels = userInputChannels                            ;
    hostInputSampleFormat = ClosestFormat(pPin->formats, inputSampleFormat)  ;
    if ( CaIsEqual ( hostInputSampleFormat , SampleFormatNotSupported ) )    {
      result = UnanticipatedHostError                                        ;
      WdmSetLastErrorInfo                                                    (
        result                                                               ,
        "PU_SCAF(%X,%X) failed (input)"                                      ,
        pPin->formats                                                        ,
        inputSampleFormat                                                  ) ;
      goto error                                                             ;
    } else
    if ( CaAND ( CaIsEqual ( pFilter->devInfo.streamingType , Type_kWaveRT ) ,
                 CaIsEqual ( hostInputSampleFormat , cafInt24          ) ) ) {
      /* For WaveRT, we choose 32 bit format instead of CaInt24, since we
       * MIGHT need to align buffer on a 128 byte boundary (see PinGetBuffer) */
      hostInputSampleFormat = cafInt32                                       ;
      /* But we'll tell the driver that it's 24 bit in 32 bit container     */
      validBitsPerSample = 24                                                ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    bool   resampling        = false                                         ;
    double nearestSampleRate = -1                                            ;
    if ( CanDoResample )                                                     {
      double nearest                                                         ;
      nearest = Nearest                                                      (
                  pPin                                                       ,
                  sampleRate                                                 ,
                  inputSampleFormat                                          ,
                  stream->userInputChannels                                  ,
                  channelMask                                              ) ;
      if ( CaNotEqual ( (int)nearest , (int)sampleRate ) )                   {
        dPrintf ( ( "Suggest input sample rate %6.1f for %f\n"               ,
                    nearest                                                  ,
                    sampleRate                                           ) ) ;
        resampling = true                                                    ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( CaAND ( resampling , CaIsGreater ( nearestSampleRate , 0 ) ) )      {
      #pragma message("Remember to implement the WDM-KS input resampling code, it is actually quite easy")
      dPrintf ( ( "Create nearest Pin for %f\n" , nearestSampleRate      ) ) ;
      dPrintf ( ( "The author was too busy or forget to implement the "
                  "input resampling code in WDM-KS.  Try remind him via"
                  " email.\n"                                            ) ) ;
    } else                                                                   {
      dPrintf ( ( "Create default Pin for %f\n" , sampleRate             ) ) ;
      ////////////////////////////////////////////////////////////////////////
      CaInitializeWaveFormatExtensible                                       (
        (CaWaveFormat *)&wfx                                                 ,
        userInputChannels                                                    ,
        inputSampleFormat                                                    ,
        CaSampleFormatToLinearWaveFormatTag ( inputSampleFormat )            ,
        sampleRate                                                           ,
        channelMask                                                        ) ;
      stream -> capture . bytesPerFrame = wfx . Format . nBlockAlign         ;
      if ( CaNotEqual ( validBitsPerSample , 0 ) )                           {
        wfx.Samples.wValidBitsPerSample = validBitsPerSample                 ;
      }                                                                      ;
      stream->capture.pPin = FilterCreatePin                                 (
                              pFilter                                        ,
                              pPin->pinId                                    ,
                              (WAVEFORMATEX*)&wfx                            ,
                              &result                                      ) ;
      stream->deviceInputChannels = userInputChannels                        ;
      ////////////////////////////////////////////////////////////////////////
    }                                                                        ;
    while ( hostInputSampleFormat <= cafUint8 )                              {
          unsigned channelsToProbe = stream->userInputChannels;
          /* Some or all KS devices can only handle the exact number of channels
          * they specify. But PortAudio clients expect to be able to
          * at least specify mono I/O on a multi-channel device
          * If this is the case, then we will do the channel mapping internally
          * The following loop tests this case
          **/
          while ( 1 ) {
              CaInitializeWaveFormatExtensible(
                (CaWaveFormat*)&wfx,
                channelsToProbe,
                hostInputSampleFormat,
                CaSampleFormatToLinearWaveFormatTag(hostInputSampleFormat),
                sampleRate,
                channelMask );
              stream->capture.bytesPerFrame = wfx.Format.nBlockAlign;
              if (validBitsPerSample != 0)
              {
                  wfx.Samples.wValidBitsPerSample = validBitsPerSample;
              }
              stream->capture.pPin = FilterCreatePin(pFilter, pPin->pinId, (WAVEFORMATEX*)&wfx, &result);
              stream->deviceInputChannels = channelsToProbe;

              if ( result != NoError && result != DeviceUnavailable ) {
                  /* Try a WAVE_FORMAT_PCM instead */
                  CaInitializeWaveFormatEx(
                    (CaWaveFormat*)&wfx,
                    channelsToProbe,
                    hostInputSampleFormat,
                    CaSampleFormatToLinearWaveFormatTag(hostInputSampleFormat),
                    sampleRate ) ;
                  if (validBitsPerSample != 0)
                  {
                      wfx.Samples.wValidBitsPerSample = validBitsPerSample;
                  }
                  stream->capture.pPin = FilterCreatePin(pFilter, pPin->pinId, (const WAVEFORMATEX*)&wfx, &result) ;
              }

              if (result == DeviceUnavailable) goto occupied;
              if (result == NoError)
              {
                  /* We're done */
                  break;
              }

              if (channelsToProbe < (unsigned)pPin->maxChannels)
              {
                  /* Go to next multiple of 2 */
                  channelsToProbe = min((((channelsToProbe>>1)+1)<<1), (unsigned)pPin->maxChannels);
                  continue;
              }

              break;
          }

          if (result == NoError)
          {
              /* We're done */
              break;
          }

          /* Go to next format in line with lower resolution */
          hostInputSampleFormat = (CaSampleFormat)(((int)hostInputSampleFormat) << 1) ;
      }

      if(stream->capture.pPin == NULL)
      {
          WdmSetLastErrorInfo(result, "Failed to create capture pin: sr=%u,ch=%u,bits=%u,align=%u",
              wfx.Format.nSamplesPerSec, wfx.Format.nChannels, wfx.Format.wBitsPerSample, wfx.Format.nBlockAlign) ;
          goto error;
      }

      /* Select correct mux input on MUX node of topology filter */
      if (pDeviceInfo->muxPosition >= 0)
      {

          result = FilterUse(pPin->parentFilter->topologyFilter);
          if (result != NoError)
          {
              WdmSetLastErrorInfo(result, "Failed to open topology filter");
              goto error;
          }

          result = WdmSetMuxNodeProperty(pPin->parentFilter->topologyFilter->handle,
              pPin->inputs[pDeviceInfo->muxPosition]->muxNodeId,
              pPin->inputs[pDeviceInfo->muxPosition]->muxPinId);

          FilterRelease(pPin->parentFilter->topologyFilter);

          if(result != NoError)
          {
              WdmSetLastErrorInfo(result, "Failed to set topology mux node");
              goto error;
          }
      }
      stream->capture.bytesPerSample = stream->capture.bytesPerFrame / stream->deviceInputChannels;
      stream->capture.pPin->frameSize /= stream->capture.bytesPerFrame;
      dPrintf(("Capture pin frames: %d\n",stream->capture.pPin->frameSize)) ;
  } else                                                                     {
    stream -> capture . pPin          = NULL                                 ;
    stream -> capture . bytesPerFrame = 0                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Instantiate the output pin if necessary                                */
  if ( CaIsGreater ( userOutputChannels , 0 ) )                              {
    dPrintf ( ( "Instantiate the output pin\n" ) )                           ;
    CaWdmFilter           * pFilter            = NULL                        ;
    WdmKsDeviceInfo       * pDeviceInfo        = NULL                        ;
    CaWdmPin              * pPin               = NULL                        ;
    unsigned                validBitsPerSample = 0                           ;
    CaWaveFormatChannelMask channelMask        = 0                           ;
    channelMask = CaDefaultChannelMask( userOutputChannels )                 ;
    //////////////////////////////////////////////////////////////////////////
    result      = SampleFormatNotSupported                                   ;
    pDeviceInfo = (WdmKsDeviceInfo*)deviceInfos[outputParameters->device]    ;
    pFilter     = pDeviceInfo->filter                                        ;
    pPin        = pFilter->pins[pDeviceInfo->pin]                            ;
    //////////////////////////////////////////////////////////////////////////
    stream -> userOutputChannels = userOutputChannels                        ;
    hostOutputSampleFormat = ClosestFormat ( pPin->formats                   ,
                                             outputSampleFormat            ) ;
    //////////////////////////////////////////////////////////////////////////
    if ( CaIsEqual ( hostOutputSampleFormat , SampleFormatNotSupported ) )   {
      result = UnanticipatedHostError                                        ;
      WdmSetLastErrorInfo                                                    (
        result                                                               ,
        "PU_SCAF(%X,%X) failed (output)"                                     ,
        pPin->formats                                                        ,
        hostOutputSampleFormat                                             ) ;
      goto error                                                             ;
    } else
    if ( CaAND ( CaIsEqual ( pFilter->devInfo.streamingType , Type_kWaveRT ) ,
                 CaIsEqual ( hostOutputSampleFormat , cafInt24         ) ) ) {
      /* For WaveRT, we choose 32 bit format instead of CaInt24, since we
       * MIGHT need to align buffer on a 128 byte boundary (see PinGetBuffer) */
      hostOutputSampleFormat = cafInt32                                      ;
      /* But we'll tell the driver that it's 24 bit in 32 bit container     */
      validBitsPerSample = 24                                                ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    bool   resampling        = false                                         ;
    double nearestSampleRate = -1                                            ;
    if ( CanDoResample )                                                     {
      double nearest                                                         ;
      nearest = Nearest                                                      (
                  pPin                                                       ,
                  sampleRate                                                 ,
                  outputSampleFormat                                         ,
                  stream->userOutputChannels                                 ,
                  channelMask                                              ) ;
      if ( CaNotEqual ( (int)nearest , (int)sampleRate ) )                   {
        dPrintf (("Suggest sample rate %6.1f for %f\n",nearest,sampleRate))  ;
        resampling = true                                                    ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( CaAND ( resampling , CaIsGreater ( nearestSampleRate , 0 ) ) )      {
      #pragma message("Remember to implement the WDM-KS output resampling code, it is actually quite easy")
      dPrintf ( ( "Create nearest Pin for %f\n" , nearestSampleRate      ) ) ;
      dPrintf ( ( "The author was too busy or forget to implement the "
                  "output resampling code in WDM-KS.  Try remind him via"
                  " email.\n"                                            ) ) ;
    } else                                                                   {
      dPrintf ( ( "Create default Pin for %f\n" , sampleRate             ) ) ;
      ////////////////////////////////////////////////////////////////////////
      CaInitializeWaveFormatExtensible                                       (
        (CaWaveFormat *)&wfx                                                 ,
        userOutputChannels                                                   ,
        outputSampleFormat                                                   ,
        CaSampleFormatToLinearWaveFormatTag ( outputSampleFormat )           ,
        sampleRate                                                           ,
        channelMask                                                        ) ;
      stream -> render . bytesPerFrame = wfx . Format . nBlockAlign          ;
      if ( CaNotEqual ( validBitsPerSample , 0 ) )                           {
        wfx.Samples.wValidBitsPerSample = validBitsPerSample                 ;
      }                                                                      ;
      stream->render.pPin = FilterCreatePin                                  (
                              pFilter                                        ,
                              pPin->pinId                                    ,
                              (WAVEFORMATEX*)&wfx                            ,
                              &result                                      ) ;
      stream->deviceOutputChannels = userOutputChannels                      ;
      ////////////////////////////////////////////////////////////////////////
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    while ( hostOutputSampleFormat <= cafUint8 )                             {
      unsigned channelsToProbe = stream->userOutputChannels                  ;
      while ( true )                                                         {
        CaInitializeWaveFormatExtensible                                     (
          (CaWaveFormat *)&wfx                                               ,
          channelsToProbe                                                    ,
          hostOutputSampleFormat                                             ,
          CaSampleFormatToLinearWaveFormatTag ( hostOutputSampleFormat )     ,
          sampleRate                                                         ,
          channelMask                                                      ) ;
        stream -> render . bytesPerFrame = wfx . Format . nBlockAlign        ;
        if ( CaNotEqual ( validBitsPerSample , 0 ) )                         {
          wfx.Samples.wValidBitsPerSample = validBitsPerSample               ;
        }                                                                    ;
        stream->render.pPin = FilterCreatePin                                (
                                pFilter                                      ,
                                pPin->pinId                                  ,
                                (WAVEFORMATEX*)&wfx                          ,
                                &result                                    ) ;
        stream->deviceOutputChannels = channelsToProbe                       ;
        if ( CaAND ( CaNotEqual ( result , NoError                     )     ,
                     CaNotEqual ( result , DeviceUnavailable           ) ) ) {
          CaInitializeWaveFormatEx                                           (
            (CaWaveFormat*)&wfx                                              ,
            channelsToProbe                                                  ,
            hostOutputSampleFormat                                           ,
            CaSampleFormatToLinearWaveFormatTag ( hostOutputSampleFormat )   ,
            sampleRate                                                     ) ;
          if ( CaNotEqual ( validBitsPerSample , 0 ) )                       {
            wfx.Samples.wValidBitsPerSample = validBitsPerSample             ;
          }                                                                  ;
          stream->render.pPin = FilterCreatePin                              (
                                  pFilter                                    ,
                                  pPin->pinId                                ,
                                  (const WAVEFORMATEX *)&wfx                 ,
                                  &result                                  ) ;
        }                                                                    ;
        if ( CaIsEqual ( result , DeviceUnavailable ) ) goto occupied        ;
        if ( CaIsEqual ( result , NoError           ) ) break                ;
        if ( CaIsLess  ( channelsToProbe , (unsigned)pPin->maxChannels ) )   {
          /* Go to next multiple of 2                                       */
          channelsToProbe = min ( ( ( ( channelsToProbe >> 1 ) + 1 ) << 1  ) ,
                                  (unsigned) pPin -> maxChannels           ) ;
          continue                                                           ;
        }                                                                    ;
        break                                                                ;
      }                                                                      ;
      if ( CaIsCorrect ( result ) ) break                                    ;
      /* Go to next format in line with lower resolution                    */
      hostOutputSampleFormat = (CaSampleFormat)(((int)hostOutputSampleFormat) << 1 );
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( CaIsNull ( stream->render.pPin ) )                                  {
      WdmSetLastErrorInfo                                                    (
        result                                                               ,
        "Failed to create render pin: sr=%u,ch=%u,bits=%u,align=%u"          ,
        wfx.Format.nSamplesPerSec                                            ,
        wfx.Format.nChannels                                                 ,
        wfx.Format.wBitsPerSample                                            ,
        wfx.Format.nBlockAlign                                             ) ;
      dPrintf ( ( "stream->render.pPin == NULL\n" ) )                        ;
      goto error                                                             ;
    }                                                                        ;
    stream->render.bytesPerSample   = stream->render.bytesPerFrame           /
                                      stream->deviceOutputChannels           ;
    stream->render.pPin->frameSize /= stream->render.bytesPerFrame           ;
    dPrintf ( ( "Render pin frames: %d\n",stream->render.pPin->frameSize ) ) ;
  } else                                                                     {
    stream->render.pPin          = NULL                                      ;
    stream->render.bytesPerFrame = 0                                         ;
  }                                                                          ;
  /* Calculate the framesPerHostXxxxBuffer size based upon the suggested latency values */
  /* Record the buffer length                                               */
      dPrintf ( ( "NULL != inputParameters\n" ) ) ;
  if ( NULL != inputParameters )                                             {
      /* Calculate the frames from the user's value - add a bit to round up */
      stream->capture.framesPerBuffer = (unsigned long)((inputParameters->suggestedLatency*sampleRate)+0.0001);
      if(stream->capture.framesPerBuffer > (unsigned long)sampleRate)
      { /* Upper limit is 1 second */
          stream->capture.framesPerBuffer = (unsigned long)sampleRate;
      }
      else if(stream->capture.framesPerBuffer < stream->capture.pPin->frameSize)
      {
          stream->capture.framesPerBuffer = stream->capture.pPin->frameSize;
      }
      dPrintf(("Input frames chosen:%ld\n",stream->capture.framesPerBuffer));

      /* Setup number of packets to use */
      stream->capture.noOfPackets = 2;

      if (inputParameters->streamInfo)
      {
          CaWdmKsInfo* pInfo = (CaWdmKsInfo *)inputParameters->streamInfo;

          if (stream->capture.pPin->parentFilter->devInfo.streamingType == Type_kWaveCyclic &&
              pInfo->noOfPackets != 0)
          {
              stream->capture.noOfPackets = pInfo->noOfPackets;
          }
      }

  }

      dPrintf ( ( "NULL != inputParameters\n" ) ) ;
  if ( NULL != outputParameters )                                            {
      /* Calculate the frames from the user's value - add a bit to round up */
      stream->render.framesPerBuffer = (unsigned long)((outputParameters->suggestedLatency*sampleRate)+0.0001) ;
      if(stream->render.framesPerBuffer > (unsigned long)sampleRate)
      { /* Upper limit is 1 second */
          stream->render.framesPerBuffer = (unsigned long)sampleRate;
      }
      else if(stream->render.framesPerBuffer < stream->render.pPin->frameSize)
      {
          stream->render.framesPerBuffer = stream->render.pPin->frameSize;
      }
      dPrintf(("Output frames chosen:%ld\n",stream->render.framesPerBuffer));

      /* Setup number of packets to use */
      stream->render.noOfPackets = 2;

      if (outputParameters->streamInfo) {
          CaWdmKsInfo* pInfo = (CaWdmKsInfo*)outputParameters->streamInfo ;

          if (stream->render.pPin->parentFilter->devInfo.streamingType == Type_kWaveCyclic &&
              pInfo->noOfPackets != 0)
          {
              stream->render.noOfPackets = pInfo->noOfPackets;
          }
      }

  }

  /* Host buffer size is bound to the largest of the input and output frame sizes */
      dPrintf ( ( "stream->bufferProcessor.Initialize\n" ) ) ;
  result = stream->bufferProcessor.Initialize(
      stream->userInputChannels, inputSampleFormat, hostInputSampleFormat,
      stream->userOutputChannels, outputSampleFormat, hostOutputSampleFormat,
      sampleRate, streamFlags, framesPerCallback,
      max(stream->capture.framesPerBuffer, stream->render.framesPerBuffer),
      cabBoundedHostBufferSize,
      conduit );
  if( result != NoError )
  {
      WdmSetLastErrorInfo(result, "PaUtil_InitializeBufferProcessor failed: ich=%u, isf=%u, hisf=%u, och=%u, osf=%u, hosf=%u, sr=%lf, flags=0x%X, fpub=%u, fphb=%u",
          stream->userInputChannels, inputSampleFormat, hostInputSampleFormat,
          stream->userOutputChannels, outputSampleFormat, hostOutputSampleFormat,
          sampleRate, streamFlags, framesPerCallback,
          max(stream->capture.framesPerBuffer, stream->render.framesPerBuffer));
      goto error;
  }

  /* Allocate/get all the buffers for host I/O */
     dPrintf ( ( "stream->userInputChannels > 0\n" ) ) ;
  if (stream->userInputChannels > 0)
  {
      stream->inputLatency = stream->capture.framesPerBuffer / sampleRate ;

      switch (stream->capture.pPin->parentFilter->devInfo.streamingType)
      {
      case Type_kWaveCyclic:
          {
              unsigned size = stream->capture.noOfPackets * stream->capture.framesPerBuffer * stream->capture.bytesPerFrame;
              /* Allocate input host buffer */
              stream->capture.hostBuffer = (char *)stream->allocGroup->alloc(size);
              dPrintf(("Input buffer allocated (size = %u)\n", size));
              if( !stream->capture.hostBuffer )
              {
                  dPrintf(("Cannot allocate host input buffer!\n"));
                  WdmSetLastErrorInfo(InsufficientMemory, "Failed to allocate input buffer");
                  result = InsufficientMemory;
                  goto error;
              }
              stream->capture.hostBufferSize = size;
              dPrintf(("Input buffer start = %p (size=%u)\n",stream->capture.hostBuffer, stream->capture.hostBufferSize));
              stream->capture.pPin->fnEventHandler = CaPinCaptureEventHandler_WaveCyclic;
              stream->capture.pPin->fnSubmitHandler = CaPinCaptureSubmitHandler_WaveCyclic;
          }
          break;
      case Type_kWaveRT:
          {
              const DWORD dwTotalSize = 2 * stream->capture.framesPerBuffer * stream->capture.bytesPerFrame;
              DWORD dwRequestedSize = dwTotalSize;
              BOOL bCallMemoryBarrier = FALSE;
              ULONG hwFifoLatency = 0;
              ULONG dummy;
              result = PinGetBuffer(stream->capture.pPin, (void**)&stream->capture.hostBuffer, &dwRequestedSize, &bCallMemoryBarrier);
              if (!result)
              {
                  dPrintf(("Input buffer start = %p, size = %u\n", stream->capture.hostBuffer, dwRequestedSize));
                  if (dwRequestedSize != dwTotalSize)
                  {
                      dPrintf(("Buffer length changed by driver from %u to %u !\n", dwTotalSize, dwRequestedSize));
                      /* Recalculate to what the driver has given us */
                      stream->capture.framesPerBuffer = dwRequestedSize / (2 * stream->capture.bytesPerFrame);
                  }
                  stream->capture.hostBufferSize = dwRequestedSize;

                  if (stream->capture.pPin->pinKsSubType == SubType_kPolled)
                  {
                      stream->capture.pPin->fnEventHandler = CaPinCaptureEventHandler_WaveRTPolled;
                      stream->capture.pPin->fnSubmitHandler = CaPinCaptureSubmitHandler_WaveRTPolled;
                  }
                  else
                  {
                      stream->capture.pPin->fnEventHandler = CaPinCaptureEventHandler_WaveRTEvent;
                      stream->capture.pPin->fnSubmitHandler = CaPinCaptureSubmitHandler_WaveRTEvent;
                  }

                  stream->capture.pPin->fnMemBarrier = bCallMemoryBarrier ? MemoryBarrierRead : MemoryBarrierDummy;
              }
              else
              {
                  dPrintf(("Failed to get input buffer (WaveRT)\n"));
                  WdmSetLastErrorInfo(UnanticipatedHostError, "Failed to get input buffer (WaveRT)");
                  result = UnanticipatedHostError;
                  goto error;
              }

              /* Get latency */
              result = PinGetHwLatency(stream->capture.pPin, &hwFifoLatency, &dummy, &dummy);
              if (result == NoError)
              {
                  stream->capture.pPin->hwLatency = hwFifoLatency;

                  /* Add HW latency into total input latency */
                  stream->inputLatency += ((hwFifoLatency / stream->capture.bytesPerFrame) / sampleRate) ;
              }
              else
              {
                  dPrintf(("Failed to get size of FIFO hardware buffer (is set to zero)\n"));
                  stream->capture.pPin->hwLatency = 0;
              }
          }
          break;
      default:
          /* Undefined wave type!! */
          result = InternalError;
          WdmSetLastErrorInfo(result, "Wave type %u ??", stream->capture.pPin->parentFilter->devInfo.streamingType);
          goto error;
      }
  } else {
      stream->capture.hostBuffer = 0;
  }

      dPrintf ( ( "stream->userOutputChannels > 0\n" ) ) ;
  if (stream->userOutputChannels > 0)
  {
    stream->outputLatency = stream->render.framesPerBuffer / sampleRate ;

      switch (stream->render.pPin->parentFilter->devInfo.streamingType)
      {
      case Type_kWaveCyclic:
          {
              unsigned size = stream->render.noOfPackets * stream->render.framesPerBuffer * stream->render.bytesPerFrame ;
              /* Allocate output device buffer */
              stream->render.hostBuffer = (char*)stream->allocGroup->alloc(size) ;
              dPrintf(("Output buffer allocated (size = %u)\n", size));
              if( !stream->render.hostBuffer )
              {
                  dPrintf(("Cannot allocate host output buffer!\n"));
                  WdmSetLastErrorInfo(InsufficientMemory, "Failed to allocate output buffer");
                  result = InsufficientMemory;
                  goto error;
              }
              stream->render.hostBufferSize = size;
              dPrintf(("Output buffer start = %p (size=%u)\n",stream->render.hostBuffer, stream->render.hostBufferSize));

              stream->render.pPin->fnEventHandler = CaPinRenderEventHandler_WaveCyclic;
              stream->render.pPin->fnSubmitHandler = CaPinRenderSubmitHandler_WaveCyclic;
          }
          break;
      case Type_kWaveRT:
          {
              const DWORD dwTotalSize = 2 * stream->render.framesPerBuffer * stream->render.bytesPerFrame;
              DWORD dwRequestedSize = dwTotalSize;
              BOOL bCallMemoryBarrier = FALSE;
              ULONG hwFifoLatency = 0;
              ULONG dummy;
              result = PinGetBuffer(stream->render.pPin, (void**)&stream->render.hostBuffer, &dwRequestedSize, &bCallMemoryBarrier);
              if (!result)
              {
                  dPrintf(("Output buffer start = %p, size = %u, membarrier = %u\n", stream->render.hostBuffer, dwRequestedSize, bCallMemoryBarrier));
                  if (dwRequestedSize != dwTotalSize)
                  {
                      dPrintf(("Buffer length changed by driver from %u to %u !\n", dwTotalSize, dwRequestedSize));
                      /* Recalculate to what the driver has given us */
                      stream->render.framesPerBuffer = dwRequestedSize / (2 * stream->render.bytesPerFrame);
                  }
                  stream->render.hostBufferSize = dwRequestedSize;

                  if (stream->render.pPin->pinKsSubType == SubType_kPolled)
                  {
                      stream->render.pPin->fnEventHandler = CaPinRenderEventHandler_WaveRTPolled;
                      stream->render.pPin->fnSubmitHandler = CaPinRenderSubmitHandler_WaveRTPolled;
                  }
                  else
                  {
                      stream->render.pPin->fnEventHandler = CaPinRenderEventHandler_WaveRTEvent;
                      stream->render.pPin->fnSubmitHandler = CaPinRenderSubmitHandler_WaveRTEvent;
                  }

                  stream->render.pPin->fnMemBarrier = bCallMemoryBarrier ? MemoryBarrierWrite : MemoryBarrierDummy;
              } else {
                  dPrintf(("Failed to get output buffer (with notification)\n"));
                  WdmSetLastErrorInfo(UnanticipatedHostError, "Failed to get output buffer (with notification)");
                  result = UnanticipatedHostError;
                  goto error;
              }

              /* Get latency */
              result = PinGetHwLatency(stream->render.pPin, &hwFifoLatency, &dummy, &dummy);
              if (result == NoError)
              {
                  stream->render.pPin->hwLatency = hwFifoLatency;

                  /* Add HW latency into total output latency */
                  stream->outputLatency += ((hwFifoLatency / stream->render.bytesPerFrame) / sampleRate);
              }
              else
              {
                  dPrintf(("Failed to get size of FIFO hardware buffer (is set to zero)\n"));
                  stream->render.pPin->hwLatency = 0;
              }
          }
          break;
      default:
          /* Undefined wave type!! */
          result = InternalError;
          WdmSetLastErrorInfo(result, "Wave type %u ??", stream->capture.pPin->parentFilter->devInfo.streamingType) ;
          goto error;
    }
  } else {
    stream->render.hostBuffer = 0 ;
  }

  dPrintf ( ( "stream->sampleRate = sampleRate\n" ) )                        ;
  stream->sampleRate = sampleRate ;
  dPrintf ( ( "BytesPerInputFrame  = %d\n",stream->capture.bytesPerFrame ) ) ;
  dPrintf ( ( "BytesPerOutputFrame = %d\n",stream->render.bytesPerFrame  ) ) ;

  /* Abort */
  stream->eventAbort          = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (stream->eventAbort == 0)
  {
      result = InsufficientMemory;
      goto error;
  }
  stream->eventStreamStart[0] = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (stream->eventStreamStart[0] == 0)
  {
      result = InsufficientMemory;
      goto error;
  }
  stream->eventStreamStart[1] = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (stream->eventStreamStart[1] == 0)
  {
      result = InsufficientMemory;
      goto error;
  }

  if(stream->userInputChannels > 0)
  {
      const unsigned bufferSizeInBytes = stream->capture.framesPerBuffer * stream->capture.bytesPerFrame;
      const unsigned ringBufferFrameSize = NextPowerOf2( 1024 + 2 * max(stream->capture.framesPerBuffer, stream->render.framesPerBuffer) );

      stream->capture.events = (HANDLE *)stream->allocGroup->alloc(stream->capture.noOfPackets * sizeof(HANDLE));
      if (stream->capture.events == NULL)
      {
          result = InsufficientMemory;
          goto error;
      }

      stream->capture.packets = (DATAPACKET *)stream->allocGroup->alloc(stream->capture.noOfPackets * sizeof(DATAPACKET));
      if (stream->capture.packets == NULL)
      {
          result = InsufficientMemory;
          goto error;
      }

      switch(stream->capture.pPin->parentFilter->devInfo.streamingType)
      {
      case Type_kWaveCyclic:
          {
              /* WaveCyclic case */
              unsigned i;
              for (i = 0; i < stream->capture.noOfPackets; ++i)
              {
                  /* Set up the packets */
                  DATAPACKET *p = stream->capture.packets + i;

                  /* Record event */
                  stream->capture.events[i] = CreateEvent(NULL, TRUE, FALSE, NULL);

                  p->Signal.hEvent = stream->capture.events[i];
                  p->Header.Data = stream->capture.hostBuffer + (i*bufferSizeInBytes);
                  p->Header.FrameExtent = bufferSizeInBytes;
                  p->Header.DataUsed = 0;
                  p->Header.Size = sizeof(p->Header);
                  p->Header.PresentationTime.Numerator = 1;
                  p->Header.PresentationTime.Denominator = 1;
              }
          }
          break;
      case Type_kWaveRT:
          {
              /* Set up the "packets" */
              DATAPACKET *p = stream->capture.packets + 0;

              /* Record event: WaveRT has a single event for 2 notification per buffer */
              stream->capture.events[0] = CreateEvent(NULL, FALSE, FALSE, NULL);

              p->Header.Data = stream->capture.hostBuffer;
              p->Header.FrameExtent = bufferSizeInBytes;
              p->Header.DataUsed = 0;
              p->Header.Size = sizeof(p->Header);
              p->Header.PresentationTime.Numerator = 1;
              p->Header.PresentationTime.Denominator = 1;

              ++p;
              p->Header.Data = stream->capture.hostBuffer + bufferSizeInBytes;
              p->Header.FrameExtent = bufferSizeInBytes;
              p->Header.DataUsed = 0;
              p->Header.Size = sizeof(p->Header);
              p->Header.PresentationTime.Numerator = 1;
              p->Header.PresentationTime.Denominator = 1;

              if (stream->capture.pPin->pinKsSubType == SubType_kNotification)
              {
                  result = PinRegisterNotificationHandle(stream->capture.pPin, stream->capture.events[0]);

                  if (result != NoError)
                  {
                      dPrintf(("Failed to register capture notification handle\n"));
                      WdmSetLastErrorInfo(UnanticipatedHostError, "Failed to register capture notification handle");
                      result = UnanticipatedHostError;
                      goto error;
                  }
              }

              result = PinRegisterPositionRegister(stream->capture.pPin);

              if (result != NoError)
              {
                  unsigned long pos = 0xdeadc0de;
                  dPrintf(("Failed to register capture position register, using PinGetAudioPositionViaIOCTL\n"));
                  stream->capture.pPin->fnAudioPosition = PinGetAudioPositionViaIOCTL;
                  /* Test position function */
                  result = (stream->capture.pPin->fnAudioPosition)(stream->capture.pPin, &pos);
                  if (result != NoError || pos != 0x0)
                  {
                      dPrintf(("Failed to read capture position register (IOCTL)\n"));
                      WdmSetLastErrorInfo(UnanticipatedHostError, "Failed to read capture position register (IOCTL)");
                      result = UnanticipatedHostError;
                      goto error;
                  }
              }
              else
              {
                  stream->capture.pPin->fnAudioPosition = PinGetAudioPositionDirect;
              }
          }
          break;
      default:
          /* Undefined wave type!! */
          result = InternalError;
          WdmSetLastErrorInfo(result, "Wave type %u ??", stream->capture.pPin->parentFilter->devInfo.streamingType);
          goto error;
      }

      /* Setup the input ring buffer here */
      stream->ringBufferData = (char *)stream->allocGroup->alloc(ringBufferFrameSize * stream->capture.bytesPerFrame) ;
      if (stream->ringBufferData == NULL)
      {
          result = InsufficientMemory;
          goto error;
      }
      stream->ringBuffer.Initialize(stream->capture.bytesPerFrame, ringBufferFrameSize, stream->ringBufferData) ;
  }

  if ( stream->userOutputChannels > 0 )                                      {
    const unsigned bufferSizeInBytes = stream->render.framesPerBuffer * stream->render.bytesPerFrame;

    stream->render.events = (HANDLE *) stream->allocGroup->alloc(stream->render.noOfPackets * sizeof(HANDLE)) ;
    if (stream->render.events == NULL) {
      result = InsufficientMemory      ;
      goto error;
    }

    stream->render.packets = (DATAPACKET*) stream->allocGroup->alloc( stream->render.noOfPackets * sizeof(DATAPACKET) ) ;
    if (stream->render.packets == NULL) {
      result = InsufficientMemory ;
      goto error;
    }

      switch(stream->render.pPin->parentFilter->devInfo.streamingType)
      {
      case Type_kWaveCyclic:
          {
              /* WaveCyclic case */
              unsigned i;
              for (i = 0; i < stream->render.noOfPackets; ++i)
              {
                  /* Set up the packets */
                  DATAPACKET *p = stream->render.packets + i;

                  /* Playback event */
                  stream->render.events[i] = CreateEvent(NULL, TRUE, FALSE, NULL);

                  /* In this case, we just use the packets as ptr to the device buffer */
                  p->Signal.hEvent = stream->render.events[i];
                  p->Header.Data = stream->render.hostBuffer + (i*bufferSizeInBytes);
                  p->Header.FrameExtent = bufferSizeInBytes;
                  p->Header.DataUsed = bufferSizeInBytes;
                  p->Header.Size = sizeof(p->Header);
                  p->Header.PresentationTime.Numerator = 1;
                  p->Header.PresentationTime.Denominator = 1;
              }
          }
      break;
      case Type_kWaveRT:
          {
              /* WaveRT case */

              /* Set up the "packets" */
              DATAPACKET *p = stream->render.packets;

              /* The only playback event */
              stream->render.events[0] = CreateEvent(NULL, FALSE, FALSE, NULL);

              /* In this case, we just use the packets as ptr to the device buffer */
              p->Header.Data = stream->render.hostBuffer;
              p->Header.FrameExtent = stream->render.framesPerBuffer*stream->render.bytesPerFrame;
              p->Header.DataUsed = stream->render.framesPerBuffer*stream->render.bytesPerFrame;
              p->Header.Size = sizeof(p->Header);
              p->Header.PresentationTime.Numerator = 1;
              p->Header.PresentationTime.Denominator = 1;

              ++p;
              p->Header.Data = stream->render.hostBuffer + stream->render.framesPerBuffer*stream->render.bytesPerFrame;
              p->Header.FrameExtent = stream->render.framesPerBuffer*stream->render.bytesPerFrame;
              p->Header.DataUsed = stream->render.framesPerBuffer*stream->render.bytesPerFrame;
              p->Header.Size = sizeof(p->Header);
              p->Header.PresentationTime.Numerator = 1;
              p->Header.PresentationTime.Denominator = 1;

              if (stream->render.pPin->pinKsSubType == SubType_kNotification)
              {
                  result = PinRegisterNotificationHandle(stream->render.pPin, stream->render.events[0]);

                  if (result != NoError)
                  {
                      dPrintf(("Failed to register rendering notification handle\n"));
                      WdmSetLastErrorInfo(UnanticipatedHostError, "Failed to register rendering notification handle");
                      result = UnanticipatedHostError;
                      goto error;
                  }
              }

              result = PinRegisterPositionRegister(stream->render.pPin);

              if (result != NoError)
              {
                  unsigned long pos = 0xdeadc0de;
                  dPrintf(("Failed to register rendering position register, using PinGetAudioPositionViaIOCTL\n"));
                  stream->render.pPin->fnAudioPosition = PinGetAudioPositionViaIOCTL;
                  /* Test position function */
                  result = (stream->render.pPin->fnAudioPosition)(stream->render.pPin, &pos);
                  if (result != NoError || pos != 0x0)
                  {
                      dPrintf(("Failed to read render position register (IOCTL)\n"));
                      WdmSetLastErrorInfo(UnanticipatedHostError, "Failed to read render position register (IOCTL)");
                      result = UnanticipatedHostError;
                      goto error;
                  }
              }
              else
              {
                  stream->render.pPin->fnAudioPosition = PinGetAudioPositionDirect;
              }
          }
      break  ;
      default:
          /* Undefined wave type!! */
          result = InternalError;
          WdmSetLastErrorInfo(result, "Wave type %u ??", stream->capture.pPin->parentFilter->devInfo.streamingType) ;
          goto error;
    }
  }
  ////////////////////////////////////////////////////////////////////////////
  stream -> streamStarted      = 0                                           ;
  stream -> streamActive       = 0                                           ;
  stream -> streamStop         = 0                                           ;
  stream -> streamAbort        = 0                                           ;
  stream -> streamFlags        = streamFlags                                 ;
  stream -> oldProcessPriority = REALTIME_PRIORITY_CLASS                     ;
  ////////////////////////////////////////////////////////////////////////////
  /* Increase ref count on filters in use, so that a CommitDeviceInfos won't delete them */
  if ( CaNotNull ( stream->capture.pPin ) )                                  {
    FilterAddRef ( stream->capture.pPin->parentFilter )                      ;
  }                                                                          ;
  if ( CaNotNull ( stream->render.pPin  ) )                                  {
    FilterAddRef ( stream->render.pPin->parentFilter )                       ;
  }                                                                          ;
  /* Ok, now update our host API specific stream info                       */
  if ( CaIsGreater ( stream -> userInputChannels , 0 ) )                     {
    WdmKsDeviceInfo * pDeviceInfo = (WdmKsDeviceInfo *)deviceInfos[inputParameters->device] ;
    stream->hostApiStreamInfo.input.device    = Core::ToDeviceIndex(Core::TypeIdToIndex(WDMKS),inputParameters->device) ;
    stream->hostApiStreamInfo.input.channels  = stream->deviceInputChannels  ;
    stream->hostApiStreamInfo.input.muxNodeId = -1                           ;
    if (stream->capture.pPin->inputs)                                        {
      stream->hostApiStreamInfo.input.muxNodeId = stream->capture.pPin->inputs[pDeviceInfo->muxPosition]->muxNodeId;
    }                                                                        ;
    stream->hostApiStreamInfo.input.endpointPinId       = pDeviceInfo->endpointPinId;
    stream->hostApiStreamInfo.input.framesPerHostBuffer = stream->capture.framesPerBuffer;
    stream->hostApiStreamInfo.input.streamingSubType    = stream->capture.pPin->pinKsSubType;
  } else                                                                     {
    stream->hostApiStreamInfo.input.device = CaNoDevice                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaIsGreater ( stream -> userOutputChannels , 0 ) )                    {
    stream->hostApiStreamInfo.output.device =  Core::ToDeviceIndex(Core::TypeIdToIndex(WDMKS),outputParameters->device) ;
    stream->hostApiStreamInfo.output.channels = stream->deviceOutputChannels ;
    stream->hostApiStreamInfo.output.framesPerHostBuffer = stream->render.framesPerBuffer;
    stream->hostApiStreamInfo.output.endpointPinId = stream->render.pPin->endpointPinId;
    stream->hostApiStreamInfo.output.streamingSubType = stream->render.pPin->pinKsSubType;
  } else                                                                     {
    stream->hostApiStreamInfo.output.device = CaNoDevice                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaAND ( CaNotNull ( conduit ) , CaNotNull ( inputParameters  ) ) )    {
    conduit -> Input  . setSample ( inputSampleFormat                        ,
                                    userInputChannels                      ) ;
  }                                                                          ;
  if ( CaAND ( CaNotNull ( conduit ) , CaNotNull ( outputParameters ) )  )   {
    conduit -> Output . setSample ( outputSampleFormat                       ,
                                    userOutputChannels                     ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> structVersion = 2                                                ;
  *s = (Stream *)stream                                                      ;
  return result                                                              ;
occupied                                                                     :
  /* Ok, someone else is hogging the pin, bail out                          */
  WdmSetLastErrorInfo ( result , "Device is occupied" )                      ;
error                                                                        :
  stream -> bufferProcessor . Terminate ( )                                  ;
  stream -> CloseEvents                 ( )                                  ;
  if ( NULL != stream->allocGroup )                                          {
    stream -> allocGroup -> release ( )                                      ;
    delete stream -> allocGroup                                              ;
    stream -> allocGroup = NULL                                              ;
  }                                                                          ;
  if ( stream -> render  . pPin ) PinClose ( stream -> render  . pPin )      ;
  if ( stream -> capture . pPin ) PinClose ( stream -> capture . pPin )      ;
  delete stream                                                              ;
  stream = NULL                                                              ;
  return result                                                              ;
}

void WdmKsHostApi::Terminate(void)
{
  /* Do not unload the libraries                                            */
  if ( NULL != DllKsUser )                                                   {
    ::FreeLibrary ( DllKsUser )                                              ;
    DllKsUser = NULL                                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( caWdmKsAvRtEntryPoints.hInstance != NULL )                            {
    ::FreeLibrary ( caWdmKsAvRtEntryPoints . hInstance )                     ;
    caWdmKsAvRtEntryPoints . hInstance = NULL                                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  WdmScanDeviceInfosResults * localScanResults                               ;
  localScanResults  = (WdmScanDeviceInfosResults *) allocations -> alloc     (
                                         sizeof(WdmScanDeviceInfosResults) ) ;
  localScanResults -> deviceInfos = deviceInfos                              ;
  DisposeDeviceInfos ( localScanResults , info . deviceCount )               ;
  deviceInfos = NULL                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  CaEraseAllocation                                                          ;
}

CaError WdmKsHostApi::isSupported                    (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double sampleRate                          )
{
  CaError                 result = NoError                                   ;
  int                     inputChannelCount                                  ;
  int                     outputChannelCount                                 ;
  CaSampleFormat          inputSampleFormat                                  ;
  CaSampleFormat          outputSampleFormat                                 ;
  CaWdmFilter           * pFilter                                            ;
  WAVEFORMATEXTENSIBLE    wfx                                                ;
  CaWaveFormatChannelMask channelMask                                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( inputParameters )                                                     {
    WdmKsDeviceInfo * pDeviceInfo = (WdmKsDeviceInfo *)deviceInfos[inputParameters->device] ;
    CaWdmPin        * pin                                                    ;
    unsigned          fmt                                                    ;
    unsigned long     testFormat  = 0                                        ;
    unsigned          validBits   = 0                                        ;
    //////////////////////////////////////////////////////////////////////////
    inputChannelCount = inputParameters->channelCount                        ;
    inputSampleFormat = inputParameters->sampleFormat                        ;
    /* all standard sample formats are supported by the buffer adapter,
       this implementation doesn't support any custom sample formats */
    if ( inputSampleFormat & cafCustomFormat )                               {
      WdmSetLastErrorInfo(SampleFormatNotSupported, "IsFormatSupported: Custom input format not supported");
      return SampleFormatNotSupported                                        ;
    }                                                                        ;
    /* unless alternate device specification is supported, reject the use of
       UseHostApiSpecificDeviceSpecification                                */
    if ( inputParameters->device==CaUseHostApiSpecificDeviceSpecification )  {
      WdmSetLastErrorInfo(InvalidDevice,"IsFormatSupported: paUseHostApiSpecificDeviceSpecification not supported") ;
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that input device can support inputChannelCount                */
    if ( inputChannelCount > deviceInfos[ inputParameters->device ]->maxInputChannels ) {
      WdmSetLastErrorInfo(InvalidChannelCount, "IsFormatSupported: Invalid input channel count");
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate inputStreamInfo                                             */
    if ( inputParameters->streamInfo )                        {
      WdmSetLastErrorInfo(IncompatibleStreamInfo, "Host API stream info not supported");
      return IncompatibleStreamInfo                           ;
      /* this implementation doesn't use custom stream info                 */
    }                                                                        ;
    pFilter = pDeviceInfo->filter                                            ;
    pin     = pFilter->pins[pDeviceInfo->pin]                                ;
    /* Find out the testing format                                          */
    for (fmt = cafFloat32; fmt <= cafUint8; fmt <<= 1)                       {
      if ((fmt & pin->formats) != 0)                                         {
        /* Found a matching format!                                         */
        testFormat = fmt                                                     ;
        break                                                                ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( testFormat == 0 )                                                   {
      WdmSetLastErrorInfo(result, "IsFormatSupported(capture) failed: no testformat found!") ;
      return UnanticipatedHostError                                          ;
    }                                                                        ;
    /* Due to special considerations, WaveRT devices with paInt24 should be tested with paInt32 and
       valid bits = 24 (instead of 24 bit samples) */
    if ( pFilter->devInfo.streamingType == Type_kWaveRT && testFormat == cafInt24 ) {
      dPrintf(("IsFormatSupported (capture): WaveRT overriding testFormat paInt24 with paInt32 (24 valid bits)")) ;
      testFormat = cafInt32                                                  ;
      validBits = 24                                                         ;
    }                                                                        ;
    /* Check that the input format is supported                             */
    channelMask = CaDefaultChannelMask(inputChannelCount)                      ;
    CaInitializeWaveFormatExtensible(
      (CaWaveFormat*)&wfx,
      inputChannelCount,
      (CaSampleFormat)testFormat,
      CaSampleFormatToLinearWaveFormatTag((CaSampleFormat)testFormat),
      sampleRate,
      channelMask );
    if (validBits != 0)                                                      {
      wfx.Samples.wValidBitsPerSample = validBits                            ;
    }                                                                        ;
    result = PinIsFormatSupported(pin, (const WAVEFORMATEX*)&wfx)            ;
    if ( result != NoError )                                                 {
      /* Try a WAVE_FORMAT_PCM instead                                      */
      CaInitializeWaveFormatEx                                               (
        (CaWaveFormat*)&wfx                                                  ,
        inputChannelCount                                                    ,
        (CaSampleFormat)testFormat                                           ,
        CaSampleFormatToLinearWaveFormatTag((CaSampleFormat)testFormat)      ,
        sampleRate                                                         ) ;
      if (validBits != 0)                                                    {
        wfx.Samples.wValidBitsPerSample = validBits                          ;
      }                                                                      ;
      result = PinIsFormatSupported(pin, (const WAVEFORMATEX*)&wfx)          ;
      if ( result != NoError )                                               {
        WdmSetLastErrorInfo(result, "IsFormatSupported(capture) failed: sr=%u,ch=%u,bits=%u", wfx.Format.nSamplesPerSec, wfx.Format.nChannels, wfx.Format.wBitsPerSample) ;
        return result                                                        ;
      }                                                                      ;
    }                                                                        ;
  } else                                                                     {
    inputChannelCount = 0                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( outputParameters )                                                    {
    WdmKsDeviceInfo * pDeviceInfo = (WdmKsDeviceInfo *)deviceInfos[outputParameters->device] ;
    CaWdmPin        * pin                                                    ;
    unsigned          fmt                                                    ;
    unsigned long     testFormat  = 0                                        ;
    unsigned          validBits   = 0                                        ;
    //////////////////////////////////////////////////////////////////////////
    outputChannelCount = outputParameters->channelCount                      ;
    outputSampleFormat = outputParameters->sampleFormat                      ;
    /* all standard sample formats are supported by the buffer adapter,
       this implementation doesn't support any custom sample formats */
    if ( outputSampleFormat & cafCustomFormat )                              {
      WdmSetLastErrorInfo(SampleFormatNotSupported, "IsFormatSupported: Custom output format not supported");
      return SampleFormatNotSupported                                        ;
    }
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    if ( outputParameters->device == CaUseHostApiSpecificDeviceSpecification ) {
      WdmSetLastErrorInfo(InvalidDevice, "IsFormatSupported: paUseHostApiSpecificDeviceSpecification not supported") ;
      return InvalidDevice                                                   ;
    }
    /* check that output device can support outputChannelCount              */
    if ( outputChannelCount > deviceInfos[ outputParameters->device ]->maxOutputChannels ) {
      WdmSetLastErrorInfo(InvalidChannelCount, "Invalid output channel count") ;
      return InvalidChannelCount;
    }
    /* validate outputStreamInfo                                            */
    if ( outputParameters->streamInfo )                       {
      WdmSetLastErrorInfo(IncompatibleStreamInfo, "Host API stream info not supported") ;
      return IncompatibleStreamInfo; /* this implementation doesn't use custom stream info */
    }
    pFilter = pDeviceInfo -> filter                                          ;
    pin     = pFilter     -> pins [ pDeviceInfo -> pin ]                     ;
    /* Find out the testing format */
    for (fmt = cafFloat32; fmt <= cafUint8; fmt <<= 1)                       {
      if ((fmt & pin->formats) != 0)                                         {
        /* Found a matching format!                                         */
        testFormat = fmt                                                     ;
        break                                                                ;
      }                                                                      ;
    }                                                                        ;
    if ( testFormat == 0 )                                                   {
      WdmSetLastErrorInfo(result, "IsFormatSupported(render) failed: no testformat found!") ;
      return UnanticipatedHostError                                          ;
    }                                                                        ;
    /* Due to special considerations, WaveRT devices with paInt24 should be tested with paInt32 and
       valid bits = 24 (instead of 24 bit samples) */
    if (pFilter->devInfo.streamingType == Type_kWaveRT && testFormat == cafInt24) {
      dPrintf(("IsFormatSupported (render): WaveRT overriding testFormat paInt24 with paInt32 (24 valid bits)")) ;
      testFormat = cafInt32;
      validBits = 24;
    }
    /* Check that the output format is supported */
    channelMask = CaDefaultChannelMask(outputChannelCount)                   ;
    CaInitializeWaveFormatExtensible (
      (CaWaveFormat*)&wfx,
      outputChannelCount,
      (CaSampleFormat)testFormat,
      CaSampleFormatToLinearWaveFormatTag((CaSampleFormat)testFormat) ,
      sampleRate,
      channelMask );
    if (validBits != 0) wfx.Samples.wValidBitsPerSample = validBits          ;
    result = PinIsFormatSupported(pin, (const WAVEFORMATEX*)&wfx)            ;
    if ( result != NoError )                                                 {
      /* Try a WAVE_FORMAT_PCM instead                                      */
      CaInitializeWaveFormatEx                                               (
        (CaWaveFormat *)&wfx                                                 ,
        outputChannelCount                                                   ,
        (CaSampleFormat)testFormat                                           ,
        CaSampleFormatToLinearWaveFormatTag((CaSampleFormat)testFormat)      ,
        sampleRate                                                         ) ;
      if (validBits != 0) wfx.Samples.wValidBitsPerSample = validBits        ;
      result = PinIsFormatSupported(pin, (const WAVEFORMATEX*)&wfx)          ;
      if ( result != NoError )                                               {
        WdmSetLastErrorInfo(result, "IsFormatSupported(render) failed: %u,%u,%u", wfx.Format.nSamplesPerSec, wfx.Format.nChannels, wfx.Format.wBitsPerSample) ;
        return result                                                        ;
      }                                                                      ;
    }                                                                        ;
  } else                                                                     {
    outputChannelCount = 0                                                   ;
  }                                                                          ;
  /*

  - if a full duplex stream is requested, check that the combination
  of input and output parameters is supported if necessary

  - check that the device supports sampleRate

  Because the buffer adapter handles conversion between all standard
  sample formats, the following checks are only required if paCustomFormat
  is implemented, or under some other unusual conditions.

  - check that input device can support inputSampleFormat, or that
  we have the capability to convert from inputSampleFormat to
  a native format

  - check that output device can support outputSampleFormat, or that
  we have the capability to convert from outputSampleFormat to
  a native format
  */
  if((inputChannelCount == 0)&&(outputChannelCount == 0))
  {
    WdmSetLastErrorInfo(SampleFormatNotSupported, "No input or output channels defined") ;
    result = SampleFormatNotSupported; /* Not right error */
  }

  return result                                                              ;
}

CaError WdmKsHostApi::ScanDeviceInfos   (
          CaHostApiIndex hostApiIndex   ,
          void        ** scanResults    ,
          int         *  newDeviceCount )
{
  CaError                      result           = NoError                    ;
  CaWdmFilter               ** ppFilters        = 0                          ;
  WdmScanDeviceInfosResults *  outArgument      = 0                          ;
  int                          filterCount      = 0                          ;
  int                          totalDeviceCount = 0                          ;
  int                          idxDevice        = 0                          ;
  ////////////////////////////////////////////////////////////////////////////
  ppFilters = BuildFilterList( &filterCount, &totalDeviceCount, &result )    ;
  if ( result != NoError ) goto error                                        ;
  if ( totalDeviceCount > 0 )                                                {
    WdmKsDeviceInfo * deviceInfoArray = 0                                    ;
    int               idxFilter                                              ;
    int               i                                                      ;
    /* Allocate the out param for all the info we need                      */
    outArgument = (WdmScanDeviceInfosResults *) allocations -> alloc         (
                                         sizeof(WdmScanDeviceInfosResults) ) ;
    if ( !outArgument )                                                      {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    outArgument->defaultInputDevice  = CaNoDevice                            ;
    outArgument->defaultOutputDevice = CaNoDevice                            ;
    outArgument->deviceInfos = new DeviceInfo * [ totalDeviceCount ]         ;
    if ( !outArgument->deviceInfos )                                         {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    /* allocate all device info structs in a contiguous block               */
    deviceInfoArray = new WdmKsDeviceInfo [ totalDeviceCount ]               ;
    if ( !deviceInfoArray )                                                  {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    /* Make sure all items in array                                         */
    for ( i = 0 ; i < totalDeviceCount; ++i )                                {
      DeviceInfo * devInfo             = (DeviceInfo *)& deviceInfoArray[i]  ;
      devInfo     -> structVersion     = 2                                   ;
      devInfo     -> hostApi           = hostApiIndex                        ;
      devInfo     -> hostType          = WDMKS                               ;
      devInfo     -> name              = 0                                   ;
      outArgument -> deviceInfos [ i ] = devInfo                             ;
      deviceInfoArray [ i ] . Initialize ( )                                 ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    idxDevice = 0                                                            ;
        for ( idxFilter = 0; idxFilter < filterCount; ++idxFilter)           {
          CaNameHashObject nameHash = {0}                                    ;
          CaWdmFilter    * pFilter  = ppFilters[idxFilter]                   ;
          if ( pFilter == NULL ) continue                                    ;
            if (InitNameHashObject(&nameHash, pFilter) != NoError)           {
              DeinitNameHashObject ( &nameHash )                             ;
              continue                                                       ;
            }                                                                ;
            //////////////////////////////////////////////////////////////////
            for (i = 0; i < pFilter->pinCount; ++i)                          {
              unsigned   m                                                   ;
              ULONG      nameIndex     = 0                                   ;
              ULONG      nameIndexHash = 0                                   ;
              CaWdmPin * pin           = pFilter->pins[i]                    ;
              if ( pin == NULL ) continue                                    ;
              for (m = 0; m < max(1, pin->inputCount); ++m)                  {
                WdmKsDeviceInfo * wdmDeviceInfo = (WdmKsDeviceInfo *)outArgument->deviceInfos[idxDevice] ;
                DeviceInfo      * deviceInfo    = (DeviceInfo      *)wdmDeviceInfo ;
                wchar_t           localCompositeName[MAX_PATH]               ;
                unsigned          nameIndex = 0                              ;
                const BOOL        isInput = (pin->dataFlow == KSPIN_DATAFLOW_OUT) ;
                //////////////////////////////////////////////////////////////
                wdmDeviceInfo->filter = pFilter;
                deviceInfo->structVersion = 2;
                deviceInfo->hostApi = hostApiIndex;
                deviceInfo->name = wdmDeviceInfo->compositeName;
                /* deviceInfo->hostApiSpecificDeviceInfo = &pFilter->devInfo; */
                wdmDeviceInfo->pin = pin->pinId;

                    /* Get the name of the "device" */
                    if (pin->inputs == NULL)
                    {
                        wcsncpy(localCompositeName, pin->friendlyName, MAX_PATH);
                        wdmDeviceInfo->muxPosition = -1;
                        wdmDeviceInfo->endpointPinId = pin->endpointPinId;
                    } else {
                        CaWdmMuxedInput * input = pin->inputs[m] ;
                        wcsncpy(localCompositeName, input->friendlyName, MAX_PATH);
                        wdmDeviceInfo->muxPosition = (int)m;
                        wdmDeviceInfo->endpointPinId = input->endpointPinId;
                    }

                    {
                        /* Get base length */
                        size_t n = wcslen(localCompositeName);

                        /* Check if there are more entries with same name (which might very well be the case), if there
                        are, the name will be postfixed with an index. */
                        nameIndex = GetNameIndex(&nameHash, localCompositeName, isInput);
                        if (nameIndex > 0)
                        {
                            /* This name has multiple instances, so we post fix with a number */
                            n += _snwprintf(localCompositeName + n, MAX_PATH - n, L" %u", nameIndex);
                        }
                        /* Postfix with filter name */
                        _snwprintf(localCompositeName + n, MAX_PATH - n, L" (%s)", pFilter->friendlyName);
                    }

                    /* Convert wide char string to utf-8 */
                    WideCharToMultiByte(CP_UTF8, 0, localCompositeName, -1, wdmDeviceInfo->compositeName, MAX_PATH, NULL, NULL);

                    /* NB! WDM/KS has no concept of a full-duplex device, each pin is either an input or and output */
                    if (isInput)
                    {
                        /* INPUT ! */
                        deviceInfo->maxInputChannels  = pin->maxChannels;
                        deviceInfo->maxOutputChannels = 0;

                        if (outArgument->defaultInputDevice == CaNoDevice)
                        {
                            outArgument->defaultInputDevice = idxDevice;
                        }
                    } else {
                        /* OUTPUT ! */
                        deviceInfo->maxInputChannels  = 0;
                        deviceInfo->maxOutputChannels = pin->maxChannels;

                        if (outArgument->defaultOutputDevice == CaNoDevice)
                        {
                            outArgument->defaultOutputDevice = idxDevice;
                        }
                    }

                    /* These low values are not very useful because
                    * a) The lowest latency we end up with can depend on many factors such
                    *    as the device buffer sizes/granularities, sample rate, channels and format
                    * b) We cannot know the device buffer sizes until we try to open/use it at
                    *    a particular setting
                    * So: we give 512x48000Hz frames as the default low input latency
                    **/
                    switch (pFilter->devInfo.streamingType)
                    {
                    case Type_kWaveCyclic:
                        if (IsEarlierThanVista())
                        {
                            /* XP doesn't tolerate low latency, unless the Process Priority Class is set to REALTIME_PRIORITY_CLASS
                            through SetPriorityClass, then 10 ms is quite feasible. However, one should then bear in mind that ALL of
                            the process is running in REALTIME_PRIORITY_CLASS, which might not be appropriate for an application with
                            a GUI . In this case it is advisable to separate the audio engine in another process and use IPC to communicate
                            with it. */
                            deviceInfo->defaultLowInputLatency = 0.02;
                            deviceInfo->defaultLowOutputLatency = 0.02;
                        }
                        else
                        {
                            /* This is a conservative estimate. Most WaveCyclic drivers will limit the available latency, but f.i. my Edirol
                            PCR-A30 can reach 3 ms latency easily... */
                            deviceInfo->defaultLowInputLatency = 0.01;
                            deviceInfo->defaultLowOutputLatency = 0.01;
                        }
                        deviceInfo->defaultHighInputLatency = (4096.0/48000.0);
                        deviceInfo->defaultHighOutputLatency = (4096.0/48000.0);
                        deviceInfo->defaultSampleRate = (double)(pin->defaultSampleRate);
                        break;
                    case Type_kWaveRT:
                        /* This is also a conservative estimate, based on WaveRT polled mode. In polled mode, the latency will be dictated
                        by the buffer size given by the driver. */
                        deviceInfo->defaultLowInputLatency = 0.01;
                        deviceInfo->defaultLowOutputLatency = 0.01;
                        deviceInfo->defaultHighInputLatency = 0.04;
                        deviceInfo->defaultHighOutputLatency = 0.04;
                        deviceInfo->defaultSampleRate = (double)(pin->defaultSampleRate);
                        break;
              default:
              break;
            }
            /* Add reference to filter */
            FilterAddRef(wdmDeviceInfo->filter);
            ++idxDevice;
          }
        }
        /* If no one has add ref'd the filter, drop it */
        if ( pFilter->filterRefCount == 0 )            {
          FilterFree ( pFilter ) ;
        }
        /* Deinitialize name hash object */
        DeinitNameHashObject(&nameHash);
      }
    }
    *scanResults    = (void *)outArgument                                    ;
    *newDeviceCount = idxDevice                                              ;
    return result                                                            ;
error:
    result = DisposeDeviceInfos(outArgument, totalDeviceCount)               ;
    return result                                                            ;
}

//////////////////////////////////////////////////////////////////////////////

CaError WdmKsHostApi::CommitDeviceInfos (
          CaHostApiIndex index          ,
          void         * scanResults    ,
          int            deviceCount    )
{
  info . deviceCount         = 0                                             ;
  info . defaultInputDevice  = CaNoDevice                                    ;
  info . defaultOutputDevice = CaNoDevice                                    ;
  /* Free any old memory which might be in the device info                  */
  if ( NULL != deviceInfos )                                                 {
    WdmScanDeviceInfosResults * localScanResults                             =
      (WdmScanDeviceInfosResults *)allocations->alloc(sizeof(WdmScanDeviceInfosResults)) ;
    localScanResults->deviceInfos = deviceInfos                              ;
    DisposeDeviceInfos(&localScanResults, info.deviceCount)                  ;
    deviceInfos = NULL                                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != scanResults )                                                 {
    WdmScanDeviceInfosResults *scanDeviceInfosResults = ( WdmScanDeviceInfosResults * ) scanResults;
    if ( deviceCount > 0 )                                                   {
      /* use the array allocated in ScanDeviceInfos() as our deviceInfos    */
      deviceInfos = scanDeviceInfosResults->deviceInfos                      ;
      info . defaultInputDevice  = scanDeviceInfosResults->defaultInputDevice  ;
      info . defaultOutputDevice = scanDeviceInfosResults->defaultOutputDevice ;
      info . deviceCount         = deviceCount                               ;
    }                                                                        ;
    allocations -> free ( scanDeviceInfosResults )                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

CaError WdmKsHostApi::DisposeDeviceInfos(void * scanResults,int deviceCount)
{
  CaRETURN ( CaIsNull( scanResults ) , NoError )                             ;
  WdmScanDeviceInfosResults * scanDeviceInfosResults                         ;
  scanDeviceInfosResults = ( WdmScanDeviceInfosResults * ) scanResults       ;
  if ( CaNotNull ( scanDeviceInfosResults->deviceInfos ) )                   {
    for (int i = 0 ; i < deviceCount ; ++i)                                  {
      WdmKsDeviceInfo * pDevice                                              ;
      pDevice = (WdmKsDeviceInfo *)scanDeviceInfosResults->deviceInfos[i]    ;
      if ( CaAND ( CaNotNull ( pDevice ) , CaNotNull ( pDevice->filter ) ) ) {
        FilterFree ( pDevice -> filter )                                     ;
      }                                                                      ;
    }                                                                        ;
    delete [] scanDeviceInfosResults -> deviceInfos[0]                       ;
    delete [] scanDeviceInfosResults -> deviceInfos                          ;
  }                                                                          ;
  allocations -> free ( scanDeviceInfosResults )                             ;
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

CaError WdmKsHostApi::ValidateSpecificStreamParameters (
          const StreamParameters * streamParameters    ,
          const CaWdmKsInfo      * streamInfo          )
{
  CaRETURN ( CaIsNull ( streamInfo ) , NoError )                             ;
  if ( ( streamInfo->size != sizeof(CaWdmKsInfo)                         )  ||
       ( streamInfo->version != 1                                        ) ) {
    dPrintf ( ( "Stream parameters: size or version not correct" ) )       ;
    return IncompatibleStreamInfo                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( streamInfo->noOfPackets != 0                                     ) &&
     ( ( streamInfo->noOfPackets<2 ) || ( streamInfo->noOfPackets>8 )   ) )  {
    dPrintf ( ( "Stream parameters: noOfPackets %u out of range [2,8]"     ,
                  streamInfo -> noOfPackets                              ) ) ;
    return IncompatibleStreamInfo                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

double WdmKsHostApi::Nearest                 (
         CaWdmPin              * Pin         ,
         double                  SampleRate  ,
         CaSampleFormat          Format      ,
         unsigned                Channels    ,
         CaWaveFormatChannelMask channelMask )
{
  bool                 WaSupported [15]                                      ;
  double               WaSample    [15]                                      ;
  int                  WaID      = 0                                         ;
  int                  Supported = 0                                         ;
  CaError              result    = NoError                                   ;
  WAVEFORMATEXTENSIBLE wfx                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "Looking for %f\n" , SampleRate ) )                            ;
  while ( CaIsGreater ( AllSamplingRates [ WaID ] , 0 ) )                    {
    WaSupported [ WaID ] = false                                             ;
    WaSample    [ WaID ] = -1                                                ;
    //////////////////////////////////////////////////////////////////////////
    CaInitializeWaveFormatExtensible                                         (
      (CaWaveFormat *)&wfx                                                   ,
      Channels                                                               ,
      Format                                                                 ,
      CaSampleFormatToLinearWaveFormatTag ( Format )                         ,
      AllSamplingRates [ WaID ]                                              ,
      channelMask                                                          ) ;
      result = PinIsFormatSupported ( Pin ,(WAVEFORMATEX *) & wfx          ) ;
    if ( CaIsCorrect ( result ) )                                            {
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
    return WaSample [ 0 ]                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "%d formats are supported.\n" , Supported ) )                  ;
  double nearest = NearestSampleRate ( SampleRate , 15 , WaSample )          ;
  ////////////////////////////////////////////////////////////////////////////
  CaRETURN ( ( CaIsGreater ( nearest , 0 ) ) , nearest )                     ;
  return SampleRate                                                          ;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
