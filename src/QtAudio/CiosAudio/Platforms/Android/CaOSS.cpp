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

// The place where you need to see: http://manuals.opensound.com/developer/

#include "CaOSS.hpp"

///////////////////////////////////////////////////////////////////////////////

#define CA_MIN(x,y) ( (x) < (y) ? (x) : (y) )
#define CA_MAX(x,y) ( (x) > (y) ? (x) : (y) )

/* Utilize GCC branch prediction for error tests */
#if defined __GNUC__ && __GNUC__ >= 3
#define UNLIKELY(expr) __builtin_expect( (expr), 0 )
#else
#define UNLIKELY(expr) (expr)
#endif

#define STRINGIZE_HELPER(expr) #expr
#define STRINGIZE(expr) STRINGIZE_HELPER(expr)

/* Check return value of system call, and map it to PaError */
#define ENSURE_(expr, code) \
  do { \
    if ( UNLIKELY( (sysErr_ = (expr)) < 0 ) ) { \
      /* SetLastHostErrorInfo should only be used in the main thread */ \
      if ( (code) == UnanticipatedHostError && pthread_self() == mainThread_ ) { \
        SetLastHostErrorInfo( OSS, sysErr_, strerror( errno ) )              ; \
      } \
      if ( NULL != globalDebugger ) { \
        globalDebugger -> printf ( \
          "Expression '" #expr \
          "' failed in '" __FILE__ \
          "', line: " STRINGIZE( __LINE__ ) \
          "\n" ); \
      } ; \
      result = (code); \
      goto error; \
    } \
  } while( 0 );

#ifndef AFMT_S16_NE
#define AFMT_S16_NE  Get_AFMT_S16_NE()
/*********************************************************************
 * Some versions of OSS do not define AFMT_S16_NE. So check CPU.
 * PowerPC is Big Endian. X86 is Little Endian.
 */
static int Get_AFMT_S16_NE(void)
{
  long testData = 1;
  char *ptr = (char *) &testData;
  int isLittle = ( *ptr == 1 ); /* Does address point to least significant byte? */
  return isLittle ? AFMT_S16_LE : AFMT_S16_BE ;
}
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

static int       sysErr_     = 0 ;
static pthread_t mainThread_ = 0 ;

#ifndef REMOVE_DEBUG_MESSAGE

#define CA_UNLESS(expr, code) \
    do { \
        if( UNLIKELY( (expr) == 0 ) ) \
        { \
            if ( NULL != globalDebugger ) { \
              globalDebugger->printf ( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ) ; \
            } ; \
            result = (code); \
            goto error; \
        } \
    } while (0);

#else

#define CA_UNLESS(expr, code) \
    do { \
        if( UNLIKELY( (expr) == 0 ) ) \
        { \
            result = (code); \
            goto error; \
        } \
    } while (0);

#endif

/* Used with CA_ENSURE */
static int caUtilErr_ ;

#ifndef REMOVE_DEBUG_MESSAGE

/* Check CaError */
#define CA_ENSURE(expr)                                  \
  do                                                   { \
    if ( UNLIKELY( (caUtilErr_ = (expr)) < NoError ) ) { \
      if ( NULL != globalDebugger )                    { \
        globalDebugger -> printf                       ( \
          "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ) ; \
      }                                                ; \
      result = caUtilErr_                              ; \
      goto error                                       ; \
    }                                                    \
  } while (0)

#else

do                                                   { \
  if ( UNLIKELY( (caUtilErr_ = (expr)) < NoError ) ) { \
    result = caUtilErr_                              ; \
    goto error                                       ; \
  }                                                    \
} while (0)

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

static int CalcHigherLogTwo(int n)
{
  int log2 = 0                       ;
  while ( ( 1 << log2 ) < n ) log2++ ;
  return log2                        ;
}

///////////////////////////////////////////////////////////////////////////////

/* Translate from CA format to OSS native. */

static CaError OssFormat(CaSampleFormat caFormat,int * ossFormat)
{
  switch ( caFormat )          {
    case cafUint8              :
      *ossFormat = AFMT_U8     ;
    break                      ;
    case cafInt8               :
      *ossFormat = AFMT_S8     ;
    break                      ;
    case cafInt16              :
      *ossFormat = AFMT_S16_NE ;
    break                      ;
    default                    :
    return InternalError       ;
  }                            ;
  return NoError               ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError ModifyBlocking(int fd,int blocking)
{
  CaError result = NoError                                                   ;
  int     fflags                                                             ;
  ENSURE_ ( fflags = fcntl( fd, F_GETFL ) , UnanticipatedHostError )         ;
  if ( blocking ) fflags &= ~O_NONBLOCK                                      ;
             else fflags |=  O_NONBLOCK                                      ;
  ENSURE_ ( fcntl( fd, F_SETFL, fflags )  , UnanticipatedHostError )         ;
error                                                                        :
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

/* Open input and output devices.
 *
 * @param idev: Returned input device file descriptor.
 * @param odev: Returned output device file descriptor.
 */
static CaError OpenDevices     (
         const char * idevName ,
         const char * odevName ,
         int        * idev     ,
         int        * odev     )
{
  CaError result = NoError                                                   ;
  int     flags  = O_NONBLOCK                                                ;
  ////////////////////////////////////////////////////////////////////////////
  *idev = *odev = -1                                                         ;
  if ( idevName && odevName )                                                {
    flags |= O_RDWR                                                          ;
  } else
  if ( idevName ) flags |= O_RDONLY                                          ;
             else flags |= O_WRONLY                                          ;
  /* open first in nonblocking mode, in case it's busy...
   * A: then unset the non-blocking attribute                               */
  if ( idevName )                                                            {
    ENSURE_( *idev = open( idevName, flags ), DeviceUnavailable )            ;
    CA_ENSURE ( ModifyBlocking( *idev, 1 ) )                                 ;
    /* Blocking                                                             */
  }                                                                          ;
  if ( NULL != odevName )                                                    {
    if ( !idevName )                                                         {
      ENSURE_( *odev = open( odevName, flags ), DeviceUnavailable )          ;
      CA_ENSURE ( ModifyBlocking( *odev, 1 ) )                               ;
      /* Blocking                                                           */
    } else                                                                   {
      ENSURE_( *odev = dup( *idev ) , UnanticipatedHostError )               ;
    }                                                                        ;
  }                                                                          ;
error:
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

static CaError QueryDirection                    (
                 const char * deviceName         ,
                 StreamMode   mode               ,
                 double     * defaultSampleRate  ,
                 int        * maxChannelCount    ,
                 double     * defaultLowLatency  ,
                 double     * defaultHighLatency ,
                 int        * version            )
{
  CaError       result     = NoError                                         ;
  int           numChannels                                                  ;
  int           maxNumChannels                                               ;
  int           busy       =  0                                              ;
  int           devHandle  = -1                                              ;
  int           sr                                                           ;
  int           temp                                                         ;
  int           frgmt                                                        ;
  int           enableBits = 0                                               ;
  unsigned long fragFrames                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  *maxChannelCount = 0                                                       ;
  /* Default value in case this fails                                       */
  devHandle = ::open( deviceName, (mode == StreamMode_In ? O_RDONLY : O_WRONLY) | O_NONBLOCK ) ;
  if ( devHandle  < 0 )                                                      {
    if ( errno == EBUSY || errno == EAGAIN )                                 {
      gPrintf ( ( "%s: Device %s busy\n", __FUNCTION__, deviceName ) )       ;
    } else                                                                   {
      /* Ignore ENOENT, which means we've tried a non-existent device       */
      if ( errno != ENOENT )                                                 {
        gPrintf ( ( "%s: Can't access device %s: %s\n"                       ,
                    __FUNCTION__                                             ,
                    deviceName                                               ,
                    strerror( errno )                                    ) ) ;
      }                                                                      ;
    }                                                                        ;
    return DeviceUnavailable                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ::ioctl ( devHandle , OSS_GETVERSION , version )                           ;
  ////////////////////////////////////////////////////////////////////////////
  /* The OSS reference instructs us to clear direction bits before setting them. */
  maxNumChannels = 0                                                         ;
  for ( numChannels = 1; numChannels <= 16; numChannels++ )                  {
    temp = numChannels                                                       ;
    if ( ::ioctl( devHandle, SNDCTL_DSP_CHANNELS, &temp ) < 0 )              {
      busy = EAGAIN == errno || EBUSY == errno                               ;
      if ( maxNumChannels >= 2 ) break                                       ;
    } else                                                                   {
      if ( (numChannels > 2) && (temp != numChannels) ) break                ;
      if ( temp > maxNumChannels ) maxNumChannels = temp                     ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( 0 == maxNumChannels ) && busy )                                     {
    result = DeviceUnavailable                                               ;
    goto error                                                               ;
  }                                                                          ;
  /* The above negotiation may fail for an old driver so try this older technique. */
  if ( maxNumChannels < 1 )                                                  {
    int stereo = 1                                                           ;
    if ( ioctl( devHandle, SNDCTL_DSP_STEREO, &stereo ) < 0 )                {
      maxNumChannels = 1                                                     ;
    } else                                                                   {
      maxNumChannels = (stereo) ? 2 : 1                                      ;
    }                                                                        ;
    gPrintf                                                                ( (
      "%s: use SNDCTL_DSP_STEREO, maxNumChannels = %d\n"                     ,
      __FUNCTION__                                                           ,
      maxNumChannels                                                     ) ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  numChannels = CA_MIN ( maxNumChannels , 2 )                                ;
  ENSURE_ ( ioctl ( devHandle , SNDCTL_DSP_CHANNELS , &numChannels )         ,
            UnanticipatedHostError                                         ) ;
  /* Get supported sample rate closest to 44100 Hz                          */
  if ( *defaultSampleRate < 0 )                                              {
    sr = 44100                                                               ;
    ENSURE_( ioctl( devHandle , SNDCTL_DSP_SPEED , &sr )                     ,
             UnanticipatedHostError                                        ) ;
    *defaultSampleRate = sr                                                  ;
  }                                                                          ;
  *maxChannelCount = maxNumChannels                                          ;
  ////////////////////////////////////////////////////////////////////////////
  fragFrames = 128                                                           ;
  frgmt = ( 4 << 16 )                                                        +
          ( CalcHigherLogTwo ( fragFrames * numChannels * 2 ) & 0xffff     ) ;
  ENSURE_ ( ioctl ( devHandle , SNDCTL_DSP_SETFRAGMENT , &frgmt )            ,
            UnanticipatedHostError                                         ) ;
  ////////////////////////////////////////////////////////////////////////////
  /* Use the value set by the ioctl to give the latency achieved            */
  fragFrames = pow( 2, frgmt & 0xffff ) / (numChannels * 2)                  ;
  *defaultLowLatency = ((frgmt >> 16) - 1) * fragFrames / *defaultSampleRate ;
  /* Cannot now try setting a high latency (device would need closing and
   * opening again).  Make high-latency 4 times the low unless the fragFrames
   * are significantly more than requested 128                              */
  temp = (fragFrames < 256) ? 4 : (fragFrames < 512) ? 2 : 1                 ;
  *defaultHighLatency = temp * *defaultLowLatency                            ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaIsCorrect ( result ) )                                              {
    unsigned char BUF[4096]                                                  ;
    enableBits = 0                                                           ;
    result     = InvalidDevice                                               ;
    ::memset  ( BUF , 0 , 4096 )                                             ;
    ::write   ( devHandle,BUF,4096 )                                         ;
    if ( ioctl (devHandle,SNDCTL_DSP_SETTRIGGER,&enableBits) >= 0 )          {
      ::write   ( devHandle,BUF,4096 )                                       ;
      if ( StreamMode_In  == mode ) enableBits = PCM_ENABLE_INPUT            ;
      if ( StreamMode_Out == mode ) enableBits = PCM_ENABLE_OUTPUT           ;
      if ( ioctl (devHandle,SNDCTL_DSP_SETTRIGGER,&enableBits) >= 0 )        {
        result = NoError                                                     ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  if ( devHandle >= 0 ) ::close ( devHandle )                                ;
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

OssStreamComponent:: OssStreamComponent (void)
{
  fd               = -1         ;
  version          =  0         ;
  devName          = NULL       ;
  userChannelCount =  0         ;
  hostChannelCount =  0         ;
  userInterleaved  =  0         ;
  buffer           = NULL       ;
  userFormat       = cafNothing ;
  hostFormat       = cafNothing ;
  latency          =  0         ;
  hostFrames       =  0         ;
  numBufs          =  0         ;
  userBuffers      = NULL       ;
}

//////////////////////////////////////////////////////////////////////////////

OssStreamComponent::~OssStreamComponent (void)
{
}

//////////////////////////////////////////////////////////////////////////////

int OssStreamComponent::Major(void)
{
  return ( version >> 16 ) & 0xFF ;
}

int OssStreamComponent::Minor(void)
{
  return ( version >>  8 ) & 0xFF ;
}

//////////////////////////////////////////////////////////////////////////////

CaError OssStreamComponent ::      Initialize   (
          const StreamParameters * Parameters   ,
          int                      CallbackMode ,
          int                      FD           ,
          const char             * DeviceName   )
{
  CaError result   = NoError                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  hostChannelCount = 0                                                       ;
  userInterleaved  = 0                                                       ;
  buffer           = NULL                                                    ;
  hostFormat       = cafNothing                                              ;
  hostFrames       = 0                                                       ;
  numBufs          = 0                                                       ;
  userBuffers      = NULL                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  fd               = FD                                                      ;
  devName          = DeviceName                                              ;
  userChannelCount = Parameters->channelCount                                ;
  userFormat       = Parameters->sampleFormat                                ;
  latency          = Parameters->suggestedLatency                            ;
  userInterleaved  = ! ( Parameters->sampleFormat & cafNonInterleaved )      ;
  ////////////////////////////////////////////////////////////////////////////
  if ( !CallbackMode && ! userInterleaved )                                  {
    /* Pre-allocate non-interleaved user provided buffers                   */
    userBuffers = (void **)Allocator :: allocate ( sizeof(void*)             *
                                                   userChannelCount        ) ;
    CA_UNLESS( userBuffers , InsufficientMemory )                            ;
    for (int i=0;i<userChannelCount;i++) userBuffers[i] = NULL               ;
  }                                                                          ;
  #if defined(SNDCTL_DSP_SILENCE)
  if ( Major() >= 4)                                                         {
    ::ioctl ( fd , SNDCTL_DSP_SILENCE , NULL )                               ;
  }                                                                          ;
  #endif
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

void OssStreamComponent::Terminate(void)
{
  if ( fd >= 0 ) ::close ( fd )     ;
  Allocator :: free ( userBuffers ) ;
  buffer      = NULL                ;
  userBuffers = NULL                ;
  fd          = -1                  ;
}

//////////////////////////////////////////////////////////////////////////////

unsigned int OssStreamComponent::FrameSize(void)
{
  CaRETURN ( CaIsEqual ( hostFormat , SampleFormatNotSupported ) , 0 ) ;
  CaRETURN ( CaIsEqual ( hostFormat , cafNothing               ) , 0 ) ;
  return Core::SampleSize( hostFormat ) * hostChannelCount             ;
}

/* Buffer size in bytes. */

unsigned long OssStreamComponent::BufferSize(void)
{
  return FrameSize() * hostFrames * numBufs ;
}

/* Return the PA-compatible formats that this device can support. */

CaError OssStreamComponent::AvailableFormats(CaSampleFormat * availableFormats)
{
  CaError result = NoError                                                   ;
  int     frmts  = cafNothing                                                ;
  int     mask   = 0                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  ENSURE_ ( ioctl( fd,SNDCTL_DSP_GETFMTS,&mask ) , UnanticipatedHostError )  ;
  if ( mask & AFMT_U8     ) frmts |= cafUint8                                ;
  if ( mask & AFMT_S8     ) frmts |= cafInt8                                 ;
  if ( mask & AFMT_S16_NE ) frmts |= cafInt16                                ;
                       else result = SampleFormatNotSupported                ;
  *availableFormats = (CaSampleFormat)frmts                                  ;
error                                                                        :
  return result                                                              ;
}

/* Configure stream component device parameters. */

CaError OssStreamComponent::Configure          (
          double               sampleRate      ,
          unsigned long        framesPerBuffer ,
          StreamMode           streamMode      ,
          OssStreamComponent * master          )
{
  CaError        result           = NoError                                  ;
  int            temp                                                        ;
  int            nativeFormat                                                ;
  int            sr               = (int)sampleRate                          ;
  CaSampleFormat availableFormats = cafNothing                               ;
  CaSampleFormat HostFormat       = cafNothing                               ;
  int            chans            = userChannelCount                         ;
  int            frgmt                                                       ;
  int            bytesPerBuf                                                 ;
  unsigned long  bufSz                                                       ;
  unsigned long  fragSz                                                      ;
  audio_buf_info bufInfo                                                     ;
  if ( !master )                                                             {
    if ( framesPerBuffer == Stream::FramesPerBufferUnspecified )             {
      fragSz = (unsigned long)( latency * sampleRate / 3 )                   ;
      bufSz  = fragSz * 4                                                    ;
    } else                                                                   {
      fragSz = framesPerBuffer                                               ;
      bufSz  = (unsigned long)( latency * sampleRate ) + fragSz              ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    CA_ENSURE ( AvailableFormats( &availableFormats ) )                      ;
    ENSURE_ ( CaNotEqual ( availableFormats , SampleFormatNotSupported )     ,
              SampleFormatNotSupported                                     ) ;
    HostFormat  = ClosestFormat((unsigned long)availableFormats,userFormat)  ;
    ENSURE_ ( CaNotEqual ( HostFormat , SampleFormatNotSupported )           ,
              SampleFormatNotSupported                                     ) ;
    numBufs     = (int)CA_MAX( bufSz / fragSz , 2 )                          ;
    bytesPerBuf = CA_MAX( fragSz * Core::SampleSize(HostFormat)*chans , 16 ) ;
    frgmt       = (numBufs << 16) + (CalcHigherLogTwo(bytesPerBuf) & 0xffff) ;
    ENSURE_ ( ioctl ( fd , SNDCTL_DSP_SETFRAGMENT , &frgmt )                 ,
              UnanticipatedHostError                                       ) ;
    CA_ENSURE( OssFormat ( HostFormat , &temp ) )                            ;
    nativeFormat = temp                                                      ;
    ENSURE_ ( ioctl ( fd , SNDCTL_DSP_SETFMT, &temp )                        ,
              UnanticipatedHostError                                       ) ;
    CA_UNLESS ( temp == nativeFormat, InternalError )                        ;
    ENSURE_ ( ioctl( fd , SNDCTL_DSP_CHANNELS , &chans                     ) ,
              SampleFormatNotSupported                                     ) ;
    CA_UNLESS ( chans >= userChannelCount , InvalidChannelCount            ) ;
    ENSURE_ ( ioctl( fd , SNDCTL_DSP_SPEED , &sr ) , InvalidSampleRate     ) ;
    if ( ( fabs ( sampleRate - sr ) / sampleRate) > 0.01 )                   {
      gPrintf                                                              ( (
        "%s: Wanted %f, closest sample rate was %d\n"                        ,
        __FUNCTION__                                                         ,
        sampleRate                                                           ,
        sr                                                               ) ) ;
      CA_ENSURE( InvalidSampleRate )                                         ;
    }                                                                        ;
    ENSURE_ ( ioctl( fd                                                      ,
                     streamMode == StreamMode_In                             ?
                     SNDCTL_DSP_GETISPACE                                    :
                     SNDCTL_DSP_GETOSPACE                                    ,
                     &bufInfo                                              ) ,
              UnanticipatedHostError                                       ) ;
    numBufs          = bufInfo.fragstotal                                    ;
    ENSURE_ ( ioctl( fd , SNDCTL_DSP_GETBLKSIZE , &bytesPerBuf             ) ,
              UnanticipatedHostError                                       ) ;
    hostFrames       = bytesPerBuf / Core::SampleSize(HostFormat) / chans    ;
    hostChannelCount = chans                                                 ;
    hostFormat       = HostFormat                                            ;
  } else                                                                     {
    hostFormat       = master->hostFormat                                    ;
    hostFrames       = master->hostFrames                                    ;
    hostChannelCount = master->hostChannelCount                              ;
    numBufs          = master->numBufs                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  buffer = Allocator::allocate ( BufferSize ( ) )                            ;
  CA_UNLESS( buffer , InsufficientMemory                                   ) ;
  ::memset ( buffer , 0 , BufferSize() )                                     ;
error                                                                        :
  return result                                                              ;
}

CaError OssStreamComponent::Read(unsigned long * frames)
{
  CaError result    = NoError                    ;
  size_t  len       = *frames * FrameSize ( )    ;
  size_t  bytesRead = read( fd, buffer, len )    ;
  ENSURE_ ( bytesRead , UnanticipatedHostError ) ;
  *frames = bytesRead / FrameSize ( )            ;
error                                            :
  return result                                  ;
}

CaError OssStreamComponent::Write(unsigned long * frames)
{
  CaError result = NoError                            ;
  int     len    = (int)( (*frames) * FrameSize ( ) ) ;
  int     bytesWritten                                ;
  if ( CaAND ( CaIsGreater ( (*frames) , 0 )          ,
               CaIsGreater ( len       , 0 ) ) )      {
    bytesWritten = ::write ( fd , buffer , len )      ;
    ENSURE_ ( bytesWritten , UnanticipatedHostError ) ;
    *frames = bytesWritten / FrameSize ( )            ;
  } else *frames = 0                                  ;
error                                                 :
  return result                                       ;
}

/* Clean up after thread exit. */

static void OnExit( void * data )
{
  if ( NULL == data ) return                         ;
  OssStream * stream = (OssStream *) data            ;
  stream -> cpuLoadMeasurer . Reset ( )              ;
  stream -> StreamStop ( stream -> callbackAbort )   ;
  gPrintf ( ( "OnExit: Stop page\n" ) )              ;
  /* Eventually notify user all buffers have played */
  if ( NULL != stream->conduit )                     {
    stream -> conduit -> finish ( )                  ;
  }                                                  ;
  stream -> callbackAbort = 0                        ;
  stream -> isActive      = 0                        ;
}

/** Thread procedure for callback processing.
 *
 * Aspect StreamState: StartStream will wait on this to initiate audio processing, useful in case the
 * callback should be used for buffer priming. When the stream is cancelled a separate function will
 * take care of the transition to the Callback Finished state (the stream isn't considered Stopped
 * before StopStream() or AbortStream() are called).
 */
static void * AudioThreadProc(void * userData)
{
  CaRETURN ( CaIsNull ( userData ) , NULL )                                  ;
  gPrintf ( ( "Open Sound System started\n" ) )                              ;
  ////////////////////////////////////////////////////////////////////////////
  CaError       result              = NoError                                ;
  OssStream   * stream              = (OssStream *)userData                  ;
  unsigned long framesAvail         = 0                                      ;
  unsigned long framesProcessed     = 0                                      ;
  int           callbackResult      = Conduit::Continue                      ;
  int           triggered           = stream->triggered                      ;
  int           initiateProcessing  = triggered                              ;
  CaStreamFlags cbFlags             = 0                                      ;
  ////////////////////////////////////////////////////////////////////////////
  pthread_cleanup_push ( &OnExit, stream   )                                 ;
  CA_ENSURE            ( stream->Prepare() )                                 ;
  ////////////////////////////////////////////////////////////////////////////
  if ( initiateProcessing )                                                  {
    if ( stream->capture  ) ModifyBlocking ( stream -> capture  -> fd , 1 )  ;
    if ( stream->playback ) ModifyBlocking ( stream -> playback -> fd , 1 )  ;
  }                                                                          ;
  while ( 1 )                                                                {
    #warning pthread_cancel need to have a replacement
//    pthread_testcancel ( )                                                   ;
    if ( stream->callbackStop && callbackResult != Conduit::Complete )       {
      gPrintf ( ( "Setting callbackResult to Complete\n" ) )                 ;
      callbackResult = Conduit::Complete                                     ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( !initiateProcessing )                                               {
      CA_ENSURE ( stream -> WaitForFrames ( &framesAvail ) )                 ;
    } else                                                                   {
      framesAvail = stream->framesPerHostBuffer                              ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    while ( framesAvail > 0 )                                                {
      unsigned long frames = framesAvail                                     ;
      #warning pthread_cancel need to have a replacement
//      pthread_testcancel ( )                                                 ;
      stream -> cpuLoadMeasurer . Begin ( )                                  ;
      if ( CaAND ( CaNotNull ( stream -> capture  )                          ,
                   CaNotNull ( stream -> conduit  ) )                      ) {
        stream -> conduit -> Input  . StatusFlags = cbFlags                  ;
      }                                                                      ;
      if ( CaAND ( CaNotNull ( stream -> playback )                          ,
                   CaNotNull ( stream -> conduit  ) )                      ) {
        stream -> conduit -> Output . StatusFlags = cbFlags                  ;
      }                                                                      ;
      if ( NULL != stream->capture )                                         {
        CA_ENSURE ( stream -> capture -> Read ( &frames ) )                  ;
        if ( frames < framesAvail )                                          {
          gPrintf                                                          ( (
            "Read %lu less frames than requested\n"                          ,
            framesAvail - frames                                         ) ) ;
          framesAvail = frames                                               ;
        }                                                                    ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      stream -> bufferProcessor . Begin ( stream -> conduit )                ;
      cbFlags = 0                                                            ;
      CA_ENSURE( stream -> SetUpBuffers ( framesAvail ) )                    ;
      framesProcessed = stream -> bufferProcessor . End ( &callbackResult  ) ;
      stream -> cpuLoadMeasurer . End ( framesProcessed )                    ;
      if ( NULL != stream->playback )                                        {
        frames = framesAvail                                                 ;
        CA_ENSURE ( stream -> playback -> Write ( &frames ) )                ;
        if ( frames < framesAvail )                                          {
          gPrintf                                                          ( (
            "Wrote %d less frames than requested\n"                          ,
            (int)(framesAvail - frames)                                  ) ) ;
        }                                                                    ;
      }                                                                      ;
      framesAvail             -= framesProcessed                             ;
      stream->framesProcessed += framesProcessed                             ;
      if ( ( callbackResult != Conduit::Postpone )                          &&
           ( callbackResult != Conduit::Continue )                   ) break ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( initiateProcessing || !triggered )                                  {
      if ( NULL != stream->capture )                                         {
        CA_ENSURE ( ModifyBlocking ( stream -> capture  -> fd , 0 ) )        ;
      }                                                                      ;
      if ( stream->playback && ! stream->sharedDevice )                      {
        CA_ENSURE ( ModifyBlocking ( stream -> playback -> fd , 0 ) )        ;
      }                                                                      ;
      initiateProcessing = 0                                                 ;
      sem_post ( stream->semaphore )                                         ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( ( callbackResult != Conduit::Continue )                            &&
         ( callbackResult != Conduit::Postpone )                           ) {
      stream->callbackAbort = callbackResult == Conduit::Abort               ;
      if (stream->callbackAbort || stream->bufferProcessor.isOutputEmpty())  {
        break                                                                ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( stream -> capture  ) && CaNotNull ( stream->conduit ) )   {
    stream->conduit->finish ( Conduit::InputDirection  , Conduit::Correct )  ;
  }                                                                          ;
  if ( CaNotNull ( stream -> playback ) && CaNotNull ( stream->conduit ) )   {
    stream->conduit->finish ( Conduit::OutputDirection , Conduit::Correct )  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  pthread_cleanup_pop( 1 )                                                   ;
error                                                                        :
  gPrintf ( ( "Open Sound System stopped\n" ) )                              ;
  pthread_exit( NULL )                                                       ;
  return NULL                                                                ;
}

//////////////////////////////////////////////////////////////////////////////

/* Initialize the OSS API implementation. */
CaError OssInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError      result     = NoError                                          ;
  OssHostApi * ossHostApi = new OssHostApi()                                 ;
  ////////////////////////////////////////////////////////////////////////////
  #if defined(SOUND_VERSION)
  #if ( MIN_OSS_VERSION > SOUND_VERSION )
  #warning Your Open sound system header is possible to old, please check and update
  #endif
  if  ( MIN_OSS_VERSION > SOUND_VERSION )                                      {
    gPrintf ( ( "Your OSS version %x is possible too old, it can run weird and not properly.\n" , SOUND_VERSION ) ) ;
  }                                                                          ;
  #endif
  ////////////////////////////////////////////////////////////////////////////
  CA_UNLESS( ossHostApi              , InsufficientMemory )                  ;
  ossHostApi->allocations = new AllocationGroup()                            ;
  CA_UNLESS( ossHostApi->allocations , InsufficientMemory )                  ;
  ossHostApi -> hostApiIndex = hostApiIndex                                  ;
  /* Initialize host API structure                                          */
  *hostApi = (HostApi *)ossHostApi                                           ;
  (*hostApi) -> info . structVersion = 1                                     ;
  (*hostApi) -> info . type          = OSS                                   ;
  (*hostApi) -> info . index         = hostApiIndex                          ;
  (*hostApi) -> info . name          = "Open Sound System"                   ;
  ////////////////////////////////////////////////////////////////////////////
  CA_ENSURE( ossHostApi->BuildDeviceList ( ) )                               ;
  mainThread_ = pthread_self ( )                                             ;
  return result                                                              ;
error                                                                        :
  if ( CaNotNull ( ossHostApi ) )                                            {
    ossHostApi -> Terminate ( )                                              ;
    delete ossHostApi                                                        ;
    ossHostApi = NULL                                                        ;
  }                                                                          ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

OssStream:: OssStream (void)
          : Stream    (    )
{
  threading           = 0                                           ;
  sharedDevice        = 0                                           ;
  framesPerHostBuffer = 0                                           ;
  triggered           = 0                                           ;
  isActive            = 0                                           ;
  isStopped           = 1                                           ;
  lastPosPtr          = 0                                           ;
  lastStreamBytes     = 0                                           ;
  framesProcessed     = 0                                           ;
  sampleRate          = 0                                           ;
  callbackMode        = 0                                           ;
  callbackStop        = 0                                           ;
  callbackAbort       = 0                                           ;
  capture             = NULL                                        ;
  playback            = NULL                                        ;
  pollTimeout         = 0                                           ;
  semaphore           = (sem_t *)Allocator::allocate(sizeof(sem_t)) ;
  ::memset ( semaphore , 0 , sizeof(sem_t) )                        ;
}

OssStream::~OssStream(void)
{
}

CaError OssStream::Start(void)
{
  CaError result  = NoError                                     ;
  isActive        = 1                                           ;
  isStopped       = 0                                           ;
  lastPosPtr      = 0                                           ;
  lastStreamBytes = 0                                           ;
  framesProcessed = 0                                           ;
  /* only use the thread for callback streams                  */
  if ( NULL != conduit )                                        {
    ::pthread_create( &threading, NULL, AudioThreadProc, this ) ;
    sem_wait  ( semaphore )                                     ;
  } else                                                        {
    CA_ENSURE ( Prepare ( ) )                                   ;
  }                                                             ;
error                                                           :
  return result                                                 ;
}

CaError OssStream::Stop(void)
{
  return RealStop ( 0 ) ;
}

CaError OssStream::Close(void)
{
  CaError result = NoError        ;
  bufferProcessor . Terminate ( ) ;
  this           -> Terminate ( ) ;
  return result                   ;
}

CaError OssStream::Abort(void)
{
  return RealStop ( 1 ) ;
}

CaError OssStream::IsStopped(void)
{
  return isStopped ;
}

CaError OssStream::IsActive(void)
{
  return isActive ;
}

CaTime OssStream::GetTime(void)
{
  count_info info                                                            ;
  int        delta                                                           ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( playback ) )                                              {
    if ( 0 == ioctl ( playback->fd , SNDCTL_DSP_GETOPTR , &info ) )          {
      delta = ( info.bytes - lastPosPtr ) /* & 0x000FFFFF*/                  ;
      return (float)(lastStreamBytes + delta) / playback->FrameSize() / sampleRate ;
    }                                                                        ;
  } else                                                                     {
    if ( 0 == ioctl ( capture ->fd , SNDCTL_DSP_GETIPTR , &info ) )          {
      delta = ( info . bytes - lastPosPtr) /*& 0x000FFFFF*/                  ;
      return (float)(lastStreamBytes + delta) / capture ->FrameSize() / sampleRate ;
    }                                                                        ;
  }                                                                          ;
  /* the ioctl failed, but we can still give a coarse estimate              */
  return framesProcessed / sampleRate                                        ;
}

double OssStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer.Value() ;
}

CaInt32 OssStream::ReadAvailable(void)
{
  CaError result = NoError                                   ;
  audio_buf_info info                                        ;
  ENSURE_( ioctl( capture->fd, SNDCTL_DSP_GETISPACE, &info ) ,
           UnanticipatedHostError                          ) ;
  return info . fragments * capture->hostFrames              ;
error                                                        :
  return result                                              ;
}

CaInt32 OssStream::WriteAvailable(void)
{
  CaError result = NoError                                                   ;
  int     delay  = 0                                                         ;
  #ifdef SNDCTL_DSP_GETODELAY
  ENSURE_( ioctl( playback->fd, SNDCTL_DSP_GETODELAY, &delay               ) ,
           UnanticipatedHostError                                          ) ;
  #endif
  return ( playback->BufferSize() - delay) / playback->FrameSize()           ;
  /* Conditionally compile this to avoid warning about unused label         */
  #ifdef SNDCTL_DSP_GETODELAY
error:
  return result                                                              ;
  #endif
}

CaError OssStream::Read(void * buffer,unsigned long frames)
{
  CaError       result = NoError                                             ;
  int           bytesRequested                                               ;
  int           bytesRead                                                    ;
  unsigned long framesRequested                                              ;
  void        * userBuffer                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( bufferProcessor . userInputIsInterleaved )                            {
    userBuffer = buffer                                                      ;
  } else                                                                     {
    userBuffer = capture -> userBuffers                                      ;
    ::memcpy( (void *)userBuffer                                             ,
              buffer                                                         ,
              sizeof (void *) * capture -> userChannelCount                ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  while ( frames )                                                           {
    framesRequested = CA_MIN( frames , capture -> hostFrames )               ;
    bytesRequested = framesRequested * capture -> FrameSize ( )              ;
    ENSURE_( ( bytesRead = ::read ( capture -> fd                            ,
                                    capture -> buffer                        ,
                                    bytesRequested                       ) ) ,
               UnanticipatedHostError                                      ) ;
    if ( bytesRequested != bytesRead )                                       {
      dPrintf                                                              ( (
          "Requested %d bytes, read %d\n"                                    ,
          bytesRequested                                                     ,
          bytesRead                                                      ) ) ;
      return UnanticipatedHostError                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    bufferProcessor . setInputFrameCount ( 0 , capture -> hostFrames )       ;
    bufferProcessor . setInterleavedInputChannels                            (
      0                                                                      ,
      0                                                                      ,
      capture -> buffer                                                      ,
      capture -> hostChannelCount                                          ) ;
    bufferProcessor . CopyInput ( &userBuffer , framesRequested )            ;
    frames -= framesRequested                                                ;
    //////////////////////////////////////////////////////////////////////////
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError OssStream::Write(const void * buffer,unsigned long frames)
{
  CaError       result = NoError                                             ;
  int           bytesRequested                                               ;
  int           bytesWritten                                                 ;
  unsigned long framesConverted                                              ;
  const void  * userBuffer                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  /* If user output is non-interleaved, CopyOutput will manipulate the channel pointers,
   * so we copy the user provided pointers                                  */
  if ( bufferProcessor . userOutputIsInterleaved )                           {
    userBuffer = buffer                                                      ;
  } else                                                                     {
    /* Copy channels into local array                                       */
    userBuffer = playback -> userBuffers                                     ;
    ::memcpy( (void *)userBuffer                                             ,
              buffer                                                         ,
              sizeof (void *) * playback -> userChannelCount               ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  while ( frames )                                                           {
    bufferProcessor . setOutputFrameCount ( 0 , playback -> hostFrames     ) ;
    bufferProcessor . setInterleavedOutputChannels                           (
      0                                                                      ,
      0                                                                      ,
      playback -> buffer                                                     ,
      playback -> hostChannelCount                                         ) ;
    framesConverted = bufferProcessor . CopyOutput ( &userBuffer , frames  ) ;
    frames -= framesConverted                                                ;
    bytesRequested = framesConverted * playback -> FrameSize ( )             ;
    ENSURE_( ( bytesWritten = ::write( playback -> fd                        ,
                                       playback -> buffer                    ,
                                       bytesRequested                    ) ) ,
              UnanticipatedHostError                                       ) ;
    if ( bytesRequested != bytesWritten )                                    {
      dPrintf                                                              ( (
          "Requested %d bytes, wrote %d\n"                                   ,
          bytesRequested                                                     ,
          bytesWritten                                                   ) ) ;
      return UnanticipatedHostError                                          ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  return result                                                              ;
}

bool OssStream::hasVolume(void)
{
  #if defined(SNDCTL_DSP_GETRECVOL) && defined(SNDCTL_DSP_GETPLAYVOL)
  return true  ;
  #else
  #warning Open sound system header might be too old
  #warning Audio volume control is disabled
  #warning Please update your OSS system header in order to enable volume control
  return false ;
  #endif
}

CaVolume OssStream::MinVolume(void)
{
  return 0.0 ;
}

CaVolume OssStream::MaxVolume(void)
{
  return 10000.0 ;
}

CaVolume OssStream::Volume(int atChannel)
{
  #if defined(SNDCTL_DSP_GETRECVOL) && defined(SNDCTL_DSP_GETPLAYVOL)
  CaVolume V     = 0                                                         ;
  int      level = 0                                                         ;
  int      L     = 0                                                         ;
  int      R     = 0                                                         ;
  int      rt                                                                ;
  if ( CaNotNull ( capture  ) && capture->Major() >= 4 )                     {
    rt = ::ioctl ( capture  -> fd , SNDCTL_DSP_GETRECVOL  , &level )         ;
    L =   level        & 0xFF                                                ;
    R = ( level >> 8 ) & 0xFF                                                ;
    if ( capture -> userChannelCount > 1 )                                   {
      if ( capture -> hostChannelCount > 1 )                                 {
        if ( 0 == atChannel )                                                {
          V = L                                                              ;
        } else
        if ( 0 == atChannel )                                                {
          V = R                                                              ;
        } else                                                               {
          V = ( L + R ) / 2                                                  ;
        }                                                                    ;
      } else                                                                 {
        V = L                                                                ;
      }                                                                      ;
    } else                                                                   {
      V = L                                                                  ;
    }                                                                        ;
    V *= 100.0                                                               ;
    return V                                                                 ;
  }                                                                          ;
  if ( CaNotNull ( playback ) && playback->Major() >= 4 )                    {
    rt = ::ioctl ( playback -> fd , SNDCTL_DSP_GETPLAYVOL , &level )         ;
    L =   level        & 0xFF                                                ;
    R = ( level >> 8 ) & 0xFF                                                ;
    if ( playback -> userChannelCount > 1 )                                  {
      if ( playback -> hostChannelCount > 1 )                                {
        if ( 0 == atChannel )                                                {
          V = L                                                              ;
        } else
        if ( 0 == atChannel )                                                {
          V = R                                                              ;
        } else                                                               {
          V = ( L + R ) / 2                                                  ;
        }                                                                    ;
      } else                                                                 {
        V = L                                                                ;
      }                                                                      ;
    } else                                                                   {
      V = L                                                                  ;
    }                                                                        ;
    V *= 100.0                                                               ;
    return V                                                                 ;
  }                                                                          ;
  #endif
  return 10000.0                                                             ;
}

CaVolume OssStream::setVolume(CaVolume volume,int atChannel)
{
  #if defined(SNDCTL_DSP_GETRECVOL) && defined(SNDCTL_DSP_GETPLAYVOL)
  CaVolume V                                                                 ;
  int      level                                                             ;
  int      L                                                                 ;
  int      R                                                                 ;
  int      vol                                                               ;
  int      rt                                                                ;
  if ( CaNotNull ( capture  ) && capture->Major()>=4 )                       {
    rt  = ::ioctl ( capture  -> fd , SNDCTL_DSP_GETRECVOL  , &level )        ;
    L   =   level        & 0xFF                                              ;
    R   = ( level >> 8 ) & 0xFF                                              ;
    if ( capture -> userChannelCount > 1 )                                   {
      if ( capture -> hostChannelCount > 1 )                                 {
        if ( 0 == atChannel )                                                {
          L = (int)( volume / 100.0 )                                        ;
          V = volume                                                         ;
        } else
        if ( 1 == atChannel )                                                {
          R = (int)( volume / 100.0 )                                        ;
          V = volume                                                         ;
        } else                                                               {
          L = (int)( volume / 100.0 )                                        ;
          R = L                                                              ;
          V = volume                                                         ;
        }                                                                    ;
      } else                                                                 {
        L = (int)( volume / 100.0 )                                          ;
        R = L                                                                ;
        V = volume                                                           ;
      }                                                                      ;
    } else                                                                   {
      L = (int)( volume / 100.0 )                                            ;
      R = L                                                                  ;
      V = volume                                                             ;
    }                                                                        ;
    vol = ( R << 8 ) | L                                                     ;
    rt = ::ioctl ( capture  -> fd , SNDCTL_DSP_SETRECVOL  , &vol   )         ;
    return V                                                                 ;
  }                                                                          ;
  if ( CaNotNull ( playback ) && playback->Major() >= 4 )                    {
    rt  = ::ioctl ( playback -> fd , SNDCTL_DSP_GETPLAYVOL , &level )        ;
    L   =   level        & 0xFF                                              ;
    R   = ( level >> 8 ) & 0xFF                                              ;
    if ( playback -> userChannelCount > 1 )                                  {
      if ( playback -> hostChannelCount > 1 )                                {
        if ( 0 == atChannel )                                                {
          L = (int)( volume / 100.0 )                                        ;
          V = volume                                                         ;
        } else
        if ( 1 == atChannel )                                                {
          R = (int)( volume / 100.0 )                                        ;
          V = volume                                                         ;
        } else                                                               {
          L = (int)( volume / 100.0 )                                        ;
          R = L                                                              ;
          V = volume                                                         ;
        }                                                                    ;
      } else                                                                 {
        L = (int)( volume / 100.0 )                                          ;
        R = L                                                                ;
        V = volume                                                           ;
      }                                                                      ;
    } else                                                                   {
      L = (int)( volume / 100.0 )                                            ;
      R = L                                                                  ;
      V = volume                                                             ;
    }                                                                        ;
    vol = ( R << 8 ) | L                                                     ;
    rt = ::ioctl ( playback -> fd , SNDCTL_DSP_SETPLAYVOL , &vol   )         ;
    return V                                                                 ;
  }                                                                          ;
  #endif
  return 10000.0                                                             ;
}

CaError OssStream::RealStop(int abort)
{
  CaError result = NoError                                     ;
  if ( callbackMode )                                          {
    if ( abort ) callbackAbort = 1                             ;
            else callbackStop  = 1                             ;
    CA_ENSURE( CaCancelThreading( &threading, !abort, NULL ) ) ;
    callbackStop  = 0                                          ;
    callbackAbort = 0                                          ;
  } else                                                       {
    CA_ENSURE( StreamStop( abort ) )                           ;
  }                                                            ;
  isStopped = 1                                                ;
error                                                          :
  return result                                                ;
}

CaError OssStream::StreamStop(int abort)
{
  CaError result      = NoError                                              ;
  int     captureErr  = 0                                                    ;
  int     playbackErr = 0                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != capture )                                                     {
    captureErr = ioctl ( capture->fd, SNDCTL_DSP_POST, 0 )                   ;
    if ( ( captureErr < 0 ) )                                                {
      dPrintf                                                              ( (
        "%s: Failed to stop capture device, error: %d\n"                     ,
        __FUNCTION__                                                         ,
        captureErr                                                       ) ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( NULL != playback ) && ( ! sharedDevice ) )                          {
    playbackErr = ioctl ( playback->fd , SNDCTL_DSP_POST , 0 )               ;
    if ( ( playbackErr < 0 ) )                                               {
      dPrintf                                                              ( (
        "%s: Failed to stop playback device, error: %d\n"                    ,
        __FUNCTION__                                                         ,
        playbackErr                                                      ) ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( 0 != captureErr ) || ( 0 != playbackErr ) )                         {
    result = UnanticipatedHostError                                          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

CaError OssStream::SetUpBuffers(unsigned long framesAvail)
{
  CaError result = NoError                                                   ;
  if ( NULL != capture )                                                     {
    bufferProcessor . setInterleavedInputChannels                            (
                        0                                                    ,
                        0                                                    ,
                        capture -> buffer                                    ,
                        capture -> hostChannelCount                        ) ;
    bufferProcessor . setInputFrameCount ( 0 , framesAvail )                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != playback )                                                    {
    bufferProcessor . setInterleavedOutputChannels                           (
                        0                                                    ,
                        0                                                    ,
                        playback -> buffer                                   ,
                        playback -> hostChannelCount                       ) ;
    bufferProcessor . setOutputFrameCount ( 0 , framesAvail )                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

CaError OssStream::Prepare(void)
{
  CaError result     = NoError                                               ;
  int     enableBits = 0                                                     ;
  bool    correct    = false                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  if ( triggered ) return result                                             ;
  /* The OSS reference instructs us to clear direction bits before setting them. */
  if ( NULL != playback )                                                    {
    ::memset( playback->buffer , 0 , playback->BufferSize() )                ;
    ::write ( playback -> fd                                                 ,
              playback -> buffer                                             ,
              playback -> BufferSize()                                     ) ;
    for (int i = 0 ; ! correct && i < 10 ; i++ )                             {
      enableBits = 0                                                         ;
      if ( ioctl ( playback -> fd,SNDCTL_DSP_SETTRIGGER,&enableBits ) >= 0 ) {
        correct = true                                                       ;
      }                                                                      ;
      if ( ! correct ) Timer :: Sleep ( 10 )                                 ;
    }                                                                        ;
    if ( ! correct ) return UnanticipatedHostError                           ;
  }                                                                          ;
  if ( NULL != capture  )                                                    {
    ::memset( capture->buffer , 0 , capture->BufferSize() )                  ;
    ::write ( capture -> fd                                                  ,
              capture -> buffer                                              ,
              capture -> BufferSize()                                      ) ;
    for (int i = 0 ; ! correct && i < 10 ; i++ )                             {
      enableBits = 0                                                         ;
      if ( ioctl ( capture -> fd,SNDCTL_DSP_SETTRIGGER,&enableBits ) >= 0 )  {
        correct = true                                                       ;
      }                                                                      ;
      if ( ! correct ) Timer :: Sleep ( 10 )                                 ;
    }                                                                        ;
    if ( ! correct ) return UnanticipatedHostError                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != playback )                                                    {
    size_t bufSz = playback -> BufferSize ( )                                ;
    ::memset  ( playback->buffer , 0                , bufSz )                ;
    ::write   ( playback->fd   , playback->buffer   , bufSz )                ;
    CA_ENSURE ( ModifyBlocking ( playback -> fd     , 0     ) )              ;
    #if defined(SNDCTL_DSP_SILENCE)
    if ( playback->Major() >= 4 )                                            {
      ::ioctl ( playback->fd   , SNDCTL_DSP_SILENCE , NULL  )                ;
    }                                                                        ;
    #endif
    ::write   ( playback->fd   , playback->buffer   , bufSz )                ;
    CA_ENSURE ( ModifyBlocking ( playback -> fd     , 1     ) )              ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  correct = false                                                            ;
  if ( 0 != sharedDevice )                                                   {
    for (int i = 0 ; ! correct && i < 10 ; i++ )                             {
      enableBits = PCM_ENABLE_INPUT | PCM_ENABLE_OUTPUT                      ;
      if ( ioctl ( capture -> fd,SNDCTL_DSP_SETTRIGGER,&enableBits ) >= 0 )  {
        correct = true                                                       ;
      }                                                                      ;
      if ( ! correct ) Timer :: Sleep ( 10 )                                 ;
    }                                                                        ;
  } else                                                                     {
    if ( NULL != capture  )                                                  {
      correct = false                                                        ;
      ::write ( capture -> fd                                                ,
                capture -> buffer                                            ,
                capture -> BufferSize()                                    ) ;
      for (int i = 0 ; ! correct && i < 10 ; i++ )                           {
        enableBits = PCM_ENABLE_INPUT                                        ;
        if ( ioctl ( capture ->fd,SNDCTL_DSP_SETTRIGGER,&enableBits ) >= 0 ) {
          correct = true                                                     ;
        }                                                                    ;
        if ( ! correct ) Timer :: Sleep ( 10 )                               ;
      }                                                                      ;
      if ( correct )                                                         {
        dPrintf ( ( "Open Sound System Version %d.%d for audio capture\n"    ,
                    capture->Major()                                         ,
                    capture->Minor()                                     ) ) ;
      }                                                                      ;
    }                                                                        ;
    if ( NULL != playback )                                                  {
      correct = false                                                        ;
      ::write ( playback -> fd                                               ,
                playback -> buffer                                           ,
                playback -> BufferSize()                                   ) ;
      for (int i = 0 ; ! correct && i < 10 ; i++ )                           {
        enableBits = PCM_ENABLE_OUTPUT                                       ;
        if ( ioctl ( playback->fd,SNDCTL_DSP_SETTRIGGER,&enableBits ) >= 0 ) {
          correct = true                                                     ;
        }                                                                    ;
        if ( ! correct ) Timer :: Sleep ( 10 )                               ;
      }                                                                      ;
      if ( correct )                                                         {
        dPrintf ( ( "Open Sound System Version %d.%d for audio playback\n"   ,
                    playback->Major()                                        ,
                    playback->Minor()                                    ) ) ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  if ( ! correct ) return UnanticipatedHostError                             ;
  /* Ok, we have triggered the stream                                       */
  triggered = 1                                                              ;
error                                                                        :
  return result                                                              ;
}

/* Poll on I/O filedescriptors. */

CaError OssStream::WaitForFrames(unsigned long * frames)
{
  CaError        result        = NoError                                     ;
  int            pollPlayback  = 0                                           ;
  int            pollCapture   = 0                                           ;
  int            captureAvail  = INT_MAX                                     ;
  int            playbackAvail = INT_MAX                                     ;
  int            commonAvail                                                 ;
  audio_buf_info bufInfo                                                     ;
  fd_set         readFds                                                     ;
  fd_set         writeFds                                                    ;
  int            nfds          = 0                                           ;
  struct timeval selectTimeval = { 0 , 0 }                                   ;
  unsigned long  timeout       = pollTimeout                                 ;
  int            captureFd     = -1                                          ;
  int            playbackFd    = -1                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( capture  ) )                                              {
    pollCapture  = 1                                                         ;
    captureFd    = capture  -> fd                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( playback ) )                                              {
    pollPlayback = 1                                                         ;
    playbackFd   = playback -> fd                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  FD_ZERO ( &readFds  )                                                      ;
  FD_ZERO ( &writeFds )                                                      ;
  ////////////////////////////////////////////////////////////////////////////
  while ( pollPlayback || pollCapture )                                      {
    #warning pthread_cancel need to have a replacement
//    pthread_testcancel ( )                                                   ;
    /* select may modify the timeout parameter                              */
    selectTimeval . tv_usec = timeout                                        ;
    nfds                    = 0                                              ;
    //////////////////////////////////////////////////////////////////////////
    if ( 0 != pollCapture )                                                  {
      FD_SET ( captureFd , &readFds )                                        ;
      nfds = captureFd + 1                                                   ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( 0 != pollPlayback )                                                 {
      FD_SET ( playbackFd , &writeFds )                                      ;
      nfds = CA_MAX ( nfds , playbackFd + 1 )                                ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    ENSURE_( select ( nfds , &readFds , &writeFds , NULL , &selectTimeval  ) ,
             UnanticipatedHostError                                        ) ;
    #warning pthread_cancel need to have a replacement
//    pthread_testcancel ( )                                                   ;
    //////////////////////////////////////////////////////////////////////////
    if ( 0 != pollCapture )                                                  {
      if ( FD_ISSET( captureFd, &readFds ) )                                 {
        FD_CLR ( captureFd, &readFds )                                       ;
        pollCapture = 0                                                      ;
      } else
      if ( NULL != playback )                                                {
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( 0 != pollPlayback )                                                 {
      if ( FD_ISSET( playbackFd, &writeFds ) )                               {
        FD_CLR ( playbackFd, &writeFds )                                     ;
        pollPlayback = 0                                                     ;
      } else
      if ( NULL != capture )                                                 {
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != capture )                                                     {
    ENSURE_ ( ioctl ( captureFd, SNDCTL_DSP_GETISPACE, &bufInfo )            ,
              UnanticipatedHostError                                       ) ;
    captureAvail = bufInfo . fragments * capture -> hostFrames               ;
//    if ( !captureAvail )                                                     {
//      dPrintf ( ( "%s: captureAvail: 0\n" , __FUNCTION__  ) )                ;
//    }                                                                        ;
    captureAvail = captureAvail == 0 ? INT_MAX : captureAvail                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != playback )                                                    {
    ENSURE_ ( ioctl( playbackFd, SNDCTL_DSP_GETOSPACE, &bufInfo            ) ,
              UnanticipatedHostError                                       ) ;
    playbackAvail = bufInfo . fragments * playback -> hostFrames             ;
//    if ( !playbackAvail )                                                    {
//      dPrintf ( ( "%s: playbackAvail: 0\n" , __FUNCTION__ ) )                ;
//    }                                                                        ;
    playbackAvail = ( playbackAvail == 0 ) ? INT_MAX : playbackAvail         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  commonAvail  = CA_MIN ( captureAvail , playbackAvail )                     ;
  if ( commonAvail == INT_MAX ) commonAvail = 0                              ;
  commonAvail -= commonAvail % framesPerHostBuffer                           ;
  *frames      = commonAvail                                                 ;
error                                                                        :
  return result                                                              ;
}

CaError OssStream ::    Configure       (
          double        SampleRate      ,
          unsigned long FramesPerBuffer ,
          double      * InputLatency    ,
          double      * OutputLatency   )
{
  CaError       result = NoError                                             ;
  int           duplex = ( NULL != capture ) && ( NULL != playback )         ;
  unsigned long FramesPerHostBuffer = 0                                      ;
  /* We should request full duplex first thing after opening the device     */
  if ( duplex && sharedDevice )                                              {
    ENSURE_ ( ioctl ( capture->fd                                            ,
                      SNDCTL_DSP_SETDUPLEX                                   ,
                      0                                                    ) ,
              UnanticipatedHostError                                       ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != capture )                                                     {
    OssStreamComponent * component = capture                                 ;
    CA_ENSURE ( component -> Configure                                       (
                  SampleRate                                                 ,
                  FramesPerBuffer                                            ,
                  StreamMode_In                                              ,
                  NULL                                                   ) ) ;
    *InputLatency = ( component -> hostFrames * ( component->numBufs - 1 ) ) /
                      SampleRate                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != playback )                                                    {
    OssStreamComponent * component = playback                                ;
    OssStreamComponent * master    = sharedDevice ? capture : NULL           ;
    CA_ENSURE ( component -> Configure                                       (
                  SampleRate                                                 ,
                  FramesPerBuffer                                            ,
                  StreamMode_Out                                             ,
                  master                                                 ) ) ;
    *OutputLatency = ( component -> hostFrames * ( component->numBufs - 1 )) /
                       SampleRate                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0    != duplex   )                                                    {
    FramesPerHostBuffer = CA_MIN( capture  -> hostFrames                     ,
                                  playback -> hostFrames                   ) ;
  } else
  if ( NULL != capture  )                                                    {
    FramesPerHostBuffer = capture  -> hostFrames                             ;
  } else
  if ( NULL != playback )                                                    {
    FramesPerHostBuffer = playback -> hostFrames                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  framesPerHostBuffer = FramesPerHostBuffer                                  ;
  pollTimeout = (int) ceil( 1e6 * framesPerHostBuffer / SampleRate )         ;
  sampleRate  = SampleRate                                                   ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  return result                                                              ;
}

void OssStream::Terminate(void)
{
  if ( CaNotNull ( capture  ) )     {
    capture  -> Terminate ( )       ;
    delete capture                  ;
    capture  = NULL                 ;
  }                                 ;
  if ( CaNotNull ( playback ) )     {
    playback -> Terminate ( )       ;
    delete playback                 ;
    playback = NULL                 ;
  }                                 ;
  if ( CaNotNull ( semaphore ) )    {
    ::sem_destroy     ( semaphore ) ;
    Allocator :: free ( semaphore ) ;
    semaphore = NULL                ;
  }                                 ;
}

///////////////////////////////////////////////////////////////////////////////

OssDeviceInfo:: OssDeviceInfo (void)
              : DeviceInfo    (    )
{
  version = 0 ;
}

OssDeviceInfo::~OssDeviceInfo (void)
{
}

CaError OssDeviceInfo::ValidateParameters     (
          const StreamParameters * parameters ,
          StreamMode               mode       )
{
  int maxChans                                                               ;
  if ( CaUseHostApiSpecificDeviceSpecification == parameters->device )       {
    return InvalidDevice                                                     ;
  }                                                                          ;
  maxChans = ( StreamMode_In==mode ? maxInputChannels : maxOutputChannels  ) ;
  if ( parameters->channelCount > maxChans ) return InvalidChannelCount      ;
  return NoError                                                             ;
}

CaError OssDeviceInfo ::    Initialize                (
          const char      * Name                      ,
          CaHostApiIndex    HostApiIndex              ,
          int               MaxInputChannels          ,
          int               MaxOutputChannels         ,
          CaTime            DefaultLowInputLatency    ,
          CaTime            DefaultLowOutputLatency   ,
          CaTime            DefaultHighInputLatency   ,
          CaTime            DefaultHighOutputLatency  ,
          double            DefaultSampleRate         ,
          AllocationGroup * allocations               )
{
  CaError result = NoError                            ;
  structVersion = 2                                   ;
  if ( NULL != allocations )                          {
    size_t len = strlen( Name ) + 1                   ;
    name = (const char *)allocations -> alloc ( len ) ;
    CA_UNLESS( name , InsufficientMemory )            ;
    strncpy( (char *)name, Name, len )                ;
  } else name = Name                                  ;
  /////////////////////////////////////////////////////
  hostType                 = OSS                      ;
  hostApi                  = HostApiIndex             ;
  maxInputChannels         = MaxInputChannels         ;
  maxOutputChannels        = MaxOutputChannels        ;
  defaultLowInputLatency   = DefaultLowInputLatency   ;
  defaultLowOutputLatency  = DefaultLowOutputLatency  ;
  defaultHighInputLatency  = DefaultHighInputLatency  ;
  defaultHighOutputLatency = DefaultHighOutputLatency ;
  defaultSampleRate        = DefaultSampleRate        ;
  /////////////////////////////////////////////////////
error                                                 :
  return result                                       ;
}

//////////////////////////////////////////////////////////////////////////////

OssHostApi:: OssHostApi (void)
           : HostApi    (    )
{
  allocations  = NULL ;
  hostApiIndex = 0    ;
}

OssHostApi::~OssHostApi(void)
{
  CaEraseAllocation ;
}

HostApi::Encoding OssHostApi::encoding(void) const
{
  return UTF8 ;
}

bool OssHostApi::hasDuplex(void) const
{
  return true ;
}

CaError OssHostApi ::               Open              (
          Stream                 ** s                 ,
          const StreamParameters *  inputParameters   ,
          const StreamParameters *  outputParameters  ,
          double                    sampleRate        ,
          CaUint32                  framesPerCallback ,
          CaStreamFlags             streamFlags       ,
          Conduit                *  conduit           )
{
  CaError            result             = NoError                            ;
  OssStream        * stream             = NULL                               ;
  int                inputChannelCount  = 0                                  ;
  int                outputChannelCount = 0                                  ;
  CaSampleFormat     inputSampleFormat  = cafNothing                         ;
  CaSampleFormat     outputSampleFormat = cafNothing                         ;
  CaSampleFormat     inputHostFormat    = cafNothing                         ;
  CaSampleFormat     outputHostFormat   = cafNothing                         ;
  const DeviceInfo * inputDeviceInfo    = 0                                  ;
  const DeviceInfo * outputDeviceInfo   = 0                                  ;
  int                bpInitialized      = 0                                  ;
  double             inLatency          = 0.0                                ;
  double             outLatency         = 0.0                                ;
  int                i                  = 0                                  ;
  ////////////////////////////////////////////////////////////////////////////
  /* validate platform specific flags                                       */
  if ( ( streamFlags & csfPlatformSpecificFlags ) != 0 ) return InvalidFlag  ;
  /* unexpected platform specific flag                                      */
  if ( NULL != inputParameters )                                             {
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    inputDeviceInfo = deviceInfos  [ inputParameters  -> device ]            ;
    CA_ENSURE ( ( (OssDeviceInfo *) inputDeviceInfo ) -> ValidateParameters  (
                  inputParameters                                            ,
                  StreamMode_In                                          ) ) ;
    inputChannelCount = inputParameters->channelCount                        ;
    inputSampleFormat = inputParameters->sampleFormat                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputDeviceInfo = deviceInfos [ outputParameters -> device ]            ;
    CA_ENSURE ( ( (OssDeviceInfo *) outputDeviceInfo ) -> ValidateParameters (
                  outputParameters                                           ,
                  StreamMode_Out                                         ) ) ;
    outputChannelCount = outputParameters->channelCount                      ;
    outputSampleFormat = outputParameters->sampleFormat                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( inputChannelCount > 0 && outputChannelCount > 0 )                     {
    if ( inputParameters->device == outputParameters->device )               {
      if ( inputParameters->channelCount != outputParameters->channelCount ) {
        return InvalidChannelCount                                           ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Round framesPerBuffer to the next power-of-two to make OSS happy.      */
  if ( framesPerCallback != Stream::FramesPerBufferUnspecified )             {
    framesPerCallback &= INT_MAX                                             ;
    for (i = 1; framesPerCallback > i; i <<= 1)                              ;
    framesPerCallback = i                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* allocate and do basic initialization of the stream structure           */
  stream = new OssStream ( )                                                 ;
  CA_UNLESS( stream , InsufficientMemory                                   ) ;
  stream -> conduit  = conduit                                               ;
  stream -> debugger = debugger                                              ;
  CA_ENSURE( Initialize ( stream                                             ,
                          inputParameters                                    ,
                          outputParameters                                   ,
                          conduit                                            ,
                          streamFlags                                    ) ) ;
  CA_ENSURE( stream -> Configure ( sampleRate                                ,
                                   framesPerCallback                         ,
                                   &inLatency                                ,
                                   &outLatency                           ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> cpuLoadMeasurer . Initialize ( sampleRate )                      ;
  if ( NULL != inputParameters  )                                            {
    inputHostFormat = stream -> capture   -> hostFormat                      ;
    stream -> inputLatency  = inLatency                                      +
                              stream->bufferProcessor.InputLatencyFrames()   /
                              sampleRate                                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputHostFormat = stream -> playback -> hostFormat                      ;
    stream -> outputLatency = outLatency                                     +
                              stream->bufferProcessor.OutputLatencyFrames()  /
                              sampleRate                                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  CA_ENSURE( stream -> bufferProcessor . Initialize                          (
               inputChannelCount                                             ,
               inputSampleFormat                                             ,
               inputHostFormat                                               ,
               outputChannelCount                                            ,
               outputSampleFormat                                            ,
               outputHostFormat                                              ,
               sampleRate                                                    ,
               streamFlags                                                   ,
               framesPerCallback                                             ,
               stream -> framesPerHostBuffer                                 ,
               cabFixedHostBufferSize                                        ,
               conduit                                                   ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( inputParameters  ) )                                      {
    conduit -> Input  . setSample ( inputSampleFormat                        ,
                                    inputChannelCount                      ) ;
  }                                                                          ;
  if ( CaNotNull ( outputParameters ) )                                      {
    conduit -> Output . setSample ( outputSampleFormat                       ,
                                    outputChannelCount                     ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  bpInitialized = 1                                                          ;
  *s            = (Stream *)stream                                           ;
  return result                                                              ;
error                                                                        :
  if ( bpInitialized  ) stream -> bufferProcessor . Terminate ( )            ;
  if ( NULL != stream ) stream -> Terminate                   ( )            ;
  return result                                                              ;
}

void OssHostApi::Terminate(void)
{
  CaEraseAllocation ;
}

CaError OssHostApi::isSupported                      (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double                    sampleRate       )
{
  CaError        result = NoError                                            ;
  CaDeviceIndex  device                                                      ;
  CaSampleFormat inputSampleFormat                                           ;
  CaSampleFormat outputSampleFormat                                          ;
  DeviceInfo   * deviceInfo                                                  ;
  char         * deviceName                                                  ;
  int            inputChannelCount                                           ;
  int            outputChannelCount                                          ;
  int            tempDevHandle = -1                                          ;
  int            flags                                                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    inputChannelCount = inputParameters -> channelCount                      ;
    inputSampleFormat = inputParameters -> sampleFormat                      ;
    if ( inputParameters->device==CaUseHostApiSpecificDeviceSpecification )  {
      return InvalidDevice                                                   ;
    }                                                                        ;
    if ( inputChannelCount > deviceInfos[ inputParameters->device ]->maxInputChannels ) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    if ( NULL != inputParameters->streamInfo )                {
      return IncompatibleStreamInfo                           ;
    }                                                                        ;
  } else                                                                     {
    inputChannelCount = 0                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputChannelCount = outputParameters->channelCount                      ;
    outputSampleFormat = outputParameters->sampleFormat                      ;
    if ( outputParameters->device==CaUseHostApiSpecificDeviceSpecification ) {
      return InvalidDevice                                                   ;
    }                                                                        ;
    if ( outputChannelCount > deviceInfos[ outputParameters->device ]->maxOutputChannels ) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    if ( NULL != outputParameters->streamInfo )               {
      return IncompatibleStreamInfo                           ;
    }                                                                        ;
  } else                                                                     {
    outputChannelCount = 0                                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( inputChannelCount == 0 ) && ( outputChannelCount == 0 ) )           {
    return InvalidChannelCount                                               ;
  }                                                                          ;
  if ( ( inputChannelCount  >  0                                          ) &&
       ( outputChannelCount >  0                                          ) &&
       ( inputParameters->device != outputParameters->device            ) )  {
    return InvalidDevice                                                     ;
  }                                                                          ;
  if ( ( inputChannelCount   > 0                                          ) &&
       ( outputChannelCount  > 0                                          ) &&
       ( inputChannelCount != outputChannelCount                        ) )  {
    return InvalidChannelCount                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( inputChannelCount > 0 )                                               {
    result = ToHostDeviceIndex ( &device , inputParameters  -> device )      ;
    if ( NoError != result ) return result                                   ;
  } else                                                                     {
    result = ToHostDeviceIndex ( &device , outputParameters -> device )      ;
    if ( NoError != result ) return result                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  deviceInfo = deviceInfos [ device ]                                        ;
  deviceName = (char *) deviceInfo -> name                                   ;
  flags      = O_NONBLOCK                                                    ;
  if ( inputChannelCount > 0 && outputChannelCount > 0) flags |= O_RDWR ; else
  if ( inputChannelCount > 0 ) flags |= O_RDONLY                        ; else
                               flags |= O_WRONLY                             ;
  tempDevHandle = ::open( deviceInfo->name, flags )                          ;
  ENSURE_ ( tempDevHandle , DeviceUnavailable )                              ;
error                                                                        :
  if ( tempDevHandle >= 0 ) ::close ( tempDevHandle )                        ;
  return result                                                              ;
}

CaError OssHostApi ::              Initialize       (
          OssStream              * stream           ,
          const StreamParameters * inputParameters  ,
          const StreamParameters * outputParameters ,
          Conduit                * conduit          ,
          CaStreamFlags            streamFlags      )
{
  CaError      result   = NoError                                            ;
  const char * idevName = NULL                                               ;
  const char * odevName = NULL                                               ;
  int          idev                                                          ;
  int          odev                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> isStopped = 1                                                    ;
  stream -> threading = 0                                                    ;
  if ( ( NULL != inputParameters ) && ( NULL != outputParameters ) )         {
    if ( inputParameters->device == outputParameters->device )               {
      stream -> sharedDevice = 1                                             ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters  )                                            {
    idevName = deviceInfos [ inputParameters  -> device ] -> name            ;
  }                                                                          ;
  if ( NULL != outputParameters )                                            {
    odevName = deviceInfos [ outputParameters -> device ] -> name            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  CA_ENSURE ( OpenDevices ( idevName , odevName , &idev , &odev ) )          ;
  if ( NULL != inputParameters )                                             {
    stream -> capture = new OssStreamComponent ( )                           ;
    stream -> capture  -> version = ((OssDeviceInfo *)deviceInfos[inputParameters ->device])->version ;
    CA_UNLESS ( stream -> capture , InsufficientMemory )                     ;
    CA_ENSURE ( stream -> capture -> Initialize                              (
                  inputParameters                                            ,
                  conduit != NULL                                            ,
                  idev                                                       ,
                  idevName                                               ) ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    stream -> playback = new OssStreamComponent ( )                          ;
    stream -> playback -> version = ((OssDeviceInfo *)deviceInfos[outputParameters->device])->version ;
    CA_UNLESS ( stream -> playback , InsufficientMemory )                    ;
    CA_ENSURE ( stream -> playback -> Initialize                             (
                  outputParameters                                           ,
                  conduit != NULL                                            ,
                  odev                                                       ,
                  odevName                                               ) ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> conduit = conduit                                                ;
  if ( conduit != NULL )                                                     {
    stream -> callbackMode = 1                                               ;
  } else                                                                     {
    stream -> callbackMode = 0                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ENSURE_ ( sem_init ( stream -> semaphore , 0 , 0 ) , InternalError )       ;
error                                                                        :
  return result                                                              ;
}

/* Query OSS device. */

CaError OssHostApi::QueryDevice(char * deviceName,OssDeviceInfo ** devInfo)
{
  CaError result        = NoError                                            ;
  CaError tmpRes        = NoError                                            ;
  double  sampleRate    = -1.0                                               ;
  int     busy          = 0                                                  ;
  int     inputVersion  = 0                                                  ;
  int     outputVersion = 0                                                  ;
  int     maxInputChannels                                                   ;
  int     maxOutputChannels                                                  ;
  CaTime  defaultLowInputLatency                                             ;
  CaTime  defaultLowOutputLatency                                            ;
  CaTime  defaultHighInputLatency                                            ;
  CaTime  defaultHighOutputLatency                                           ;
  ////////////////////////////////////////////////////////////////////////////
  *devInfo = NULL                                                            ;
  tmpRes = QueryDirection                                                    (
             deviceName                                                      ,
             StreamMode_In                                                   ,
             &sampleRate                                                     ,
             &maxInputChannels                                               ,
             &defaultLowInputLatency                                         ,
             &defaultHighInputLatency                                        ,
             &inputVersion                                                 ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NoError != tmpRes)                                                    {
    if ( tmpRes != DeviceUnavailable )                                       {
      gPrintf                                                              ( (
          "%s: Querying device %s for capture failed!\n"                     ,
          __FUNCTION__                                                       ,
          deviceName                                                     ) ) ;
    }                                                                        ;
    ++busy                                                                   ;
    maxInputChannels = 0                                                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  tmpRes = QueryDirection                                                    (
             deviceName                                                      ,
             StreamMode_Out                                                  ,
             &sampleRate                                                     ,
             &maxOutputChannels                                              ,
             &defaultLowOutputLatency                                        ,
             &defaultHighOutputLatency                                       ,
             &outputVersion                                                ) ;
  if ( NoError != tmpRes )                                                   {
    if ( tmpRes != DeviceUnavailable )                                       {
      gPrintf                                                              ( (
        "%s: Querying device %s for playback failed!\n"                      ,
        __FUNCTION__                                                         ,
        deviceName                                                       ) ) ;
    }                                                                        ;
    ++busy                                                                   ;
    maxOutputChannels = 0                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 2 == busy )                                                           {
    result = DeviceUnavailable                                               ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  CA_UNLESS( ( (*devInfo) = new OssDeviceInfo() ) , InsufficientMemory )     ;
  CA_ENSURE( (*devInfo) -> Initialize                                        (
               deviceName                                                    ,
               hostApiIndex                                                  ,
               maxInputChannels                                              ,
               maxOutputChannels                                             ,
               defaultLowInputLatency                                        ,
               defaultLowOutputLatency                                       ,
               defaultHighInputLatency                                       ,
               defaultHighOutputLatency                                      ,
               sampleRate                                                    ,
               allocations                                               ) ) ;
  if ( inputVersion  > 0 ) (*devInfo)->version = inputVersion                ;
  if ( outputVersion > 0 ) (*devInfo)->version = outputVersion               ;
  gPrintf (( "Device %s has version %x\n",deviceName,(*devInfo)->version  )) ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  return result                                                              ;
}

/* Query host devices */

CaError OssHostApi::BuildDeviceList(void)
{
  CaError          result         = NoError                                  ;
  int              i                                                         ;
  int              numDevices     = 0                                        ;
  int              maxDeviceInfos = 1                                        ;
  OssDeviceInfo ** devInfos       = NULL                                     ;
  /* These two will be set to the first working input and output device, respectively */
  info . defaultInputDevice  = CaNoDevice                                    ;
  info . defaultOutputDevice = CaNoDevice                                    ;
  for ( i = 0; i < 100; i++ )                                                {
    char            deviceName [ 32 ]                                        ;
    OssDeviceInfo * deviceInfo                                               ;
    int             testResult                                               ;
    //////////////////////////////////////////////////////////////////////////
    if ( 0 == i )                                                            {
      ::snprintf(deviceName,sizeof(deviceName),"%s", DEVICE_NAME_BASE      ) ;
    } else                                                                   {
      ::snprintf(deviceName,sizeof(deviceName),"%s%d", DEVICE_NAME_BASE,i  ) ;
    }                                                                        ;
    testResult = QueryDevice ( deviceName , &deviceInfo )                    ;
    if ( NoError != testResult )                                             {
      if ( DeviceUnavailable != testResult )                                 {
        CA_ENSURE( testResult )                                              ;
      }                                                                      ;
      continue                                                               ;
    }                                                                        ;
    ++numDevices                                                             ;
    if ( ( NULL == devInfos ) || numDevices > maxDeviceInfos )               {
       maxDeviceInfos *= 2                                                   ;
       devInfos = (OssDeviceInfo **) realloc( devInfos                       ,
                                              maxDeviceInfos                 *
                                              sizeof (OssDeviceInfo *)     ) ;
       CA_UNLESS ( devInfos , InsufficientMemory )                           ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    int devIdx = numDevices - 1                                              ;
    devInfos [ devIdx ] = deviceInfo                                         ;
    if ( info        . defaultInputDevice  == CaNoDevice                    &&
         deviceInfo -> maxInputChannels > 0                                ) {
      info . defaultInputDevice  =  devIdx                                   ;
    }                                                                        ;
    if ( info        . defaultOutputDevice == CaNoDevice                    &&
         deviceInfo -> maxOutputChannels > 0                               ) {
      info . defaultOutputDevice =  devIdx                                   ;
    }                                                                        ;
  }                                                                          ;
  /* Make an array of PaDeviceInfo pointers out of the linked list          */
  gPrintf                                                                  ( (
    "OSS %s: Total number of devices found: %d\n"                            ,
    __FUNCTION__                                                             ,
    numDevices                                                           ) ) ;
  deviceInfos = new DeviceInfo * [ numDevices ]                              ;
  for (int z=0;z<numDevices;z++) deviceInfos[z] = (DeviceInfo *)devInfos[z]  ;
  info . deviceCount = numDevices                                            ;
error                                                                        :
  Allocator :: free ( devInfos )                                             ;
  devInfos = NULL                                                            ;
  if ( CaIsLess ( numDevices , 1 ) )                                         {
    gPrintf ( ( "Open Sound System does not have any device in your system.\n" ) ) ;
  }                                                                          ;
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
