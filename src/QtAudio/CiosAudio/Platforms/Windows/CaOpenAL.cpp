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

#include "CaOpenAL.hpp"

#if defined(WIN32) || defined(_WIN32)
#pragma message("OpenAL is considered very expertimental, use it at your own risk.")
#pragma message("So far, there are no platform tested successfully.")
#else
#warning OpenAL is considered very expertimental, use it at your own risk.
#warning So far, there are no platform tested successfully.
#endif

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

static unsigned OpenALProc(void * arg)
{
  if ( NULL == arg ) return NoError               ;
  OpenALStream * stream = (OpenALStream *)arg ;
  CaError          result = stream -> Processor() ;
  return result                                   ;
}

#else

static void * OpenALProc(void * arg)
{
  if ( NULL == arg ) return NULL                  ;
  OpenALStream * stream = (OpenALStream *)arg ;
  CaError          result = stream -> Processor() ;
  return NULL                                     ;
}

#endif

///////////////////////////////////////////////////////////////////////////////

CaError OpenALInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError         result        = NoError                                    ;
  OpenALHostApi * openalHostApi = NULL                                       ;
  ////////////////////////////////////////////////////////////////////////////
  openalHostApi = new OpenALHostApi ( )                                   ;
  if ( CaIsNull ( openalHostApi ) )                                          {
    result = InsufficientMemory                                              ;
    return result                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *hostApi                                 = (HostApi *)openalHostApi        ;
  (*hostApi) -> info . structVersion       = 1                               ;
  (*hostApi) -> info . type                = OpenAL                          ;
  (*hostApi) -> info . index               = hostApiIndex                    ;
  (*hostApi) -> info . name                = "OpenAL"                        ;
  (*hostApi) -> info . deviceCount         = 0                               ;
  (*hostApi) -> info . defaultInputDevice  = CaNoDevice                      ;
  (*hostApi) -> info . defaultOutputDevice = CaNoDevice                      ;
  gPrintf ( ( "Assign Skeleton Host API\n" ) )                               ;
  ////////////////////////////////////////////////////////////////////////////
  openalHostApi -> allocations = new AllocationGroup ( )                     ;
  if ( NULL == openalHostApi->allocations )                                  {
    gPrintf ( ( "Insufficient memory when allocate AllocationGroup\n" ) )    ;
    result = InsufficientMemory                                              ;
    delete openalHostApi                                                     ;
    return result                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  openalHostApi -> FindDevices ( )                                           ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

OpenALStream:: OpenALStream (void)
             : Stream       (    )
{
  isInputChannel    = false ;
  isOutputChannel   = false ;
  isStopped         = 0     ;
  isActive          = 0     ;
  stopProcessing    = 0     ;
  abortProcessing   = 0     ;
  ThreadId          = 0     ;
  spec              = NULL  ;
}

OpenALStream::~OpenALStream(void)
{
}

CaError OpenALStream::Start(void)
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
    (unsigned int(__stdcall *)(void*))OpenALProc                             ,
    (LPVOID)this                                                             ,
    dwCreationFlags                                                          ,
    &dwThreadID                                                            ) ;
  #else
  int th_retcode                                                             ;
  th_retcode = ::pthread_create                                              (
                 &ThreadId                                                   ,
                 NULL                                                        ,
                 OpenALProc                                                  ,
                 (void *)this                                              ) ;
  #endif
  if ( 0 == ThreadId ) result = UnanticipatedHostError                       ;
  return result                                                              ;
}

CaError OpenALStream::Stop(void)
{
  stopProcessing = 1                                ;
  while ( ( 1 == isActive ) && ( 1 != isStopped ) ) {
    Timer::Sleep(10)                                ;
  }                                                 ;
  return NoError                                    ;
}

CaError OpenALStream::Close(void)
{
  return NoError ;
}

CaError OpenALStream::Abort(void)
{
  stopProcessing = 1                                ;
  while ( ( 1 == isActive ) && ( 1 != isStopped ) ) {
    Timer::Sleep(10)                                ;
  }                                                 ;
  return NoError                                    ;
}

CaError OpenALStream::IsStopped(void)
{
  return isStopped ;
}

CaError OpenALStream::IsActive(void)
{
  return isActive  ;
}

CaTime OpenALStream::GetTime(void)
{
  return Timer::Time() ;
}

double OpenALStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer . Value ( ) ;
}

CaInt32 OpenALStream::ReadAvailable(void)
{
  return 0 ;
}

CaInt32 OpenALStream::WriteAvailable(void)
{
  return 0 ;
}

CaError OpenALStream::Read(void * buffer,unsigned long frames)
{
  return 0 ;
}

CaError OpenALStream::Write(const void * buffer,unsigned long frames)
{
  return 0 ;
}

bool OpenALStream::hasVolume(void)
{
  return true ;
}

CaVolume OpenALStream::MinVolume(void)
{
  return 0 ;
}

CaVolume OpenALStream::MaxVolume(void)
{
  return 10000.0 ;
}

CaVolume OpenALStream::Volume(int atChannel)
{
  return 10000.0 ;
}

CaVolume OpenALStream::setVolume(CaVolume volume,int atChannel)
{
  return 10000.0 ;
}

bool OpenALStream::VerifyInfo(void * Spec)
{
  if ( CaNotNull ( Spec ) )                          {
    OpenALStreamInfo * si = (OpenALStreamInfo *)Spec ;
    bool correct = true                              ;
    if ( sizeof(OpenALStreamInfo) != si->size )      {
      correct = false                                ;
    }                                                ;
    if ( OpenAL != si->hostApiType ) correct = false ;
    if ( correct ) spec = (OpenALStreamInfo *) si    ;
  }                                                  ;
  ////////////////////////////////////////////////////
  if ( CaIsNull ( spec ) )                           {
    spec  = new OpenALStreamInfo ( )                 ;
    spec -> size        = sizeof(OpenALStreamInfo)   ;
    spec -> hostApiType = OpenAL                     ;
    spec -> version     = 1511                       ;
    spec -> name        = NULL                       ;
    spec -> device      = NULL                       ;
    spec -> context     = NULL                       ;
    spec -> source      = -1                         ;
    spec -> buffer      = -1                         ;
    spec -> useExternal = false                      ;
  }                                                  ;
  spec   -> isCapture   = isInputChannel             ;
  spec   -> isPlayback  = isOutputChannel            ;
  spec   -> format      = (int)alFormat              ;
  return true                                        ;
}

CaError OpenALStream::Processor(void)
{
  if ( isInputChannel  ) return Put    ( ) ;
  if ( isOutputChannel ) return Obtain ( ) ;
  return NoError                           ;
}

CaError OpenALStream::Obtain(void)
{
  dPrintf ( ( "OpenAL player started\n" ) )                                  ;
  ////////////////////////////////////////////////////////////////////////////
  CaError         result     = NoError                                       ;
  bool            done       = false                                         ;
  long long       bufferSize = Frames * BytesPerSample                       ;
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
  ////////////////////////////////////////////////////////////////////////////
  ALCdevice  * device     = NULL                                             ;
  ALCcontext * context    = NULL                                             ;
  ALenum       alFmt      = alFormat                                         ;
  bool         sourceInit = false                                            ;
  bool         bufferInit = false                                            ;
  bool         openal     = false                                            ;
  ALuint       source                                                        ;
  ALuint       buffer                                                        ;
  ALenum       err                                                           ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ! done )                                                              {
    if ( CaIsNull(spec->name) ) done = true                                  ;
    if ( CaAND ( ! done , strlen(spec->name) <= 0 ) ) done = true            ;
    if ( CaAND ( ! done , CaNotNull ( spec->device ) ) )                     {
      device = (ALCdevice *)spec->device                                     ;
    }                                                                        ;
    if ( CaAND ( ! done , CaIsNull ( device ) ) )                            {
      device         = ::alcOpenDevice ( spec->name )                        ;
      spec -> device = (void *)device                                        ;
    }                                                                        ;
    if ( CaIsNull ( device ) ) done = true                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaAND ( ! done , CaNotNull ( device ) ) )                             {
    if ( CaAND ( ! done , CaNotNull(spec->context) ) )                       {
      context = (ALCcontext *)spec->context                                  ;
    }                                                                        ;
    if ( CaAND ( ! done , CaIsNull ( context ) ) )                           {
      context = ::alcCreateContext ( device , NULL )                         ;
      spec->context = (void *)context                                        ;
    }                                                                        ;
    if ( CaIsNull ( context ) ) done = true                                  ;
    if ( ! done )                                                            {
      if ( CaNOT ( ::alcMakeContextCurrent ( context ) ) )                   {
        done = true                                                          ;
      }                                                                      ;
      if ( CaNotEqual ( ::alGetError() , AL_NO_ERROR ) )                     {
        done = true                                                          ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f }             ;
  alListener3f(AL_POSITION, 0, 0, 1.0f);
  // check for errors
  alListener3f(AL_VELOCITY, 0, 0, 0);
  // check for errors
  alListenerfv(AL_ORIENTATION, listenerOri);
  ////////////////////////////////////////////////////////////////////////////
  if ( ! done )                                                              {
    if ( spec->useExternal )                                                 {
      alFmt  = (ALenum)spec->format                                          ;
      source = (ALuint)spec->source                                          ;
      buffer = (ALuint)spec->buffer                                          ;
    } else                                                                   {
      ////////////////////////////////////////////////////////////////////////
      ::alGenSources ( (ALuint)1 , &source )                                 ;
      if ( CaNotEqual ( ::alGetError() , AL_NO_ERROR ) )                     {
        done = true                                                          ;
      } else                                                                 {
        sourceInit     = true                                                ;
        spec -> source = (int) source                                        ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      if ( ! done )                                                          {
        ::alGenBuffers ( (ALuint)1 , &buffer )                               ;
        if ( CaAND ( ! done                                                  ,
                     CaNotEqual ( ::alGetError() , AL_NO_ERROR ) ) )         {
          done = true                                                        ;
        }                                                                    ;
        if ( ! done )                                                        {
          bufferInit     = true                                              ;
          spec -> buffer = (int) buffer                                      ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ! done )                                                              {
      gPrintf ( ( "Format : %x\n" , (int)alFmt ) )                           ;
    ::alBufferData ( buffer,alFmt,dat,bufferSize,(ALsizei)sampleRate )       ;
    if ( CaIsEqual ( ::alGetError() , AL_NO_ERROR ) )                        {
//      ::alSourcei ( source , AL_BUFFER , buffer )                            ;
//      if ( CaIsEqual ( ::alGetError() , AL_NO_ERROR ) )                      {
        openal = true                                                        ;
//      } else done = true                                                     ;
    } else done = true                                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( openal )                                                              {
    if ( ! spec->useExternal )                                               {
      ::alSourcef  ( source , AL_PITCH    , 1         )                      ;
      ::alSourcef  ( source , AL_GAIN     , 1         )                      ;
      ::alSource3f ( source , AL_POSITION , 0 , 0 , 0 )                      ;
      ::alSource3f ( source , AL_VELOCITY , 0 , 0 , 0 )                      ;
      ::alSourcei  ( source , AL_LOOPING  , AL_FALSE  )                      ;
    }                                                                        ;
    ::alSourceQueueBuffers ( source , 1 , & buffer )                         ;
    err = ::alGetError()                                                     ;
    if ( CaNotEqual ( err , AL_NO_ERROR ) )                                  {
      dPrintf ( ( "alSourceQueueBuffers : %x\n",(int)err ) )                 ;
    }                                                                        ;
    ::alSourcePlay         ( source                )                         ;
    err = ::alGetError()                                                     ;
    if ( CaNotEqual ( err , AL_NO_ERROR ) )                                  {
      dPrintf ( ( "alSourcePlay seems ... can not start : %x\n",(int)err ) ) ;
      openal = false                                                         ;
      done   = true                                                          ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  LT  = Timer::Time()                                                        ;
  LT *= 1000.0                                                               ;
  while ( ! done )                                                           {
    if ( 1 == isStopped       ) done = true                                  ;
    if ( 1 == stopProcessing  ) done = true                                  ;
    if ( 1 == abortProcessing ) done = true                                  ;
    if ( 0 == isActive        ) done = true                                  ;
    if ( done                 ) break                                        ;
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
    if ( openal )                                                            {
      ALint val                                                              ;
      ::alGetSourcei(source, AL_BUFFERS_PROCESSED, &val)                     ;
      err = alGetError();
      ::alSourceUnqueueBuffers(source,  1,  &buffer); ;
        err = alGetError();
      ::alBufferData ( buffer,alFmt,dat,bufferSize,(ALsizei)sampleRate )     ;
        err = alGetError();
      ::alSourceQueueBuffers ( source , 1 , & buffer )                       ;
        err = alGetError();
    }                                                                        ;
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
  if (bufferInit)           ::alDeleteBuffers   ( 1 , &buffer )              ;
  if (sourceInit)           ::alDeleteSources   ( 1 , &source )              ;
  ::alcMakeContextCurrent                       ( NULL        )              ;
  if ( CaNotNull(context) ) ::alcDestroyContext ( context     )              ;
  if ( CaNotNull(device ) ) ::alcCloseDevice    ( device      )              ;
  ////////////////////////////////////////////////////////////////////////////
  isStopped       = 1                                                        ;
  isActive        = 0                                                        ;
  stopProcessing  = 0                                                        ;
  abortProcessing = 0                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  conduit -> finish ( Conduit::OutputDirection , Conduit::Correct )          ;
  cpuLoadMeasurer . Reset ( )                                                ;
  ////////////////////////////////////////////////////////////////////////////
  delete [] dat                                                              ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "OpenAL player stopped\n" ) )                                  ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

CaError OpenALStream::Put(void)
{
  dPrintf ( ( "OpenAL recorder started\n" ) )                                ;
  ////////////////////////////////////////////////////////////////////////////
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
  int             sample     = 0                                             ;
  int             callbackResult                                             ;
  ////////////////////////////////////////////////////////////////////////////
  Interval *= 1000.0                                                         ;
  Interval /= sampleRate                                                     ;
  memset ( dat , 0 , bufferSize )                                            ;
  ////////////////////////////////////////////////////////////////////////////
  ALCdevice  * device     = NULL                                             ;
  ALenum       alFmt      = alFormat                                         ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ! done )                                                              {
    if ( CaIsNull(spec->name) ) done = true                                  ;
    if ( CaAND ( ! done , strlen(spec->name) <= 0 ) ) done = true            ;
    if ( CaAND ( ! done , CaNotNull ( spec->device ) ) )                     {
      device = (ALCdevice *)spec->device                                     ;
    }                                                                        ;
    if ( CaAND ( ! done , CaIsNull ( device ) ) )                            {
      if ( spec->useExternal ) alFmt = (ALenum)spec->format                  ;
      device         = ::alcCaptureOpenDevice                                (
                           spec->name                                        ,
                           (ALCuint)sampleRate                               ,
                           alFmt                                             ,
                           bufferSize                                      ) ;
      spec -> device = (void *)device                                        ;
    }                                                                        ;
    if ( CaIsNull ( device ) ) done = true                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaAND ( ! done , CaNotNull ( device ) ) )                             {
    ::alcCaptureStart ( device )                                             ;
    if ( CaNotEqual ( ::alGetError() , AL_NO_ERROR ) )                       {
      dPrintf ( ( "alcCaptureStart seems ... can not start\n" ) )            ;
      done   = true                                                          ;
    }                                                                        ;
  }                                                                          ;
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
    sample = 0                                                               ;
    ::alcGetIntegerv ( device , ALC_CAPTURE_SAMPLES , (ALCsizei)BPF,&sample) ;
    if ( sample > 0 )                                                        {
      ::alcCaptureSamples ( device, (ALCvoid *)dat, sample)                  ;
    }                                                                        ;
    if ( sample <= 0 ) continue                                              ;
    //////////////////////////////////////////////////////////////////////////
    conduit -> Input . Buffer     = dat                                      ;
    conduit -> Input . FrameCount = sample                                   ;
    conduit -> Input . AdcDac     = IBA                                      ;
    conduit -> LockConduit   ( )                                             ;
    callbackResult                = conduit -> put ( )                       ;
    conduit -> UnlockConduit ( )                                             ;
    conduit -> Input . Buffer     = NULL                                     ;
    OutputSize                    += sample * BPF                            ;
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
  if ( CaNotNull ( device ) )                                                {
    ::alcCaptureStop        ( device )                                       ;
    ::alcCaptureCloseDevice ( device )                                       ;
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
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "OpenAL recorder stopped\n" ) )                                ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

OpenALHostApiInfo:: OpenALHostApiInfo (void)
                  : HostApiInfo       (    )
{
}

OpenALHostApiInfo::~OpenALHostApiInfo (void)
{
}

//////////////////////////////////////////////////////////////////////////////

const char * OpenALSupportedFormats [] = {
  "AL_FORMAT_MONO8"                      ,
  "AL_FORMAT_MONO16"                     ,
  "AL_FORMAT_STEREO8"                    ,
  "AL_FORMAT_STEREO16"                   ,
  "AL_FORMAT_IMA_ADPCM_MONO16_EXT"       ,
  "AL_FORMAT_IMA_ADPCM_STEREO16_EXT"     ,
  "AL_FORMAT_WAVE_EXT"                   ,
  "AL_FORMAT_VORBIS_EXT"                 ,
  "AL_FORMAT_QUAD8_LOKI"                 ,
  "AL_FORMAT_QUAD16_LOKI"                ,
  "AL_FORMAT_MONO_FLOAT32"               ,
  "AL_FORMAT_STEREO_FLOAT32"             ,
  "AL_FORMAT_MONO_DOUBLE_EXT"            ,
  "AL_FORMAT_STEREO_DOUBLE_EXT"          ,
  "AL_FORMAT_MONO_MULAW_EXT"             ,
  "AL_FORMAT_STEREO_MULAW_EXT"           ,
  "AL_FORMAT_MONO_ALAW_EXT"              ,
  "AL_FORMAT_STEREO_ALAW_EXT"            ,
  "AL_FORMAT_QUAD8"                      ,
  "AL_FORMAT_QUAD16"                     ,
  "AL_FORMAT_QUAD32"                     ,
  "AL_FORMAT_REAR8"                      ,
  "AL_FORMAT_REAR16"                     ,
  "AL_FORMAT_REAR32"                     ,
  "AL_FORMAT_51CHN8"                     ,
  "AL_FORMAT_51CHN16"                    ,
  "AL_FORMAT_51CHN32"                    ,
  "AL_FORMAT_61CHN8"                     ,
  "AL_FORMAT_61CHN16"                    ,
  "AL_FORMAT_61CHN32"                    ,
  "AL_FORMAT_71CHN8"                     ,
  "AL_FORMAT_71CHN16"                    ,
  "AL_FORMAT_71CHN32"                    ,
  "AL_FORMAT_MONO_MULAW"                 ,
  "AL_FORMAT_STEREO_MULAW"               ,
  "AL_FORMAT_QUAD_MULAW"                 ,
  "AL_FORMAT_REAR_MULAW"                 ,
  "AL_FORMAT_51CHN_MULAW"                ,
  "AL_FORMAT_61CHN_MULAW"                ,
  "AL_FORMAT_71CHN_MULAW"                ,
  "AL_FORMAT_MONO_IMA4"                  ,
  "AL_FORMAT_STEREO_IMA4"                ,
  "AL_INTERNAL_FORMAT_SOFT"              ,
  NULL                                 } ;

static CaDeviceConf OpenALSupported [] = {
  {  1 , cafUint8   , 0 , -1 }           ,
  {  1 , cafInt16   , 0 , -1 }           ,
  {  2 , cafUint8   , 0 , -1 }           ,
  {  2 , cafInt16   , 0 , -1 }           ,
  {  1 , cafInt16   , 0 , -1 }           ,
  {  2 , cafInt16   , 0 , -1 }           ,
  { -1 , cafNothing , 0 , -1 }           ,
  { -1 , cafNothing , 0 , -1 }           ,
  { -1 , cafUint8   , 0 , -1 }           ,
  { -1 , cafInt16   , 0 , -1 }           ,
  {  1 , cafFloat32 , 0 , -1 }           ,
  {  2 , cafFloat32 , 0 , -1 }           ,
  {  1 , cafFloat64 , 0 , -1 }           ,
  {  2 , cafFloat64 , 0 , -1 }           ,
  { -1 , cafNothing , 0 , -1 }           ,
  { -1 , cafNothing , 0 , -1 }           ,
  { -1 , cafNothing , 0 , -1 }           ,
  { -1 , cafNothing , 0 , -1 }           ,
  {  4 , cafUint8   , 0 , -1 }           ,
  {  4 , cafInt16   , 0 , -1 }           ,
  {  4 , cafInt32   , 0 , -1 }           ,
  {  1 , cafUint8   , 0 , -1 }           ,
  {  1 , cafInt16   , 0 , -1 }           ,
  {  1 , cafInt32   , 0 , -1 }           ,
  {  5 , cafUint8   , 0 , -1 }           ,
  {  5 , cafInt16   , 0 , -1 }           ,
  {  5 , cafInt32   , 0 , -1 }           ,
  {  6 , cafUint8   , 0 , -1 }           ,
  {  6 , cafInt16   , 0 , -1 }           ,
  {  6 , cafInt32   , 0 , -1 }           ,
  {  7 , cafUint8   , 0 , -1 }           ,
  {  7 , cafInt16   , 0 , -1 }           ,
  {  7 , cafInt32   , 0 , -1 }           ,
  {  1 , cafNothing , 0 , -1 }           ,
  {  2 , cafNothing , 0 , -1 }           ,
  {  4 , cafNothing , 0 , -1 }           ,
  {  1 , cafNothing , 0 , -1 }           ,
  {  5 , cafNothing , 0 , -1 }           ,
  {  6 , cafNothing , 0 , -1 }           ,
  {  7 , cafNothing , 0 , -1 }           ,
  {  1 , cafNothing , 0 , -1 }           ,
  {  2 , cafNothing , 0 , -1 }           ,
  {  1 , cafNothing , 0 , -1 }           ,
  {  0 , cafNothing , 0 ,  0 }         } ;

OpenALDeviceInfo:: OpenALDeviceInfo (void)
                 : DeviceInfo       (    )
{
  canInput  = false ;
  canOutput = false ;
  MaxMono   = 0     ;
  MaxStereo = 0     ;
  TotalConf = 0     ;
  MaxConf   = 0     ;
  CONFs     = NULL  ;
  Formats   = NULL  ;
}

OpenALDeviceInfo::~OpenALDeviceInfo (void)
{
  if ( CaNotNull(name))           {
    ::free((void *)name)          ;
    name = NULL                   ;
  }                               ;
  /////////////////////////////////
  if ( CaNotNull(CONFs))          {
    int i = 0                     ;
    while ( CaNotNull(CONFs[i]) ) {
      delete CONFs[i]             ;
      i++                         ;
    }                             ;
    delete [] CONFs               ;
    CONFs = NULL                  ;
  }                               ;
  /////////////////////////////////
  if ( CaNotNull(Formats))        {
    delete [] Formats             ;
    Formats = NULL                ;
  }                               ;
}

bool OpenALDeviceInfo::ObtainDetails(ALCdevice * device)
{
  if ( maxOutputChannels > 0 ) ObtainOutput ( device ) ;
  if ( maxInputChannels  > 0 ) ObtainInput  (        ) ;
  SuitableConf ( )                                     ;
  return true                                          ;
}

bool OpenALDeviceInfo::VerifyCapture (int index)
{
  if ( index                 >= TotalConf ) return false                     ;
  if ( CONFs[index]->support >  0         ) return true                      ;
  if ( CONFs[index]->support == 0         ) return false                     ;
  ////////////////////////////////////////////////////////////////////////////
  CaDeviceConf * conf   = CONFs   [ index ]                                  ;
  ALCdevice    * device = NULL                                               ;
  ALenum         e      = Formats [ index ]                                  ;
  ////////////////////////////////////////////////////////////////////////////
  device = ::alcCaptureOpenDevice ( name , (int)conf->rate , e , 128 )       ;
  if ( CaNotNull ( device ) )                                                {
    ::alcCaptureCloseDevice ( device )                                       ;
    conf->support = 1                                                        ;
    if ( CaIsGreater ( conf->channels , maxInputChannels ) )                 {
      maxInputChannels = conf->channels                                      ;
    }                                                                        ;
  } else                                                                     {
    conf->support = 0                                                        ;
  }                                                                          ;
  return true                                                                ;
}

bool OpenALDeviceInfo::ObtainInput(void)
{
  int f = 0                                                                  ;
  ////////////////////////////////////////////////////////////////////////////
  while ( CaAND ( CaNotNull  ( OpenALSupportedFormats [ f ]                ) ,
                  CaNotEqual ( OpenALSupported [ f ] . channels , 0 ) )    ) {
    ALenum e = alGetEnumValue ( OpenALSupportedFormats [ f ] )               ;
    if ( CaNotEqual ( e , AL_INVALID_ENUM ) )                                {
      int i = 0                                                              ;
      while ( CaIsGreater ( AllSamplingRates [ i ] , 0 ) )                   {
        CaDeviceConf * conf = new CaDeviceConf ( )                           ;
        int SampleRate      = AllSamplingRates [ i ]                         ;
        ::memcpy ( conf , &OpenALSupported[f] , sizeof(CaDeviceConf) )       ;
        conf->rate = SampleRate                                              ;
        addConf ( conf , e )                                                 ;
        i++                                                                  ;
      }                                                                      ;
    } else                                                                   {
      gPrintf ( ( "%s is not supported\n" , OpenALSupportedFormats [ f ] ) ) ;
    }                                                                        ;
    f++                                                                      ;
  }                                                                          ;
  return true                                                                ;
}

bool OpenALDeviceInfo::ObtainOutput(ALCdevice * device)
{
  int           f       = 0                                                  ;
  ALCcontext  * context = NULL                                               ;
  unsigned char silence [ 4096 ]                                             ;
  ALuint        source                                                       ;
  ALuint        buffer                                                       ;
  ::memset ( silence , 0 , 4096 )                                            ;
  ////////////////////////////////////////////////////////////////////////////
  context = ::alcCreateContext ( device , NULL )                             ;
  CaRETURN ( CaIsNull ( context ) , false )                                  ;
  if ( CaNOT ( ::alcMakeContextCurrent ( context ) ) )                       {
    ::alcDestroyContext ( context )                                          ;
    return false                                                             ;
  }                                                                          ;
  if ( CaNotEqual ( ::alGetError() , AL_NO_ERROR ) )                         {
    ::alcDestroyContext ( context )                                          ;
    return false                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ::alGenSources ( (ALuint)1 , &source )                                     ;
  if ( CaNotEqual ( ::alGetError() , AL_NO_ERROR ) )                         {
    ::alcMakeContextCurrent ( NULL    )                                      ;
    ::alcDestroyContext     ( context )                                      ;
    return false                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ::alGenBuffers ( (ALuint)1 , &buffer )                                     ;
  if ( CaNotEqual ( ::alGetError() , AL_NO_ERROR ) )                         {
    ::alDeleteSources       ( 1 , &source )                                  ;
    ::alcMakeContextCurrent ( NULL        )                                  ;
    ::alcDestroyContext     ( context     )                                  ;
    return false                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  while ( CaAND ( CaNotNull  ( OpenALSupportedFormats [ f ]           )      ,
                  CaNotEqual ( OpenALSupported [ f ] . channels , 0 ) )    ) {
    ALenum e = alGetEnumValue ( OpenALSupportedFormats [ f ] )               ;
    if ( CaNotEqual ( e , AL_INVALID_ENUM ) )                                {
      int i = 0                                                              ;
      while ( CaIsGreater ( AllSamplingRates [ i ] , 0 ) )                   {
        CaDeviceConf * conf = new CaDeviceConf ( )                           ;
        int SampleRate      = AllSamplingRates [ i ]                         ;
        ::memcpy ( conf , &OpenALSupported[f] , sizeof(CaDeviceConf) )       ;
        conf -> rate    = SampleRate                                         ;
        conf -> support = 0                                                  ;
        if ( CaAND ( CaIsGreater ( conf->channels , 0          )             ,
                     CaNotEqual  ( conf->format   , cafNothing )         ) ) {
          ::alBufferData( buffer,e,silence,4096,(ALsizei)conf->rate )        ;
          if ( CaIsEqual ( ::alGetError() , AL_NO_ERROR ) )                  {
            conf->support = 1                                                ;
            if ( CaIsGreater ( conf->channels , maxOutputChannels ) )        {
              maxOutputChannels = conf->channels                             ;
            }                                                                ;
          }                                                                  ;
        }                                                                    ;
        if ( CaIsEqual ( conf->support , 0 ) )                               {
          delete conf                                                        ;
        } else                                                               {
          addConf ( conf , e )                                               ;
        }                                                                    ;
        i++                                                                  ;
      }                                                                      ;
    } else                                                                   {
      gPrintf ( ( "%s is not supported\n" , OpenALSupportedFormats [ f ] ) ) ;
    }                                                                        ;
    f++                                                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ::alDeleteBuffers       ( 1 , &buffer )                                    ;
  ::alDeleteSources       ( 1 , &source )                                    ;
  ::alcMakeContextCurrent ( NULL        )                                    ;
  ::alcDestroyContext     ( context     )                                    ;
  ////////////////////////////////////////////////////////////////////////////
  return true                                                                ;
}

int OpenALDeviceInfo::addConf(CaDeviceConf * conf,ALenum format)
{
  if ( CaIsEqual ( MaxConf , 0 ) )           {
    MaxConf   = 1024                         ;
    CONFs     = new CaDeviceConf * [ 1024 ]  ;
    Formats   = new ALenum         [ 1024 ]  ;
    for (int i=0;i<1024;i++)                 {
      CONFs   [ i ] = NULL                   ;
      Formats [ i ] = AL_INVALID_ENUM        ;
    }                                        ;
  }                                          ;
  ////////////////////////////////////////////
  if ( ( TotalConf + 1 ) >= MaxConf )        {
    int maxConf = MaxConf + 1024             ;
    CaDeviceConf ** OldCONFs = CONFs         ;
    CaDeviceConf ** NewCONFs = NULL          ;
    ALenum       *  NewEnum  = NULL          ;
    NewCONFs = new CaDeviceConf * [maxConf]  ;
    NewEnum  = new ALenum         [maxConf]  ;
    for (int i=0;i<maxConf;i++)              {
      NewCONFs [ i ] = NULL                  ;
      NewEnum  [ i ] = AL_INVALID_ENUM       ;
    }                                        ;
    for (int i=0;i<TotalConf;i++)            {
      NewCONFs [ i ] = CONFs   [ i ]         ;
      NewEnum  [ i ] = Formats [ i ]         ;
    }                                        ;
    delete [] OldCONFs                       ;
    delete [] Formats                        ;
    CONFs   = NewCONFs                       ;
    Formats = NewEnum                        ;
  }                                          ;
  ////////////////////////////////////////////
  if ( CaIsGreater ( TotalConf , 0 ) )       {
    int len = sizeof(CaDeviceConf)           ;
    int r                                    ;
    for (int i = 0 ; i < TotalConf ; i++ )   {
      r = ::memcmp ( conf , CONFs[i] , len ) ;
      if ( CaIsEqual ( r , 0 ) )             {
        return TotalConf                     ;
      }                                      ;
    }                                        ;
  }                                          ;
  ////////////////////////////////////////////
  CONFs   [ TotalConf ] = conf               ;
  Formats [ TotalConf ] = format             ;
  TotalConf ++                               ;
  CONFs   [ TotalConf ] = NULL               ;
  Formats [ TotalConf ] = AL_INVALID_ENUM    ;
  ////////////////////////////////////////////
  return TotalConf                           ;
}

void OpenALDeviceInfo::SuitableConf(void)
{
  if ( ( TotalConf + 1 ) == MaxConf ) return ;
  if ( CaIsEqual ( TotalConf , 0 ) )         {
    delete [] CONFs                          ;
    MaxConf = 0                              ;
    return                                   ;
  }                                          ;
  ////////////////////////////////////////////
  CaDeviceConf ** OldCONFs = CONFs           ;
  CaDeviceConf ** NewCONFs = NULL            ;
  MaxConf  = TotalConf + 1                   ;
  NewCONFs = new CaDeviceConf * [ MaxConf ]  ;
  NewCONFs [ TotalConf ] = NULL              ;
  for (int i=0;i<TotalConf;i++)              {
    NewCONFs [ i ] = OldCONFs [ i ]          ;
  }                                          ;
  ////////////////////////////////////////////
  CONFs = NewCONFs                           ;
  delete [] OldCONFs                         ;
}

int OpenALDeviceInfo::SupportIndex (
      int            channels      ,
      CaSampleFormat format        ,
      double         rate          )
{
  for (int i=0;i<TotalConf;i++)                              {
    if ( CaIsEqual ( channels  ,      CONFs[i]->channels )  &&
         CaIsEqual ( format    ,      CONFs[i]->format   )  &&
         CaIsEqual ( (int)rate , (int)CONFs[i]->rate     ) ) {
      return i                                               ;
    }                                                        ;
  }                                                          ;
  return -1                                                  ;
}

ALenum OpenALDeviceInfo::GetFormat (
         int            channels   ,
         CaSampleFormat format     ,
         double         rate       )
{
  int index = SupportIndex ( channels , format , rate ) ;
  if ( index < 0 ) return AL_INVALID_ENUM               ;
  return Formats [ index ]                              ;
}

///////////////////////////////////////////////////////////////////////////////

OpenALHostApi:: OpenALHostApi (void)
              : HostApi       (    )
{
  allocations       = NULL ;
  inputDeviceCount  = 0    ;
  outputDeviceCount = 0    ;
}

OpenALHostApi::~OpenALHostApi(void)
{
}

OpenALHostApi::Encoding OpenALHostApi::encoding(void) const
{
  #if defined(WIN32) || defined(_WIN32)
  return NATIVE ;
  #else
  return UTF8   ;
  #endif
}

bool OpenALHostApi::hasDuplex(void) const
{
  return false ;
}

CaError OpenALHostApi::Open                           (
          Stream                 ** s                 ,
          const StreamParameters *  inputParameters   ,
          const StreamParameters *  outputParameters  ,
          double                    sampleRate        ,
          CaUint32                  framesPerCallback ,
          CaStreamFlags             streamFlags       ,
          Conduit                *  conduit           )
{
  CaError        result = NoError                                            ;
  OpenALStream * stream = NULL                                               ;
  CaSampleFormat hostInputSampleFormat                                       ;
  CaSampleFormat hostOutputSampleFormat                                      ;
  CaSampleFormat inputSampleFormat                                           ;
  CaSampleFormat outputSampleFormat                                          ;
  int            inputChannelCount                                           ;
  int            outputChannelCount                                          ;
  double         suggestedInputLatency                                       ;
  double         suggestedOutputLatency                                      ;
  ////////////////////////////////////////////////////////////////////////////
  result = isSupported ( inputParameters , outputParameters , sampleRate )   ;
  if ( CaIsWrong ( result ) ) return InvalidSampleRate                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( inputParameters  ) )                                      {
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
  if ( CaNotNull ( outputParameters ) )                                      {
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
  stream = new OpenALStream ( )                                              ;
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
    //////////////////////////////////////////////////////////////////////////
    int did = inputParameters->device                                        ;
    if ( did > info.deviceCount ) return InvalidDevice                       ;
    OpenALDeviceInfo * dev = (OpenALDeviceInfo *)deviceInfos[did]            ;
    if ( CaIsNull(dev) ) return UnanticipatedHostError                       ;
    //////////////////////////////////////////////////////////////////////////
    stream -> SampleFormat   = inputSampleFormat                             ;
    stream -> Channels       = inputChannelCount                             ;
    stream -> BytesPerSample = Core::SampleSize(inputSampleFormat)           ;
    stream -> alFormat       = dev -> GetFormat                              (
                                 inputChannelCount                           ,
                                 inputSampleFormat                           ,
                                 sampleRate                                ) ;
    stream -> VerifyInfo ( inputParameters  -> streamInfo )   ;
    if ( CaIsNull(stream->spec->name)) stream->spec->name = (char *)dev->name;
    ((StreamParameters *)inputParameters ) -> streamInfo = stream->spec ;
    conduit -> Input . setSample ( inputSampleFormat , inputChannelCount )   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    //////////////////////////////////////////////////////////////////////////
    int did = outputParameters->device                                       ;
    if ( did > info.deviceCount ) return InvalidDevice                       ;
    OpenALDeviceInfo * dev = (OpenALDeviceInfo *)deviceInfos[did]            ;
    if ( CaIsNull(dev) ) return UnanticipatedHostError                       ;
    //////////////////////////////////////////////////////////////////////////
    stream -> SampleFormat   = outputSampleFormat                            ;
    stream -> Channels       = outputChannelCount                            ;
    stream -> BytesPerSample = Core::SampleSize(outputSampleFormat)          ;
    stream -> alFormat       = dev -> GetFormat                              (
                                 outputChannelCount                          ,
                                 outputSampleFormat                          ,
                                 sampleRate                                ) ;
    stream -> VerifyInfo ( outputParameters -> streamInfo )   ;
    if ( CaIsNull(stream->spec->name)) stream->spec->name = (char *)dev->name;
    ((StreamParameters *)outputParameters) -> streamInfo = stream->spec ;
    conduit -> Output . setSample ( outputSampleFormat,outputChannelCount  ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *s = (Stream *)stream                                                      ;
  return result                                                              ;
}

void OpenALHostApi::Terminate(void)
{
  if ( CaIsNull ( deviceInfos ) ) return ;
  for (int i=0;i<info.deviceCount;i++)   {
    delete deviceInfos[i]                ;
  }                                      ;
  delete [] deviceInfos                  ;
  deviceInfos = NULL                     ;
}

CaError OpenALHostApi::isSupported                 (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double sampleRate                          )
{
  int            inputChannelCount                                           ;
  int            outputChannelCount                                          ;
  CaSampleFormat inputSampleFormat                                           ;
  CaSampleFormat outputSampleFormat                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaAND ( CaNotNull ( inputParameters  )                                ,
               CaNotNull ( outputParameters )                            ) ) {
    return InvalidFlag                                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    inputChannelCount = inputParameters -> channelCount                      ;
    inputSampleFormat = inputParameters -> sampleFormat                      ;
    if ( CaUseHostApiSpecificDeviceSpecification==inputParameters->device  ) {
      return InvalidDevice                                                   ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    int did = inputParameters->device                                        ;
    if ( did > info.deviceCount ) return InvalidDevice                       ;
    OpenALDeviceInfo * dev = (OpenALDeviceInfo *)deviceInfos[did]            ;
    if ( CaIsNull(dev) ) return UnanticipatedHostError                       ;
    int index = dev -> SupportIndex                                          (
                  inputChannelCount                                          ,
                  inputSampleFormat                                          ,
                  sampleRate                                               ) ;
    if ( CaIsLess ( index , 0 ) ) return InvalidSampleRate                   ;
    //////////////////////////////////////////////////////////////////////////
    if ( ! dev -> VerifyCapture ( index ) ) return InvalidSampleRate         ;
    //////////////////////////////////////////////////////////////////////////
    if ( CaIsLess ( dev->CONFs[index]->support , 1 ) )                       {
      return InvalidSampleRate                                               ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputChannelCount = outputParameters -> channelCount                    ;
    outputSampleFormat = outputParameters -> sampleFormat                    ;
    if ( CaUseHostApiSpecificDeviceSpecification==outputParameters->device ) {
      return InvalidDevice                                                   ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    int did = outputParameters->device                                       ;
    if ( did > info.deviceCount ) return InvalidDevice                       ;
    OpenALDeviceInfo * dev = (OpenALDeviceInfo *)deviceInfos[did]            ;
    if ( CaIsNull(dev) ) return UnanticipatedHostError                       ;
    int index = dev -> SupportIndex                                          (
                  outputChannelCount                                         ,
                  outputSampleFormat                                         ,
                  sampleRate                                               ) ;
    if ( CaIsLess ( index , 0 ) ) return InvalidSampleRate                   ;
    if ( CaIsLess ( dev->CONFs[index]->support , 1 ) )                       {
      return InvalidSampleRate                                               ;
    }                                                                        ;
  }                                                                          ;
  return NoError                                                             ;
}

bool OpenALHostApi::FindDevices(void)
{
  deviceInfos = new DeviceInfo * [ 1000 ]                                    ;
  ////////////////////////////////////////////////////////////////////////////
  for (int i=0;i<1000;i++) deviceInfos [ i ] = NULL                          ;
  ////////////////////////////////////////////////////////////////////////////
  FindPlayback ( )                                                           ;
  FindCapture  ( )                                                           ;
  ////////////////////////////////////////////////////////////////////////////
  if ( info.deviceCount <= 0 )                                               {
    delete [] deviceInfos                                                    ;
    deviceInfos = NULL                                                       ;
  } else                                                                     {
    DeviceInfo ** devInfo = new DeviceInfo * [ info.deviceCount + 1]         ;
    devInfo [ info.deviceCount ] = NULL                                      ;
    for (int i = 0 ; i < info . deviceCount ; i++ )                          {
      devInfo [ i ] = deviceInfos [ i ]                                      ;
    }                                                                        ;
    delete [] deviceInfos                                                    ;
    deviceInfos = devInfo                                                    ;
  }                                                                          ;
  return true                                                                ;
}

bool OpenALHostApi::FindPlayback(void)
{
  const ALchar * pList = ::alcGetString ( NULL , ALC_DEVICE_SPECIFIER)       ;
  while ( CaNotNull ( *pList ) )                                             {
    ALCdevice * device = ::alcOpenDevice ( pList )                           ;
    if ( CaNotNull ( device ) )                                              {
      ALCint mono   = 0                                                      ;
      ALCint stereo = 0                                                      ;
      ::alcGetIntegerv ( device , ALC_MONO_SOURCES   , 1 , &mono   )         ;
      ::alcGetIntegerv ( device , ALC_STEREO_SOURCES , 1 , &stereo )         ;
      if ( CaOR ( CaIsGreater ( mono   , 0 )                                 ,
                  CaIsGreater ( stereo , 0 )                             ) ) {
        OpenALDeviceInfo * dev = new OpenALDeviceInfo ( )                    ;
        dev -> structVersion            = 2                                  ;
        dev -> name                     = (const char *)strdup(pList)        ;
        dev -> hostApi                  = info.index                         ;
        dev -> hostType                 = OpenAL                             ;
        dev -> maxInputChannels         = 0                                  ;
        dev -> maxOutputChannels        = 0                                  ;
        dev -> defaultLowInputLatency   = 0                                  ;
        dev -> defaultLowOutputLatency  = 0                                  ;
        dev -> defaultHighInputLatency  = 0                                  ;
        dev -> defaultHighOutputLatency = 0                                  ;
        dev -> defaultSampleRate        = 44100                              ;
        dev -> MaxMono                  = mono                               ;
        dev -> MaxStereo                = stereo                             ;
        if ( stereo > 0 )                                                    {
          dev -> maxOutputChannels = 2                                       ;
        } else
          dev -> maxOutputChannels = 1                                       ;
        if ( mono   > 0 )                                                    {
        } else                                                               {
          delete [] dev->name                                                ;
          delete dev                                                         ;
          dev = NULL                                                         ;
        }                                                                    ;
        dev -> ObtainDetails ( device )                                      ;
        if ( CaNotNull ( dev ) )                                             {
          deviceInfos [ info.deviceCount ] = dev                             ;
          info.deviceCount++                                                 ;
        }                                                                    ;
      }                                                                      ;
      ::alcCloseDevice ( device )                                            ;
    }                                                                        ;
    pList += strlen(pList) + 1                                               ;
  }                                                                          ;
  return true                                                                ;
}

bool OpenALHostApi::FindCapture(void)
{
  const ALchar * pList                                                       ;
  pList = ::alcGetString ( NULL , ALC_CAPTURE_DEVICE_SPECIFIER)              ;
  while ( CaNotNull ( *pList ) )                                             {
    int         channels = 0                                                 ;
    ALCdevice * device   = NULL                                              ;
    device   = ::alcCaptureOpenDevice ( pList,8000,AL_FORMAT_STEREO16,4096 ) ;
    if ( CaIsNull ( device ) )                                               {
      device = ::alcCaptureOpenDevice ( pList,8000,AL_FORMAT_STEREO8 ,4096 ) ;
    } else channels = 2                                                      ;
    if ( CaIsNull ( device ) )                                               {
      device = ::alcCaptureOpenDevice ( pList,8000,AL_FORMAT_MONO16  ,4096 ) ;
    } else channels = 2                                                      ;
    if ( CaIsNull ( device ) )                                               {
      device = ::alcCaptureOpenDevice ( pList,8000,AL_FORMAT_MONO8   ,4096 ) ;
      if ( CaNotNull ( device ) ) channels = 1                               ;
    } else channels = 1                                                      ;
    if ( CaIsNull ( device ) ) channels = 0 ; else                           {
      ::alcCaptureCloseDevice ( device )                                     ;
    }                                                                        ;
    if ( channels > 0 )                                                      {
      OpenALDeviceInfo * dev = new OpenALDeviceInfo ( )                      ;
      dev -> structVersion            = 2                                    ;
      dev -> name                     = (const char *)strdup(pList)          ;
      dev -> hostApi                  = info.index                           ;
      dev -> hostType                 = OpenAL                               ;
      dev -> maxInputChannels         = channels                             ;
      dev -> maxOutputChannels        = 0                                    ;
      dev -> defaultLowInputLatency   = 0                                    ;
      dev -> defaultLowOutputLatency  = 0                                    ;
      dev -> defaultHighInputLatency  = 0                                    ;
      dev -> defaultHighOutputLatency = 0                                    ;
      dev -> defaultSampleRate        = 44100                                ;
      dev -> MaxMono                  = 0                                    ;
      dev -> MaxStereo                = 0                                    ;
      dev -> ObtainDetails ( device )                                        ;
      deviceInfos [ info.deviceCount ] = dev                                 ;
      info.deviceCount++                                                     ;
    }                                                                        ;
    pList += strlen(pList) + 1                                               ;
  }                                                                          ;
  return true                                                                ;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
