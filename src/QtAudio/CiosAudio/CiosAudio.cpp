/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2015/02/01                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CiosAudioPrivate.hpp"

#include <memory.h>
#include <assert.h>

/* CA_VALIDATE_ENDIANNESS compares the compile time and runtime endianness,
 and raises an assertion if they don't match. <assert.h> must be included in
 the context in which this macro is used.                                   */

#if defined(NDEBUG)
  #define CA_VALIDATE_ENDIANNESS
#else
  #if defined(CA_LITTLE_ENDIAN)
    #define CA_VALIDATE_ENDIANNESS \
    { \
      const long nativeOne = 1; \
      assert( "CiosAudio: compile time and runtime endianness don't match" && (((char *)&nativeOne)[0]) == 1 ); \
    }
  #elif defined(CA_BIG_ENDIAN)
    #define CA_VALIDATE_ENDIANNESS \
    { \
      const long nativeOne = 1; \
      assert( "CiosAudio: compile time and runtime endianness don't match" && (((char *)&nativeOne)[0]) == 0 ); \
    }
  #endif
#endif

/* **************************************************************************
 * VALIDATE_TYPE_SIZES compares the size of the integer types at runtime to
 * ensure that PortAudio was configured correctly, and raises an assertion if
 * they don't match the expected values. <assert.h> must be included in the
 * context in which this macro is used.
 ****************************************************************************/

#define CA_VALIDATE_TYPE_SIZES                                                     \
  {                                                                                \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaUint8   ) == 1 ); \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaInt8    ) == 1 ); \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaUint16  ) == 2 ); \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaInt16   ) == 2 ); \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaUint32  ) == 4 ); \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaInt32   ) == 4 ); \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaInt64   ) == 8 ); \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaUint64  ) == 8 ); \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaFloat32 ) == 4 ); \
    assert( "CiosAudio: type sizes are not correct" && sizeof( CaFloat64 ) == 8 ); \
  }

//////////////////////////////////////////////////////////////////////////////

#define CA_VERSION      1511
#define CA_VERSION_TEXT "CIOS Audio Core Version 1.5.11 (built " __DATE__  " " __TIME__ ")"

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

static bool        FFmpegInitialized   = false ;
static int         initializationCount = 0     ;
static int         hostApisCount       = 0     ;
static int         deviceCount         = 0     ;
static HostApi  ** hostAPIs            = NULL  ;
static Core     *  globalAudioCore     = NULL  ;
static Debugger *  debugger            = NULL  ;
       Debugger *  globalDebugger      = NULL  ;

void ReplaceHostApiInitializer ( HostApiInitializer ** replacement )
{
  caHostApiInitializers = replacement ;
}

Core:: Core                 ( void )
     : defaultHostApiIndex  ( 0    )
     , firstOpenStream      ( NULL )
{
  if ( ! FFmpegInitialized )                 {
    #if defined(FFMPEGLIB)
    ::avcodec_register_all  (              ) ;
    ::av_register_all       (              ) ;
    ::av_log_set_level      ( AV_LOG_QUIET ) ;
    FFmpegInitialized = true                 ;
    #endif
  }                                          ;
}

Core::~Core (void)
{
}

int Core::Version(void) const
{
  return CA_VERSION ;
}

const char * Core::VersionString(void)
{
  return CA_VERSION_TEXT ;
}

Core * Core::instance(void)
{
  if ( NULL != globalAudioCore ) return globalAudioCore ;
  globalAudioCore = new Core ( )                        ;
  return globalAudioCore                                ;
}

bool Core::isInitialized (void)
{
  return ( 0 != initializationCount ) ;
}

bool Core::IsLittleEndian(void)
{
  #if defined(CA_LITTLE_ENDIAN)
  return true                 ;
  #else
  return false                ;
  #endif
}

bool Core::IsBigEndian(void)
{
  #if defined(CA_BIG_ENDIAN)
  return true                 ;
  #else
  return false                ;
  #endif
}

CaError Core::Initialize(void)
{
  CaError result = NoError                     ;
  dPrintf ( ( "CIOS Audio Core Initialize" ) ) ;
  if ( isInitialized() )                       {
    ++ initializationCount                     ;
    result = NoError                           ;
  } else                                       {
    CA_VALIDATE_TYPE_SIZES                     ;
    CA_VALIDATE_ENDIANNESS                     ;
    result = InitializeHostApis ( )            ;
    if ( NoError == result )                   {
      ++ initializationCount                   ;
    }                                          ;
  }                                            ;
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitError( "Initialize", result )    ;
  #endif
  return result                                ;
}

CaError Core::Terminate(void)
{
  CaError result = NoError                 ;
  CaLogApiEnter ( "Terminate" )            ;
  if ( isInitialized() )                   {
    -- initializationCount                 ;
    if ( ! isInitialized() )               {
      CloseStreams      ( )                ;
      TerminateHostApis ( )                ;
    }                                      ;
    result = NoError                       ;
  } else                                   {
    result = NotInitialized                ;
  }                                        ;
  CaLogApiExitError( "Terminate", result ) ;
  return result                            ;
}

int Core::SampleSize(int format)
{
  int result = SampleFormatNotSupported ;
  format    &= ~cafNonInterleaved       ;
  switch ( format )                     {
    case cafUint8                       :
    case cafInt8                        :
      result = 1                        ;
    break                               ;
    case cafInt16                       :
      result = 2                        ;
    break                               ;
    case cafInt24                       :
      result = 3                        ;
    break                               ;
    case cafFloat32                     :
    case cafInt32                       :
      result = 4                        ;
    break                               ;
  }                                     ;
  return result                         ;
}

bool Core::isValid(int format)
{
  switch ( format & ~cafNonInterleaved ) {
    case cafFloat32      : return true   ;
    case cafInt16        : return true   ;
    case cafInt32        : return true   ;
    case cafInt24        : return true   ;
    case cafInt8         : return true   ;
    case cafUint8        : return true   ;
    case cafCustomFormat : return true   ;
  }                                      ;
  return false                           ;
}

void Core::setDebugger(Debugger * debug)
{
  debugger       = debug                    ;
  globalDebugger = debug                    ;
  for (int i=0;i<hostApisCount;i++)         {
    hostAPIs [ i ] -> setDebugger(debugger) ;
  }                                         ;
}

int Core::HostAPIs(void)
{
  int result = 0                                   ;
  while( NULL != caHostApiInitializers[ result ] ) {
    ++ result                                      ;
  }                                                ;
  return result                                    ;
}

CaError Core::InitializeHostApis(void)
{
  CaError result           = NoError                                         ;
  int     initializerCount = HostAPIs ( )                                    ;
  int     baseDeviceIndex                                                    ;
  int     i                                                                  ;
  ////////////////////////////////////////////////////////////////////////////
  hostAPIs = new HostApi * [ initializerCount + 1 ]                          ;
  if ( NULL == hostAPIs ) return InsufficientMemory                          ;
  for ( i = 0 ; i <= initializerCount ; i++ ) hostAPIs [ i ] = NULL          ;
  ////////////////////////////////////////////////////////////////////////////
  hostAPIs [ initializerCount ] = NULL                                       ;
  hostApisCount                 =  0                                         ;
  defaultHostApiIndex           = -1                                         ;
  deviceCount                   =  0                                         ;
  baseDeviceIndex               =  0                                         ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < initializerCount ; ++i )                                 {
    hostAPIs [ hostApisCount ] = NULL                                        ;
    gPrintf ( ( "before caHostApiInitializers [ %d ].\n" , i ) )             ;
    //////////////////////////////////////////////////////////////////////////
    result = caHostApiInitializers [ i ]                                     (
               &hostAPIs [ hostApisCount ]                                   ,
                           hostApisCount                                   ) ;
    if ( NoError != result )                                                 {
      TerminateHostApis ( )                                                  ;
      return result                                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( NULL != hostAPIs [ hostApisCount ] )                                {
      gPrintf ( ( "after caHostApiInitializers [ %d ] <%s>.\n"               ,
                  i                                                          ,
                  hostAPIs [ hostApisCount ]->info.name                  ) ) ;
      HostApi * hostApi = hostAPIs [ hostApisCount ]                         ;
      if ( hostApi->info.defaultInputDevice >= hostApi->info.deviceCount )   {
        TerminateHostApis ( )                                                ;
        return InvalidHostApi                                                ;
      }                                                                      ;
      if ( hostApi->info.defaultOutputDevice >= hostApi->info.deviceCount )  {
        TerminateHostApis ( )                                                ;
        return InvalidHostApi                                                ;
      }                                                                      ;
      /* the first successfully initialized host API with a default input *or*
         output device is used as the default host API.                     */
      if ( (   -1       == defaultHostApiIndex                            ) &&
           ( ( CaNoDevice != hostApi->info.defaultInputDevice  )            ||
             ( CaNoDevice != hostApi->info.defaultOutputDevice )         ) ) {
         defaultHostApiIndex = hostApisCount                                 ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      hostApi -> baseDeviceIndex = baseDeviceIndex                           ;
      if ( CaNoDevice != hostApi->info.defaultInputDevice  )                 {
        hostApi -> info . defaultInputDevice  += baseDeviceIndex             ;
      }                                                                      ;
      if ( CaNoDevice != hostApi->info.defaultOutputDevice )                 {
        hostApi -> info . defaultOutputDevice += baseDeviceIndex             ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      baseDeviceIndex += hostApi -> info . deviceCount                       ;
      deviceCount     += hostApi -> info . deviceCount                       ;
      if ( NULL != debugger ) hostApi -> setDebugger ( debugger )            ;
      ++ hostApisCount                                                       ;
      ////////////////////////////////////////////////////////////////////////
    } else                                                                   {
      gPrintf ( ( "Fail to initialize caHostApiInitializers [ %d ].\n",i ) ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* if no host APIs have devices, the default host API is the first initialized host API */
  if ( -1 == defaultHostApiIndex ) defaultHostApiIndex = 0                   ;
  return result                                                              ;
}

void Core::TerminateHostApis(void)
{
  if ( NULL == hostAPIs ) return                          ;
  /* terminate in reverse order from initialization      */
  gPrintf ( ( "TerminateHostApis in \n" ) )               ;
  /////////////////////////////////////////////////////////
  while ( hostApisCount > 0 )                             {
    -- hostApisCount                                      ;
    gPrintf ( ( "Before Terminate %d : %s \n"             ,
                hostApisCount                             ,
                hostAPIs [ hostApisCount ]->info.name ) ) ;
    hostAPIs [ hostApisCount ] -> Terminate ( )           ;
    delete hostAPIs [ hostApisCount ]                     ;
    gPrintf ( ( "After Terminate %d \n",hostApisCount ) ) ;
  }                                                       ;
  hostApisCount       = 0                                 ;
  defaultHostApiIndex = 0                                 ;
  deviceCount         = 0                                 ;
  /////////////////////////////////////////////////////////
  if ( NULL != hostAPIs ) delete [] hostAPIs              ;
  /////////////////////////////////////////////////////////
  hostAPIs        = NULL                                  ;
  globalAudioCore = NULL                                  ;
  gPrintf ( ( "TerminateHostApis out\n" ) )               ;
}

int Core::FindHostApi(CaDeviceIndex device,int * hostSpecificDeviceIndex)
{
  int i = 0                                                    ;
  if ( device < 0         ) return -1                          ;
  if ( ! isInitialized()  ) return -1                          ;
  //////////////////////////////////////////////////////////////
  while ( ( i      <  hostApisCount                        )  &&
          ( device >= hostAPIs [ i ] -> info . deviceCount ) ) {
    device -= hostAPIs [ i ] -> info . deviceCount             ;
    ++i                                                        ;
  }                                                            ;
  //////////////////////////////////////////////////////////////
  if ( i >= hostApisCount ) return -1                          ;
  if ( 0 != hostSpecificDeviceIndex )                          {
    *hostSpecificDeviceIndex = device                          ;
  }                                                            ;
  //////////////////////////////////////////////////////////////
  return i                                                     ;
}

CaHostApiIndex Core::HostApiCount(void)
{
  int result                       ;
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnter ( "HostApiCount" ) ;
  #endif
  if ( ! isInitialized() )         {
    result = NotInitialized        ;
  } else                           {
    result = hostApisCount         ;
  }                                ;
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitErrorOrTResult       (
    "HostApiCount"                 ,
    "PaHostApiIndex: %d"           ,
    result                       ) ;
  #endif
  return (CaHostApiIndex) result   ;
}

CaHostApiIndex Core::TypeIdToIndex(CaHostApiTypeId type)
{
  CaHostApiIndex result                                            ;
  int            i                                                 ;
  //////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "TypeIdToIndex"                          ) ;
  CaLogApi            ( ("\tCaHostApiTypeId type: %d\n" , type ) ) ;
  #endif
  //////////////////////////////////////////////////////////////////
  if ( ! isInitialized() )                                         {
    result = NotInitialized                                        ;
  } else                                                           {
    result = HostApiNotFound                                       ;
    for ( i = 0 ; i < hostApisCount ; ++i )                        {
      if ( type == hostAPIs [ i ] -> info . type )                 {
        result = i                                                 ;
        break                                                      ;
      }                                                            ;
    }                                                              ;
  }                                                                ;
  //////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitErrorOrTResult ( "TypeIdToIndex"                     ,
                               "CaHostApiIndex: %d"                ,
                               result                            ) ;
  #endif
  //////////////////////////////////////////////////////////////////
  return result                                                    ;
}

CaHostApiIndex Core::DefaultHostApi(void)
{
  int result                                             ;
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnter ( "DefaultHostApi" )                     ;
  #endif
  ////////////////////////////////////////////////////////
  if ( ! isInitialized() )                               {
    result = NotInitialized                              ;
  } else                                                 {
    result = defaultHostApiIndex                         ;
    if ( ( result < 0 ) || ( result >= hostApisCount ) ) {
      result = InternalError                             ;
    }                                                    ;
  }                                                      ;
  ////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitErrorOrTResult                             (
    "DefaultHostApi"                                     ,
    "CaHostApiIndex: %d"                                 ,
    result                                             ) ;
  #endif
  ////////////////////////////////////////////////////////
  return result                                          ;
}

CaHostApiIndex Core::setDefaultHostApi(CaHostApiIndex index)
{
  int result                                             ;
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnter ( "setDefaultHostApi" )                  ;
  #endif
  ////////////////////////////////////////////////////////
  if ( ! isInitialized() )                               {
    result = NotInitialized                              ;
  } else                                                 {
    result = index                                       ;
    if ( ( result < 0 ) || ( result >= hostApisCount ) ) {
      result = InternalError                             ;
    } else                                               {
      defaultHostApiIndex = index                        ;
    }                                                    ;
  }                                                      ;
  ////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitErrorOrTResult                             (
    "DefaultHostApi"                                     ,
    "CaHostApiIndex: %d"                                 ,
    result                                             ) ;
  #endif
  ////////////////////////////////////////////////////////
  return result                                          ;
}

CaError Core::GetHostApi(HostApi ** hostApi,CaHostApiTypeId type)
{
  if ( ! isInitialized() )                               {
    return NotInitialized                                ;
  } else                                                 {
    for ( int i = 0 ; i < hostApisCount ; ++i )          {
      if ( type == hostAPIs [ i ] -> info . type )       {
        *hostApi = hostAPIs [ i ]                        ;
        return NoError                                   ;
      }                                                  ;
    }                                                    ;
  }                                                      ;
  ////////////////////////////////////////////////////////
  return HostApiNotFound                                 ;
}

CaDeviceIndex Core::ToDeviceIndex                 (
                CaHostApiIndex hostApi            ,
                int            hostApiDeviceIndex )
{
  CaDeviceIndex result                                                       ;
  ////////////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "ToDeviceIndex"                                    ) ;
  CaLogApi ( ( "CaHostApiIndex hostApi: %d\n"   , hostApi                ) ) ;
  CaLogApi ( ( "\tint hostApiDeviceIndex: %d\n" , hostApiDeviceIndex     ) ) ;
  #endif
  ////////////////////////////////////////////////////////////////////////////
  if ( ! isInitialized ( ) )                                                 {
    result = NotInitialized                                                  ;
  } else                                                                     {
    if ( ( hostApi < 0 ) || ( hostApi >= hostApisCount )                   ) {
      result = InvalidHostApi                                                ;
    } else                                                                   {
      if ( ( hostApiDeviceIndex <  0                                     )  ||
           ( hostApiDeviceIndex >= hostAPIs[hostApi]->info.deviceCount   ) ) {
        result = InvalidDevice                                               ;
      } else                                                                 {
        result = hostAPIs [hostApi]->baseDeviceIndex + hostApiDeviceIndex    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitErrorOrTResult ( "ToDeviceIndex","CaDeviceIndex: %d" ,result ) ;
  #endif
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

HostApiInfo * Core::GetHostApiInfo(CaHostApiIndex hostApi)
{
  HostApiInfo * info                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "GetHostApiInfo"                                   ) ;
  CaLogApi            ( ("\tCaHostApiIndex hostApi: %d\n" , hostApi )      ) ;
  #endif
  ////////////////////////////////////////////////////////////////////////////
  if ( ! isInitialized() )                                                   {
    info = NULL                                                              ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "GetHostApiInfo returned:\n"                            ) ) ;
    CaLogApi ( ( "\tHostApiInfo*: NULL [ CIOS Audio not initialized ]\n" ) ) ;
    #endif
  } else
  if ( ( hostApi < 0 ) || ( hostApi >= hostApisCount )                     ) {
    info = NULL                                                              ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "GetHostApiInfo returned:\n"                            ) ) ;
    CaLogApi ( ( "\tHostApiInfo*: NULL [ hostApi out of range ]\n"       ) ) ;
    #endif
  } else                                                                     {
    info = & hostAPIs [ hostApi ] -> info                                    ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "GetHostApiInfo returned:\n"                            ) ) ;
    CaLogApi ( ( "\tHostApiInfo*: 0x%p\n"         , info                 ) ) ;
    CaLogApi ( ( "\t{\n"                                                 ) ) ;
    CaLogApi ( ( "\t\tint structVersion: %d\n"    , info->structVersion  ) ) ;
    CaLogApi ( ( "\t\tHostApiTypeId type: %d\n"   , info->type           ) ) ;
    CaLogApi ( ( "\t\tconst char *name: %s\n"     , info->name           ) ) ;
    CaLogApi ( ( "\t}\n"                                                 ) ) ;
    #endif
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return info                                                                ;
}

CaDeviceIndex Core::DeviceCount(void)
{
  CaDeviceIndex result            ;
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnter ( "DeviceCount" ) ;
  #endif
  /////////////////////////////////
  if ( ! isInitialized ( )      ) {
    result = NotInitialized       ;
  } else                          {
    result = deviceCount          ;
  }                               ;
  /////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitErrorOrTResult      (
    "DeviceCount"                 ,
    "CaDeviceIndex: %d"           ,
    result                      ) ;
  #endif
  /////////////////////////////////
  return result                   ;
}

CaDeviceIndex Core::DefaultInputDevice(void)
{
  CaHostApiIndex hostApi                                       ;
  CaDeviceIndex  result                                        ;
  //////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnter ( "DefaultInputDevice" )                       ;
  #endif
  //////////////////////////////////////////////////////////////
  hostApi = DefaultHostApi ( )                                 ;
  if ( hostApi < 0 )                                           {
    result = CaNoDevice                                        ;
  } else                                                       {
    result = hostAPIs [ hostApi ] -> info . defaultInputDevice ;
  }                                                            ;
  //////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitT ( "DefaultInputDevice"                         ,
                  "CaDeviceIndex: %d"                          ,
                  result                                     ) ;
  #endif
  //////////////////////////////////////////////////////////////
  return result                                                ;
}

CaDeviceIndex Core::DefaultOutputDevice(void)
{
  CaHostApiIndex hostApi                                        ;
  CaDeviceIndex  result                                         ;
  ///////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnter ( "DefaultOutputDevice" )                       ;
  #endif
  ///////////////////////////////////////////////////////////////
  hostApi = DefaultHostApi ( )                                  ;
  if ( hostApi < 0 )                                            {
    result = CaNoDevice                                         ;
  } else                                                        {
    result = hostAPIs [ hostApi ] -> info . defaultOutputDevice ;
  }                                                             ;
  ///////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitT ( "DefaultOutputDevice"                         ,
                  "CaDeviceIndex: %d"                           ,
                  result                                      ) ;
  #endif
  ///////////////////////////////////////////////////////////////
  return result                                                 ;
}

CaDeviceIndex Core::DefaultDecoderDevice (void)
{
  CaDeviceIndex result = CaNoDevice                             ;
  ///////////////////////////////////////////////////////////////
  for (int i=0;(result == CaNoDevice) && i<DeviceCount();i++)   {
    DeviceInfo * dev                                            ;
    dev = GetDeviceInfo((CaDeviceIndex)i)                       ;
    if ( NULL != dev && dev->maxInputChannels>0)                {
      CaHostApiTypeId hid = dev->hostType                       ;
      if ( FFMPEG == hid ) result = i                           ;
      if ( VLC    == hid ) result = i                           ;
    }                                                           ;
  }                                                             ;
  ///////////////////////////////////////////////////////////////
  return result                                                 ;
}

CaDeviceIndex Core::DefaultEncoderDevice (void)
{
  CaDeviceIndex result = CaNoDevice                             ;
  ///////////////////////////////////////////////////////////////
  for (int i=0;(result == CaNoDevice) && i<DeviceCount();i++)   {
    DeviceInfo * dev                                            ;
    dev = GetDeviceInfo((CaDeviceIndex)i)                       ;
    if ( NULL != dev && dev->maxOutputChannels>0)               {
      CaHostApiTypeId hid = dev->hostType                       ;
      if ( FFMPEG == hid ) result = i                           ;
      if ( VLC    == hid ) result = i                           ;
    }                                                           ;
  }                                                             ;
  ///////////////////////////////////////////////////////////////
  return result                                                 ;
}

DeviceInfo * Core::GetDeviceInfo(CaDeviceIndex device)
{
  DeviceInfo * result       = NULL                                           ;
  int          hostSpecificDeviceIndex                                       ;
  int          hostApiIndex = FindHostApi(device,&hostSpecificDeviceIndex)   ;
  ////////////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "GetDeviceInfo" )                                    ;
  CaLogApi            ( ( "\tCaDeviceIndex device: %d\n", device ) )         ;
  #endif
  ////////////////////////////////////////////////////////////////////////////
  if ( hostApiIndex < 0 )                                                    {
    result = NULL                                                            ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi (( "GetDeviceInfo returned:\n"                               )) ;
    CaLogApi (( "\tDeviceInfo * NULL [ invalid device index ]\n"          )) ;
    #endif
  } else                                                                     {
    result = hostAPIs [hostApiIndex]->deviceInfos[hostSpecificDeviceIndex]   ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi (("GetDeviceInfo returned:\n"                                )) ;
    CaLogApi (("\tDeviceInfo*: 0x%p:\n",result                            )) ;
    CaLogApi (("\t{\n"                                                    )) ;
    CaLogApi (("\t\tint structVersion: %d\n",result->structVersion        )) ;
    CaLogApi (("\t\tconst char *name: %s\n",result->name                  )) ;
    CaLogApi (("\t\tPaHostApiIndex hostApi: %d\n",result->hostApi         )) ;
    CaLogApi (("\t\tint maxInputChannels: %d\n",result->maxInputChannels  )) ;
    CaLogApi (("\t\tint maxOutputChannels: %d\n",result->maxOutputChannels)) ;
    CaLogApi (("\t}\n"                                                    )) ;
    #endif
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

void Core::Add(Stream * stream)
{
  stream -> next  = firstOpenStream ;
  firstOpenStream = stream          ;
}

void Core::Remove(Stream * stream)
{
  Stream * previous = NULL                 ;
  Stream * current  = firstOpenStream      ;
  //////////////////////////////////////////
  while ( NULL != current                ) {
    if ( current == stream               ) {
      if ( NULL  == previous             ) {
        firstOpenStream  = current -> next ;
      } else                               {
        previous -> next = current -> next ;
      }                                    ;
      return                               ;
    } else                                 {
      previous = current                   ;
      current  = current -> next           ;
    }                                      ;
  }                                        ;
}

void Core::CloseStreams(void)
{
  while ( NULL != firstOpenStream ) {
    Close ( firstOpenStream  )      ;
  }                                 ;
  firstOpenStream = NULL            ;
}

CaError Core::ValidateStreamPointer(Stream * stream)
{
  if ( ! isInitialized()   ) return NotInitialized ;
  if (   NULL == stream    ) return BadStreamPtr   ;
  if ( ! stream->isValid() ) return BadStreamPtr   ;
  return NoError                                   ;
}

CaError Core::setConduit(Stream * stream,Conduit * conduit)
{
  CaError result = ValidateStreamPointer ( stream )                  ;
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "setConduit"                               ) ;
  CaLogApi            ( ("\tStream  * stream  : 0x%p\n", stream  ) ) ;
  CaLogApi            ( ("\tConduit * conduit : 0x%p\n", conduit ) ) ;
  #endif
  ////////////////////////////////////////////////////////////////////
  if ( NoError == result )                                           {
    result = stream -> IsStopped ( )                                 ;
    if ( 0 == result ) result = StreamIsNotStopped                   ;
    if ( 1 == result )                                               {
      stream -> conduit = conduit                                    ;
      result            = NoError                                    ;
    }                                                                ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitError ( "setConduit" , result )                        ;
  #endif
  ////////////////////////////////////////////////////////////////////
  return result                                                      ;
}

CaError Core::Start(Stream * stream)
{
  CaError result = ValidateStreamPointer ( stream )               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "Start"                                 ) ;
  CaLogApi            ( ("\tStream * stream: 0x%p\n" , stream ) ) ;
  #endif
  /////////////////////////////////////////////////////////////////
  if ( NoError == result )                                        {
    result = stream -> IsStopped ( )                              ;
    if ( 0 == result )                                            {
      result = StreamIsNotStopped                                 ;
    } else
    if ( 1 == result )                                            {
      result = stream -> Start ( )                                ;
    }                                                             ;
  }                                                               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitError ( "Start" , result )                          ;
  #endif
  /////////////////////////////////////////////////////////////////
  return result                                                   ;
}

CaError Core::Stop(Stream * stream)
{
  CaError result = ValidateStreamPointer ( stream )               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "Stop"                                  ) ;
  CaLogApi            ( ("\tStream * stream: 0x%p\n" , stream ) ) ;
  #endif
  /////////////////////////////////////////////////////////////////
  if ( NoError == result )                                        {
    result = stream -> IsStopped ( )                              ;
    if ( 0 == result )                                            {
      result = stream -> Stop ( )                                 ;
    } else
    if ( 1 == result )                                            {
      result = StreamIsStopped                                    ;
    }                                                             ;
  }                                                               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( StreamIsStopped != result && NoError != result )           {
    CaLogApiExitError ( "Stop" , result )                         ;
  }                                                               ;
  #endif
  /////////////////////////////////////////////////////////////////
  return result                                                   ;
}

CaError Core::Close(Stream * stream)
{
  CaError result = ValidateStreamPointer ( stream )               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "Close"                                 ) ;
  CaLogApi            ( ("\tStream * stream: 0x%p\n" , stream ) ) ;
  #endif
  /////////////////////////////////////////////////////////////////
  /* always remove the open stream from our list, even if this function
     eventually returns an error. Otherwise CloseOpenStreams() will
     get stuck in an infinite loop                               */
  Remove ( stream )                                               ;
  /* be sure to call this _before_ closing the stream            */
  /////////////////////////////////////////////////////////////////
  if ( NoError == result )                                        {
    /* abort the stream if it isn't stopped                      */
    result = stream -> IsStopped ( )                              ;
    if ( 1 == result )                                            {
      result = NoError                                            ;
    } else
    if ( 0 == result )                                            {
      result = stream -> Abort ( )                                ;
    }                                                             ;
    if ( NoError == result ) result = stream -> Close ( )         ;
  }                                                               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitError ( "Close" , result )                          ;
  #endif
  /////////////////////////////////////////////////////////////////
  return result                                                   ;
}

CaError Core::Abort(Stream * stream)
{
  CaError result = ValidateStreamPointer ( stream )               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "Abort"                                 ) ;
  CaLogApi            ( ("\tStream * stream: 0x%p\n" , stream ) ) ;
  #endif
  /////////////////////////////////////////////////////////////////
  if ( NoError == result )                                        {
    result = stream -> IsStopped ( )                              ;
    if ( 0 == result )                                            {
      result = stream -> Abort ( )                                ;
    } else
    if ( 1 == result )                                            {
      result = StreamIsStopped                                    ;
    }                                                             ;
  }                                                               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitError ( "Abort" , result )                          ;
  #endif
  /////////////////////////////////////////////////////////////////
  return result                                                   ;
}

CaError Core::IsStopped(Stream * stream)
{
  CaError result = ValidateStreamPointer ( stream )                ;
  //////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
//  CaLogApiEnterParams ( "IsStopped"                              ) ;
//  CaLogApi            ( ( "\tStream * stream: 0x%p\n" , stream ) ) ;
  #endif
  //////////////////////////////////////////////////////////////////
  if ( NoError == result ) result = stream -> IsStopped ( )        ;
  //////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( result != 1 )                                               {
//    CaLogApiExitError   ( "IsStopped" , result                   ) ;
  }                                                                ;
  #endif
  //////////////////////////////////////////////////////////////////
  return result                                                    ;
}

CaError Core::IsActive(Stream * stream)
{
  CaError result = ValidateStreamPointer ( stream )                ;
  //////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
//  CaLogApiEnterParams ( "IsActive"                               ) ;
//  CaLogApi            ( ( "\tStream * stream: 0x%p\n" , stream ) ) ;
  #endif
  //////////////////////////////////////////////////////////////////
  if ( NoError == result ) result = stream->IsActive()             ;
  //////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
//  CaLogApiExitError ( "IsActive" , result )                        ;
  #endif
  //////////////////////////////////////////////////////////////////
  return result                                                    ;
}

CaTime Core::GetTime(Stream * stream)
{
  CaError error = ValidateStreamPointer ( stream )                 ;
  CaTime  result                                                   ;
  //////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "GetStreamTime"                          ) ;
  CaLogApi            ( ( "\tStream * stream: 0x%p\n" , stream ) ) ;
  #endif
  //////////////////////////////////////////////////////////////////
  if ( NoError != error )                                          {
    result = 0                                                     ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "GetTime returned:\n"                         ) ) ;
    CaLogApi ( ( "\tCaTime: 0 [CaError error:%d ( %s )]\n"         ,
                 result                                            ,
                 error                                             ,
                 debugger->Error( error )                      ) ) ;
    #endif
  } else                                                           {
    result = stream -> GetTime ( )                                 ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "GetTime returned:\n"                         ) ) ;
    CaLogApi ( ( "\tCaTime: %g\n", result                      ) ) ;
    #endif
  }                                                                ;
  //////////////////////////////////////////////////////////////////
  return result                                                    ;
}

double Core::GetCpuLoad(Stream * stream)
{
  CaError error = ValidateStreamPointer ( stream )                ;
  double  result                                                  ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "GetCpuLoad"                            ) ;
  CaLogApi            ( ( "\tStream * stream: 0x%p\n", stream ) ) ;
  #endif
  /////////////////////////////////////////////////////////////////
  if ( NoError != error )                                         {
    result = 0.0                                                  ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "GetCpuLoad returned:\n"                     ) ) ;
    CaLogApi ( ( "\tdouble: 0.0 [CaError error: %d ( %s )]\n"     ,
                 error                                            ,
                 debugger -> Error ( error )                  ) ) ;
    #endif
  } else                                                          {
    result = stream -> GetCpuLoad ( )                             ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "GetCpuLoad returned:\n"                     ) ) ;
    CaLogApi ( ( "\tdouble: %g\n", result                     ) ) ;
    #endif
  }                                                               ;
  /////////////////////////////////////////////////////////////////
  return result                                                   ;
}

CaError Core::Read(Stream * stream,void * buffer,unsigned long frames)
{
  CaError result = ValidateStreamPointer ( stream )               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "Read"                                  ) ;
  CaLogApi            ( ("\tStream * stream: 0x%p\n", stream  ) ) ;
  #endif
  /////////////////////////////////////////////////////////////////
  if ( NoError == result )                                        {
    if ( 0 == frames )                                            {
      result = NoError                                            ;
    } else
    if ( 0 == buffer )                                            {
      result = BadBufferPtr                                       ;
    } else                                                        {
      result = stream -> IsStopped ( )                            ;
      if ( 0 == result )                                          {
        result = stream -> Read ( buffer , frames )               ;
      } else
      if ( 1 == result )                                          {
        result = StreamIsStopped                                  ;
      }                                                           ;
    }                                                             ;
  }                                                               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitError ( "Read" , result )                           ;
  #endif
  /////////////////////////////////////////////////////////////////
  return result                                                   ;
}

CaError Core::Write(Stream * stream,const void * buffer,unsigned long frames)
{
  CaError result = ValidateStreamPointer ( stream )               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "Write" )                                 ;
  CaLogApi            ( ("\tStream * stream: 0x%p\n", stream )  ) ;
  #endif
  /////////////////////////////////////////////////////////////////
  if ( NoError == result )                                        {
    if ( 0 == frames )                                            {
      result = NoError                                            ;
    } else
    if ( 0 == buffer )                                            {
      result = BadBufferPtr                                       ;
    } else                                                        {
      result = stream -> IsStopped ( )                            ;
      if ( 0 == result )                                          {
        result = stream -> Write ( buffer , frames )              ;
      } else
      if ( 1 == result )                                          {
        result = StreamIsStopped                                  ;
      }                                                           ;
    }                                                             ;
  }                                                               ;
  /////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiExitError ( "Write" , result )                          ;
  #endif
  /////////////////////////////////////////////////////////////////
  return result                                                   ;
}

CaInt32 Core::ReadAvailable(Stream * stream)
{
  CaError error = ValidateStreamPointer ( stream )                   ;
  CaInt32 result                                                     ;
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "ReadAvailable"                         )    ;
  CaLogApi            ( ("\tStream * stream: 0x%p\n" , stream ) )    ;
  #endif
  ////////////////////////////////////////////////////////////////////
  if ( NoError != error )                                            {
    result = 0                                                       ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "ReadAvailable returned:\n"                     ) ) ;
    CaLogApi ( ( "\tunsigned long: 0 [ CaError error: %d ( %s ) ]\n" ,
                 error, debugger->Error( error                 ) ) ) ;
    #endif
  } else                                                             {
    result = stream -> ReadAvailable ( )                             ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "ReadAvailable returned:\n"                     ) ) ;
    CaLogApi ( ( "\tCaError: %d ( %s )\n"                            ,
                 result                                              ,
                 debugger->Error ( result )                      ) ) ;
    #endif
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  return result                                                      ;
}

CaInt32 Core::WriteAvailable(Stream * stream)
{
  CaError error = ValidateStreamPointer ( stream )                   ;
  CaInt32 result                                                     ;
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "WriteAvailable"                        )    ;
  CaLogApi            ( ("\tStream * stream: 0x%p\n" , stream ) )    ;
  #endif
  ////////////////////////////////////////////////////////////////////
  if ( NoError != error )                                            {
    result = 0                                                       ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "WriteAvailable returned:\n"                    ) ) ;
    CaLogApi ( ( "\tunsigned long: 0 [ CaError error: %d ( %s ) ]\n" ,
                 error                                               ,
                 debugger->Error(error)                          ) ) ;
    #endif
  } else                                                             {
    result = stream -> WriteAvailable ( )                            ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "WriteAvailable returned:\n"                    ) ) ;
    CaLogApi ( ( "\tCaError: %d ( %s )\n"                            ,
                 result                                              ,
                 debugger->Error( result )                       ) ) ;
    #endif
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  return result                                                      ;
}

bool Core::hasVolume(Stream * stream)
{
  return stream->hasVolume() ;
}

CaVolume Core::MinVolume(Stream * stream)
{
  return stream->MinVolume() ;
}

CaVolume Core::MaxVolume(Stream * stream)
{
  return stream->MaxVolume() ;
}

CaVolume Core::Volume         (
           Stream * stream    ,
           int      atChannel )
{
  return stream -> Volume ( atChannel ) ;
}

CaVolume Core::setVolume      (
           Stream * stream    ,
           CaVolume volume    ,
           int      atChannel )
{
  return stream -> setVolume ( volume , atChannel ) ;
}

CaError Core::isSupported                           (
          const StreamParameters * inputParameters  ,
          const StreamParameters * outputParameters ,
          double                   sampleRate       )
{
  if ( CaNotNull ( inputParameters  ) )                              {
    if ( inputParameters  -> device >= deviceCount )                 {
      return InvalidDevice                                           ;
    }                                                                ;
  }                                                                  ;
  if ( CaNotNull ( outputParameters ) )                              {
    if ( outputParameters -> device >= deviceCount )                 {
      return InvalidDevice                                           ;
    }                                                                ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  CaError            result                                          ;
  HostApi          * hostApi             = NULL                      ;
  CaDeviceIndex      hostApiInputDevice  = CaNoDevice                ;
  CaDeviceIndex      hostApiOutputDevice = CaNoDevice                ;
  StreamParameters   hostApiInputParameters                          ;
  StreamParameters   hostApiOutputParameters                         ;
  StreamParameters * hostApiInputParametersPtr                       ;
  StreamParameters * hostApiOutputParametersPtr                      ;
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "isSupported" )                              ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL == inputParameters  )                                    {
    CaLogApi ( ( "\tStreamParameters * inputParameters: NULL\n"  ) ) ;
  } else                                                             {
    CaLogApi ( ( "\tStreamParameters * inputParameters: 0x%p\n", inputParameters )) ;
    CaLogApi ( ( "\tCaDeviceIndex inputParameters->device: %d\n", inputParameters->device )) ;
    CaLogApi ( ( "\tint inputParameters->channelCount: %d\n", inputParameters->channelCount )) ;
    CaLogApi ( ( "\tCaSampleFormat inputParameters->sampleFormat: %d\n", inputParameters->sampleFormat )) ;
    CaLogApi ( ( "\tCaTime inputParameters->suggestedLatency: %f\n", inputParameters->suggestedLatency )) ;
    CaLogApi ( ( "\tvoid * inputParameters->streamInfo: 0x%p\n", inputParameters->streamInfo )) ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL == outputParameters )                                    {
    CaLogApi ( ( "\tStreamParameters * outputParameters: NULL\n" ) ) ;
  } else                                                             {
    CaLogApi ( ( "\tStreamParameters * outputParameters: 0x%p\n", outputParameters )) ;
    CaLogApi ( ( "\tCaDeviceIndex outputParameters->device: %d\n", outputParameters->device )) ;
    CaLogApi ( ( "\tint outputParameters->channelCount: %d\n", outputParameters->channelCount )) ;
    CaLogApi ( ( "\tCaSampleFormat outputParameters->sampleFormat: %d\n", outputParameters->sampleFormat )) ;
    CaLogApi ( ( "\tCaTime outputParameters->suggestedLatency: %f\n", outputParameters->suggestedLatency )) ;
    CaLogApi ( ( "\tvoid * outputParameters->streamInfo: 0x%p\n", outputParameters->streamInfo )) ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  CaLogApi ( ( "\tdouble sampleRate: %g\n" , sampleRate ) )          ;
  #endif
  ////////////////////////////////////////////////////////////////////
  if ( ! isInitialized() )                                           {
    result = NotInitialized                                          ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApiExitError ( "isSupported" , result )                     ;
    #endif
    return result                                                    ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  result = ValidateStream ( inputParameters                          ,
                            outputParameters                         ,
                            sampleRate                               ,
                            0                                        ,
                            csfNoFlag                                ,
                            0                                        ,
                            &hostApi                                 ,
                            &hostApiInputDevice                      ,
                            &hostApiOutputDevice                   ) ;
  if ( NoError != result )                                           {
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApiExitError ( "isSupported" , result )                     ;
    #endif
    return result                                                    ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                     {
    hostApiInputParameters.device                    = hostApiInputDevice                         ;
    hostApiInputParameters.channelCount              = inputParameters->channelCount              ;
    hostApiInputParameters.sampleFormat              = inputParameters->sampleFormat              ;
    hostApiInputParameters.suggestedLatency          = inputParameters->suggestedLatency          ;
    hostApiInputParameters.streamInfo = inputParameters->streamInfo ;
    hostApiInputParametersPtr                        = &hostApiInputParameters                    ;
  } else                                                             {
    hostApiInputParametersPtr                        = NULL          ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                    {
    hostApiOutputParameters.device                    = hostApiOutputDevice                         ;
    hostApiOutputParameters.channelCount              = outputParameters->channelCount              ;
    hostApiOutputParameters.sampleFormat              = outputParameters->sampleFormat              ;
    hostApiOutputParameters.suggestedLatency          = outputParameters->suggestedLatency          ;
    hostApiOutputParameters.streamInfo = outputParameters->streamInfo ;
    hostApiOutputParametersPtr                        = &hostApiOutputParameters                    ;
  } else                                                             {
    hostApiOutputParametersPtr                        = NULL         ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  result = hostApi -> isSupported ( hostApiInputParametersPtr        ,
                                    hostApiOutputParametersPtr       ,
                                    sampleRate                     ) ;
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApi ( ( "isSupported returned:\n" ) )                         ;
  if ( 0 == result )                                                 {
    CaLogApi ( ( "\tCaError: 0 [ FormatIsSupported ]\n"          ) ) ;
  } else                                                             {
    CaLogApi ( ( "\tCaError: %d ( %s )\n"                            ,
                 result                                              ,
                 debugger -> Error ( result ) )                    ) ;
  }                                                                  ;
  #endif
  ////////////////////////////////////////////////////////////////////
  return result                                                      ;
}

CaError Core::Open                                   (
          Stream                 ** stream           ,
          const StreamParameters *  inputParameters  ,
          const StreamParameters *  outputParameters ,
          double                    sampleRate       ,
          CaUint32                  framesPerBuffer  ,
          CaStreamFlags             streamFlags      ,
          Conduit                *  CONDUIT          )
{
  if ( CaNotNull ( inputParameters  ) )                              {
    if ( inputParameters  -> device >= deviceCount )                 {
      return InvalidDevice                                           ;
    }                                                                ;
  }                                                                  ;
  if ( CaNotNull ( outputParameters ) )                              {
    if ( outputParameters -> device >= deviceCount )                 {
      return InvalidDevice                                           ;
    }                                                                ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  CaError            result                     = NoError            ;
  HostApi          * hostApi                    = NULL               ;
  CaDeviceIndex      hostApiInputDevice         = CaNoDevice         ;
  CaDeviceIndex      hostApiOutputDevice        = CaNoDevice         ;
  StreamParameters   hostApiInputParameters                          ;
  StreamParameters   hostApiOutputParameters                         ;
  StreamParameters * hostApiInputParametersPtr  = NULL               ;
  StreamParameters * hostApiOutputParametersPtr = NULL               ;
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "Open"                                     ) ;
  CaLogApi            ( ( "\tStream ** stream: 0x%p\n", stream   ) ) ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL == inputParameters )                                     {
    CaLogApi ( ( "\tStreamParameters *inputParameters: NULL\n"   ) ) ;
  } else                                                             {
    CaLogApi ( ( "\tStreamParameters *inputParameters: 0x%p\n", inputParameters )) ;
    CaLogApi ( ( "\tCaDeviceIndex inputParameters->device: %d\n", inputParameters->device )) ;
    CaLogApi ( ( "\tint inputParameters->channelCount: %d\n", inputParameters->channelCount )) ;
    CaLogApi ( ( "\tCaSampleFormat inputParameters->sampleFormat: %d\n", inputParameters->sampleFormat )) ;
    CaLogApi ( ( "\tCaTime inputParameters->suggestedLatency: %f\n", inputParameters->suggestedLatency )) ;
    CaLogApi ( ( "\tvoid *inputParameters->streamInfo: 0x%p\n", inputParameters->streamInfo )) ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL == outputParameters )                                    {
    CaLogApi ( ( "\tStreamParameters *outputParameters: NULL\n" )) ;
  } else                                                             {
    CaLogApi ( ( "\tStreamParameters *outputParameters: 0x%p\n", outputParameters )) ;
    CaLogApi ( ( "\tCaDeviceIndex outputParameters->device: %d\n", outputParameters->device )) ;
    CaLogApi ( ( "\tint outputParameters->channelCount: %d\n", outputParameters->channelCount )) ;
    CaLogApi ( ( "\tCaSampleFormat outputParameters->sampleFormat: %d\n", outputParameters->sampleFormat )) ;
    CaLogApi ( ( "\tCaTime outputParameters->suggestedLatency: %f\n", outputParameters->suggestedLatency )) ;
    CaLogApi ( ( "\tvoid *outputParameters->streamInfo: 0x%p\n", outputParameters->streamInfo )) ;
  }                                                                  ;
  #endif
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApi   ( ( "\tdouble sampleRate: %g\n", sampleRate ) ) ;
  CaLogApi   ( ( "\tunsigned long framesPerBuffer: %d\n", framesPerBuffer ) ) ;
  CaLogApi   ( ( "\tCaStreamFlags streamFlags: 0x%x\n",streamFlags ) ) ;
  CaLogApi   ( ( "\tConduit *conduit: 0x%p\n", CONDUIT           ) ) ;
  #endif
  ////////////////////////////////////////////////////////////////////
  if ( ! isInitialized() )                                           {
    result = NotInitialized                                          ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "Open returned:\n"                              ) ) ;
    CaLogApi ( ( "\t*(Stream ** stream): undefined\n"            ) ) ;
    CaLogApi ( ( "\tCaError: %d ( %s )\n"                            ,
                 result                                              ,
                 debugger -> Error ( result )                    ) ) ;
    #endif
    return result                                                    ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  /* Check for parameter errors.
     NOTE: make sure this validation list is kept syncronised with the one in CaHostApi.cpp
   */
  if ( NULL == stream )                                              {
    result = BadStreamPtr                                            ;
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "Open returned:\n"                              ) ) ;
    CaLogApi ( ( "\t*(Stream ** stream): undefined\n"            ) ) ;
    CaLogApi ( ( "\tCaError: %d ( %s )\n"                            ,
                 result                                              ,
                 debugger -> Error ( result )                    ) ) ;
    #endif
    return result                                                    ;
  }                                                                  ;
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( NULL != debugger )                                            {
    debugger->printf("ValidateStream\n")                             ;
  }                                                                  ;
  #endif
  ////////////////////////////////////////////////////////////////////
  result = ValidateStream ( inputParameters                          ,
                            outputParameters                         ,
                            sampleRate                               ,
                            framesPerBuffer                          ,
                            streamFlags                              ,
                            CONDUIT                                  ,
                            &hostApi                                 ,
                            &hostApiInputDevice                      ,
                            &hostApiOutputDevice                   ) ;
  if ( NoError != result )                                           {
    #ifndef REMOVE_DEBUG_MESSAGE
    CaLogApi ( ( "Open returned:\n"                              ) ) ;
    CaLogApi ( ( "\t*(Stream ** stream): undefined\n"            ) ) ;
    CaLogApi ( ( "\tCaError: %d ( %s )\n"                            ,
                 result                                              ,
                 debugger->Error( result )                       ) ) ;
    #endif
    return result                                                    ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                     {
    hostApiInputParameters.device                    = hostApiInputDevice                         ;
    hostApiInputParameters.channelCount              = inputParameters->channelCount              ;
    hostApiInputParameters.sampleFormat              = inputParameters->sampleFormat              ;
    hostApiInputParameters.suggestedLatency          = inputParameters->suggestedLatency          ;
    hostApiInputParameters.streamInfo = inputParameters->streamInfo ;
    hostApiInputParametersPtr                        = &hostApiInputParameters                    ;
  } else                                                             {
    hostApiInputParametersPtr = NULL                                 ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                    {
    hostApiOutputParameters.device                    = hostApiOutputDevice                         ;
    hostApiOutputParameters.channelCount              = outputParameters->channelCount              ;
    hostApiOutputParameters.sampleFormat              = outputParameters->sampleFormat              ;
    hostApiOutputParameters.suggestedLatency          = outputParameters->suggestedLatency          ;
    hostApiOutputParameters.streamInfo = outputParameters->streamInfo ;
    hostApiOutputParametersPtr                        = &hostApiOutputParameters                    ;
  } else                                                             {
    hostApiOutputParametersPtr = NULL                                ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  result = hostApi -> Open                                           (
                        stream                                       ,
                        hostApiInputParametersPtr                    ,
                        hostApiOutputParametersPtr                   ,
                        sampleRate                                   ,
                        framesPerBuffer                              ,
                        streamFlags                                  ,
                        CONDUIT                                    ) ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                     {
    ((StreamParameters *)inputParameters )->streamInfo = hostApiInputParameters.streamInfo  ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                    {
    ((StreamParameters *)outputParameters)->streamInfo = hostApiOutputParameters.streamInfo ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NoError == result ) Add ( *stream )                           ;
  if ( NULL != (*stream) ) (*stream)->debugger = debugger            ;
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApi ( ( "Open returned:\n"                                ) ) ;
  CaLogApi ( ( "\t*(Stream ** stream): 0x%p\n", *stream          ) ) ;
  CaLogApi ( ( "\tCaError: %d ( %s )\n"                              ,
               result                                                ,
               debugger->Error( result )                         ) ) ;
  #endif
  ////////////////////////////////////////////////////////////////////
  return result                                                      ;
}

CaError Core::Default                       (
          Stream      ** stream             ,
          int            inputChannelCount  ,
          int            outputChannelCount ,
          CaSampleFormat sampleFormat       ,
          double         sampleRate         ,
          CaUint32       framesPerBuffer    ,
          Conduit      * CONDUIT            )
{
  CaError            result                     = NoError            ;
  StreamParameters   hostApiInputParameters                          ;
  StreamParameters   hostApiOutputParameters                         ;
  StreamParameters * hostApiInputParametersPtr  = NULL               ;
  StreamParameters * hostApiOutputParametersPtr = NULL               ;
  ////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApiEnterParams ( "Default" )                                             ;
  CaLogApi ( ( "\tStream ** stream: 0x%p\n"            , stream             ) ) ;
  CaLogApi ( ( "\tint inputChannelCount: %d\n"         , inputChannelCount  ) ) ;
  CaLogApi ( ( "\tint outputChannelCount: %d\n"        , outputChannelCount ) ) ;
  CaLogApi ( ( "\tCaSampleFormat sampleFormat: %d\n"   , sampleFormat       ) ) ;
  CaLogApi ( ( "\tdouble sampleRate: %g\n"             , sampleRate         ) ) ;
  CaLogApi ( ( "\tunsigned long framesPerBuffer: %d\n" , framesPerBuffer    ) ) ;
  CaLogApi ( ( "Conduit * conduit: 0x%p\n"             , CONDUIT            ) ) ;
  #endif
  ////////////////////////////////////////////////////////////////////
  if ( inputChannelCount > 0 )                                       {
    hostApiInputParameters.device = DefaultInputDevice ( )           ;
    if ( CaNoDevice == hostApiInputParameters.device )               {
      return DeviceUnavailable                                       ;
    }                                                                ;
    hostApiInputParameters.channelCount = inputChannelCount          ;
    hostApiInputParameters.sampleFormat = sampleFormat               ;
    /* defaultHighInputLatency is used below instead of
       defaultLowInputLatency because it is more important for the default
       stream to work reliably than it is for it to work with the lowest
       latency.                                                     */
    hostApiInputParameters.suggestedLatency                          =
      GetDeviceInfo( hostApiInputParameters.device )->defaultHighInputLatency ;
    hostApiInputParameters.streamInfo = NULL          ;
    hostApiInputParametersPtr = &hostApiInputParameters              ;
  } else                                                             {
    hostApiInputParametersPtr = NULL                                 ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( outputChannelCount > 0 )                                      {
    hostApiOutputParameters.device = DefaultOutputDevice ( )         ;
    if ( CaNoDevice == hostApiOutputParameters.device )              {
      return DeviceUnavailable                                       ;
    }                                                                ;
    hostApiOutputParameters.channelCount = outputChannelCount        ;
    hostApiOutputParameters.sampleFormat = sampleFormat              ;
    /* defaultHighOutputLatency is used below instead of
       defaultLowOutputLatency because it is more important for the default
       stream to work reliably than it is for it to work with the lowest
       latency.                                                     */
    hostApiOutputParameters.suggestedLatency                         =
      GetDeviceInfo( hostApiOutputParameters.device )->defaultHighOutputLatency ;
    hostApiOutputParameters.streamInfo = NULL         ;
    hostApiOutputParametersPtr = &hostApiOutputParameters            ;
  } else                                                             {
    hostApiOutputParametersPtr = NULL                                ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  result = Open                                                      (
             stream                                                  ,
             hostApiInputParametersPtr                               ,
             hostApiOutputParametersPtr                              ,
             sampleRate                                              ,
             framesPerBuffer                                         ,
             csfNoFlag                                               ,
             CONDUIT                                               ) ;
  #ifndef REMOVE_DEBUG_MESSAGE
  CaLogApi ( ( "Open returned:\n"                                ) ) ;
  CaLogApi ( ( "\t*(Stream ** stream): 0x%p", *stream            ) ) ;
  CaLogApi ( ( "\tCaError: %d ( %s )\n"                              ,
               result                                                ,
               debugger->Error( result )                         ) ) ;
  #endif
  ////////////////////////////////////////////////////////////////////
  return result                                                      ;
}

CaError Core ::                    ValidateStream      (
          const StreamParameters * inputParameters     ,
          const StreamParameters * outputParameters    ,
          double                   sampleRate          ,
          CaUint32                 framesPerBuffer     ,
          CaStreamFlags            streamFlags         ,
          Conduit                * CONDUIT             ,
          HostApi               ** hostApi             ,
          CaDeviceIndex          * hostApiInputDevice  ,
          CaDeviceIndex          * hostApiOutputDevice )
{
  int inputHostApiIndex  = -1 ; /* Surpress uninitialised var warnings: compiler does */
  int outputHostApiIndex = -1 ; /* not see that if inputParameters and outputParame-  */
                                /* ters are both nonzero, these indices are set.      */
  ////////////////////////////////////////////////////////////////////
  if ( ( NULL == inputParameters ) && ( NULL == outputParameters ) ) {
    return InvalidDevice                                             ;
    /** @todo should be a new error code "invalid device parameters" or something */
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL == inputParameters )                                     {
    *hostApiInputDevice = CaNoDevice                                 ;
  } else
  if ( CaUseHostApiSpecificDeviceSpecification == inputParameters->device ) {
    if ( NULL != inputParameters->streamInfo )        {
      inputHostApiIndex = TypeIdToIndex(((StreamInfoHeader *)inputParameters->streamInfo )->hostApiType) ;
      if ( -1 != inputHostApiIndex )                                 {
        *hostApiInputDevice = CaUseHostApiSpecificDeviceSpecification;
        *hostApi            = hostAPIs [inputHostApiIndex]           ;
      } else                                                         {
        return InvalidDevice                                         ;
      }                                                              ;
    } else                                                           {
      return InvalidDevice                                           ;
    }                                                                ;
  } else                                                             {
    if ( ( inputParameters->device < 0                           )  ||
         ( inputParameters->device >= deviceCount                ) ) {
      return InvalidDevice                                           ;
    }                                                                ;
    //////////////////////////////////////////////////////////////////
    inputHostApiIndex = FindHostApi                                  (
                          inputParameters->device                    ,
                          hostApiInputDevice                       ) ;
    if ( inputHostApiIndex < 0 ) return InternalError                ;
    *hostApi = hostAPIs [ inputHostApiIndex ]                        ;
    if ( inputParameters->channelCount <= 0 )                        {
      return InvalidChannelCount                                     ;
    }                                                                ;
    if ( ! isValid ( inputParameters->sampleFormat ) )               {
      return SampleFormatNotSupported                                ;
    }                                                                ;
    //////////////////////////////////////////////////////////////////
    if ( NULL != inputParameters->streamInfo )                       {
      if ( ((StreamInfoHeader *)inputParameters->streamInfo)->hostApiType != (*hostApi)->info.type ) {
        return IncompatibleStreamInfo                                ;
      }                                                              ;
    }                                                                ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( NULL == outputParameters )                                    {
    *hostApiOutputDevice = CaNoDevice                                ;
  } else
  if ( CaUseHostApiSpecificDeviceSpecification == outputParameters->device ) {
    if ( NULL != outputParameters->streamInfo )       {
      outputHostApiIndex = TypeIdToIndex(((StreamInfoHeader *)outputParameters->streamInfo)->hostApiType) ;
      if ( -1 != outputHostApiIndex )                                {
        *hostApiOutputDevice = CaUseHostApiSpecificDeviceSpecification ;
        *hostApi             = hostAPIs [ outputHostApiIndex ]       ;
      } else                                                         {
        return InvalidDevice                                         ;
      }                                                              ;
    } else                                                           {
      return InvalidDevice                                           ;
    }                                                                ;
  } else                                                             {
    if ( ( outputParameters->device < 0                          )  ||
         ( outputParameters->device >= deviceCount               ) ) {
      return InvalidDevice                                           ;
    }                                                                ;
    //////////////////////////////////////////////////////////////////
    outputHostApiIndex = FindHostApi                                 (
                           outputParameters->device                  ,
                           hostApiOutputDevice                     ) ;
    if ( outputHostApiIndex < 0 ) return InternalError               ;
    *hostApi = hostAPIs [ outputHostApiIndex ]                       ;
    if ( outputParameters->channelCount <= 0 )                       {
      return InvalidChannelCount                                     ;
    }                                                                ;
    if ( !isValid( outputParameters->sampleFormat ) )                {
      return SampleFormatNotSupported                                ;
    }                                                                ;
    //////////////////////////////////////////////////////////////////
    if ( NULL != outputParameters->streamInfo )                      {
      if ( ((StreamInfoHeader*)outputParameters->streamInfo)->hostApiType != (*hostApi)->info.type ) {
        return IncompatibleStreamInfo                                ;
      }                                                              ;
    }                                                                ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( ( NULL != inputParameters ) && ( NULL != outputParameters ) ) {
    /* ensure that both devices use the same API                    */
    if ( inputHostApiIndex != outputHostApiIndex )                   {
      return BadIODeviceCombination                                  ;
    }                                                                ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  /* Check for absurd sample rates.                                 */
  if ( ( sampleRate < 1000.0 ) || ( sampleRate > 384000.0 ) )        {
    return InvalidSampleRate                                         ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( ( ( streamFlags & ~csfPlatformSpecificFlags                 ) &
        ~( csfClipOff                                                |
           csfDitherOff                                              |
           csfNeverDropInput                                         |
           csfPrimeOutputBuffersUsingStreamCallback ) ) != 0       ) {
    return InvalidFlag                                               ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  if ( 0 != ( streamFlags & csfNeverDropInput ) )                    {
   /* must be a callback stream                                     */
   if ( NULL == CONDUIT ) return InvalidFlag                         ;
   /* must be a full duplex stream                                  */
   if ( ( NULL==inputParameters  ) || ( NULL==outputParameters )   ) {
     return InvalidFlag                                              ;
   }                                                                 ;
   /* must use Stream::FramesPerBufferUnspecified                   */
   if ( Stream::FramesPerBufferUnspecified != framesPerBuffer )      {
     return InvalidFlag                                              ;
   }                                                                 ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  return NoError                                                     ;
}

/* Play conduit into specific device */
bool Core::Play ( Conduit      * conduit       ,
                  CaDeviceIndex  deviceId      ,
                  int            channelCount  ,
                  CaSampleFormat sampleFormat  ,
                  int            sampleRate    ,
                  int            frameCount    ,
                  CaStreamFlags  streamFlags   ,
                  int            sleepInterval )
{
  if ( NULL == conduit ) return false                                        ;
  ////////////////////////////////////////////////////////////////////////////
  StreamParameters INSP ( deviceId , channelCount , sampleFormat )           ;
  Stream         * stream  = NULL                                            ;
  CaError          rt                                                        ;
  bool             correct = true                                            ;
  ////////////////////////////////////////////////////////////////////////////
  Initialize ( )                                                             ;
  ////////////////////////////////////////////////////////////////////////////
  INSP . suggestedLatency = GetDeviceInfo(deviceId)->defaultLowOutputLatency ;
  ////////////////////////////////////////////////////////////////////////////
  rt = Open ( &stream                                                        ,
              NULL                                                           ,
              &INSP                                                          ,
              sampleRate                                                     ,
              frameCount                                                     ,
              streamFlags                                                    ,
              conduit                                                      ) ;
  if ( ( NoError != rt ) || ( NULL == stream ) ) correct = false             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( correct )                                                             {
    rt = Start ( stream )                                                    ;
    if ( NoError != rt ) correct = false                                     ;
    while ( correct && ( 1 == IsActive  ( stream ) ) )                       {
      Timer :: Sleep ( sleepInterval )                                       ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( correct && ( 0 != IsStopped ( stream ) ) ) Stop ( stream )          ;
  }                                                                          ;
  Close     ( stream )                                                       ;
  Terminate (        )                                                       ;
  return correct                                                             ;
}

/* Record specific device into conduit */
bool Core::Record ( Conduit      * conduit       ,
                    CaDeviceIndex  deviceId      ,
                    int            channelCount  ,
                    CaSampleFormat sampleFormat  ,
                    int            sampleRate    ,
                    int            frameCount    ,
                    CaStreamFlags  streamFlags   ,
                    int            sleepInterval )
{
  if ( NULL == conduit ) return false                                        ;
  ////////////////////////////////////////////////////////////////////////////
  StreamParameters INSP ( deviceId , channelCount , sampleFormat )           ;
  Stream         * stream  = NULL                                            ;
  CaError          rt                                                        ;
  bool             correct = true                                            ;
  ////////////////////////////////////////////////////////////////////////////
  Initialize ( )                                                             ;
  ////////////////////////////////////////////////////////////////////////////
  INSP . suggestedLatency = GetDeviceInfo(deviceId)->defaultLowInputLatency  ;
  ////////////////////////////////////////////////////////////////////////////
  rt = Open ( &stream                                                        ,
              &INSP                                                          ,
              NULL                                                           ,
              sampleRate                                                     ,
              frameCount                                                     ,
              streamFlags                                                    ,
              conduit                                                      ) ;
  if ( ( NoError != rt ) || ( NULL == stream ) ) correct = false             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( correct )                                                             {
    rt = Start ( stream )                                                    ;
    if ( NoError != rt ) correct = false                                     ;
  ////////////////////////////////////////////////////////////////////////////
    while ( correct && ( 1 == IsActive  ( stream ) ) )                       {
      Timer :: Sleep ( sleepInterval )                                       ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( correct &&  ( 0 != IsStopped ( stream ) ) ) Stop ( stream )         ;
  }                                                                          ;
  Close     ( stream )                                                       ;
  Terminate (        )                                                       ;
  return correct                                                             ;
}

bool Core::hasEncoder(void)
{
  HostApi * hostApi = NULL                           ;
  if ( NoError == GetHostApi(&hostApi,FFMPEG) )      {
    if ( NULL != hostApi )                           {
      if ( FFMPEG == hostApi->info.type) return true ;
    }                                                ;
  }                                                  ;
  if ( NoError == GetHostApi(&hostApi,VLC   ) )      {
    if ( NULL != hostApi )                           {
      if ( VLC    == hostApi->info.type) return true ;
    }                                                ;
  }                                                  ;
  return false                                       ;
}

bool Core::hasDecoder(void)
{
  HostApi * hostApi = NULL                           ;
  if ( NoError == GetHostApi(&hostApi,FFMPEG) )      {
    if ( NULL != hostApi )                           {
      if ( FFMPEG == hostApi->info.type) return true ;
    }                                                ;
  }                                                  ;
  if ( NoError == GetHostApi(&hostApi,VLC   ) )      {
    if ( NULL != hostApi )                           {
      if ( VLC    == hostApi->info.type) return true ;
    }                                                ;
  }                                                  ;
  return false                                       ;
}

bool Core::Play(const char * filename,bool threading)
{
  return Play ( filename , -1 , threading ) ;
}

bool Core::Play(const char * filename,int outputDevice,bool threading)
{
  if ( NULL == filename ) return false          ;
  #if defined(CAUTILITIES)
  #if defined(FFMPEGLIB)
  Core core                                     ;
  int  decodeId = -1                            ;
  core . Initialize ( )                         ;
  decodeId = (int)core.DefaultDecoderDevice ( ) ;
  core . Terminate  ( )                         ;
  if ( threading )                              {
    OffshorePlayer * player                     ;
    player  = new OffshorePlayer ( )            ;
    player -> filename = ::strdup(filename)     ;
    player -> deviceId = outputDevice           ;
    player -> decodeId = decodeId               ;
    player -> start ( )                         ;
    return true                                 ;
  } else                                        {
    return MediaPlay ( (char *)filename         ,
                       outputDevice             ,
                       decodeId                 ,
                       NULL                   ) ;
  }                                             ;
  #endif
  #endif
  return false                                  ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
