/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/18                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CaSkeleton.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#if defined(WIN32) || defined(_WIN32)

#include <windows.h>
#include <process.h>

#else

#include <pthread.h>

#endif

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

#if defined(WIN32) || defined(_WIN32)

static unsigned SkeletonProc(void * arg)
{
  if ( NULL == arg ) return NoError               ;
  SkeletonStream * stream = (SkeletonStream *)arg ;
  CaError          result = stream -> Processor() ;
  return result                                   ;
}

#else

static void * SkeletonProc(void * arg)
{
  if ( NULL == arg ) return NULL                  ;
  SkeletonStream * stream = (SkeletonStream *)arg ;
  CaError          result = stream -> Processor() ;
  return NULL                                     ;
}

#endif

///////////////////////////////////////////////////////////////////////////////

CaError SkeletonInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError           result = NoError                                         ;
  SkeletonHostApi * skeleteonHostApi = NULL                                  ;
  ////////////////////////////////////////////////////////////////////////////
  skeleteonHostApi = new SkeletonHostApi ( )                                 ;
  if ( NULL == skeleteonHostApi )                                            {
    result = InsufficientMemory                                              ;
    return result                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "Initialize Skeleton Host API.\n"
              "Skeleton API was never actually 'tested'.  "
              "You should not assume it will run as you thought.\n" ) )      ;
  ////////////////////////////////////////////////////////////////////////////
  *hostApi = (HostApi *)skeleteonHostApi                                     ;
  (*hostApi) -> info . structVersion       = 1                               ;
  (*hostApi) -> info . type                = Skeleton                        ;
  (*hostApi) -> info . index               = hostApiIndex                    ;
  (*hostApi) -> info . name                = "Skeleton Host API"             ;
  (*hostApi) -> info . deviceCount         = 0                               ;
  (*hostApi) -> info . defaultInputDevice  = CaNoDevice                      ;
  (*hostApi) -> info . defaultOutputDevice = CaNoDevice                      ;
  gPrintf ( ( "Assign Skeleton Host API\n" ) )                               ;
  ////////////////////////////////////////////////////////////////////////////
  skeleteonHostApi->allocations = new AllocationGroup ( )                    ;
  if ( NULL == skeleteonHostApi->allocations )                               {
    gPrintf ( ( "Insufficient memory when allocate AllocationGroup\n" ) )    ;
    result = InsufficientMemory                                              ;
    delete skeleteonHostApi                                                  ;
    return result                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  SkeletonDeviceInfo * oudev = new SkeletonDeviceInfo ( )                    ;
  SkeletonDeviceInfo * indev = new SkeletonDeviceInfo ( )                    ;
  skeleteonHostApi->info.deviceCount += 2                                    ;
  skeleteonHostApi->inputDeviceCount  = 1                                    ;
  skeleteonHostApi->outputDeviceCount = 1                                    ;
  skeleteonHostApi->deviceInfos = new DeviceInfo * [ 2 ]                     ;
  skeleteonHostApi->deviceInfos[0] = (DeviceInfo *) oudev                    ;
  skeleteonHostApi->deviceInfos[1] = (DeviceInfo *) indev                    ;
  ////////////////////////////////////////////////////////////////////////////
  indev->structVersion            = 2                                        ;
  indev->name                     = "Dumb John audio output"                 ;
  indev->hostApi                  = hostApiIndex                             ;
  indev->hostType                 = Skeleton                                 ;
  indev->maxInputChannels         = 0                                        ;
  indev->maxOutputChannels        = 2                                        ;
  indev->defaultLowInputLatency   = 0                                        ;
  indev->defaultLowOutputLatency  = 0                                        ;
  indev->defaultHighInputLatency  = 0                                        ;
  indev->defaultHighOutputLatency = 0                                        ;
  indev->defaultSampleRate        = 48000                                    ;
  ////////////////////////////////////////////////////////////////////////////
  oudev->structVersion            = 2                                        ;
  oudev->name                     = "Deaf Jack audio input"                  ;
  oudev->hostApi                  = hostApiIndex                             ;
  oudev->hostType                 = Skeleton                                 ;
  oudev->maxInputChannels         = 1                                        ;
  oudev->maxOutputChannels        = 0                                        ;
  oudev->defaultLowInputLatency   = 0                                        ;
  oudev->defaultLowOutputLatency  = 0                                        ;
  oudev->defaultHighInputLatency  = 0                                        ;
  oudev->defaultHighOutputLatency = 0                                        ;
  oudev->defaultSampleRate        = 16000                                    ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

SkeletonStream:: SkeletonStream (void   )
               : Stream         (       )
{
  FalseVolume [ 0 ] = 10000.0 ;
  FalseVolume [ 1 ] = 10000.0 ;
  isInputChannel    = false   ;
  isOutputChannel   = false   ;
  isStopped         = 0       ;
  isActive          = 0       ;
  stopProcessing    = 0       ;
  abortProcessing   = 0       ;
  ThreadId          = 0       ;
}

SkeletonStream::~SkeletonStream(void)
{
}

CaError SkeletonStream::Start(void)
{
  if ( NULL == conduit                         ) return NullCallback         ;
  if ( ! ( isInputChannel || isOutputChannel ) ) return InvalidFlag          ;
  CaError result = NoError                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  isStopped       = 0                                                        ;
  isActive        = 1                                                        ;
  stopProcessing  = 0                                                        ;
  abortProcessing = 0                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  #if defined(WIN32) || defined(_WIN32)
  DWORD        dwCreationFlags = 0                                           ;
  unsigned int dwThreadID                                                    ;
  ThreadId = (unsigned) ::_beginthreadex                                     (
    NULL                                                                     ,
    0                                                                        ,
    (unsigned int(__stdcall *)(void*))SkeletonProc                           ,
    (LPVOID)this                                                             ,
    dwCreationFlags                                                          ,
    &dwThreadID                                                            ) ;
  #else
  int th_retcode                                                             ;
  th_retcode = ::pthread_create                                              (
                 &ThreadId                                                   ,
                 NULL                                                        ,
                 SkeletonProc                                                ,
                 (void *)this                                              ) ;
  #endif
  if ( 0 == ThreadId ) result = UnanticipatedHostError                       ;
  return result                                                              ;
}

CaError SkeletonStream::Stop(void)
{
  stopProcessing = 1                                ;
  while ( ( 1 == isActive ) && ( 1 != isStopped ) ) {
    Timer::Sleep(10)                                ;
  }                                                 ;
  return NoError                                    ;
}

CaError SkeletonStream::Close(void)
{
  return NoError ;
}

CaError SkeletonStream::Abort(void)
{
  stopProcessing = 1                                ;
  while ( ( 1 == isActive ) && ( 1 != isStopped ) ) {
    Timer::Sleep(10)                                ;
  }                                                 ;
  return NoError                                    ;
}

CaError SkeletonStream::IsStopped(void)
{
  return isStopped ;
}

CaError SkeletonStream::IsActive(void)
{
  return isActive  ;
}

CaTime SkeletonStream::GetTime(void)
{
  return Timer::Time() ;
}

double SkeletonStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer . Value ( ) ;
}

CaInt32 SkeletonStream::ReadAvailable(void)
{
  return 0 ;
}

CaInt32 SkeletonStream::WriteAvailable(void)
{
  return 0 ;
}

CaError SkeletonStream::Read(void * buffer,unsigned long frames)
{
  return 0 ;
}

CaError SkeletonStream::Write(const void * buffer,unsigned long frames)
{
  return 0 ;
}

bool SkeletonStream::hasVolume(void)
{
  return true ;
}

CaVolume SkeletonStream::MinVolume(void)
{
  return 0 ;
}

CaVolume SkeletonStream::MaxVolume(void)
{
  return 10000.0 ;
}

CaVolume SkeletonStream::Volume(int atChannel)
{
  if ( atChannel < 0 ) atChannel = 0 ;
  if ( atChannel > 1 ) atChannel = 1 ;
  return FalseVolume[atChannel]      ;
}

CaVolume SkeletonStream::setVolume(CaVolume volume,int atChannel)
{
  if ( atChannel < 0 ) atChannel = 0 ;
  if ( atChannel > 1 ) atChannel = 1 ;
  FalseVolume [ atChannel ] = volume ;
  return FalseVolume [ atChannel ]   ;
}

CaError SkeletonStream::Processor(void)
{
  if ( isInputChannel  ) return Put    ( ) ;
  if ( isOutputChannel ) return Obtain ( ) ;
  return NoError                           ;
}

CaError SkeletonStream::Obtain(void)
{
  CaError         result     = NoError                                       ;
  bool            done       = false                                         ;
  long long       bufferSize = Frames     * BytesPerSample                   ;
  unsigned char * dat        = new unsigned char [ bufferSize ]              ;
  int             BPF        = BytesPerSample                                ;
  int             RingUnit   = (int)(sampleRate * BytesPerSample)            ;
  CaTime          Interval   = Frames                                        ;
  CaTime          DT         = 0                                             ;
  CaTime          LT         = 0                                             ;
  CaTime          IBA        = 0                                             ;
  int             OutputSize = 0                                             ;
  int             callbackResult                                             ;
  ////////////////////////////////////////////////////////////////////////////
  Interval *= 1000.0                                                         ;
  Interval /= sampleRate                                                     ;
  memset ( dat , 0 , bufferSize )                                            ;
  conduit -> Output . BytesPerSample = BPF                                   ;
  ////////////////////////////////////////////////////////////////////////////
  LT  = Timer::Time()                                                        ;
  LT *= 1000.0                                                               ;
  while ( ! done )                                                           {
    if ( 1 == isStopped       ) done = true                                  ;
    if ( 1 == stopProcessing  ) done = true                                  ;
    if ( 1 == abortProcessing ) done = true                                  ;
    if ( 0 == isActive        ) done = true                                  ;
    if ( done                 ) continue                                     ;
    DT  = Timer::Time()                                                      ;
    DT *= 1000.0                                                             ;
    DT -= LT                                                                 ;
    //////////////////////////////////////////////////////////////////////////
    IBA = (CaTime)OutputSize                                                 ;
    IBA *= 1000.0                                                            ;
    IBA /= RingUnit                                                          ;
    //////////////////////////////////////////////////////////////////////////
    if ( DT < Interval )                                                     {
      Timer :: Sleep ( 5 )                                                   ;
      continue                                                               ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    LT  = Timer::Time()                                                      ;
    LT *= 1000.0                                                             ;
    //////////////////////////////////////////////////////////////////////////
    conduit -> Output . Buffer     = dat                                     ;
    conduit -> Output . FrameCount = Frames                                  ;
    conduit -> Output . AdcDac     = IBA                                     ;
    conduit -> LockConduit   ( )                                             ;
    callbackResult                 = conduit -> obtain ( )                   ;
    conduit -> UnlockConduit ( )                                             ;
    conduit -> Output . Buffer     = NULL                                    ;
    OutputSize                    += bufferSize                              ;
    //////////////////////////////////////////////////////////////////////////
    if ( Conduit::Continue == callbackResult )                               {
    } else
    if ( Conduit::Abort    == callbackResult )                               {
      abortProcessing = 1                                                    ;
    } else
    if ( Conduit::Postpone == callbackResult )                               {
    } else                                                                   {
      stopProcessing  = 1                                                    ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  isStopped       = 1                                                        ;
  isActive        = 0                                                        ;
  stopProcessing  = 0                                                        ;
  abortProcessing = 0                                                        ;
  if ( NULL != conduit )                                                     {
    conduit -> finish ( Conduit::OutputDirection , Conduit::Correct )        ;
  }                                                                          ;
  cpuLoadMeasurer . Reset ( )                                                ;
  ////////////////////////////////////////////////////////////////////////////
  delete [] dat                                                              ;
  return result                                                              ;
}

CaError SkeletonStream::Put(void)
{
  CaError         result     = NoError                                       ;
  bool            done       = false                                         ;
  long long       bufferSize = Frames     * BytesPerSample                   ;
  unsigned char * dat        = new unsigned char [ bufferSize ]              ;
  int             BPF        = BytesPerSample                                ;
  int             RingUnit   = (int)(sampleRate * BytesPerSample)            ;
  CaTime          Interval   = Frames                                        ;
  CaTime          DT         = 0                                             ;
  CaTime          LT         = 0                                             ;
  CaTime          IBA        = 0                                             ;
  int             OutputSize = 0                                             ;
  int             callbackResult                                             ;
  ////////////////////////////////////////////////////////////////////////////
  Interval *= 1000.0                                                         ;
  Interval /= sampleRate                                                     ;
  memset ( dat , 0 , bufferSize )                                            ;
  conduit -> Output . BytesPerSample = BPF                                   ;
  ////////////////////////////////////////////////////////////////////////////
  LT  = Timer::Time()                                                        ;
  LT *= 1000.0                                                               ;
  while ( ! done )                                                           {
    if ( 1 == isStopped       ) done = true                                  ;
    if ( 1 == stopProcessing  ) done = true                                  ;
    if ( 1 == abortProcessing ) done = true                                  ;
    if ( 0 == isActive        ) done = true                                  ;
    if ( done                 ) continue                                     ;
    DT  = Timer::Time()                                                      ;
    DT *= 1000.0                                                             ;
    DT -= LT                                                                 ;
    //////////////////////////////////////////////////////////////////////////
    IBA = (CaTime)OutputSize                                                 ;
    IBA *= 1000.0                                                            ;
    IBA /= RingUnit                                                          ;
    //////////////////////////////////////////////////////////////////////////
    if ( DT < Interval )                                                     {
      Timer :: Sleep ( 5 )                                                   ;
      continue                                                               ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    LT  = Timer::Time()                                                      ;
    LT *= 1000.0                                                             ;
    //////////////////////////////////////////////////////////////////////////
    conduit -> Input . Buffer     = dat                                      ;
    conduit -> Input . FrameCount = Frames                                   ;
    conduit -> Input . AdcDac     = IBA                                      ;
    conduit -> LockConduit   ( )                                             ;
    callbackResult                = conduit -> put ( )                       ;
    conduit -> UnlockConduit ( )                                             ;
    conduit -> Input . Buffer     = NULL                                     ;
    OutputSize                    += bufferSize                              ;
    //////////////////////////////////////////////////////////////////////////
    if ( Conduit::Continue == callbackResult )                               {
    } else
    if ( Conduit::Abort    == callbackResult )                               {
      abortProcessing = 1                                                    ;
    } else
    if ( Conduit::Postpone == callbackResult )                               {
    } else                                                                   {
      stopProcessing  = 1                                                    ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  isStopped       = 1                                                        ;
  isActive        = 0                                                        ;
  stopProcessing  = 0                                                        ;
  abortProcessing = 0                                                        ;
  if ( NULL != conduit )                                                     {
    conduit -> finish ( Conduit::InputDirection , Conduit::Correct )         ;
  }                                                                          ;
  cpuLoadMeasurer . Reset ( )                                                ;
  ////////////////////////////////////////////////////////////////////////////
  delete [] dat                                                              ;
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

SkeletonHostApiInfo:: SkeletonHostApiInfo (void)
                    : HostApiInfo         (    )
{
}

SkeletonHostApiInfo::~SkeletonHostApiInfo (void)
{
}

//////////////////////////////////////////////////////////////////////////////

SkeletonDeviceInfo:: SkeletonDeviceInfo (void)
                   : DeviceInfo         (    )
{
}

SkeletonDeviceInfo::~SkeletonDeviceInfo (void)
{
}

///////////////////////////////////////////////////////////////////////////////

SkeletonHostApi:: SkeletonHostApi (void)
                : HostApi         (    )
{
  allocations       = NULL ;
  inputDeviceCount  = 0    ;
  outputDeviceCount = 0    ;
}

SkeletonHostApi::~SkeletonHostApi(void)
{
}

SkeletonHostApi::Encoding SkeletonHostApi::encoding(void) const
{
  return UTF8 ;
}

bool SkeletonHostApi::hasDuplex(void) const
{
  return false ;
}

CaError SkeletonHostApi::Open                         (
          Stream                 ** s                 ,
          const StreamParameters *  inputParameters   ,
          const StreamParameters *  outputParameters  ,
          double                    sampleRate        ,
          CaUint32                  framesPerCallback ,
          CaStreamFlags             streamFlags       ,
          Conduit                *  conduit           )
{
  CaError          result = NoError                                          ;
  SkeletonStream * stream = NULL                                             ;
  CaSampleFormat   hostInputSampleFormat                                     ;
  CaSampleFormat   hostOutputSampleFormat                                    ;
  CaSampleFormat   inputSampleFormat                                         ;
  CaSampleFormat   outputSampleFormat                                        ;
  int              inputChannelCount                                         ;
  int              outputChannelCount                                        ;
  double           suggestedInputLatency                                     ;
  double           suggestedOutputLatency                                    ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    inputSampleFormat      = inputParameters->sampleFormat                   ;
    inputChannelCount      = inputParameters->channelCount                   ;
    suggestedInputLatency  = inputParameters->suggestedLatency               ;

  } else                                                                     {
    inputSampleFormat      = cafNothing                                      ;
    hostInputSampleFormat  = cafNothing                                      ;
    inputChannelCount      = 0                                               ;
    suggestedInputLatency  = 0.0                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputSampleFormat     = outputParameters -> sampleFormat                ;
    outputChannelCount     = outputParameters -> channelCount                ;
    suggestedOutputLatency = outputParameters -> suggestedLatency            ;
  } else                                                                     {
    outputSampleFormat     = cafNothing                                      ;
    hostOutputSampleFormat = cafNothing                                      ;
    outputChannelCount     = 0                                               ;
    suggestedOutputLatency = 0.0                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != ( streamFlags & csfPlatformSpecificFlags) )                      {
    return InvalidFlag                                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream = new SkeletonStream ( )                                            ;
  if ( NULL == stream )                                                      {
    result = InsufficientMemory                                              ;
    return result                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> ThreadId        = 0                                              ;
  stream -> debugger        = debugger                                       ;
  stream -> conduit         = conduit                                        ;
  stream -> cpuLoadMeasurer . Initialize ( sampleRate )                      ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> inputLatency    = suggestedInputLatency                          ;
  stream -> outputLatency   = suggestedOutputLatency                         ;
  stream -> sampleRate      = sampleRate                                     ;
  stream -> Frames          = framesPerCallback                              ;
  stream -> isInputChannel  = ( inputChannelCount  > 0 )                     ;
  stream -> isOutputChannel = ( outputChannelCount > 0 )                     ;
  stream -> isStopped       = 1                                              ;
  stream -> isActive        = 0                                              ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    stream -> SampleFormat   = inputSampleFormat                             ;
    stream -> Channels       = inputChannelCount                             ;
    stream -> BytesPerSample = Core::SampleSize(inputSampleFormat)           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    stream -> SampleFormat   = outputSampleFormat                            ;
    stream -> Channels       = outputChannelCount                            ;
    stream -> BytesPerSample = Core::SampleSize(outputSampleFormat)          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *s = (Stream *)stream                                                      ;
  return result                                                              ;
}

void SkeletonHostApi::Terminate(void)
{
  if ( CaIsNull ( deviceInfos ) ) return      ;
  for (int i=0;i<2;i++) delete deviceInfos[i] ;
  delete [] deviceInfos                       ;
  deviceInfos = NULL                          ;
}

CaError SkeletonHostApi::isSupported                 (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double sampleRate                          )
{
  int            inputChannelCount                                           ;
  int            outputChannelCount                                          ;
  CaSampleFormat inputSampleFormat                                           ;
  CaSampleFormat outputSampleFormat                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
     inputChannelCount = inputParameters -> channelCount                     ;
     inputSampleFormat = inputParameters -> sampleFormat                     ;
     if ( CaUseHostApiSpecificDeviceSpecification==inputParameters->device ) {
       return InvalidDevice                                                  ;
     }                                                                       ;
     if ( inputChannelCount > 2 )                                            {
       return InvalidChannelCount                                            ;
     }                                                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputChannelCount = outputParameters -> channelCount                    ;
    outputSampleFormat = outputParameters -> sampleFormat                    ;
    if ( CaUseHostApiSpecificDeviceSpecification==outputParameters->device ) {
      return InvalidDevice                                                   ;
    }                                                                        ;
    if ( outputChannelCount > 2 )                                            {
      return InvalidChannelCount                                             ;
    }                                                                        ;
  }                                                                          ;
  return NoError ;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
