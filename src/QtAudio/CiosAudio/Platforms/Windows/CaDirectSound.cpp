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

#include "CaDirectSound.hpp"

#if defined(WIN32) || defined(_WIN32)
#else
#error "Windows Direct Sound works only on Windows platform"
#endif

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

//////////////////////////////////////////////////////////////////////////////

/* provided in newer platform sdks and x64                                  */

#ifndef DWORD_PTR
  #if defined(_WIN64)
  #  define DWORD_PTR unsigned __int64
  #else
  #  define DWORD_PTR unsigned long
  #endif
#endif

#define CA_USE_HIGH_LATENCY   (0)
#if     CA_USE_HIGH_LATENCY
#define CA_DS_WIN_9X_DEFAULT_LATENCY_     (.500)
#define CA_DS_WIN_NT_DEFAULT_LATENCY_     (.600)
#else
#define CA_DS_WIN_9X_DEFAULT_LATENCY_     (.140)
#define CA_DS_WIN_NT_DEFAULT_LATENCY_     (.280)
#endif

#define CA_DS_WIN_WDM_DEFAULT_LATENCY_    (.120)

/* we allow the polling period to range between 1 and 100ms.
   prior to August 2011 we limited the minimum polling period to 10ms.
*/
#define CA_DS_MINIMUM_POLLING_PERIOD_SECONDS    (0.001) /* 1ms */
#define CA_DS_MAXIMUM_POLLING_PERIOD_SECONDS    (0.100) /* 100ms */
#define CA_DS_POLLING_JITTER_SECONDS            (0.001) /* 1ms */

#define SECONDS_PER_MSEC      (0.001)
#define MSECS_PER_SECOND       (1000)

///////////////////////////////////////////////////////////////////////////////

typedef struct                                                                  {
  HINSTANCE hInstance_                                                          ;
  HRESULT (WINAPI * DllGetClassObject           )(REFCLSID                      ,
                                                  REFIID                        ,
                                                  LPVOID                    * ) ;
  HRESULT (WINAPI * DirectSoundCreate           )(LPGUID                        ,
                                                  LPDIRECTSOUND               * ,
                                                  LPUNKNOWN                   ) ;
  HRESULT (WINAPI * DirectSoundEnumerateW       )(LPDSENUMCALLBACKW             ,
                                                  LPVOID                      ) ;
  HRESULT (WINAPI * DirectSoundEnumerateA       )(LPDSENUMCALLBACKA             ,
                                                  LPVOID                      ) ;
  HRESULT (WINAPI * DirectSoundCaptureCreate    )(LPGUID                        ,
                                                  LPDIRECTSOUNDCAPTURE        * ,
                                                  LPUNKNOWN                   ) ;
  HRESULT (WINAPI * DirectSoundCaptureEnumerateW)(LPDSENUMCALLBACKW             ,
                                                  LPVOID                      ) ;
  HRESULT (WINAPI * DirectSoundCaptureEnumerateA)(LPDSENUMCALLBACKA             ,
                                                  LPVOID                      ) ;
  HRESULT (WINAPI * DirectSoundFullDuplexCreate8)(LPCGUID                       ,
                                                  LPCGUID                       ,
                                                  LPCDSCBUFFERDESC              ,
                                                  LPCDSBUFFERDESC               ,
                                                  HWND                          ,
                                                  DWORD                         ,
                                                  LPDIRECTSOUNDFULLDUPLEX     * ,
                                                  LPDIRECTSOUNDCAPTUREBUFFER8 * ,
                                                  LPDIRECTSOUNDBUFFER8        * ,
                                                  LPUNKNOWN                   ) ;
} CaDsDSoundEntryPoints                                                         ;

CaDsDSoundEntryPoints caDsDSoundEntryPoints = { 0 , 0 , 0 , 0 , 0 , 0 , 0 } ;

static HRESULT WINAPI DummyDllGetClassObject(REFCLSID,REFIID,LPVOID*)
{
  return CLASS_E_CLASSNOTAVAILABLE ;
}

static HRESULT WINAPI DummyDirectSoundCreate(LPGUID,LPDIRECTSOUND*,LPUNKNOWN)
{
  return E_NOTIMPL ;
}

static HRESULT WINAPI DummyDirectSoundEnumerateW(LPDSENUMCALLBACKW,LPVOID)
{
  return E_NOTIMPL ;
}

static HRESULT WINAPI DummyDirectSoundEnumerateA(LPDSENUMCALLBACKA,LPVOID)
{
  return E_NOTIMPL ;
}

static HRESULT WINAPI DummyDirectSoundCaptureCreate(LPGUID,LPDIRECTSOUNDCAPTURE*,LPUNKNOWN)
{
  return E_NOTIMPL ;
}

static HRESULT WINAPI DummyDirectSoundCaptureEnumerateW(LPDSENUMCALLBACKW, LPVOID)
{
  return E_NOTIMPL ;
}

static HRESULT WINAPI DummyDirectSoundCaptureEnumerateA(LPDSENUMCALLBACKA,LPVOID)
{
  return E_NOTIMPL ;
}

static HRESULT WINAPI DummyDirectSoundFullDuplexCreate8 (
         LPCGUID                                        ,
         LPCGUID                                        ,
         LPCDSCBUFFERDESC                               ,
         LPCDSBUFFERDESC                                ,
         HWND                                           ,
         DWORD                                          ,
         LPDIRECTSOUNDFULLDUPLEX     *                  ,
         LPDIRECTSOUNDCAPTUREBUFFER8 *                  ,
         LPDIRECTSOUNDBUFFER8        *                  ,
         LPUNKNOWN                                      )
{
  return E_NOTIMPL ;
}

void CaDsInitializeDSoundEntryPoints(void)
{
  caDsDSoundEntryPoints.hInstance_ = ::LoadLibraryA("dsound.dll")             ;
  if ( caDsDSoundEntryPoints.hInstance_ != NULL )                          {
    caDsDSoundEntryPoints . DllGetClassObject                                 =
      (HRESULT (WINAPI *)(REFCLSID, REFIID , LPVOID *))
      ::GetProcAddress( caDsDSoundEntryPoints.hInstance_,"DllGetClassObject") ;
    ///////////////////////////////////////////////////////////////////////////
    if ( caDsDSoundEntryPoints.DllGetClassObject == NULL )
      caDsDSoundEntryPoints.DllGetClassObject = DummyDllGetClassObject;
    ///////////////////////////////////////////////////////////////////////////
    caDsDSoundEntryPoints.DirectSoundCreate =
      (HRESULT (WINAPI *)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN))
      ::GetProcAddress( caDsDSoundEntryPoints.hInstance_, "DirectSoundCreate" ) ;
    ///////////////////////////////////////////////////////////////////////////
    if ( caDsDSoundEntryPoints.DirectSoundCreate == NULL )
      caDsDSoundEntryPoints.DirectSoundCreate = DummyDirectSoundCreate  ;
    ///////////////////////////////////////////////////////////////////////////
    caDsDSoundEntryPoints.DirectSoundEnumerateW =
                (HRESULT (WINAPI *)(LPDSENUMCALLBACKW, LPVOID))
                ::GetProcAddress( caDsDSoundEntryPoints.hInstance_, "DirectSoundEnumerateW" );
    if ( caDsDSoundEntryPoints.DirectSoundEnumerateW == NULL )
         caDsDSoundEntryPoints.DirectSoundEnumerateW = DummyDirectSoundEnumerateW ;
    ///////////////////////////////////////////////////////////////////////////
    caDsDSoundEntryPoints.DirectSoundEnumerateA =
                (HRESULT (WINAPI *)(LPDSENUMCALLBACKA, LPVOID))
                ::GetProcAddress( caDsDSoundEntryPoints.hInstance_, "DirectSoundEnumerateA" );
    if ( caDsDSoundEntryPoints.DirectSoundEnumerateA == NULL )
         caDsDSoundEntryPoints.DirectSoundEnumerateA = DummyDirectSoundEnumerateA ;
    ///////////////////////////////////////////////////////////////////////////
    caDsDSoundEntryPoints.DirectSoundCaptureCreate =
                (HRESULT (WINAPI *)(LPGUID, LPDIRECTSOUNDCAPTURE *, LPUNKNOWN))
                ::GetProcAddress( caDsDSoundEntryPoints.hInstance_, "DirectSoundCaptureCreate" );
    if ( caDsDSoundEntryPoints.DirectSoundCaptureCreate == NULL )
         caDsDSoundEntryPoints.DirectSoundCaptureCreate = DummyDirectSoundCaptureCreate;
    ///////////////////////////////////////////////////////////////////////////
    caDsDSoundEntryPoints.DirectSoundCaptureEnumerateW =
                (HRESULT (WINAPI *)(LPDSENUMCALLBACKW, LPVOID))
                ::GetProcAddress( caDsDSoundEntryPoints.hInstance_, "DirectSoundCaptureEnumerateW" );
    if ( caDsDSoundEntryPoints.DirectSoundCaptureEnumerateW == NULL )
         caDsDSoundEntryPoints.DirectSoundCaptureEnumerateW = DummyDirectSoundCaptureEnumerateW;
    ///////////////////////////////////////////////////////////////////////////
    caDsDSoundEntryPoints.DirectSoundCaptureEnumerateA =
                (HRESULT (WINAPI *)(LPDSENUMCALLBACKA, LPVOID))
                ::GetProcAddress( caDsDSoundEntryPoints.hInstance_, "DirectSoundCaptureEnumerateA" );
    if ( caDsDSoundEntryPoints.DirectSoundCaptureEnumerateA == NULL )
         caDsDSoundEntryPoints.DirectSoundCaptureEnumerateA = DummyDirectSoundCaptureEnumerateA ;
    ///////////////////////////////////////////////////////////////////////////
    caDsDSoundEntryPoints.DirectSoundFullDuplexCreate8 =
                (HRESULT (WINAPI *)(LPCGUID, LPCGUID, LPCDSCBUFFERDESC, LPCDSBUFFERDESC,
                                    HWND, DWORD, LPDIRECTSOUNDFULLDUPLEX *, LPDIRECTSOUNDCAPTUREBUFFER8 *,
                                    LPDIRECTSOUNDBUFFER8 *, LPUNKNOWN))
                ::GetProcAddress( caDsDSoundEntryPoints.hInstance_, "DirectSoundFullDuplexCreate" ) ;
    if ( caDsDSoundEntryPoints.DirectSoundFullDuplexCreate8 == NULL )
         caDsDSoundEntryPoints.DirectSoundFullDuplexCreate8 = DummyDirectSoundFullDuplexCreate8;
  } else                                                                     {
    DWORD errorCode = GetLastError(); // 126 (0x7E) == ERROR_MOD_NOT_FOUND
//        CaDebug(("Couldn't load dsound.dll error code: %d \n",errorCode));
    /* initialize with dummy entry points to make live easy when ds isn't present */
    caDsDSoundEntryPoints . DirectSoundCreate            = DummyDirectSoundCreate            ;
    caDsDSoundEntryPoints . DirectSoundEnumerateW        = DummyDirectSoundEnumerateW        ;
    caDsDSoundEntryPoints . DirectSoundEnumerateA        = DummyDirectSoundEnumerateA        ;
    caDsDSoundEntryPoints . DirectSoundCaptureCreate     = DummyDirectSoundCaptureCreate     ;
    caDsDSoundEntryPoints . DirectSoundCaptureEnumerateW = DummyDirectSoundCaptureEnumerateW ;
    caDsDSoundEntryPoints . DirectSoundCaptureEnumerateA = DummyDirectSoundCaptureEnumerateA ;
    caDsDSoundEntryPoints . DirectSoundFullDuplexCreate8 = DummyDirectSoundFullDuplexCreate8 ;
  } ;
}

void CaDsTerminateDSoundEntryPoints(void)
{
  if ( NULL == caDsDSoundEntryPoints.hInstance_ ) return                  ;
  /* ensure that we crash reliably if the entry points arent initialised */
  caDsDSoundEntryPoints . DirectSoundCreate            = 0                ;
  caDsDSoundEntryPoints . DirectSoundEnumerateW        = 0                ;
  caDsDSoundEntryPoints . DirectSoundEnumerateA        = 0                ;
  caDsDSoundEntryPoints . DirectSoundCaptureCreate     = 0                ;
  caDsDSoundEntryPoints . DirectSoundCaptureEnumerateW = 0                ;
  caDsDSoundEntryPoints . DirectSoundCaptureEnumerateA = 0                ;
  /////////////////////////////////////////////////////////////////////////
  ::FreeLibrary( caDsDSoundEntryPoints.hInstance_ )                       ;
  caDsDSoundEntryPoints . hInstance_                   = NULL             ;
}

///////////////////////////////////////////////////////////////////////////////

/* Set minimal latency based on the current OS version. NT has higher latency. */

static double CaGetMinSystemLatencySeconds(void)
{
  double        minLatencySeconds                                            ;
  OSVERSIONINFO osvi                                                         ;
  /* Set minimal latency based on whether NT or other OS.
   * NT has higher latency.                                                 */
  osvi . dwOSVersionInfoSize = sizeof(osvi)                                  ;
  GetVersionEx ( &osvi )                                                     ;
//    DBUG ( ( "CiosAudio - Platform ID  = 0x%x\n" , osvi.dwPlatformId   ) ) ;
//    DBUG ( ( "CiosAudio - MajorVersion = 0x%x\n" , osvi.dwMajorVersion ) ) ;
//    DBUG ( ( "CiosAudio - MinorVersion = 0x%x\n" , osvi.dwMinorVersion ) ) ;
  /* Check for NT                                                           */
  if ( ( 4 == osvi.dwMajorVersion ) && ( 2 == osvi.dwPlatformId ) )          {
    minLatencySeconds = CA_DS_WIN_NT_DEFAULT_LATENCY_                        ;
  } else
  if ( osvi.dwMajorVersion >= 5 )                                            {
    minLatencySeconds = CA_DS_WIN_WDM_DEFAULT_LATENCY_                       ;
  } else                                                                     {
    minLatencySeconds = CA_DS_WIN_9X_DEFAULT_LATENCY_                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return minLatencySeconds                                                   ;
}

/*************************************************************************
** Return minimum workable latency required for this host. This is returned
** As the default stream latency in PaDeviceInfo.
** Latency can be optionally set by user by setting an environment variable.
** For example, to set latency to 200 msec, put:
**
**    set PA_MIN_LATENCY_MSEC=200
**
** in the AUTOEXEC.BAT file and reboot.
** If the environment variable is not set, then the latency will be determined
** based on the OS. Windows NT has higher latency than Win95.
*/

#define CA_LATENCY_ENV_NAME  ("CIOS_MIN_LATENCY_MSEC")
#define CA_ENV_BUF_SIZE  (32)

static double CaGetMinLatencySeconds(double sampleRate)
{
  char   envbuf [ CA_ENV_BUF_SIZE ] ;
  DWORD  hresult                    ;
  double minLatencySeconds = 0      ;
  ////////////////////////////////////////////////////////////////////////////
  /* Let user determine minimal latency by setting environment variable.    */
  hresult = ::GetEnvironmentVariableA                                        (
                CA_LATENCY_ENV_NAME                                          ,
                envbuf                                                       ,
                CA_ENV_BUF_SIZE                                            ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( hresult > 0 ) && ( hresult < CA_ENV_BUF_SIZE ) )                    {
     minLatencySeconds = ::atoi ( envbuf ) * SECONDS_PER_MSEC                ;
  } else                                                                     {
     minLatencySeconds = CaGetMinSystemLatencySeconds ( )                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return minLatencySeconds                                                   ;
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

//////////////////////////////////////////////////////////////////////////////

/*****************************************************************************
 * DSDeviceNameAndGUID, DSDeviceNameAndGUIDVector used for collecting preliminary
 * information during device enumeration.
 ****************************************************************************/

typedef struct DSDeviceNameAndGUID {
  char       * name                ; // allocated from parent's allocations, never deleted by this structure
  GUID         guid                ;
  LPGUID       lpGUID              ;
  void       * pnpInterface        ; // wchar_t* interface path, allocated using the DS host api's allocation group
}              DSDeviceNameAndGUID ;

typedef struct DSDeviceNameAndGUIDVector {
  AllocationGroup     * allocations      ;
  CaError               enumerationError ;
  int                   count            ;
  int                   free             ;
  DSDeviceNameAndGUID * items            ; // Allocated using LocalAlloc()
} DSDeviceNameAndGUIDVector              ;

typedef struct DSDeviceNamesAndGUIDs            {
  DirectSoundHostApi      * winDsHostApi        ;
  DSDeviceNameAndGUIDVector inputNamesAndGUIDs  ;
  DSDeviceNameAndGUIDVector outputNamesAndGUIDs ;
} DSDeviceNamesAndGUIDs                         ;

//////////////////////////////////////////////////////////////////////////////

static GUID cawin_CLSID_DirectSoundPrivate      = {
  0x11ab3ec0                                      ,
  0x000025ec                                      ,
  0x000011d1                                      ,
  0x000000a4                                      ,
  0x000000d8                                      ,
  0x00000000                                      ,
  0x000000c0                                      ,
  0x0000004f                                      ,
  0x000000c2                                      ,
  0x0000008a                                      ,
  0x000000ca                                    } ;

static GUID cawin_DSPROPSETID_DirectSoundDevice = {
  0x84624f82                                      ,
  0x000025ec                                      ,
  0x000011d1                                      ,
  0x000000a4                                      ,
  0x000000d8                                      ,
  0x00000000                                      ,
  0x000000c0                                      ,
  0x0000004f                                      ,
  0x000000c2                                      ,
  0x0000008a                                      ,
  0x000000ca                                    } ;

static GUID cawin_IID_IKsPropertySet            = {
  0x31efac30                                      ,
  0x0000515c                                      ,
  0x000011d0                                      ,
  0x000000a9                                      ,
  0x000000aa                                      ,
  0x00000000                                      ,
  0x000000aa                                      ,
  0x00000000                                      ,
  0x00000061                                      ,
  0x000000be                                      ,
  0x00000093                                    } ;

/* GUIDs for emulated devices which we blacklist below.
   are there more than two of them??                                        */

GUID IID_IRolandVSCEmulated1 = {
  0xc2ad1800                   ,
  0x0000b243                   ,
  0x000011ce                   ,
  0x000000a8                   ,
  0x000000a4                   ,
  0x00000000                   ,
  0x000000aa                   ,
  0x00000000                   ,
  0x0000006c                   ,
  0x00000045                   ,
  0x00000001                 } ;

GUID IID_IRolandVSCEmulated2 = {
  0xc2ad1800                   ,
  0x0000b243                   ,
  0x000011ce                   ,
  0x000000a8                   ,
  0x000000a4                   ,
  0x00000000                   ,
  0x000000aa                   ,
  0x00000000                   ,
  0x0000006c                   ,
  0x00000045                   ,
  0x00000002                 } ;

#define CA_DEFAULTSAMPLERATESEARCHORDER_COUNT  (14) /* must match array length below */
static double   * defaultSampleRateSearchOrder_ = DefaultSampleRateSearchOrders ;

//////////////////////////////////////////////////////////////////////////////

/************************************************************************************
** Duplicate the input string using the allocations allocator.
** A NULL string is converted to a zero length string.
** If memory cannot be allocated, NULL is returned.
**/

static char * DuplicateDeviceNameString       (
                AllocationGroup * allocations ,
                const char      * src         )
{
  char * result = NULL                                                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != src )                                                         {
    size_t len = ::strlen(src)                                               ;
    result = (char *) allocations -> alloc ( len + 1 )                       ;
    if ( NULL != result ) ::memcpy((void *)result,src,len+1)                 ;
  } else                                                                     {
    result = (char *) allocations -> alloc ( 1       )                       ;
    if ( result ) result[0] = '\0'                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

static void * DuplicateWCharString            (
                AllocationGroup * allocations ,
                wchar_t         * source      )
{
  size_t    len                                                           ;
  wchar_t * result = NULL                                                 ;
  /////////////////////////////////////////////////////////////////////////
  len    = ::wcslen( source )                                             ;
  result = (wchar_t *)allocations->alloc((long)((len+1)*sizeof(wchar_t))) ;
  ::wcscpy ( result, source )                                             ;
  return result                                                           ;
}

//////////////////////////////////////////////////////////////////////////////

static CaError InitializeDSDeviceNameAndGUIDVector       (
                 DSDeviceNameAndGUIDVector * guidVector  ,
                 AllocationGroup           * allocations )
{
  CaError result = NoError                                            ;
  /////////////////////////////////////////////////////////////////////
  guidVector -> allocations      = allocations                        ;
  guidVector -> enumerationError = NoError                            ;
  guidVector -> count            = 0                                  ;
  guidVector -> free             = 8                                  ;
  guidVector -> items            = (DSDeviceNameAndGUID*)::LocalAlloc (
                                      LMEM_FIXED                      ,
                                      sizeof(DSDeviceNameAndGUID)     *
                                      guidVector->free              ) ;
  if ( NULL == guidVector->items ) result = InsufficientMemory        ;
  /////////////////////////////////////////////////////////////////////
  return result                                                       ;
}

static CaError ExpandDSDeviceNameAndGUIDVector(DSDeviceNameAndGUIDVector * guidVector)
{
  CaError               result = NoError                         ;
  DSDeviceNameAndGUID * newItems                                 ;
  int                   i                                        ;
  ////////////////////////////////////////////////////////////////
  /* double size of vector                                      */
  int size = guidVector -> count + guidVector -> free            ;
  guidVector -> free += size                                     ;
  ////////////////////////////////////////////////////////////////
  newItems = (DSDeviceNameAndGUID *)::LocalAlloc                 (
               LMEM_FIXED                                        ,
              sizeof(DSDeviceNameAndGUID) * size * 2           ) ;
  if ( NULL == newItems ) return InsufficientMemory              ;
  ////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < guidVector -> count ; ++i )                  {
    newItems[i].name = guidVector->items[i].name                 ;
    if ( NULL == guidVector->items[i].lpGUID )                   {
      newItems[i].lpGUID = NULL                                  ;
    } else                                                       {
      newItems[i].lpGUID = &newItems[i].guid                     ;
      ::memcpy ( &newItems[i].guid                               ,
                 guidVector->items[i].lpGUID                     ,
                 sizeof(GUID)                                  ) ;
    }                                                            ;
    newItems[i].pnpInterface = guidVector->items[i].pnpInterface ;
  }                                                              ;
  ////////////////////////////////////////////////////////////////
  ::LocalFree ( guidVector->items )                              ;
  guidVector -> items = newItems                                 ;
  ////////////////////////////////////////////////////////////////
  return result                                                  ;
}

/* it's safe to call DSDeviceNameAndGUIDVector multiple times */
static CaError TerminateDSDeviceNameAndGUIDVector(DSDeviceNameAndGUIDVector * guidVector)
{
  CaError result = NoError                        ;
  if ( NULL == guidVector->items ) return result  ;
  /////////////////////////////////////////////////
  if ( NULL != ::LocalFree( guidVector->items ) ) {
    result = InsufficientMemory                   ;
    /** @todo this isn't the correct error to return from a deallocation failure */
  }                                               ;
  guidVector->items = NULL                        ;
  return result                                   ;
}

//////////////////////////////////////////////////////////////////////////////

/* Collect preliminary device information during DirectSound enumeration    */
static BOOL CALLBACK CollectGUIDsProcA    (
                       LPGUID lpGUID      ,
                       LPCSTR lpszDesc    ,
                       LPCSTR lpszName    ,
                       LPVOID lpContext   )
{
  DSDeviceNameAndGUIDVector * namesAndGUIDs                                  ;
  CaError                     error                                          ;
  namesAndGUIDs = (DSDeviceNameAndGUIDVector *) lpContext                    ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == namesAndGUIDs->free )                                            {
    error = ExpandDSDeviceNameAndGUIDVector ( namesAndGUIDs )                ;
    if ( NoError != error )                                                  {
      namesAndGUIDs -> enumerationError = error                              ;
      return FALSE                                                           ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Set GUID pointer, copy GUID to storage in DSDeviceNameAndGUIDVector.   */
  if ( NULL == lpGUID )                                                      {
    namesAndGUIDs->items[namesAndGUIDs->count].lpGUID = NULL                 ;
  } else                                                                     {
    namesAndGUIDs->items[namesAndGUIDs->count].lpGUID                        =
      &namesAndGUIDs->items[namesAndGUIDs->count].guid                       ;
    ::memcpy( &namesAndGUIDs->items[namesAndGUIDs->count].guid               ,
              lpGUID                                                         ,
              sizeof(GUID)                                                 ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  namesAndGUIDs->items[namesAndGUIDs->count].name                            =
    DuplicateDeviceNameString ( namesAndGUIDs->allocations , lpszDesc )      ;
  if ( NULL == namesAndGUIDs->items[namesAndGUIDs->count].name )             {
    namesAndGUIDs->enumerationError = InsufficientMemory                     ;
    return FALSE                                                             ;
  }                                                                          ;
  namesAndGUIDs->items[namesAndGUIDs->count].pnpInterface = NULL             ;
  ////////////////////////////////////////////////////////////////////////////
  ++ ( namesAndGUIDs -> count )                                              ;
  -- ( namesAndGUIDs -> free  )                                              ;
  ////////////////////////////////////////////////////////////////////////////
  return TRUE                                                                ;
}

//////////////////////////////////////////////////////////////////////////////

static BOOL CALLBACK KsPropertySetEnumerateCallback                          (
                     PDSPROPERTY_DIRECTSOUNDDEVICE_DESCRIPTION_W_DATA data   ,
                     LPVOID                                           context)
{
  /* Apparently data->Interface can be NULL in some cases.
     Possibly virtual devices without hardware.                             */
  if ( NULL == data->Interface ) return TRUE                                 ;
  int                     i                                                  ;
  DSDeviceNamesAndGUIDs * deviceNamesAndGUIDs                                ;
  deviceNamesAndGUIDs = (DSDeviceNamesAndGUIDs *) context                    ;
  if ( DIRECTSOUNDDEVICE_DATAFLOW_RENDER == data->DataFlow )                 {
    for ( i = 0 ; i < deviceNamesAndGUIDs->outputNamesAndGUIDs.count ; ++i ) {
      if ( 0 != deviceNamesAndGUIDs->outputNamesAndGUIDs.items[i].lpGUID    &&
           0 == ::memcmp                                                     (
                  &data->DeviceId                                            ,
                  deviceNamesAndGUIDs->outputNamesAndGUIDs.items[i].lpGUID   ,
                  sizeof(GUID) )                                           ) {
        deviceNamesAndGUIDs->outputNamesAndGUIDs.items[i].pnpInterface       =
          (char*)DuplicateWCharString                                        (
                   deviceNamesAndGUIDs->winDsHostApi->allocations            ,
                   data->Interface                                         ) ;
        break                                                                ;
      }                                                                      ;
    }                                                                        ;
  } else
  if ( DIRECTSOUNDDEVICE_DATAFLOW_CAPTURE == data->DataFlow )                {
    for ( i=0 ; i < deviceNamesAndGUIDs->inputNamesAndGUIDs.count ; ++i )    {
      if ( 0 != deviceNamesAndGUIDs->inputNamesAndGUIDs.items[i].lpGUID     &&
           0 == ::memcmp                                                     (
                  &data->DeviceId                                            ,
                  deviceNamesAndGUIDs->inputNamesAndGUIDs.items[i].lpGUID    ,
                  sizeof(GUID) )                                           ) {
        deviceNamesAndGUIDs->inputNamesAndGUIDs.items[i].pnpInterface        =
          (char*)DuplicateWCharString                                        (
                   deviceNamesAndGUIDs->winDsHostApi->allocations            ,
                   data->Interface                                         ) ;
        break                                                                ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return TRUE                                                                ;
}

//////////////////////////////////////////////////////////////////////////////

static void FindDevicePnpInterfaces(DSDeviceNamesAndGUIDs * deviceNamesAndGUIDs)
{
  IClassFactory * pClassFactory = NULL                                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( S_OK == caDsDSoundEntryPoints.DllGetClassObject                       (
         (REFCLSID)cawin_CLSID_DirectSoundPrivate                            ,
         (REFIID  )IID_IClassFactory                                         ,
         (LPVOID *)&pClassFactory )                                        ) {
    IKsPropertySet * pPropertySet = NULL                                     ;
    if ( S_OK == pClassFactory->CreateInstance                               (
                   NULL                                                      ,
                   (REFIID  )cawin_IID_IKsPropertySet                        ,
                   (LPVOID *)&pPropertySet)                                ) {
      DSPROPERTY_DIRECTSOUNDDEVICE_ENUMERATE_W_DATA data                     ;
      ULONG                                         bytesReturned            ;
      ////////////////////////////////////////////////////////////////////////
      data . Callback = KsPropertySetEnumerateCallback                       ;
      data . Context  = deviceNamesAndGUIDs                                  ;
      ////////////////////////////////////////////////////////////////////////
      IKsPropertySet_Get                                                     (
        pPropertySet                                                         ,
        (REFIID)cawin_DSPROPSETID_DirectSoundDevice                          ,
        DSPROPERTY_DIRECTSOUNDDEVICE_ENUMERATE_W                             ,
        NULL                                                                 ,
        0                                                                    ,
        &data                                                                ,
        sizeof(data)                                                         ,
        &bytesReturned                                                     ) ;
      IKsPropertySet_Release ( pPropertySet )                                ;
    }                                                                        ;
    pClassFactory->Release ( )                                               ;
  }                                                                          ;
}

//////////////////////////////////////////////////////////////////////////////

/************************************************************************************
** Extract capabilities from an output device, and add it to the device info list
** if successful. This function assumes that there is enough room in the
** device info list to accomodate all entries.
**
** The device will not be added to the device list if any errors are encountered.
*/
static CaError AddOutputDeviceInfoFromDirectSound  (
                 DirectSoundHostApi * winDsHostApi ,
                 char               * name         ,
                 LPGUID               lpGUID       ,
                 char               * pnpInterface )
{
  HostApi               * hostApi         = (HostApi *)winDsHostApi          ;
  DirectSoundDeviceInfo * winDsDeviceInfo = (DirectSoundDeviceInfo *)hostApi->deviceInfos[hostApi->info.deviceCount] ;
  DeviceInfo            * deviceInfo      = (DeviceInfo            *)winDsDeviceInfo ;
  int                     deviceOK        = TRUE                             ;
  CaError                 result          = NoError                          ;
  HRESULT                 hr                                                 ;
  LPDIRECTSOUND           lpDirectSound                                      ;
  DSCAPS                  caps                                               ;
  int                     i                                                  ;
  ////////////////////////////////////////////////////////////////////////////
  /* Copy GUID to the device info structure. Set pointer.                   */
  if ( NULL == lpGUID )                                                      {
    winDsDeviceInfo->lpGUID = NULL                                           ;
  } else                                                                     {
    ::memcpy( &winDsDeviceInfo->guid, lpGUID, sizeof(GUID) )                 ;
    winDsDeviceInfo->lpGUID = &winDsDeviceInfo->guid                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != lpGUID )                                                      {
    if ( IsEqualGUID ( IID_IRolandVSCEmulated1 , *lpGUID )                  ||
         IsEqualGUID ( IID_IRolandVSCEmulated2 , *lpGUID )                 ) {
     gPrintf ( ( "BLACKLISTED: %s \n" , name ) )                             ;
     return NoError                                                          ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Create a DirectSound object for the specified GUID
      Note that using CoCreateInstance doesn't work on windows CE.          */
  hr = caDsDSoundEntryPoints . DirectSoundCreate                             (
         lpGUID                                                              ,
         &lpDirectSound                                                      ,
         NULL                                                              ) ;
  /* try using CoCreateInstance because DirectSoundCreate was hanging under
        some circumstances - note this was probably related to the
        #define BOOL short bug which has now been fixed
        @todo delete this comment and the following code once we've ensured
        there is no bug.                                                    */
  /*
    hr = CoCreateInstance( &CLSID_DirectSound, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectSound, (void**)&lpDirectSound );

    if ( hr == S_OK )                                                        {
        hr = IDirectSound_Initialize( lpDirectSound, lpGUID )                ;
    }

  */
  if ( DS_OK != hr )                                                         {
    if ( ( DSERR_ALLOCATED == hr ) && ( NULL != globalDebugger ) )           {
      gPrintf ( ( "AddOutputDeviceInfoFromDirectSound %s DSERR_ALLOCATED\n",name) ) ;
    }                                                                        ;
    gPrintf ( ( "Cannot create DirectSound for %s. Result = 0x%x\n", name, hr ) ) ;
    if ( ( NULL != lpGUID ) && ( NULL != globalDebugger ) )                  {
      gPrintf                                                              ( (
        "%s's GUID: {0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x, 0x%x} \n",
        name                                                                 ,
        lpGUID->Data1                                                        ,
        lpGUID->Data2                                                        ,
        lpGUID->Data3                                                        ,
        lpGUID->Data4[0]                                                     ,
        lpGUID->Data4[1]                                                     ,
        lpGUID->Data4[2]                                                     ,
        lpGUID->Data4[3]                                                     ,
        lpGUID->Data4[4]                                                     ,
        lpGUID->Data4[5]                                                     ,
        lpGUID->Data4[6]                                                     ,
        lpGUID->Data4[7]                                                 ) ) ;
    }                                                                        ;
    deviceOK = FALSE                                                         ;
  } else                                                                     {
    /* Query device characteristics.                                        */
    ::memset ( &caps , 0 , sizeof(caps) )                                    ;
    caps.dwSize = sizeof(caps)                                               ;
    hr = IDirectSound_GetCaps ( lpDirectSound , &caps )                      ;
    if ( DS_OK != hr )                                                       {
      gPrintf ( ( "Cannot GetCaps() for DirectSound device %s. Result = 0x%x\n" , name , hr ) ) ;
      deviceOK = FALSE                                                       ;
    } else                                                                   {
      if ( 0 != ( caps.dwFlags & DSCAPS_EMULDRIVER ) )                       {
        /* If WMME supported, then reject Emulated drivers because they are lousy. */
        deviceOK = FALSE                                                     ;
      }                                                                      ;
      if ( TRUE == deviceOK )                                                {
        deviceInfo      -> maxInputChannels               = 0                ;
        winDsDeviceInfo -> deviceInputChannelCountIsKnown = 1                ;
        /* DS output capabilities only indicate supported number of channels
           using two flags which indicate mono and/or stereo.
           We assume that stereo devices may support more than 2 channels
           (as is the case with 5.1 devices for example) and so
            set deviceOutputChannelCountIsKnown to 0 (unknown).
            In this case OpenStream will try to open the device
            when the user requests more than 2 channels, rather than
            returning an error.                                             */
        if ( 0 != ( caps.dwFlags & DSCAPS_PRIMARYSTEREO ) )                  {
          deviceInfo      -> maxOutputChannels               = 2             ;
          winDsDeviceInfo -> deviceOutputChannelCountIsKnown = 0             ;
        } else                                                               {
          deviceInfo      -> maxOutputChannels               = 1             ;
          winDsDeviceInfo -> deviceOutputChannelCountIsKnown = 1             ;
        }                                                                    ;
        /* Guess channels count from speaker configuration. We do it only when
           pnpInterface is NULL or when CAWIN_USE_WDMKS_DEVICE_INFO is undefined.
         */
        if ( NULL == pnpInterface )                                          {
          DWORD spkrcfg                                                      ;
          if ( SUCCEEDED(IDirectSound_GetSpeakerConfig                       (
                           lpDirectSound                                     ,
                           &spkrcfg                 ) )                    ) {
            int count = 0                                                    ;
            //////////////////////////////////////////////////////////////////
            switch (DSSPEAKER_CONFIG(spkrcfg))                               {
              case DSSPEAKER_HEADPHONE        : count = 2 ; break            ;
              case DSSPEAKER_MONO             : count = 1 ; break            ;
              case DSSPEAKER_QUAD             : count = 4 ; break            ;
              case DSSPEAKER_STEREO           : count = 2 ; break            ;
              case DSSPEAKER_SURROUND         : count = 4 ; break            ;
              case DSSPEAKER_5POINT1          : count = 6 ; break            ;
              case DSSPEAKER_7POINT1          : count = 8 ; break            ;
              #ifndef DSSPEAKER_7POINT1_SURROUND
              #define DSSPEAKER_7POINT1_SURROUND 0x00000008
              #endif
              case DSSPEAKER_7POINT1_SURROUND : count = 8 ; break            ;
              #ifndef DSSPEAKER_5POINT1_SURROUND
              #define DSSPEAKER_5POINT1_SURROUND 0x00000009
              #endif
              case DSSPEAKER_5POINT1_SURROUND : count = 6 ; break            ;
            }                                                                ;
            //////////////////////////////////////////////////////////////////
            if ( count > 0 )                                                 {
              deviceInfo      -> maxOutputChannels               = count     ;
              winDsDeviceInfo -> deviceOutputChannelCountIsKnown = 1         ;
            }                                                                ;
          }                                                                  ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        if ( NULL != pnpInterface )                                          {
          int count = CaWdmksQueryFilterMaximumChannelCount                  (
                        pnpInterface                                         ,
                        0                                                  ) ;
          if ( count > 0 )                                                   {
            deviceInfo      -> maxOutputChannels               = count       ;
            winDsDeviceInfo -> deviceOutputChannelCountIsKnown = 1           ;
          }                                                                  ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        /* initialize defaultSampleRate                                     */
        if ( 0 != ( caps.dwFlags & DSCAPS_CONTINUOUSRATE ) )                 {
          /* initialize to caps.dwMaxSecondarySampleRate incase none of the standard rates match */
          deviceInfo->defaultSampleRate = caps.dwMaxSecondarySampleRate      ;
          for ( i = 0 ; i < CA_DEFAULTSAMPLERATESEARCHORDER_COUNT ; ++i )    {
            if ( defaultSampleRateSearchOrder_[i] >= caps.dwMinSecondarySampleRate &&
                 defaultSampleRateSearchOrder_[i] <= caps.dwMaxSecondarySampleRate ) {
              deviceInfo->defaultSampleRate = defaultSampleRateSearchOrder_[i];
              break                                                          ;
            }                                                                ;
          }                                                                  ;
        } else
        if ( caps.dwMinSecondarySampleRate == caps.dwMaxSecondarySampleRate ) {
          if ( 0 == caps.dwMinSecondarySampleRate )                          {
            /* On my Thinkpad 380Z, DirectSoundV6 returns min-max=0 !!
               But it supports continuous sampling.
               So fake range of rates, and hope it really supports it.
             */
            deviceInfo->defaultSampleRate = 48000.0f                         ;
            /* assume 48000 as the default                                  */
            gPrintf ( ( "PA - Reported rates both zero. Setting to fake values for device #%s\n" , name ) ) ;
          } else                                                             {
            deviceInfo->defaultSampleRate = caps.dwMaxSecondarySampleRate    ;
          }                                                                  ;
        } else
        if ( ( caps.dwMinSecondarySampleRate <  1000.0 )                    &&
             ( caps.dwMaxSecondarySampleRate > 50000.0 )                   ) {
          /* The EWS88MT drivers lie, lie, lie. The say they only support two rates, 100 & 100000.
             But we know that they really support a range of rates!
             So when we see a ridiculous set of rates, assume it is a range.
           */
          deviceInfo->defaultSampleRate = 48000.0f                           ; /* assume 48000 as the default */
          gPrintf ( ( "PA - Sample rate range used instead of two odd values for device #%s\n" , name ) ) ;
        } else                                                               {
          deviceInfo->defaultSampleRate = caps.dwMaxSecondarySampleRate      ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        // printf( "min %d max %d\n", caps.dwMinSecondarySampleRate, caps.dwMaxSecondarySampleRate );
        // dwFlags | DSCAPS_CONTINUOUSRATE
        deviceInfo->defaultLowInputLatency   = 0.0f                          ;
        deviceInfo->defaultHighInputLatency  = 0.0f                          ;
        deviceInfo->defaultLowOutputLatency  = CaGetMinLatencySeconds(deviceInfo->defaultSampleRate) ;
        deviceInfo->defaultHighOutputLatency = deviceInfo->defaultLowOutputLatency * 2 ;
      }                                                                      ;
    }                                                                        ;
    IDirectSound_Release ( lpDirectSound )                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( TRUE == deviceOK )                                                    {
    deviceInfo->name = name                                                  ;
    if ( NULL == lpGUID )                                                    {
      hostApi->info.defaultOutputDevice = hostApi->info.deviceCount          ;
    }                                                                        ;
    (hostApi->info.deviceCount)++                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

/************************************************************************************
** Extract capabilities from an input device, and add it to the device info list
** if successful. This function assumes that there is enough room in the
** device info list to accomodate all entries.
**
** The device will not be added to the device list if any errors are encountered.
*/
static CaError AddInputDeviceInfoFromDirectSoundCapture (
                 DirectSoundHostApi * winDsHostApi      ,
                 char               * name              ,
                 LPGUID               lpGUID            ,
                 char               * pnpInterface      )
{
  HostApi               * hostApi         = (HostApi *)winDsHostApi          ;
  DirectSoundDeviceInfo * winDsDeviceInfo = (DirectSoundDeviceInfo *)hostApi->deviceInfos[hostApi->info.deviceCount] ;
  DeviceInfo            * deviceInfo      = (DeviceInfo *)winDsDeviceInfo    ;
  int                     deviceOK        = TRUE                             ;
  CaError                 result          = NoError                          ;
  HRESULT                 hr                                                 ;
  LPDIRECTSOUNDCAPTURE    lpDirectSoundCapture                               ;
  DSCCAPS                 caps                                               ;
  ////////////////////////////////////////////////////////////////////////////
  /* Copy GUID to the device info structure. Set pointer.                   */
  if ( NULL == lpGUID )                                                      {
    winDsDeviceInfo->lpGUID = NULL                                           ;
  } else                                                                     {
    winDsDeviceInfo->lpGUID = &winDsDeviceInfo->guid                         ;
    ::memcpy ( &winDsDeviceInfo->guid, lpGUID, sizeof(GUID) )                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  hr = caDsDSoundEntryPoints . DirectSoundCaptureCreate                      (
         lpGUID                                                              ,
         &lpDirectSoundCapture                                               ,
         NULL                                                              ) ;
  /* try using CoCreateInstance because DirectSoundCreate was hanging under
     some circumstances - note this was probably related to the
     #define BOOL short bug which has now been fixed
     @todo delete this comment and the following code once we've ensured
     there is no bug.                                                       */
  /*
    hr = CoCreateInstance( &CLSID_DirectSoundCapture, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectSoundCapture, (void**)&lpDirectSoundCapture );
  */
  if ( DS_OK != hr )                                                         {
    gPrintf ( ( "Cannot create Capture for %s. Result = 0x%x\n" , name , hr ) ) ;
    deviceOK = FALSE                                                         ;
  } else                                                                     {
    /* Query device characteristics.                                        */
    ::memset ( &caps, 0, sizeof(caps) )                                      ;
    caps.dwSize = sizeof(caps)                                               ;
    hr = IDirectSoundCapture_GetCaps ( lpDirectSoundCapture, &caps )         ;
    if ( DS_OK != hr )                                                       {
      gPrintf ( ( "Cannot GetCaps() for Capture device %s. Result = 0x%x\n" , name , hr ) ) ;
      deviceOK = FALSE                                                       ;
    } else                                                                   {
      if ( 0 != ( caps.dwFlags & DSCAPS_EMULDRIVER ) )                       {
        /* If WMME supported, then reject Emulated drivers because they are lousy. */
        deviceOK = FALSE                                                     ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      if ( TRUE == deviceOK )                                                {
        deviceInfo      -> maxInputChannels                = caps.dwChannels ;
        winDsDeviceInfo -> deviceInputChannelCountIsKnown  = 1               ;
        deviceInfo      -> maxOutputChannels               = 0               ;
        winDsDeviceInfo -> deviceOutputChannelCountIsKnown = 1               ;
        //////////////////////////////////////////////////////////////////////
        if ( NULL != pnpInterface )                                          {
          int count = CaWdmksQueryFilterMaximumChannelCount                  (
                        pnpInterface                                         ,
                        1                                                  ) ;
          if ( count > 0 )                                                   {
            deviceInfo      -> maxInputChannels               = count        ;
            winDsDeviceInfo -> deviceInputChannelCountIsKnown = 1            ;
          }                                                                  ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        #ifndef WAVE_FORMAT_48M08
        #define WAVE_FORMAT_48M08 0x00001000 /* 48     kHz, Mono,   8-bit   */
        #define WAVE_FORMAT_48S08 0x00002000 /* 48     kHz, Stereo, 8-bit   */
        #define WAVE_FORMAT_48M16 0x00004000 /* 48     kHz, Mono,   16-bit  */
        #define WAVE_FORMAT_48S16 0x00008000 /* 48     kHz, Stereo, 16-bit  */
        #define WAVE_FORMAT_96M08 0x00010000 /* 96     kHz, Mono,   8-bit   */
        #define WAVE_FORMAT_96S08 0x00020000 /* 96     kHz, Stereo, 8-bit   */
        #define WAVE_FORMAT_96M16 0x00040000 /* 96     kHz, Mono,   16-bit  */
        #define WAVE_FORMAT_96S16 0x00080000 /* 96     kHz, Stereo, 16-bit  */
        #endif
        //////////////////////////////////////////////////////////////////////
        /* defaultSampleRate                                                */
        if ( 2 == caps.dwChannels )                                          {
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_4S16  ) )                 {
            deviceInfo->defaultSampleRate = 44100.0                          ;
          } else
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_48S16 ) )                 {
            deviceInfo->defaultSampleRate = 48000.0                          ;
          } else
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_2S16  ) )                 {
            deviceInfo->defaultSampleRate = 22050.0                          ;
          } else
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_1S16  ) )                 {
            deviceInfo->defaultSampleRate = 11025.0                          ;
          } else
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_96S16 ) )                 {
            deviceInfo->defaultSampleRate = 96000.0                          ;
          } else                                                             {
            /* assume 48000 as the default                                  */
            deviceInfo->defaultSampleRate = 48000.0                          ;
          }                                                                  ;
        } else
        if ( 1 == caps.dwChannels )                                          {
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_4M16  ) )                 {
            deviceInfo->defaultSampleRate = 44100.0                          ;
          } else
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_48M16 ) )                 {
            deviceInfo->defaultSampleRate = 48000.0                          ;
          } else
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_2M16  ) )                 {
            deviceInfo->defaultSampleRate = 22050.0                          ;
          } else
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_1M16  ) )                 {
            deviceInfo->defaultSampleRate = 11025.0                          ;
          } else
          if ( 0 != ( caps.dwFormats & WAVE_FORMAT_96M16 ) )                 {
            deviceInfo->defaultSampleRate = 96000.0                          ;
          } else                                                             {
            /* assume 48000 as the default                                  */
            deviceInfo->defaultSampleRate = 48000.0                          ;
          }                                                                  ;
        } else                                                               {
           /* assume 48000 as the default                                   */
          deviceInfo->defaultSampleRate = 48000.0                            ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        deviceInfo->defaultLowOutputLatency  = 0.0                           ;
        deviceInfo->defaultHighOutputLatency = 0.0                           ;
        deviceInfo->defaultLowInputLatency   = CaGetMinLatencySeconds( deviceInfo->defaultSampleRate ) ;
        deviceInfo->defaultHighInputLatency  = deviceInfo->defaultLowInputLatency * 2 ;
      }                                                                      ;
    }                                                                        ;
    IDirectSoundCapture_Release ( lpDirectSoundCapture )                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( TRUE == deviceOK )                                                    {
    deviceInfo->name = name                                                  ;
    if ( NULL == lpGUID )                                                    {
      hostApi->info.defaultInputDevice = hostApi->info.deviceCount           ;
    }                                                                        ;
    (hostApi->info.deviceCount)++                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

static CaError ValidateWinDirectSoundSpecificStreamInfo  (
        const StreamParameters        * streamParameters ,
        const CaDirectSoundStreamInfo * streamInfo       )
{
  if ( NULL == streamInfo ) return NoError                                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( sizeof(CaDirectSoundStreamInfo) != streamInfo->size                  ||
       streamInfo->version != 2                                            ) {
    return IncompatibleStreamInfo                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( streamInfo->flags & CaWinDirectSoundUseLowLevelLatencyParameters )    {
    if ( streamInfo->framesPerBuffer <= 0 )                                  {
      return IncompatibleStreamInfo                           ;
    }                                                                        ;
  }                                                                          ;
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

static HRESULT InitFullDuplexInputOutputBuffers                 (
                 DirectSoundStream     * stream                 ,
                 DirectSoundDeviceInfo * inputDevice            ,
                 CaSampleFormat          hostInputSampleFormat  ,
                 WORD                    inputChannelCount      ,
                 int                     bytesPerInputBuffer    ,
                 CaWaveFormatChannelMask inputChannelMask       ,
                 DirectSoundDeviceInfo * outputDevice           ,
                 CaSampleFormat          hostOutputSampleFormat ,
                 WORD                    outputChannelCount     ,
                 int                     bytesPerOutputBuffer   ,
                 CaWaveFormatChannelMask outputChannelMask      ,
                 unsigned long           nFrameRate             )
{
  HRESULT                     hr                                             ;
  DSCBUFFERDESC               captureDesc                                    ;
  CaWaveFormat                captureWaveFormat                              ;
  DSBUFFERDESC                secondaryRenderDesc                            ;
  CaWaveFormat                renderWaveFormat                               ;
  LPDIRECTSOUNDBUFFER8        pRenderBuffer8                                 ;
  LPDIRECTSOUNDCAPTUREBUFFER8 pCaptureBuffer8                                ;
  ////////////////////////////////////////////////////////////////////////////
  /* capture buffer description                                             */
  /* only try wave format extensible. assume it's available on all ds 8 systems */
  CaInitializeWaveFormatExtensible                                           (
    &captureWaveFormat                                                       ,
    inputChannelCount                                                        ,
    hostInputSampleFormat                                                    ,
    CaSampleFormatToLinearWaveFormatTag ( hostInputSampleFormat )            ,
    nFrameRate                                                               ,
    inputChannelMask                                                       ) ;
  ////////////////////////////////////////////////////////////////////////////
  ZeroMemory ( &captureDesc , sizeof(DSCBUFFERDESC) )                        ;
  captureDesc . dwSize        = sizeof(DSCBUFFERDESC)                        ;
  captureDesc . dwFlags       = 0                                            ;
  captureDesc . dwBufferBytes = bytesPerInputBuffer                          ;
  captureDesc . lpwfxFormat   = (WAVEFORMATEX*)&captureWaveFormat            ;
  /* render buffer description                                              */
  CaInitializeWaveFormatExtensible                                           (
    &renderWaveFormat                                                        ,
    outputChannelCount                                                       ,
    hostOutputSampleFormat                                                   ,
    CaSampleFormatToLinearWaveFormatTag ( hostOutputSampleFormat )           ,
    nFrameRate                                                               ,
    outputChannelMask                                                      ) ;
  ////////////////////////////////////////////////////////////////////////////
  ZeroMemory ( &secondaryRenderDesc , sizeof(DSBUFFERDESC) )                 ;
  secondaryRenderDesc . dwSize        = sizeof(DSBUFFERDESC)                 ;
  secondaryRenderDesc . dwFlags       = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 ;
  secondaryRenderDesc . dwBufferBytes = bytesPerOutputBuffer                 ;
  secondaryRenderDesc . lpwfxFormat   = (WAVEFORMATEX*)&renderWaveFormat     ;
  /* note that we don't create a primary buffer here at all                 */
  hr = caDsDSoundEntryPoints . DirectSoundFullDuplexCreate8                  (
            inputDevice  -> lpGUID                                           ,
            outputDevice -> lpGUID                                           ,
            &captureDesc                                                     ,
            &secondaryRenderDesc                                             ,
            GetDesktopWindow()                                               ,
            DSSCL_EXCLUSIVE                                                  ,
            &stream->pDirectSoundFullDuplex8                                 ,
            &pCaptureBuffer8                                                 ,
            &pRenderBuffer8                                                  ,
            NULL                                  /* pUnkOuter must be NULL */
       )                                                                     ;
  /* see InitOutputBuffer() for a discussion of whether this is a good idea */
  if ( DS_OK == hr )                                                         {
    gPrintf ( ( "DirectSoundFullDuplexCreate succeeded!\n" ) )               ;
    /* retrieve the pre ds 8 buffer interfaces which are used by the rest of the code */
    hr = IUnknown_QueryInterface                                             (
           pCaptureBuffer8                                                   ,
           IID_IDirectSoundCaptureBuffer                                     ,
           (LPVOID *)&stream->pDirectSoundInputBuffer                      ) ;
    if ( DS_OK == hr )                                                       {
      hr = IUnknown_QueryInterface                                           (
             pRenderBuffer8                                                  ,
             IID_IDirectSoundBuffer                                          ,
             (LPVOID *)&stream->pDirectSoundOutputBuffer                   ) ;
    }                                                                        ;
    /* release the ds 8 interfaces, we don't need them                      */
    IUnknown_Release ( pCaptureBuffer8 )                                     ;
    IUnknown_Release ( pRenderBuffer8  )                                     ;
    //////////////////////////////////////////////////////////////////////////
    if ( NULL == stream->pDirectSoundInputBuffer                            ||
         NULL == stream->pDirectSoundOutputBuffer                          ) {
      /* couldn't get pre ds 8 interfaces for some reason. clean up.        */
      if ( NULL != stream->pDirectSoundInputBuffer )                         {
        IUnknown_Release ( stream->pDirectSoundInputBuffer )                 ;
        stream->pDirectSoundInputBuffer = NULL                               ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      if ( NULL != stream->pDirectSoundOutputBuffer )                        {
        IUnknown_Release ( stream->pDirectSoundOutputBuffer )                ;
        stream->pDirectSoundOutputBuffer = NULL                              ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      IUnknown_Release ( stream->pDirectSoundFullDuplex8 )                   ;
      stream->pDirectSoundFullDuplex8 = NULL                                 ;
    }                                                                        ;
  } else                                                                     {
    gPrintf ( ( "DirectSoundFullDuplexCreate failed. hr=%d\n" , hr ) )       ;
  }                                                                          ;
  return hr                                                                  ;
}

static HRESULT InitInputBuffer                          (
                 DirectSoundStream     * stream         ,
                 DirectSoundDeviceInfo * device         ,
                 CaSampleFormat          sampleFormat   ,
                 unsigned long           nFrameRate     ,
                 WORD                    nChannels      ,
                 int                     bytesPerBuffer ,
                 CaWaveFormatChannelMask channelMask    )
{
  DSCBUFFERDESC captureDesc                                                  ;
  CaWaveFormat  waveFormat                                                   ;
  HRESULT       result                                                       ;
  ////////////////////////////////////////////////////////////////////////////
  result = caDsDSoundEntryPoints . DirectSoundCaptureCreate                  (
              device  -> lpGUID                                              ,
              &stream -> pDirectSoundCapture                                 ,
              NULL                                                         ) ;
  if ( DS_OK != result )                                                     {
    gPrintf ( ( "CIOS Audio: DirectSoundCaptureCreate() failed!\n" ) )       ;
    return result                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Setup the secondary buffer description                                 */
  ZeroMemory ( &captureDesc , sizeof(DSCBUFFERDESC) )                        ;
  captureDesc . dwSize        = sizeof(DSCBUFFERDESC)                        ;
  captureDesc . dwFlags       = 0                                            ;
  captureDesc . dwBufferBytes = bytesPerBuffer                               ;
  captureDesc . lpwfxFormat   = (WAVEFORMATEX*)&waveFormat                   ;
  ////////////////////////////////////////////////////////////////////////////
  /* Create the capture buffer                                              */
  /* first try WAVEFORMATEXTENSIBLE. if this fails, fall back to WAVEFORMATEX */
  CaInitializeWaveFormatExtensible                                           (
    &waveFormat                                                              ,
    nChannels                                                                ,
    sampleFormat                                                             ,
    CaSampleFormatToLinearWaveFormatTag ( sampleFormat )                     ,
    nFrameRate                                                               ,
    channelMask                                                            ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( DS_OK != IDirectSoundCapture_CreateCaptureBuffer                      (
                  stream->pDirectSoundCapture                                ,
                  &captureDesc                                               ,
                  &stream->pDirectSoundInputBuffer                           ,
                  NULL                                                   ) ) {
    CaInitializeWaveFormatEx                                                 (
      &waveFormat                                                            ,
      nChannels                                                              ,
      sampleFormat                                                           ,
      CaSampleFormatToLinearWaveFormatTag ( sampleFormat )                   ,
      nFrameRate                                                           ) ;
    result = IDirectSoundCapture_CreateCaptureBuffer                         (
               stream->pDirectSoundCapture                                   ,
               &captureDesc                                                  ,
               &stream->pDirectSoundInputBuffer                              ,
               NULL                                                        ) ;
    if ( DS_OK != result ) return result                                     ;
  }                                                                          ;
  /* reset last read position to start of buffer                            */
  stream->readOffset = 0                                                     ;
  ////////////////////////////////////////////////////////////////////////////
  return DS_OK                                                               ;
}

static HRESULT InitOutputBuffer                         (
                 DirectSoundStream     * stream         ,
                 DirectSoundDeviceInfo * device         ,
                 CaSampleFormat          sampleFormat   ,
                 unsigned long           nFrameRate     ,
                 WORD                    nChannels      ,
                 int                     bytesPerBuffer ,
                 CaWaveFormatChannelMask channelMask    )
{
  HRESULT      result                                                        ;
  HWND         hWnd                                                          ;
  HRESULT      hr                                                            ;
  CaWaveFormat waveFormat                                                    ;
  DSBUFFERDESC primaryDesc                                                   ;
  DSBUFFERDESC secondaryDesc                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  hr = caDsDSoundEntryPoints . DirectSoundCreate                             (
         device->lpGUID                                                      ,
         &stream->pDirectSound                                               ,
         NULL                                                              ) ;
  if ( DS_OK != hr )                                                         {
    gPrintf ( ( "CIOS Audio: DirectSoundCreate() failed!\n" ) )              ;
    return hr                                                                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // We were using getForegroundWindow() but sometimes the ForegroundWindow may not be the
  // applications's window. Also if that window is closed before the Buffer is closed
  // then DirectSound can crash. (Thanks for Scott Patterson for reporting this.)
  // So we will use GetDesktopWindow() which was suggested by Miller Puckette.
  // hWnd = GetForegroundWindow();
  //
  //  FIXME: The example code I have on the net creates a hidden window that
  //      is managed by our code - I think we should do that - one hidden
  //      window for the whole of Pa_DS
  ////////////////////////////////////////////////////////////////////////////
  hWnd = ::GetDesktopWindow ( )                                              ;
  // Set cooperative level to DSSCL_EXCLUSIVE so that we can get 16 bit output, 44.1 KHz.
  // exclusive also prevents unexpected sounds from other apps during a performance.
  hr = IDirectSound_SetCooperativeLevel                                      (
         stream->pDirectSound                                                ,
         hWnd                                                                ,
         DSSCL_EXCLUSIVE                                                   ) ;
  if ( DS_OK != hr ) return hr                                               ;
  // -----------------------------------------------------------------------
  // Create primary buffer and set format just so we can specify our custom format.
  // Otherwise we would be stuck with the default which might be 8 bit or 22050 Hz.
  // Setup the primary buffer description
  ZeroMemory ( &primaryDesc , sizeof(DSBUFFERDESC) )                         ;
  primaryDesc . dwSize        = sizeof(DSBUFFERDESC)                         ;
  primaryDesc . dwFlags       = DSBCAPS_PRIMARYBUFFER                        ; // all panning, mixing, etc done by synth
  primaryDesc . dwBufferBytes = 0                                            ;
  primaryDesc . lpwfxFormat   = NULL                                         ;
  /* Create the buffer                                                      */
  result = IDirectSound_CreateSoundBuffer                                    (
             stream->pDirectSound                                            ,
             &primaryDesc                                                    ,
             &stream->pDirectSoundPrimaryBuffer                              ,
             NULL                                                          ) ;
  if ( DS_OK != result ) goto error                                          ;
  /* Set the primary buffer's format                                        */
  /* first try WAVEFORMATEXTENSIBLE. if this fails, fall back to WAVEFORMATEX */
  CaInitializeWaveFormatExtensible                                           (
    &waveFormat                                                              ,
    nChannels                                                                ,
    sampleFormat                                                             ,
    CaSampleFormatToLinearWaveFormatTag ( sampleFormat )                     ,
    nFrameRate                                                               ,
    channelMask                                                            ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( DS_OK != IDirectSoundBuffer_SetFormat                                 (
                  stream->pDirectSoundPrimaryBuffer                          ,
                  (WAVEFORMATEX*)&waveFormat )                             ) {
    CaInitializeWaveFormatEx                                                 (
      &waveFormat                                                            ,
      nChannels                                                              ,
      sampleFormat                                                           ,
      CaSampleFormatToLinearWaveFormatTag ( sampleFormat )                   ,
      nFrameRate                                                           ) ;
    result = IDirectSoundBuffer_SetFormat                                    (
               stream->pDirectSoundPrimaryBuffer                             ,
               (WAVEFORMATEX*)&waveFormat                                  ) ;
      if ( DS_OK != result ) goto error                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Setup the secondary buffer description                                 */
  ZeroMemory ( &secondaryDesc , sizeof(DSBUFFERDESC) )                       ;
  secondaryDesc . dwSize        = sizeof(DSBUFFERDESC)                       ;
  secondaryDesc . dwFlags       = DSBCAPS_GLOBALFOCUS                        |
                                  DSBCAPS_GETCURRENTPOSITION2                |
                                  DSBCAPS_CTRLVOLUME                         |
                                  DSBCAPS_CTRLPAN                            |
                                  DSBCAPS_CTRLPOSITIONNOTIFY                 ;
  secondaryDesc . dwBufferBytes = bytesPerBuffer                             ;
  secondaryDesc . lpwfxFormat   = (WAVEFORMATEX*)&waveFormat                 ;
  /* waveFormat contains whatever format was negotiated for the primary buffer above */
  /* Create the secondary buffer                                            */
  result = IDirectSound_CreateSoundBuffer                                    (
             stream->pDirectSound                                            ,
             &secondaryDesc                                                  ,
             &stream->pDirectSoundOutputBuffer                               ,
             NULL                                                          ) ;
  if ( DS_OK != result ) goto error                                          ;
  return DS_OK                                                               ;
error:
  if ( NULL != stream->pDirectSoundPrimaryBuffer )                           {
    IDirectSoundBuffer_Release ( stream->pDirectSoundPrimaryBuffer )         ;
    stream->pDirectSoundPrimaryBuffer = NULL                                 ;
  }                                                                          ;
  return result                                                              ;
}

static void CalculateBufferSettings                        (
              unsigned long * hostBufferSizeFrames         ,
              unsigned long * pollingPeriodFrames          ,
              int             isFullDuplex                 ,
              unsigned long   suggestedInputLatencyFrames  ,
              unsigned long   suggestedOutputLatencyFrames ,
              double          sampleRate                   ,
              unsigned long   userFramesPerBuffer          )
{
  unsigned long minimumPollingPeriodFrames = (unsigned long)( sampleRate * CA_DS_MINIMUM_POLLING_PERIOD_SECONDS ) ;
  unsigned long maximumPollingPeriodFrames = (unsigned long)( sampleRate * CA_DS_MAXIMUM_POLLING_PERIOD_SECONDS ) ;
  unsigned long pollingJitterFrames        = (unsigned long)( sampleRate * CA_DS_POLLING_JITTER_SECONDS         ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( Stream::FramesPerBufferUnspecified == userFramesPerBuffer )           {
    unsigned long targetBufferingLatencyFrames = max                         (
                    suggestedInputLatencyFrames                              ,
                    suggestedOutputLatencyFrames                           ) ;
    *pollingPeriodFrames = targetBufferingLatencyFrames / 4                  ;
    if ( *pollingPeriodFrames < minimumPollingPeriodFrames )                 {
      *pollingPeriodFrames = minimumPollingPeriodFrames                      ;
    } else
    if ( *pollingPeriodFrames > maximumPollingPeriodFrames )                 {
      *pollingPeriodFrames = maximumPollingPeriodFrames                      ;
    }                                                                        ;
    *hostBufferSizeFrames = *pollingPeriodFrames                             +
                            max ( *pollingPeriodFrames + pollingJitterFrames ,
                                  targetBufferingLatencyFrames             ) ;
  } else                                                                      {
    unsigned long targetBufferingLatencyFrames = suggestedInputLatencyFrames  ;
    if ( 0 != isFullDuplex )                                                  {
      /* In full duplex streams we know that the buffer adapter adds userFramesPerBuffer
         extra fixed latency. so we subtract it here as a fixed latency before computing
         the buffer size. being careful not to produce an unrepresentable negative result.

         Note: this only works as expected if output latency is greater than input latency.
         Otherwise we use input latency anyway since we do max(in,out).
       */
      if ( userFramesPerBuffer < suggestedOutputLatencyFrames )              {
        unsigned long adjustedSuggestedOutputLatencyFrames                   =
                        suggestedOutputLatencyFrames - userFramesPerBuffer   ;
        /* maximum of input and adjusted output suggested latency           */
        if ( adjustedSuggestedOutputLatencyFrames > targetBufferingLatencyFrames ) {
          targetBufferingLatencyFrames = adjustedSuggestedOutputLatencyFrames ;
        }                                                                    ;
      }                                                                      ;
    } else                                                                   {
      /* maximum of input and output suggested latency                      */
      if ( suggestedOutputLatencyFrames > suggestedInputLatencyFrames )      {
        targetBufferingLatencyFrames = suggestedOutputLatencyFrames          ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    *hostBufferSizeFrames = userFramesPerBuffer                              +
                            max ( userFramesPerBuffer + pollingJitterFrames  ,
                                  targetBufferingLatencyFrames             ) ;
    *pollingPeriodFrames  = max( max ( 1 , userFramesPerBuffer /  4        ) ,
                                 targetBufferingLatencyFrames  / 16        ) ;
    if ( *pollingPeriodFrames > maximumPollingPeriodFrames )                 {
      *pollingPeriodFrames = maximumPollingPeriodFrames                      ;
    }                                                                        ;
  }                                                                          ;
}

static void CalculatePollingPeriodFrames           (
              unsigned long   hostBufferSizeFrames ,
              unsigned long * pollingPeriodFrames  ,
              double          sampleRate           ,
              unsigned long   userFramesPerBuffer  )
{
  unsigned long minimumPollingPeriodFrames = (unsigned long)(sampleRate * CA_DS_MINIMUM_POLLING_PERIOD_SECONDS) ;
  unsigned long maximumPollingPeriodFrames = (unsigned long)(sampleRate * CA_DS_MAXIMUM_POLLING_PERIOD_SECONDS) ;
  unsigned long pollingJitterFrames        = (unsigned long)(sampleRate * CA_DS_POLLING_JITTER_SECONDS        ) ;
  *pollingPeriodFrames = max ( max ( 1 , userFramesPerBuffer / 4 )           ,
                               hostBufferSizeFrames / 16                   ) ;
  if ( *pollingPeriodFrames > maximumPollingPeriodFrames )                   {
    *pollingPeriodFrames = maximumPollingPeriodFrames                        ;
  }                                                                          ;
}

static void SetStreamInfoLatencies                    (
              DirectSoundStream * stream              ,
              unsigned long       userFramesPerBuffer ,
              unsigned long       pollingPeriodFrames ,
              double              sampleRate          )
{
  /* compute the stream info actual latencies based on framesPerBuffer, polling period, hostBufferSizeFrames,
     and the configuration of the buffer processor                          */
  unsigned long effectiveFramesPerBuffer                                     =
                 (userFramesPerBuffer == Stream::FramesPerBufferUnspecified) ?
                                           pollingPeriodFrames               :
                                           userFramesPerBuffer               ;
  ////////////////////////////////////////////////////////////////////////////
  if ( stream->bufferProcessor.inputChannelCount > 0 )                       {
    /* stream info input latency is the minimum buffering latency
       (unlike suggested and default which are *maximums*)                  */
    stream->inputLatency = ( stream->bufferProcessor.InputLatencyFrames()    +
                             effectiveFramesPerBuffer ) / sampleRate         ;
  } else                                                                     {
    stream->inputLatency = 0                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( stream->bufferProcessor.outputChannelCount > 0 )                      {
    stream->outputLatency                                                    =
      (double)( stream->bufferProcessor.OutputLatencyFrames()                +
              ( stream->hostBufferSizeFrames - effectiveFramesPerBuffer)   ) /
                sampleRate                                                   ;
  } else                                                                     {
    stream -> outputLatency = 0                                              ;
  }                                                                          ;
}

/************************************************************************************
 * Determine how much space can be safely written to in DS buffer.
 * Detect underflows and overflows.
 * Does not allow writing into safety gap maintained by DirectSound.
 */
static HRESULT QueryOutputSpace                 (
                 DirectSoundStream * stream     ,
                 long              * bytesEmpty )
{
  HRESULT hr                                                                 ;
  DWORD   playCursor                                                         ;
  DWORD   writeCursor                                                        ;
  long    numBytesEmpty                                                      ;
  long    playWriteGap                                                       ;
  ////////////////////////////////////////////////////////////////////////////
  /* Query to see how much room is in buffer.                               */
  hr = IDirectSoundBuffer_GetCurrentPosition                                 (
         stream->pDirectSoundOutputBuffer                                    ,
         &playCursor                                                         ,
         &writeCursor                                                      ) ;
  if ( hr != DS_OK ) return hr                                               ;
  ////////////////////////////////////////////////////////////////////////////
  // Determine size of gap between playIndex and WriteIndex that we cannot write into.
  playWriteGap = writeCursor - playCursor                                    ;
  if ( playWriteGap < 0 )                                                    {
    playWriteGap += stream->outputBufferSizeBytes                            ; // unwrap
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* DirectSound doesn't have a large enough playCursor so we cannot detect wrap-around. */
  /* Attempt to detect playCursor wrap-around and correct it.               */
  if ( stream->outputIsRunning                                              &&
     ( stream->perfCounterTicksPerBuffer.QuadPart != 0 )                   ) {
    /* How much time has elapsed since last check.                          */
    LARGE_INTEGER currentTime                                                ;
    LARGE_INTEGER elapsedTime                                                ;
    long          bytesPlayed                                                ;
    long          bytesExpected                                              ;
    long          buffersWrapped                                             ;
    //////////////////////////////////////////////////////////////////////////
    QueryPerformanceCounter ( &currentTime )                                 ;
    elapsedTime . QuadPart         = currentTime.QuadPart                    -
                                     stream->previousPlayTime.QuadPart       ;
    stream     -> previousPlayTime = currentTime                             ;
    /* How many bytes does DirectSound say have been played.                */
    bytesPlayed = playCursor - stream->previousPlayCursor                    ;
    if ( bytesPlayed < 0 )                                                   {
      /* unwrap                                                             */
      bytesPlayed += stream->outputBufferSizeBytes                           ;
    }                                                                        ;
    stream->previousPlayCursor = playCursor                                  ;
    //////////////////////////////////////////////////////////////////////////
    /* Calculate how many bytes we would have expected to been played by now. */
    bytesExpected  = (long) ( ( elapsedTime.QuadPart                         *
                                stream->outputBufferSizeBytes              ) /
                                stream->perfCounterTicksPerBuffer.QuadPart ) ;
    buffersWrapped = ( bytesExpected - bytesPlayed )                         /
                       stream -> outputBufferSizeBytes                       ;
    if ( buffersWrapped > 0 )                                                {
      DWORD ds     = (buffersWrapped * stream->outputBufferSizeBytes)        ;
      playCursor  += ds                                                      ;
      bytesPlayed += ds                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  numBytesEmpty = playCursor - stream->outputBufferWriteOffsetBytes          ;
  if ( numBytesEmpty < 0 )                                                   {
    /* unwrap offset                                                        */
    numBytesEmpty += stream->outputBufferSizeBytes                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Have we underflowed?                                                   */
  if ( numBytesEmpty > ( stream->outputBufferSizeBytes - playWriteGap ) )    {
    if ( 0 != stream->outputIsRunning )                                      {
      stream->outputUnderflowCount += 1                                      ;
    }                                                                        ;
    /* From MSDN:
         The write cursor indicates the position at which it is safe
         to write new data to the buffer. The write cursor always leads the
         play cursor, typically by about 15 milliseconds' worth of audio
         data.
         It is always safe to change data that is behind the position
         indicated by the lpdwCurrentPlayCursor parameter.                  */
    stream -> outputBufferWriteOffsetBytes = writeCursor                     ;
    numBytesEmpty = stream->outputBufferSizeBytes - playWriteGap             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *bytesEmpty = numBytesEmpty                                                ;
  return hr                                                                  ;
}

static int TimeSlice( DirectSoundStream * stream )
{
  if ( stream->timeSlicing ) return Conduit::Continue                        ;
  stream -> timeSlicing = true                                               ;
  ////////////////////////////////////////////////////////////////////////////
  long    numFrames           = 0                                            ;
  long    bytesEmpty          = 0                                            ;
  long    bytesFilled         = 0                                            ;
  long    bytesToXfer         = 0                                            ;
  long    framesToXfer        = 0                                            ;
  long    numInFramesReady    = 0                                            ;
  long    numOutFramesReady   = 0                                            ;
  long    bytesProcessed      = 0                                            ;
  HRESULT hresult             = 0                                            ;
  double  outputLatency       = 0                                            ;
  double  inputLatency        = 0                                            ;
  CaTime  inputBufferAdcTime  = 0                                            ;
  CaTime  currentTime         = 0                                            ;
  CaTime  outputBufferDacTime = 0                                            ;
  /* Input                                                                  */
  LPBYTE  lpInBuf1            = NULL                                         ;
  LPBYTE  lpInBuf2            = NULL                                         ;
  DWORD   dwInSize1           = 0                                            ;
  DWORD   dwInSize2           = 0                                            ;
  /* Output                                                                 */
  LPBYTE  lpOutBuf1           = NULL                                         ;
  LPBYTE  lpOutBuf2           = NULL                                         ;
  DWORD   dwOutSize1          = 0                                            ;
  DWORD   dwOutSize2          = 0                                            ;
  ////////////////////////////////////////////////////////////////////////////
  /* How much input data is available?                                      */
  if ( stream->bufferProcessor.inputChannelCount > 0 )                       {
    HRESULT hr                                                               ;
    DWORD   capturePos                                                       ;
    DWORD   readPos                                                          ;
    long    filled = 0                                                       ;
    //////////////////////////////////////////////////////////////////////////
    // Query to see how much data is in buffer.
    // We don't need the capture position but sometimes DirectSound doesn't handle NULLS correctly
    // so let's pass a pointer just to be safe.
    //////////////////////////////////////////////////////////////////////////
    hr = IDirectSoundCaptureBuffer_GetCurrentPosition                        (
           stream->pDirectSoundInputBuffer                                   ,
           &capturePos                                                       ,
           &readPos                                                        ) ;
    //////////////////////////////////////////////////////////////////////////
    if ( DS_OK == hr )                                                       {
      filled = readPos - stream->readOffset                                  ;
      if ( filled < 0 )                                                      {
        /* unwrap offset                                                    */
        filled += stream->inputBufferSizeBytes                               ;
      }                                                                      ;
      bytesFilled  = filled                                                  ;
      inputLatency = ((double)bytesFilled) * stream->secondsPerHostByte      ;
    }                                                                        ;
    // FIXME: what happens if IDirectSoundCaptureBuffer_GetCurrentPosition fails?
    numInFramesReady = bytesFilled / stream->inputFrameSizeBytes             ;
    framesToXfer     = numInFramesReady                                      ;
    /* @todo Check for overflow                                             */
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* How much output room is available?                                     */
  if ( stream->bufferProcessor.outputChannelCount > 0 )                      {
    UINT previousUnderflowCount = stream->outputUnderflowCount               ;
    QueryOutputSpace ( stream, &bytesEmpty )                                 ;
    numOutFramesReady = bytesEmpty / stream->outputFrameSizeBytes            ;
    framesToXfer      = numOutFramesReady                                    ;
    /* Check for underflow                                                  */
    /* FIXME QueryOutputSpace should not adjust underflow count as a side effect.
       A query function should be a const operator on the stream and return
       a flag on underflow.                                                 */
    if ( stream->outputUnderflowCount != previousUnderflowCount )            {
      stream->callbackFlags |= StreamIO::OutputUnderflow                     ;
    }                                                                        ;
    /* We are about to compute audio into the first byte of empty space in the output buffer.
       This audio will reach the DAC after all of the current (non-empty) audio
       in the buffer has played. Therefore the output time is the current time
       plus the time it takes to play the non-empty bytes in the buffer,
       computed here:                                                       */
    outputLatency = ((double)(stream->outputBufferSizeBytes - bytesEmpty))   *
                              stream->secondsPerHostByte                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* if it's a full duplex stream, set framesToXfer to the minimum of input and output frames ready */
  if ( ( stream->bufferProcessor.inputChannelCount  > 0 )                   &&
       ( stream->bufferProcessor.outputChannelCount > 0 )                  ) {
    framesToXfer = (numOutFramesReady < numInFramesReady)                    ?
                   numOutFramesReady                                         :
                   numInFramesReady                                          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( framesToXfer > 0 )                                                    {
    stream -> cpuLoadMeasurer . Begin ( )                                    ;
    /* The outputBufferDacTime parameter should indicates the time at which
       the first sample of the output buffer is heard at the DACs.          */
    currentTime = Timer::Time()                                              ;
    if ( NULL != stream->conduit )                                           {
      if ( stream->bufferProcessor.inputChannelCount  > 0 )                  {
        stream -> conduit -> Input  . CurrentTime  = currentTime             ;
        stream -> conduit -> Input  . StatusFlags  = stream->callbackFlags   ;
      }                                                                      ;
      if ( stream->bufferProcessor.outputChannelCount > 0 )                  {
        stream -> conduit -> Output . CurrentTime = currentTime              ;
        stream -> conduit -> Output . StatusFlags = stream->callbackFlags    ;
      }                                                                      ;
      stream -> bufferProcessor . Begin ( stream -> conduit )                ;
    }                                                                        ;
    stream->callbackFlags = 0                                                ;
    /* Input                                                                */
    if ( stream->bufferProcessor.inputChannelCount > 0 )                     {
      inputBufferAdcTime = currentTime - inputLatency                        ;
      bytesToXfer        = framesToXfer * stream->inputFrameSizeBytes        ;
      hresult            = IDirectSoundCaptureBuffer_Lock                    (
                             stream->pDirectSoundInputBuffer                 ,
                             stream->readOffset, bytesToXfer                 ,
                             (void **) &lpInBuf1                             ,
                             &dwInSize1                                      ,
                             (void **) &lpInBuf2                             ,
                             &dwInSize2                                      ,
                             0                                             ) ;
      ////////////////////////////////////////////////////////////////////////
      if ( DS_OK != hresult )                                                {
        gPrintf ( ( "DirectSound IDirectSoundCaptureBuffer_Lock failed, hresult = 0x%x\n",hresult ) ) ;
        /* PA_DS_SET_LAST_DIRECTSOUND_ERROR( hresult );                     */
        stream -> bufferProcessor . Reset ( )                                ;
        /* flush the buffer processor                                       */
        stream -> callbackResult  = Conduit::Complete                        ;
        goto error2                                                          ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      numFrames = dwInSize1 / stream->inputFrameSizeBytes                    ;
      if ( ( numFrames > 0 ) && ( NULL != lpInBuf1 ) )                       {
        stream -> bufferProcessor . setInputFrameCount ( 0 , numFrames )     ;
        stream -> bufferProcessor . setInterleavedInputChannels              (
          0                                                                  ,
          0                                                                  ,
          lpInBuf1                                                           ,
          0                                                                ) ;
      }                                                                      ;
      /* Is input split into two regions.                                   */
      if ( ( dwInSize2 > 0 ) && ( NULL != lpInBuf2 ) )                       {
        numFrames = dwInSize2 / stream->inputFrameSizeBytes                  ;
        stream -> bufferProcessor . setInputFrameCount ( 1 , numFrames )     ;
        stream -> bufferProcessor . setInterleavedInputChannels              (
          1                                                                  ,
          0                                                                  ,
          lpInBuf2                                                           ,
          0                                                                ) ;
      }                                                                      ;
    }                                                                        ;
    /* Output                                                               */
    if ( stream->bufferProcessor.outputChannelCount > 0 )                    {
      /* We don't currently add outputLatency here because it appears to produce worse
         results than not adding it. Need to do more testing to verify this.*/
      /* timeInfo.outputBufferDacTime = timeInfo.currentTime + outputLatency; */
      outputBufferDacTime = currentTime                                      ;
      bytesToXfer         = framesToXfer * stream->outputFrameSizeBytes      ;
      hresult             = IDirectSoundBuffer_Lock                          (
                              stream->pDirectSoundOutputBuffer               ,
                              stream->outputBufferWriteOffsetBytes           ,
                              bytesToXfer                                    ,
                              (void **) &lpOutBuf1                           ,
                              &dwOutSize1                                    ,
                              (void **) &lpOutBuf2                           ,
                              &dwOutSize2                                    ,
                              0                                            ) ;
      ////////////////////////////////////////////////////////////////////////
      if ( DS_OK != hresult )                                                {
        gPrintf ( ( "DirectSound IDirectSoundBuffer_Lock failed, hresult = 0x%x\n" , hresult ) ) ;
        /* flush the buffer processor                                       */
        stream -> bufferProcessor . Reset ( )                                ;
        stream -> callbackResult  = Conduit::Complete                        ;
        goto error1                                                          ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      if ( ( dwOutSize1 > 0 ) && ( NULL != lpOutBuf1 ) )                     {
        numFrames = dwOutSize1 / stream->outputFrameSizeBytes                ;
        if ( numFrames > 0 )                                                 {
          stream -> bufferProcessor . setOutputFrameCount ( 0 , numFrames )  ;
          stream -> bufferProcessor . setInterleavedOutputChannels           (
            0                                                                ,
            0                                                                ,
            lpOutBuf1                                                        ,
            0                                                              ) ;
        }                                                                    ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      /* Is output split into two regions.                                  */
      if ( ( dwOutSize2 > 0 ) && ( NULL != lpOutBuf2 ) )                     {
        numFrames = dwOutSize2 / stream->outputFrameSizeBytes                ;
        if ( numFrames > 0 )                                                 {
          stream -> bufferProcessor . setOutputFrameCount ( 1 , numFrames )  ;
          stream -> bufferProcessor . setInterleavedOutputChannels           (
            1                                                                ,
            0                                                                ,
            lpOutBuf2                                                        ,
            0                                                              ) ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    numFrames = stream -> bufferProcessor . End ( & stream->callbackResult ) ;
    stream   -> framesWritten += numFrames                                   ;
    //////////////////////////////////////////////////////////////////////////
    if ( stream->bufferProcessor.outputChannelCount > 0 )                    {
      /* FIXME: an underflow could happen here                              */
      /* Update our buffer offset and unlock sound buffer                   */
      bytesProcessed = numFrames * stream->outputFrameSizeBytes              ;
      stream -> outputBufferWriteOffsetBytes                                 =
        ( stream->outputBufferWriteOffsetBytes + bytesProcessed )            %
          stream->outputBufferSizeBytes                                      ;
      IDirectSoundBuffer_Unlock                                              (
        stream->pDirectSoundOutputBuffer                                     ,
        lpOutBuf1                                                            ,
        dwOutSize1                                                           ,
        lpOutBuf2                                                            ,
        dwOutSize2                                                         ) ;
    }                                                                        ;
error1:
    if ( stream->bufferProcessor.inputChannelCount > 0 )                     {
      /* FIXME: an overflow could happen here                               */
      /* Update our buffer offset and unlock sound buffer                   */
      bytesProcessed     = numFrames * stream->inputFrameSizeBytes           ;
      stream->readOffset = ( stream -> readOffset + bytesProcessed )         %
                             stream -> inputBufferSizeBytes                  ;
      IDirectSoundCaptureBuffer_Unlock                                       (
        stream->pDirectSoundInputBuffer                                      ,
        lpInBuf1                                                             ,
        dwInSize1                                                            ,
        lpInBuf2                                                             ,
        dwInSize2                                                          ) ;
    }                                                                        ;
error2:
    stream -> cpuLoadMeasurer . End ( numFrames )                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> timeSlicing = false                                              ;
  if ( ( Conduit::Complete == stream->callbackResult )                      &&
       ( ! stream->bufferProcessor.isOutputEmpty()   )                     ) {
    /* don't return completed until the buffer processor has been drained   */
    return Conduit::Continue                                                 ;
  }                                                                          ;
  return stream -> callbackResult                                            ;
}

/*******************************************************************/

static HRESULT ZeroAvailableOutputSpace( DirectSoundStream * stream )
{
  LPBYTE  lpbuf1  = NULL                                                     ;
  LPBYTE  lpbuf2  = NULL                                                     ;
  DWORD   dwsize1 = 0                                                        ;
  DWORD   dwsize2 = 0                                                        ;
  long    bytesEmpty                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  HRESULT hr                                                                 ;
  hr = QueryOutputSpace ( stream, &bytesEmpty )                              ;
  if ( DS_OK != hr         ) return hr                                       ;
  if ( 0     == bytesEmpty ) return DS_OK                                    ;
  ////////////////////////////////////////////////////////////////////////////
  /* Lock free space in the DS                                              */
  hr = IDirectSoundBuffer_Lock                                               (
         stream -> pDirectSoundOutputBuffer                                  ,
         stream -> outputBufferWriteOffsetBytes                              ,
         bytesEmpty                                                          ,
         (void **) &lpbuf1                                                   ,
         &dwsize1                                                            ,
         (void **) &lpbuf2                                                   ,
         &dwsize2                                                            ,
         0                                                                 ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( DS_OK == hr )                                                         {
    /* Copy the buffer into the DS                                          */
    ZeroMemory   ( lpbuf1 , dwsize1 )                                        ;
    if ( NULL != lpbuf2 )                                                    {
      ZeroMemory ( lpbuf2 , dwsize2 )                                        ;
    }                                                                        ;
    /* Update our buffer offset and unlock sound buffer                     */
    stream -> outputBufferWriteOffsetBytes                                   =
      ( stream -> outputBufferWriteOffsetBytes + dwsize1 + dwsize2 )         %
        stream -> outputBufferSizeBytes                                      ;
    IDirectSoundBuffer_Unlock                                                (
      stream -> pDirectSoundOutputBuffer                                     ,
      lpbuf1                                                                 ,
      dwsize1                                                                ,
      lpbuf2                                                                 ,
      dwsize2                                                              ) ;
    stream -> finalZeroBytesWritten += dwsize1 + dwsize2                     ;
  }                                                                          ;
  return hr                                                                  ;
}

static void CALLBACK TimerCallback      (
                       UINT      uID    ,
                       UINT      uMsg   ,
                       DWORD_PTR dwUser ,
                       DWORD     dw1    ,
                       DWORD     dw2    )
{
  DirectSoundStream * stream     = NULL                                      ;
  int                 isFinished = 0                                         ;
  ////////////////////////////////////////////////////////////////////////////
  /* suppress unused variable warnings                                      */
   stream = (DirectSoundStream *) dwUser                                     ;
  if ( NULL == stream           ) return                                     ;
  if ( 0    == stream->isActive ) return                                     ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != stream->abortProcessing )                                        {
    isFinished = 1                                                           ;
  } else
  if ( 0 != stream->stopProcessing  )                                        {
    if ( stream->bufferProcessor.outputChannelCount > 0 )                    {
      ZeroAvailableOutputSpace ( stream )                                    ;
      if ( stream->finalZeroBytesWritten >= stream->outputBufferSizeBytes )  {
        /* once we've flushed the whole output buffer with zeros we know all
         * data has been played                                             */
        isFinished = 1                                                       ;
      }                                                                      ;
    } else                                                                   {
      isFinished = 1                                                         ;
    }                                                                        ;
  } else                                                                     {
    int callbackResult = TimeSlice ( stream )                                ;
    if ( Conduit::Postpone == callbackResult )                               {
      ZeroAvailableOutputSpace ( stream )                                    ;
    } else
    if ( Conduit::Continue != callbackResult )                               {
      /* FIXME implement handling of paComplete and paAbort if possible
         At the moment this should behave as if paComplete was called and
         flush the buffer.                                                  */
      stream->stopProcessing = 1                                             ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != isFinished )                                                     {
    if ( NULL != stream->conduit ) stream->conduit->finish()                 ;
    /* don't set this until the stream really is inactive                   */
    stream->isActive = 0                                                     ;
    SetEvent ( stream->processingCompleted )                                 ;
  }                                                                          ;
}

//////////////////////////////////////////////////////////////////////////////

CaError DirectSoundInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  int                     i               = 0                                ;
  int                     deviceCount     = 0                                ;
  CaError                 result          = NoError                          ;
  DirectSoundHostApi    * winDsHostApi    = NULL                             ;
  DSDeviceNamesAndGUIDs   deviceNamesAndGUIDs                                ;
  DirectSoundDeviceInfo * deviceInfoArray = NULL                             ;
  ////////////////////////////////////////////////////////////////////////////
  CaDsInitializeDSoundEntryPoints ( )                                        ;
  deviceNamesAndGUIDs . winDsHostApi                = NULL                   ;
  deviceNamesAndGUIDs . inputNamesAndGUIDs  . items = NULL                   ;
  deviceNamesAndGUIDs . outputNamesAndGUIDs . items = NULL                   ;
  gPrintf ( ( "Initialize Direct Sound entry points\n" ) )                   ;
  ////////////////////////////////////////////////////////////////////////////
  winDsHostApi = new DirectSoundHostApi ( )                                  ;
  CaExprCorrect ( CaIsNull ( winDsHostApi ) , InsufficientMemory           ) ;
  gPrintf ( ( "Initialize Direct Sound HostApi\n" ) )                        ;
  ////////////////////////////////////////////////////////////////////////////
  /* initialise guid vectors so they can be safely deleted on error         */
  result = CaComInitialize ( DirectSound                                     ,
                             &winDsHostApi->comInitializationResult        ) ;
  if ( NoError != result ) goto error                                        ;
  gPrintf ( ( "Initialize Windows COM\n" ) )                                 ;
  ////////////////////////////////////////////////////////////////////////////
  winDsHostApi->allocations = new AllocationGroup ( )                        ;
  CaExprCorrect ( CaIsNull ( winDsHostApi->allocations )                     ,
                  InsufficientMemory                                       ) ;
  gPrintf ( ( "Set up Direct Sound allocation group\n" ) )                   ;
  ////////////////////////////////////////////////////////////////////////////
   *hostApi                                = (HostApi *)winDsHostApi         ;
  (*hostApi) -> info . structVersion       = 1                               ;
  (*hostApi) -> info . type                = DirectSound                     ;
  (*hostApi) -> info . index               = hostApiIndex                    ;
  (*hostApi) -> info . name                = "Windows Direct Sound"          ;
  (*hostApi) -> info . deviceCount         = 0                               ;
  (*hostApi) -> info . defaultInputDevice  = CaNoDevice                      ;
  (*hostApi) -> info . defaultOutputDevice = CaNoDevice                      ;
  gPrintf ( ( "Assign Direct Sound HostApi\n" ) )                            ;
  ////////////////////////////////////////////////////////////////////////////
  /* DSound - enumerate devices to count them and to gather their GUIDs     */
  result = InitializeDSDeviceNameAndGUIDVector                               (
             &deviceNamesAndGUIDs . inputNamesAndGUIDs                       ,
             winDsHostApi->allocations                                     ) ;
  if ( NoError != result ) goto error                                        ;
  result = InitializeDSDeviceNameAndGUIDVector                               (
             &deviceNamesAndGUIDs . outputNamesAndGUIDs                      ,
             winDsHostApi->allocations                                     ) ;
  if ( NoError != result ) goto error                                        ;
  gPrintf ( ( "Initialize Direct Sound device Names and GUID vector\n" ) )   ;
  ////////////////////////////////////////////////////////////////////////////
  caDsDSoundEntryPoints . DirectSoundCaptureEnumerateA                       (
    (LPDSENUMCALLBACKA)CollectGUIDsProcA                                     ,
    (LPVOID)&deviceNamesAndGUIDs . inputNamesAndGUIDs                      ) ;
  caDsDSoundEntryPoints . DirectSoundEnumerateA                              (
    (LPDSENUMCALLBACKA)CollectGUIDsProcA                                     ,
    (LPVOID)&deviceNamesAndGUIDs . outputNamesAndGUIDs                     ) ;
  gPrintf ( ( "Assign Direct Sound CollectGUIDsProcA I/O\n" )  )             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NoError != deviceNamesAndGUIDs.inputNamesAndGUIDs.enumerationError  ) {
    result = deviceNamesAndGUIDs.inputNamesAndGUIDs.enumerationError         ;
    goto error                                                               ;
  }                                                                          ;
  if ( NoError != deviceNamesAndGUIDs.outputNamesAndGUIDs.enumerationError ) {
    result = deviceNamesAndGUIDs.outputNamesAndGUIDs.enumerationError        ;
    goto error                                                               ;
  }                                                                          ;
  gPrintf ( ( "Device names and GUIDs I/O\n" ) )                             ;
  ////////////////////////////////////////////////////////////////////////////
  deviceCount = deviceNamesAndGUIDs . inputNamesAndGUIDs  . count            +
                deviceNamesAndGUIDs . outputNamesAndGUIDs . count            ;
  gPrintf ( ( "Device count is %d\n" , deviceCount ) )                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( deviceCount > 0 )                                                     {
    deviceNamesAndGUIDs.winDsHostApi = winDsHostApi                          ;
    FindDevicePnpInterfaces ( &deviceNamesAndGUIDs )                         ;
  }                                                                          ;
  gPrintf ( ( "Find device PnP interfaces\n" ) )                             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( deviceCount > 0 )                                                     {
    /* allocate array for pointers to PaDeviceInfo structs                  */
    (*hostApi)->deviceInfos = (DeviceInfo **)new DeviceInfo * [deviceCount]  ;
    CaExprCorrect ( CaIsNull ( (*hostApi)->deviceInfos )                     ,
                    InsufficientMemory                                     ) ;
    gPrintf ( ( "Allocate DeviceInfo array\n" ) )                            ;
    //////////////////////////////////////////////////////////////////////////
    /* allocate all PaDeviceInfo structs in a contiguous block              */
    deviceInfoArray = new DirectSoundDeviceInfo [ deviceCount ]              ;
    CaExprCorrect ( CaIsNull ( deviceInfoArray ) , InsufficientMemory )      ;
    gPrintf ( ( "Allocate DeviceInfo real location\n" ) )                    ;
    //////////////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < deviceCount ; ++i )                                    {
      DeviceInfo * deviceInfo    = (DeviceInfo *)&deviceInfoArray[i]         ;
      deviceInfo->structVersion  = 2                                         ;
      deviceInfo->hostApi        = hostApiIndex                              ;
      deviceInfo->hostType       = DirectSound                               ;
      deviceInfo->name           = 0                                         ;
      (*hostApi)->deviceInfos[i] = deviceInfo                                ;
    }                                                                        ;
    gPrintf ( ( "Place DeviceInfo at right place\n" ) )                      ;
    //////////////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < deviceNamesAndGUIDs.inputNamesAndGUIDs.count ; ++i )   {
      gPrintf                                                              ( (
        "Input device (%d) : %s\n"                                           ,
        winDsHostApi->info.deviceCount                                       ,
        deviceNamesAndGUIDs.inputNamesAndGUIDs.items[i].name             ) ) ;
      result = AddInputDeviceInfoFromDirectSoundCapture                      (
                  winDsHostApi                                               ,
                  deviceNamesAndGUIDs.inputNamesAndGUIDs.items[i].name       ,
                  deviceNamesAndGUIDs.inputNamesAndGUIDs.items[i].lpGUID     ,
      (char *)deviceNamesAndGUIDs.inputNamesAndGUIDs.items[i].pnpInterface ) ;
      if ( CaIsWrong ( result ) ) goto error                                 ;
    }                                                                        ;
    gPrintf ( ( "Add input DeviceInfo from Direct Sound capturer\n" ) )      ;
    //////////////////////////////////////////////////////////////////////////
    for ( i=0 ; i < deviceNamesAndGUIDs.outputNamesAndGUIDs.count ; ++i )    {
      gPrintf                                                              ( (
        "Output device (%d): %s\n"                                           ,
        winDsHostApi->info.deviceCount                                       ,
        deviceNamesAndGUIDs.outputNamesAndGUIDs.items[i].name            ) ) ;
      result = AddOutputDeviceInfoFromDirectSound                            (
                 winDsHostApi                                                ,
                 deviceNamesAndGUIDs.outputNamesAndGUIDs.items[i].name       ,
                 deviceNamesAndGUIDs.outputNamesAndGUIDs.items[i].lpGUID     ,
     (char *)deviceNamesAndGUIDs.outputNamesAndGUIDs.items[i].pnpInterface ) ;
      if ( CaIsWrong ( result ) ) goto error                                 ;
    }                                                                        ;
    gPrintf ( ( "Add output DeviceInfo from Direct Sound\n" ) )              ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  result = TerminateDSDeviceNameAndGUIDVector                                (
             &deviceNamesAndGUIDs.inputNamesAndGUIDs                       ) ;
  if ( NoError != result ) goto error                                        ;
  result = TerminateDSDeviceNameAndGUIDVector                                (
             &deviceNamesAndGUIDs.outputNamesAndGUIDs                      ) ;
  if ( NoError != result ) goto error                                        ;
  gPrintf ( ( "Terminate Direct Sound device name and GUID vector\n" ) )     ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  TerminateDSDeviceNameAndGUIDVector( &deviceNamesAndGUIDs.inputNamesAndGUIDs  ) ;
  TerminateDSDeviceNameAndGUIDVector( &deviceNamesAndGUIDs.outputNamesAndGUIDs ) ;
  delete winDsHostApi                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

DirectSoundStream:: DirectSoundStream ( void     )
                  : Stream            (          )
{
  ::memset ( &perfCounterTicksPerBuffer , 0 , sizeof(LARGE_INTEGER) ) ;
  ::memset ( &previousPlayTime          , 0 , sizeof(LARGE_INTEGER) ) ;
  pDirectSoundFullDuplex8       = NULL     ;
  pDirectSound                  = NULL     ;
  pDirectSoundPrimaryBuffer     = NULL     ;
  pDirectSoundOutputBuffer      = NULL     ;
  outputBufferWriteOffsetBytes  = 0        ;
  outputBufferSizeBytes         = 0        ;
  outputFrameSizeBytes          = 0        ;
  previousPlayCursor            = 0        ;
  outputUnderflowCount          = 0        ;
  outputIsRunning               = 0        ;
  finalZeroBytesWritten         = 0        ;
  pDirectSoundCapture           = NULL     ;
  pDirectSoundInputBuffer       = NULL     ;
  inputFrameSizeBytes           = 0        ;
  readOffset                    = 0        ;
  inputBufferSizeBytes          = 0        ;
  hostBufferSizeFrames          = 0        ;
  framesWritten                 = 0        ;
  secondsPerHostByte            = 0        ;
  pollingPeriodSeconds          = 0        ;
  callbackFlags                 = 0        ;
  streamFlags                   = 0        ;
  callbackResult                = 0        ;
  processingCompleted           = 0        ;
  isStarted                     = 0        ;
  isActive                      = 0        ;
  stopProcessing                = 0        ;
  abortProcessing               = 0        ;
  systemTimerResolutionPeriodMs = 0        ;
  timerID                       = 0        ;
  Direction                     = NoStream ;
  timeSlicing                   = false    ;
}

DirectSoundStream::~DirectSoundStream (void)
{
}

CaError DirectSoundStream::Start(void)
{
  CaError result = NoError                                                   ;
  HRESULT hr                                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  callbackResult  = Conduit::Continue                                        ;
  bufferProcessor . Reset ( )                                                ;
  ResetEvent ( processingCompleted )                                         ;
  ////////////////////////////////////////////////////////////////////////////
  if ( bufferProcessor . inputChannelCount > 0 )                             {
    /* Start the buffer capture                                             */
    if ( NULL != pDirectSoundInputBuffer )                                   {
      /* FIXME: not sure this check is necessary                            */
      hr = IDirectSoundCaptureBuffer_Start                                   (
             pDirectSoundInputBuffer                                         ,
             DSCBSTART_LOOPING                                             ) ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    dPrintf ( ( "Start : DSW_StartInput returned = 0x%X.\n" , hr ) )         ;
    //////////////////////////////////////////////////////////////////////////
    if ( DS_OK != hr )                                                       {
      result = UnanticipatedHostError                                        ;
      goto error                                                             ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  framesWritten   = 0                                                        ;
  callbackFlags   = 0                                                        ;
  abortProcessing = 0                                                        ;
  stopProcessing  = 0                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( bufferProcessor.outputChannelCount > 0 )                              {
    ::QueryPerformanceCounter ( &previousPlayTime )                          ;
    finalZeroBytesWritten = 0                                                ;
    hr = ClearOutputBuffer    (                   )                          ;
    //////////////////////////////////////////////////////////////////////////
    if ( DS_OK != hr )                                                       {
      result = UnanticipatedHostError                                        ;
//      PA_DS_SET_LAST_DIRECTSOUND_ERROR ( hr )                                ;
      goto error                                                             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( NULL != conduit                                                    &&
         (streamFlags & csfPrimeOutputBuffersUsingStreamCallback)          ) {
      callbackFlags = StreamIO::PrimingOutput                                ;
      TimeSlice ( this )                                                     ;
      /* we ignore the return value from TimeSlice here and start the stream as usual.
              The first timer callback will detect if the callback has completed. */
      callbackFlags = 0                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    /* Start the buffer playback in a loop.                                 */
    if ( NULL != pDirectSoundOutputBuffer )                                  {
      /* FIXME: not sure this needs to be checked here                      */
      hr = IDirectSoundBuffer_Play                                           (
             pDirectSoundOutputBuffer                                        ,
             0                                                               ,
             0                                                               ,
             DSBPLAY_LOOPING                                               ) ;
      dPrintf ( ( "Start : IDirectSoundBuffer_Play returned = 0x%X.\n", hr ) ) ;
      if ( DS_OK != hr )                                                     {
        result = UnanticipatedHostError                                      ;
        goto error                                                           ;
      }                                                                      ;
      outputIsRunning = TRUE                                                 ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != conduit )                                                     {
    TIMECAPS timecaps                                                        ;
    int timerPeriodMs = (int)( pollingPeriodSeconds * MSECS_PER_SECOND )     ;
    if ( timerPeriodMs < 1 ) timerPeriodMs = 1                               ;
    /* set windows scheduler granularity only as fine as needed, no finer   */
    /* Although this is not fully documented by MS, it appears that
       timeBeginPeriod() affects the scheduling granulatity of all timers
       including Waitable Timer Objects. So we always call timeBeginPeriod, whether
       we're using an MM timer callback via timeSetEvent or not.            */
//    assert ( systemTimerResolutionPeriodMs == 0 )                            ;
    if ( MMSYSERR_NOERROR==timeGetDevCaps( &timecaps, sizeof(TIMECAPS) )    &&
         ( timecaps.wPeriodMin > 0 )                                       ) {
      /* aim for resolution 4 times higher than polling rate                */
      systemTimerResolutionPeriodMs = (UINT)((pollingPeriodSeconds * MSECS_PER_SECOND) * 0.25) ;
      if ( systemTimerResolutionPeriodMs < timecaps.wPeriodMin )             {
        systemTimerResolutionPeriodMs = timecaps.wPeriodMin                  ;
      }                                                                      ;
      if ( systemTimerResolutionPeriodMs > timecaps.wPeriodMax )             {
        systemTimerResolutionPeriodMs = timecaps.wPeriodMax                  ;
      }                                                                      ;
      if ( MMSYSERR_NOERROR != timeBeginPeriod( systemTimerResolutionPeriodMs ) ) {
        /* timeBeginPeriod failed, so we don't need to call timeEndPeriod() later */
        systemTimerResolutionPeriodMs = 0                                    ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    /* Create timer that will wake us up so we can fill the DSound buffer.  */
    /* We have deprecated timeSetEvent because all MM timer callbacks
       are serialised onto a single thread. Which creates problems with multiple
       PA streams, or when also using timers for other time critical tasks  */
    timerID = timeSetEvent                                                   (
                timerPeriodMs                                                ,
                systemTimerResolutionPeriodMs                                ,
                (LPTIMECALLBACK) TimerCallback                               ,
                (DWORD_PTR) this                                             ,
                TIME_PERIODIC | TIME_KILL_SYNCHRONOUS                      ) ;
    if ( 0 == timerID )                                                      {
      isActive = 0                                                           ;
      result   = UnanticipatedHostError                                      ;
//      PA_DS_SET_LAST_DIRECTSOUND_ERROR ( GetLastError() )                    ;
      goto error                                                             ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  isActive  = 1                                                              ;
  isStarted = 1                                                              ;
  ////////////////////////////////////////////////////////////////////////////
//  assert ( NoError == result  )                                              ;
  return result                                                              ;
error:
  if ( ( pDirectSoundOutputBuffer != NULL ) && outputIsRunning )             {
    IDirectSoundBuffer_Stop ( pDirectSoundOutputBuffer )                     ;
  }                                                                          ;
  outputIsRunning = FALSE                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

CaError DirectSoundStream::Stop(void)
{
  CaError result = NoError                                                   ;
  HRESULT hr                                                                 ;
  int     timeoutMsec                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != conduit )                                                     {
    stopProcessing = 1                                                       ;
    /* Set timeout at 4 times maximum time we might wait.                   */
    timeoutMsec = (int) ( 4 * MSECS_PER_SECOND                               *
                          hostBufferSizeFrames                               /
                          sampleRate                                       ) ;
    ::WaitForSingleObject ( processingCompleted, timeoutMsec )               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != timerID )                                                        {
    /* Stop callback timer                                                  */
    timeKillEvent ( timerID )                                                ;
    timerID = 0                                                              ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( systemTimerResolutionPeriodMs > 0 )                                   {
    timeEndPeriod ( systemTimerResolutionPeriodMs )                          ;
    systemTimerResolutionPeriodMs = 0                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( bufferProcessor.outputChannelCount > 0 )                              {
    /* Stop the buffer playback                                             */
    if ( NULL != pDirectSoundOutputBuffer )                                  {
      outputIsRunning = FALSE                                                ;
      /* FIXME: what happens if IDirectSoundBuffer_Stop returns an error?   */
      hr = IDirectSoundBuffer_Stop ( pDirectSoundOutputBuffer )              ;
      if ( NULL != pDirectSoundPrimaryBuffer )                               {
         /* FIXME we never started the primary buffer so I'm not sure we need to stop it */
        IDirectSoundBuffer_Stop ( pDirectSoundPrimaryBuffer )                ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( bufferProcessor.inputChannelCount > 0 )                               {
    /* Stop the buffer capture                                              */
    if ( NULL != pDirectSoundInputBuffer )                                   {
      // FIXME: what happens if IDirectSoundCaptureBuffer_Stop returns an error?
      hr = IDirectSoundCaptureBuffer_Stop ( pDirectSoundInputBuffer )        ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  isStarted = 0                                                              ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

/*****************************************************************************
    When Close() is called, the multi-api layer ensures that the stream has
    already been stopped or aborted.                                         */

CaError DirectSoundStream::Close(void)
{
  CaError result = NoError                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  ::CloseHandle ( processingCompleted       )                                ;
  ////////////////////////////////////////////////////////////////////////////
  /* Cleanup the sound buffers                                              */
  if ( NULL != pDirectSoundOutputBuffer )                                    {
    IDirectSoundBuffer_Stop           ( pDirectSoundOutputBuffer  )          ;
    IDirectSoundBuffer_Release        ( pDirectSoundOutputBuffer  )          ;
    pDirectSoundOutputBuffer  = NULL                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != pDirectSoundPrimaryBuffer )                                   {
    IDirectSoundBuffer_Release        ( pDirectSoundPrimaryBuffer )          ;
    pDirectSoundPrimaryBuffer = NULL                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != pDirectSoundInputBuffer )                                     {
    IDirectSoundCaptureBuffer_Stop    ( pDirectSoundInputBuffer   )          ;
    IDirectSoundCaptureBuffer_Release ( pDirectSoundInputBuffer   )          ;
    pDirectSoundInputBuffer   = NULL                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != pDirectSoundCapture )                                         {
    IDirectSoundCapture_Release       ( pDirectSoundCapture       )          ;
    pDirectSoundCapture       = NULL                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != pDirectSound )                                                {
    IDirectSound_Release              ( pDirectSound              )          ;
    pDirectSound              = NULL                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != pDirectSoundFullDuplex8 )                                     {
    IDirectSoundFullDuplex_Release ( pDirectSoundFullDuplex8 )               ;
    pDirectSoundFullDuplex8 = NULL                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  bufferProcessor . Terminate ( )                                            ;
  Terminate                   ( )                                            ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

CaError DirectSoundStream::Abort(void)
{
  abortProcessing = 1 ;
  return Stop ( )     ;
}

CaError DirectSoundStream::IsStopped(void)
{
  return ( ! isStarted ) ;
}

CaError DirectSoundStream::IsActive(void)
{
  return isActive ;
}

CaTime DirectSoundStream::GetTime(void)
{
  return Timer :: Time ( ) ;
}

double DirectSoundStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer . Value ( ) ;
}

CaInt32 DirectSoundStream::ReadAvailable(void)
{
  switch ( Direction )  {
    case NoStream       :
    break               ;
    case CallbackStream :
    break               ;
    case BlockingStream :
    break               ;
  }                     ;
  return 0              ;
}

CaInt32 DirectSoundStream::WriteAvailable(void)
{
  switch ( Direction )  {
    case NoStream       :
    break               ;
    case CallbackStream :
    break               ;
    case BlockingStream :
    break               ;
  }                     ;
  return 0              ;
}

CaError DirectSoundStream::Read(void * buffer,unsigned long frames)
{
  switch ( Direction )  {
    case NoStream       :
    break               ;
    case CallbackStream :
    break               ;
    case BlockingStream :
    break               ;
  }                     ;
  return NoError        ;
}

CaError DirectSoundStream::Write(const void * buffer,unsigned long frames)
{
  switch ( Direction )  {
    case NoStream       :
    break               ;
    case CallbackStream :
    break               ;
    case BlockingStream :
    break               ;
  }                     ;
  return NoError        ;
}

HRESULT DirectSoundStream::ClearOutputBuffer(void)
{
  CaError         result      = NoError                                      ;
  unsigned char * pDSBuffData = NULL                                         ;
  DWORD           dwDataLen                                                  ;
  HRESULT         hr                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  hr = IDirectSoundBuffer_SetCurrentPosition ( pDirectSoundOutputBuffer,0 )  ;
  dPrintf ( ( "ClearOutputBuffer: IDirectSoundBuffer_SetCurrentPosition returned = 0x%X.\n" , hr ) ) ;
  if ( DS_OK != hr ) return hr                                               ;
  ////////////////////////////////////////////////////////////////////////////
  /* Lock the DS buffer                                                     */
  hr = IDirectSoundBuffer_Lock                                               (
         pDirectSoundOutputBuffer                                            ,
         0                                                                   ,
         outputBufferSizeBytes                                               ,
         (LPVOID *)&pDSBuffData                                              ,
         &dwDataLen                                                          ,
         NULL                                                                ,
         0                                                                   ,
         0                                                                 ) ;
  if ( DS_OK != hr ) return hr                                               ;
  ////////////////////////////////////////////////////////////////////////////
  /* Zero the DS buffer                                                     */
  ZeroMemory ( pDSBuffData , dwDataLen )                                     ;
  /* Unlock the DS buffer                                                   */
  hr = IDirectSoundBuffer_Unlock                                             (
         pDirectSoundOutputBuffer                                            ,
         pDSBuffData                                                         ,
         dwDataLen                                                           ,
         NULL                                                                ,
         0                                                                 ) ;
  if ( DS_OK != hr ) return hr                                               ;
  ////////////////////////////////////////////////////////////////////////////
  // Let DSound set the starting write position because if we set it to zero, it looks like the
  // buffer is full to begin with. This causes a long pause before sound starts when using large buffers.
  hr = IDirectSoundBuffer_GetCurrentPosition                                 (
         pDirectSoundOutputBuffer                                            ,
         &previousPlayCursor                                                 ,
         &outputBufferWriteOffsetBytes                                     ) ;
  if ( DS_OK != hr ) return hr                                               ;
  /* printf("DSW_InitOutputBuffer: playCursor = %d, writeCursor = %d\n", playCursor, dsw->dsw_WriteOffset ); */
  return DS_OK                                                               ;
}

bool DirectSoundStream::hasVolume(void)
{
  return true ;
}

CaVolume DirectSoundStream::MinVolume(void)
{
  return 0.0 ;
}

CaVolume DirectSoundStream::MaxVolume(void)
{
  return 10000.0 ;
}

CaVolume DirectSoundStream::toVolume(CaVolume dB)
{ // y = 10000 * ( 10 ^ ( x / 2000.0 ) )
  if ( dB > 0 ) dB = -dB       ;
  CaVolume y = dB              ;
  y /= 2000.0                  ;
  y  = ::pow(10,y)             ;
  y *= 10000.0                 ;
  dB = (int) y                 ;
  if ( dB < 0     ) dB = 0     ;
  if ( dB > 10000 ) dB = 10000 ;
  return dB                    ;
}

CaVolume DirectSoundStream::toDB(CaVolume volume)
{ // x = 2000.0 * log10 ( y / 10000 )
  CaVolume x = volume                    ;
  x /= 10000                             ;
  x  = ::log10(x)                        ;
  x *= 2000.0                            ;
  volume = (int)x                        ;
  if ( volume >      0 ) volume =      0 ;
  if ( volume < -10000 ) volume = -10000 ;
  return volume                          ;
}

CaVolume DirectSoundStream::Volume(int atChannel)
{
  LONG     V = 0                                                 ;
  LONG     P = 0                                                 ;
  CaVolume S = 0                                                 ;
  CaVolume L = 0                                                 ;
  CaVolume R = 0                                                 ;
  CaVolume v                                                     ;
  IDirectSoundBuffer_GetVolume ( pDirectSoundOutputBuffer , &V ) ;
  v  = toVolume ( V )                                            ;
  if ( atChannel >= 0 )                                          {
    IDirectSoundBuffer_GetPan  ( pDirectSoundOutputBuffer , &P ) ;
    if ( P != 0 )                                                {
      S  = toVolume ( P )                                        ;
      L  = v                                                     ;
      R  = v                                                     ;
      S *= v                                                     ;
      S /= 10000                                                 ;
      if ( P < 0 ) R = S ; else L = S                            ;
      if ( 0 == atChannel ) v = L                           ; else
      if ( 1 == atChannel ) v = R                                ;
    }                                                            ;
  }                                                              ;
  return (CaVolume) v                                            ;
}

CaVolume DirectSoundStream::setVolume(CaVolume volume,int atChannel)
{
  LONG     P = 0                                                 ;
  LONG     V = 0                                                 ;
  CaVolume L = 0                                                 ;
  CaVolume R = 0                                                 ;
  CaVolume S = 0                                                 ;
  CaVolume v = 0                                                 ;
  IDirectSoundBuffer_GetVolume ( pDirectSoundOutputBuffer , &V ) ;
  IDirectSoundBuffer_GetPan    ( pDirectSoundOutputBuffer , &P ) ;
  ////////////////////////////////////////////////////////////////
  if ( atChannel >= 0 )                                          {
    v  = toVolume ( V )                                          ;
    L  = v                                                       ;
    R  = v                                                       ;
    if ( P != 0 )                                                {
      S  = toVolume ( P )                                        ;
      S *= L                                                     ;
      S /= 10000                                                 ;
      if ( P < 0 ) R = S ; else L = S                            ;
    }                                                            ;
    if ( 0 == atChannel ) L = volume ; else R = volume           ;
    if ( L > R ) v = L ; else v = R                              ;
    if ( L > R )                                                 {
      S  = R                                                     ;
      S *= 10000                                                 ;
      S /= v                                                     ;
      S  = toDB ( S )                                            ;
      P  = (LONG)S                                               ;
    } else
    if ( R > L )                                                 {
      S  = L                                                     ;
      S *= 10000                                                 ;
      S /= v                                                     ;
      S  = toDB ( S )                                            ;
      P  = (LONG)-S                                              ;
    } else P = 0                                                 ;
    V = (LONG)toDB ( v      )                                    ;
  } else                                                         {
    P = 0                                                        ;
    V = (LONG)toDB ( volume )                                    ;
  }                                                              ;
  ////////////////////////////////////////////////////////////////
  if ( V < DSBVOLUME_MIN ) V = DSBVOLUME_MIN                     ;
  if ( V > DSBVOLUME_MAX ) V = DSBVOLUME_MAX                     ;
  IDirectSoundBuffer_SetVolume ( pDirectSoundOutputBuffer , V  ) ;
  IDirectSoundBuffer_SetPan    ( pDirectSoundOutputBuffer , P  ) ;
  return Volume ( atChannel )                                    ;
}

//////////////////////////////////////////////////////////////////////////////

DirectSoundHostApiInfo:: DirectSoundHostApiInfo (void)
                       : HostApiInfo            (    )
{
}

DirectSoundHostApiInfo::~DirectSoundHostApiInfo (void)
{
}

//////////////////////////////////////////////////////////////////////////////

DirectSoundDeviceInfo:: DirectSoundDeviceInfo (void)
                      : DeviceInfo            (    )
{
  lpGUID                          = NULL ;
  sampleRates [ 0 ]               = 0    ;
  sampleRates [ 1 ]               = 0    ;
  sampleRates [ 2 ]               = 0    ;
  deviceInputChannelCountIsKnown  = 0    ;
  deviceOutputChannelCountIsKnown = 0    ;
}

DirectSoundDeviceInfo::~DirectSoundDeviceInfo (void)
{
}

//////////////////////////////////////////////////////////////////////////////

DirectSoundHostApi:: DirectSoundHostApi      ( void )
                   : HostApi                 (      )
                   , allocations             ( NULL )
{
  memset ( &comInitializationResult , 0 , sizeof(CaComInitializationResult) ) ;
}

DirectSoundHostApi::~DirectSoundHostApi (void)
{
  CaEraseAllocation ;
}

HostApi::Encoding DirectSoundHostApi::encoding(void) const
{
  return NATIVE ;
}

bool DirectSoundHostApi::hasDuplex(void) const
{
  return true ;
}

CaError DirectSoundHostApi::Open                     (
          Stream                ** s                 ,
          const StreamParameters * inputParameters   ,
          const StreamParameters * outputParameters  ,
          double                   sampleRate        ,
          CaUint32                 framesPerCallback ,
          CaStreamFlags            streamFlags       ,
          Conduit                * conduit           )
{
  CaError                   result                                  = NoError;
  DirectSoundStream       * stream                                  = NULL   ;
  DirectSoundDeviceInfo   * inputWinDsDeviceInfo                    = NULL   ;
  DirectSoundDeviceInfo   * outputWinDsDeviceInfo                   = NULL   ;
  DeviceInfo              * inputDeviceInfo                         = NULL   ;
  DeviceInfo              * outputDeviceInfo                        = NULL   ;
  CaDirectSoundStreamInfo * inputStreamInfo                         = NULL   ;
  CaDirectSoundStreamInfo * outputStreamInfo                        = NULL   ;
  CaSampleFormat            inputSampleFormat                                ;
  CaSampleFormat            outputSampleFormat                               ;
  CaSampleFormat            hostInputSampleFormat                            ;
  CaSampleFormat            hostOutputSampleFormat                           ;
  CaWaveFormatChannelMask   inputChannelMask                                 ;
  CaWaveFormatChannelMask   outputChannelMask                                ;
  int                       inputChannelCount                       = 0      ;
  int                       outputChannelCount                      = 0      ;
  int                       userRequestedHostInputBufferSizeFrames  = 0      ;
  int                       userRequestedHostOutputBufferSizeFrames = 0      ;
  int                       bufferProcessorIsInitialized            = 0      ;
  int                       streamRepresentationIsInitialized       = 0      ;
  unsigned long             suggestedInputLatencyFrames             = 0      ;
  unsigned long             suggestedOutputLatencyFrames            = 0      ;
  unsigned long             pollingPeriodFrames                     = 0      ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( NULL                   != inputParameters  )                       &&
       ( inputParameters->device < info.deviceCount )                      ) {
    inputWinDsDeviceInfo        = (DirectSoundDeviceInfo *) deviceInfos[ inputParameters->device ] ;
    inputDeviceInfo             = (DeviceInfo            *) deviceInfos[ inputParameters->device ] ;
    inputChannelCount           = inputParameters->channelCount              ;
    inputSampleFormat           = inputParameters->sampleFormat              ;
    suggestedInputLatencyFrames = (unsigned long)( inputParameters->suggestedLatency * sampleRate ) ;
    /* IDEA: the following 3 checks could be performed by default by pa_front
          unless some flag indicated otherwise                              */
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    if ( inputParameters->device==CaUseHostApiSpecificDeviceSpecification )  {
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that input device can support inputChannelCount                */
    if ( ( 0 != inputWinDsDeviceInfo->deviceInputChannelCountIsKnown )      &&
         ( inputChannelCount > inputDeviceInfo->maxInputChannels     )     ) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate streamInfo                                   */
    inputStreamInfo = (CaDirectSoundStreamInfo *)inputParameters->streamInfo;
    result          = ValidateWinDirectSoundSpecificStreamInfo               (
                        inputParameters                                      ,
                        inputStreamInfo                                    ) ;
    if ( NoError != result ) return result                                   ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( NULL != inputStreamInfo )                                        &&
         ( inputStreamInfo->flags & CaWinDirectSoundUseLowLevelLatencyParameters ) ) {
      userRequestedHostInputBufferSizeFrames = inputStreamInfo->framesPerBuffer ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( NULL != inputStreamInfo )                                        &&
         ( inputStreamInfo->flags & CaWinDirectSoundUseChannelMask )       ) {
      inputChannelMask = inputStreamInfo->channelMask                        ;
    } else                                                                   {
      inputChannelMask = CaDefaultChannelMask ( inputChannelCount )          ;
    }                                                                        ;
    if ( NULL != conduit )                                                   {
      conduit->Input . BytesPerSample  = inputChannelCount                   ;
      conduit->Input . BytesPerSample *= Core::SampleSize(inputSampleFormat) ;
    }                                                                        ;
  } else                                                                     {
    inputChannelCount           = 0                                          ;
    inputSampleFormat           = cafNothing                                 ;
    suggestedInputLatencyFrames = 0                                          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputWinDsDeviceInfo        = (DirectSoundDeviceInfo *)deviceInfos[ outputParameters->device ] ;
    outputDeviceInfo             = (DeviceInfo            *)outputWinDsDeviceInfo ;
    outputChannelCount           = outputParameters -> channelCount ;
    outputSampleFormat           = outputParameters -> sampleFormat ;
    suggestedOutputLatencyFrames = (unsigned long)(outputParameters->suggestedLatency * sampleRate);
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification */
    if ( CaUseHostApiSpecificDeviceSpecification == outputParameters->device ) {
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that output device can support outputChannelCount              */
    if ( ( 0 != outputWinDsDeviceInfo->deviceOutputChannelCountIsKnown )    &&
         ( outputChannelCount > outputDeviceInfo->maxOutputChannels    )   ) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate streamInfo                                   */
    outputStreamInfo = (CaDirectSoundStreamInfo *)outputParameters->streamInfo;
    result           = ValidateWinDirectSoundSpecificStreamInfo              (
                         outputParameters                                    ,
                         outputStreamInfo                                  ) ;
    if ( NoError != result ) return result                                   ;
    if ( ( NULL != outputStreamInfo )                                       &&
         ( outputStreamInfo->flags & CaWinDirectSoundUseLowLevelLatencyParameters ) ) {
      userRequestedHostOutputBufferSizeFrames = outputStreamInfo->framesPerBuffer ;
    }                                                                        ;
    if ( ( NULL != outputStreamInfo )                                       &&
         ( outputStreamInfo->flags & CaWinDirectSoundUseChannelMask )      ) {
      outputChannelMask = outputStreamInfo->channelMask                      ;
    } else                                                                   {
      outputChannelMask = CaDefaultChannelMask ( outputChannelCount )        ;
    }                                                                        ;
    if ( NULL != conduit )                                                   {
      conduit->Output . BytesPerSample  = outputChannelCount                 ;
      conduit->Output . BytesPerSample *= Core::SampleSize(outputSampleFormat)  ;
    }                                                                        ;
  } else                                                                     {
    outputChannelCount           = 0                                         ;
    outputSampleFormat           = cafNothing                                ;
    suggestedOutputLatencyFrames = 0                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* If low level host buffer size is specified for both input and output
     the current code requires the sizes to match.                          */
  if ( ( ( userRequestedHostInputBufferSizeFrames  > 0 )                    &&
         ( userRequestedHostOutputBufferSizeFrames > 0 ) )                  &&
         ( userRequestedHostInputBufferSizeFrames != userRequestedHostOutputBufferSizeFrames ) ) {
    return IncompatibleStreamInfo                             ;
  }                                                                          ;
  /*
      IMPLEMENT ME:

      ( the following two checks are taken care of by PaUtil_InitializeBufferProcessor() )

          - check that input device can support inputSampleFormat, or that
              we have the capability to convert from outputSampleFormat to
              a native format

          - check that output device can support outputSampleFormat, or that
              we have the capability to convert from outputSampleFormat to
              a native format

          - if a full duplex stream is requested, check that the combination
              of input and output parameters is supported

          - check that the device supports sampleRate

          - alter sampleRate to a close allowable rate if possible / necessary

          - validate suggestedInputLatency and suggestedOutputLatency parameters,
              use default values where necessary
  */
  /* validate platform specific flags                                       */
  if ( ( streamFlags & csfPlatformSpecificFlags ) != 0 )                     {
    /* unexpected platform specific flag                                    */
    return InvalidFlag                                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream  = new DirectSoundStream ( )                                        ;
  stream -> debugger = debugger                                              ;
  if ( NULL == stream )                                                      {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  streamRepresentationIsInitialized = 1                                      ;
  stream -> conduit = conduit                                                ;
  stream -> streamFlags             = streamFlags                            ;
  stream -> cpuLoadMeasurer . Initialize ( sampleRate )                      ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    /* IMPLEMENT ME - establish which  host formats are available           */
    unsigned long nativeInputFormats  = cafInt16 | cafInt32 | cafFloat32     ;
    /* CaSampleFormat nativeFormats = cafUInt8 | cafInt16 | cafInt24 | cafInt32 | cafFloat32; */
    hostInputSampleFormat  = ClosestFormat                                   (
                               nativeInputFormats                            ,
                               inputParameters->sampleFormat               ) ;
  } else                                                                     {
    hostInputSampleFormat  = cafNothing                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    /* IMPLEMENT ME - establish which  host formats are available           */
    unsigned long nativeOutputFormats = cafInt16 | cafInt32 | cafFloat32     ;
    /* CaSampleFormat nativeOutputFormats = cafUInt8 | cafInt16 | cafInt24 | cafInt32 | cafFloat32; */
     hostOutputSampleFormat = ClosestFormat                                  (
                                nativeOutputFormats                          ,
                                outputParameters->sampleFormat             ) ;
  } else                                                                     {
    hostOutputSampleFormat = cafNothing                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  result = stream->bufferProcessor.Initialize                                (
             inputChannelCount                                               ,
             inputSampleFormat                                               ,
             hostInputSampleFormat                                           ,
             outputChannelCount                                              ,
             outputSampleFormat                                              ,
             hostOutputSampleFormat                                          ,
             sampleRate                                                      ,
             streamFlags                                                     ,
             framesPerCallback                                               ,
             0                                                               ,
             cabVariableHostBufferSizePartialUsageAllowed                    ,
             conduit                                                       ) ;
  if ( NoError != result ) goto error                                        ;
  bufferProcessorIsInitialized = 1                                           ;
  ////////////////////////////////////////////////////////////////////////////
  /* DirectSound specific initialization                                    */
  HRESULT       hr                                                           ;
  unsigned long integerSampleRate = (unsigned long) ( sampleRate + 0.5 )     ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> processingCompleted = CreateEvent                                (
                                    NULL                                     ,
                                    TRUE  /* bManualReset  */                ,
                                    FALSE /* bInitialState */                ,
                                    NULL                                   ) ;
  if ( NULL == stream->processingCompleted )                                 {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  /* set up i/o parameters                                                  */
  if ( ( userRequestedHostInputBufferSizeFrames  > 0 )                      ||
       ( userRequestedHostOutputBufferSizeFrames > 0 )                     ) {
    /* use low level parameters                                             */
    /* since we use the same host buffer size for input and output
             we choose the highest user specified value.                    */
    stream->hostBufferSizeFrames = max                                       (
                                   userRequestedHostInputBufferSizeFrames    ,
                                   userRequestedHostOutputBufferSizeFrames ) ;
    CalculatePollingPeriodFrames                                             (
      stream -> hostBufferSizeFrames                                         ,
      &pollingPeriodFrames                                                   ,
      sampleRate                                                             ,
      framesPerCallback                                                    ) ;
  } else                                                                     {
    CalculateBufferSettings                                                  (
      &stream->hostBufferSizeFrames                                          ,
      &pollingPeriodFrames                                                   ,
      (inputParameters && outputParameters) /* isFullDuplex = */             ,
      suggestedInputLatencyFrames                                            ,
      suggestedOutputLatencyFrames                                           ,
      sampleRate                                                             ,
      framesPerCallback                                                    ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> pollingPeriodSeconds = pollingPeriodFrames / sampleRate          ;
  dPrintf                                                                  ( (
      "DirectSound host buffer size frames: %d, polling period seconds: %f, @ sr: %f\n",
      stream->hostBufferSizeFrames                                           ,
      stream->pollingPeriodSeconds                                           ,
      sampleRate                                                         ) ) ;
  /* ------------------ OUTPUT                                              */
  if ( NULL != outputParameters )                                            {
    LARGE_INTEGER  counterFrequency                                          ;
    /* PaDeviceInfo *deviceInfo = hostApi->deviceInfos[ outputParameters->device ];
       DBUG(("PaHost_OpenStream: deviceID = 0x%x\n", outputParameters->device));
     */
    int sampleSizeBytes = Core :: SampleSize ( hostOutputSampleFormat )      ;
    stream -> outputFrameSizeBytes  = outputParameters->channelCount         *
                                      sampleSizeBytes                        ;
    stream -> outputBufferSizeBytes = stream -> hostBufferSizeFrames         *
                                      stream -> outputFrameSizeBytes         ;
    if ( stream->outputBufferSizeBytes < DSBSIZE_MIN )                       {
      result = BufferTooSmall                                                ;
      goto error                                                             ;
    } else
    if ( stream->outputBufferSizeBytes > DSBSIZE_MAX )                       {
      result = BufferTooBig                                                  ;
      goto error                                                             ;
    }                                                                        ;
    /* Calculate value used in latency calculation to avoid real-time divides. */
    stream -> secondsPerHostByte = 1.0                                       /
                          ( stream->bufferProcessor.bytesPerHostOutputSample *
                            outputChannelCount                               *
                            sampleRate                                     ) ;
    stream -> outputIsRunning      = FALSE                                   ;
    stream -> outputUnderflowCount = 0                                       ;
    /* perfCounterTicksPerBuffer is used by QueryOutputSpace for overflow detection */
    if ( QueryPerformanceFrequency ( &counterFrequency ) )                   {
      stream -> perfCounterTicksPerBuffer . QuadPart                         =
        ( counterFrequency.QuadPart * stream->hostBufferSizeFrames)          /
        integerSampleRate                                                    ;
    } else                                                                   {
      stream -> perfCounterTicksPerBuffer . QuadPart = 0                     ;
    }                                                                        ;
  }                                                                          ;
  /* ------------------ INPUT                                               */
  if ( NULL != inputParameters )                                             {
    /* DeviceInfo * deviceInfo = deviceInfos[ inputParameters->device ]      ;
       DBUG(("PaHost_OpenStream: deviceID = 0x%x\n", inputParameters->device));
     */
    int sampleSizeBytes = Core::SampleSize(hostInputSampleFormat)            ;
    stream -> inputFrameSizeBytes  = inputParameters->channelCount           *
                                     sampleSizeBytes                         ;
    stream -> inputBufferSizeBytes = stream->hostBufferSizeFrames            *
                                     stream->inputFrameSizeBytes             ;
    if ( stream->inputBufferSizeBytes < DSBSIZE_MIN )                        {
      result = BufferTooSmall                                                ;
      goto error                                                             ;
    } else
    if ( stream->inputBufferSizeBytes > DSBSIZE_MAX )                        {
      result = BufferTooBig                                                  ;
      goto error                                                             ;
    }                                                                        ;
  }                                                                          ;
  /* open/create the DirectSound buffers                                    */
  /* interface ptrs should be zeroed when stream is zeroed.                 */
//  assert ( stream -> pDirectSoundCapture       == NULL )                     ;
//  assert ( stream -> pDirectSoundInputBuffer   == NULL )                     ;
//  assert ( stream -> pDirectSound              == NULL )                     ;
//  assert ( stream -> pDirectSoundPrimaryBuffer == NULL )                     ;
//  assert ( stream -> pDirectSoundOutputBuffer  == NULL )                     ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( NULL != inputParameters ) && ( NULL != outputParameters ) )         {
    /* try to use the full-duplex DX8 API to create the buffers.
       if that fails we fall back to the half-duplex API below              */
    hr = InitFullDuplexInputOutputBuffers                                    (
           stream                                                            ,
           (DirectSoundDeviceInfo *)deviceInfos[inputParameters  -> device]  ,
           hostInputSampleFormat                                             ,
           (WORD)inputParameters->channelCount                               ,
           stream->inputBufferSizeBytes                                      ,
           inputChannelMask                                                  ,
           (DirectSoundDeviceInfo *)deviceInfos[outputParameters -> device]  ,
           hostOutputSampleFormat                                            ,
           (WORD)outputParameters->channelCount                              ,
           stream->outputBufferSizeBytes                                     ,
           outputChannelMask                                                 ,
           integerSampleRate                                               ) ;
    dPrintf ( ( "InitFullDuplexInputOutputBuffers() returns %x\n", hr ) )    ;
    /* ignore any error returned by InitFullDuplexInputOutputBuffers.
       we retry opening the buffers below                                   */
  }                                                                          ;
  /* create half duplex buffers. also used for full-duplex streams which didn't
     succeed when using the full duplex API. that could happen because
     DX8 or greater isnt installed, the i/o devices aren't the same
     physical device. etc.                                                  */
  if ( ( NULL != outputParameters           )                               &&
       ( ! stream->pDirectSoundOutputBuffer )                              ) {
    hr = InitOutputBuffer                                                    (
           stream                                                            ,
           (DirectSoundDeviceInfo *)deviceInfos[outputParameters->device]    ,
           hostOutputSampleFormat                                            ,
           integerSampleRate                                                 ,
           (WORD)outputParameters -> channelCount                            ,
           stream -> outputBufferSizeBytes                                   ,
           outputChannelMask                                               ) ;
    dPrintf ( ( "InitOutputBuffer() returns %x\n" , hr ) )                   ;
    if ( DS_OK != hr )                                                       {
      result = UnanticipatedHostError                                        ;
//      PA_DS_SET_LAST_DIRECTSOUND_ERROR ( hr )                                ;
      goto error                                                             ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( NULL != inputParameters           )                                &&
       ( ! stream->pDirectSoundInputBuffer )                               ) {
    hr = InitInputBuffer                                                     (
           stream                                                            ,
           (DirectSoundDeviceInfo *)deviceInfos[inputParameters->device]     ,
           hostInputSampleFormat                                             ,
           integerSampleRate                                                 ,
           (WORD) inputParameters -> channelCount                            ,
           stream -> inputBufferSizeBytes                                    ,
           inputChannelMask                                                ) ;
    dPrintf ( ( "InitInputBuffer() returns %x\n" , hr ) )                    ;
    if ( DS_OK != hr )                                                       {
      dPrintf ( ( "CIOS Audio: InitInputBuffer() returns %x\n" , hr ) )      ;
      result = UnanticipatedHostError                                        ;
      goto error                                                             ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  SetStreamInfoLatencies                                                     (
    stream                                                                   ,
    framesPerCallback                                                        ,
    pollingPeriodFrames                                                      ,
    sampleRate                                                             ) ;
  stream -> sampleRate = sampleRate                                          ;
  *s = (Stream *)stream                                                      ;
  return result                                                              ;
error:
  if ( NULL == stream ) return result                                        ;
  if ( NULL != stream->processingCompleted )                                 {
    ::CloseHandle ( stream->processingCompleted )                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != stream->pDirectSoundOutputBuffer )                            {
    IDirectSoundBuffer_Stop    ( stream->pDirectSoundOutputBuffer )          ;
    IDirectSoundBuffer_Release ( stream->pDirectSoundOutputBuffer )          ;
    stream->pDirectSoundOutputBuffer = NULL                                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != stream->pDirectSoundPrimaryBuffer )                           {
    IDirectSoundBuffer_Release ( stream->pDirectSoundPrimaryBuffer )         ;
    stream->pDirectSoundPrimaryBuffer = NULL                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != stream->pDirectSoundInputBuffer )                             {
    IDirectSoundCaptureBuffer_Stop    ( stream->pDirectSoundInputBuffer )    ;
    IDirectSoundCaptureBuffer_Release ( stream->pDirectSoundInputBuffer )    ;
    stream->pDirectSoundInputBuffer = NULL                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != stream->pDirectSoundCapture )                                 {
    IDirectSoundCapture_Release ( stream->pDirectSoundCapture )              ;
    stream->pDirectSoundCapture = NULL                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != stream->pDirectSound )                                        {
    IDirectSound_Release ( stream->pDirectSound )                            ;
    stream->pDirectSound = NULL                                              ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != stream->pDirectSoundFullDuplex8 )                             {
    IDirectSoundFullDuplex_Release ( stream->pDirectSoundFullDuplex8 )       ;
    stream->pDirectSoundFullDuplex8 = NULL                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( bufferProcessorIsInitialized ) stream->bufferProcessor.Terminate()    ;
  if ( streamRepresentationIsInitialized ) stream->Terminate()               ;
  delete stream                                                              ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

void DirectSoundHostApi::Terminate(void)
{
  CaEraseAllocation                  ;
  CaComUninitialize                  (
    DirectSound                      ,
    &comInitializationResult       ) ;
  CaDsTerminateDSoundEntryPoints ( ) ;
  ////////////////////////////////////
  // delete myself by outsider
}

CaError DirectSoundHostApi::isSupported              (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double                    sampleRate       )
{
  DirectSoundDeviceInfo   * inputWinDsDeviceInfo                             ;
  DirectSoundDeviceInfo   * outputWinDsDeviceInfo                            ;
  DeviceInfo              * inputDeviceInfo                                  ;
  DeviceInfo              * outputDeviceInfo                                 ;
  CaDirectSoundStreamInfo * inputStreamInfo                                  ;
  CaDirectSoundStreamInfo * outputStreamInfo                                 ;
  int                       inputChannelCount                                ;
  int                       outputChannelCount                               ;
  CaSampleFormat            inputSampleFormat                                ;
  CaSampleFormat            outputSampleFormat                               ;
  CaError                   result                                           ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
     inputWinDsDeviceInfo = (DirectSoundDeviceInfo *)deviceInfos[ inputParameters->device ] ;
     inputDeviceInfo      = (DeviceInfo            *)&inputWinDsDeviceInfo   ;
     inputChannelCount    = inputParameters -> channelCount                  ;
     inputSampleFormat    = inputParameters -> sampleFormat                  ;
     /* unless alternate device specification is supported, reject the use of
        CaUseHostApiSpecificDeviceSpecification                             */
     if ( CaUseHostApiSpecificDeviceSpecification==inputParameters->device ) {
       return InvalidDevice                                                  ;
     }                                                                       ;
     /* check that input device can support inputChannelCount               */
     if ( ( 0 != inputWinDsDeviceInfo->deviceInputChannelCountIsKnown )     &&
          ( inputChannelCount > inputDeviceInfo->maxInputChannels     )    ) {
       return InvalidChannelCount                                            ;
     }                                                                       ;
     /* validate inputStreamInfo                                            */
     inputStreamInfo = (CaDirectSoundStreamInfo *)inputParameters->streamInfo ;
     result          = ValidateWinDirectSoundSpecificStreamInfo              (
                         inputParameters                                     ,
                         inputStreamInfo                                   ) ;
     if ( NoError != result ) return result                                  ;
  } else                                                                     {
    inputChannelCount = 0                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputWinDsDeviceInfo = (DirectSoundDeviceInfo *)deviceInfos[ outputParameters->device ] ;
    outputDeviceInfo      = (DeviceInfo            *)outputWinDsDeviceInfo   ;
    outputChannelCount    = outputParameters -> channelCount                 ;
    outputSampleFormat    = outputParameters -> sampleFormat                 ;
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    if ( CaUseHostApiSpecificDeviceSpecification==outputParameters->device ) {
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that output device can support inputChannelCount               */
    if ( ( 0 != outputWinDsDeviceInfo->deviceOutputChannelCountIsKnown )    &&
         ( outputChannelCount > outputDeviceInfo->maxOutputChannels    )   ) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate outputStreamInfo                                            */
    outputStreamInfo = (CaDirectSoundStreamInfo *)outputParameters->streamInfo ;
    result           = ValidateWinDirectSoundSpecificStreamInfo              (
                         outputParameters                                    ,
                         outputStreamInfo                                  ) ;
    if ( NoError != result ) return result                                   ;
  } else                                                                     {
    outputChannelCount = 0                                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
