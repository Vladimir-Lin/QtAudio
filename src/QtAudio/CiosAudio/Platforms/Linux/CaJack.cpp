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

#include "CaJack.hpp"

///////////////////////////////////////////////////////////////////////////////

#define STRINGIZE_HELPER(expr) #expr
#define STRINGIZE(expr) STRINGIZE_HELPER(expr)

/* Check PaError */
#define ENSURE_PA(expr) \
    do { \
        CaError caErr; \
        if( (caErr = (expr)) < NoError ) \
        { \
            if( (caErr) == UnanticipatedHostError && pthread_self() == mainThread_ ) \
            { \
                const char *err = jackErr_; \
                if (! err ) err = "unknown error"; \
                SetLastHostErrorInfo( JACK, -1, err ); \
            } \
            if ( NULL != globalDebugger ) { \
              globalDebugger -> printf ( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ) ; \
            } ; \
            result = caErr; \
            goto error; \
        } \
    } while( 0 )

#define UNLESS(expr, code) \
    do { \
        if( (expr) == 0 ) \
        { \
            if( (code) == UnanticipatedHostError && pthread_self() == mainThread_ ) \
            { \
                const char *err = jackErr_; \
                if (!err) err = "unknown error"; \
                SetLastHostErrorInfo( JACK, -1, err ); \
            } \
            if ( NULL != globalDebugger ) { \
              globalDebugger -> printf ( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ) ; \
            } ; \
            result = (code); \
            goto error; \
        } \
    } while( 0 )

#define ASSERT_CALL(expr, success) \
    do { \
        int err = (expr); \
        assert( err == success ); \
    } while( 0 )

/* In calls to jack_get_ports() this filter expression is used instead of ""
 * to prevent any other types (eg Midi ports etc) being listed */
#define JACK_PORT_TYPE_FILTER "audio"

#define TRUE 1
#define FALSE 0

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

static pthread_t    mainThread_                ;
static char       * jackErr_    = NULL         ;
static const char * clientName_ = "CIOS Audio" ;

CaError JackSetClientName(const char * name)
{
  if ( strlen( name ) > jack_client_name_size() ) {
    /* OK, I don't know any better error code    */
    return InvalidFlag                            ;
  }                                               ;
  clientName_ = name                              ;
  return NoError                                  ;
}

CaError JackGetClientName(const char ** clientName)
{
  CaError result = NoError                                        ;
  JackHostApi *  jackHostApi = NULL                               ;
  HostApi     ** ref         = (HostApi **)&jackHostApi           ;
  ENSURE_PA ( Core :: GetHostApi ( ref , JACK ) )                 ;
  *clientName = jack_get_client_name ( jackHostApi->jack_client ) ;
error                                                             :
  return result                                                   ;
}

/* Allocate buffer. */
static CaError BlockingInitFIFO             (
                 RingBuffer * rbuf          ,
                 long         numFrames     ,
                 long         bytesPerFrame )
{
  long   numBytes = numFrames * bytesPerFrame                   ;
  char * buffer   = (char *) malloc ( numBytes )                ;
  if ( NULL == buffer ) return InsufficientMemory               ;
  ::memset ( buffer, 0, numBytes )                              ;
  return (CaError) rbuf -> Initialize ( 1 , numBytes , buffer ) ;
}

static CaError BlockingTermFIFO(RingBuffer * rbuf)
{
  if ( rbuf->buffer ) free ( rbuf->buffer ) ;
  rbuf -> buffer = NULL                     ;
  return NoError                            ;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

static int BlockingCallback                (
             const void  * inputBuffer     ,
             void        * outputBuffer    ,
             unsigned long framesPerBuffer ,
             Conduit     * conduit         )
{
    struct PaJackStream *stream = (PaJackStream *)userData;
    long numBytes = stream->bytesPerFrame * framesPerBuffer;

    /* This may get called with NULL inputBuffer during initial setup. */
    if( inputBuffer != NULL )
    {
        PaUtil_WriteRingBuffer( &stream->inFIFO, inputBuffer, numBytes );
    }
    if( outputBuffer != NULL )
    {
        int numRead = PaUtil_ReadRingBuffer( &stream->outFIFO, outputBuffer, numBytes );
        /* Zero out remainder of buffer if we run out of data. */
        memset( (char *)outputBuffer + numRead, 0, numBytes - numRead );
    }

    if( !stream->data_available )
    {
        stream->data_available = 1;
        sem_post( &stream->data_semaphore );
    }
  return Conduit::Continue                                                   ;
}

#endif

//static void JackErrorCallback(const char * msg)
//{
//  if ( pthread_self() == mainThread_ )                         {
//    jackErr_ = (char *)realloc( jackErr_ , strlen( msg ) + 1 ) ;
//    strcpy ( jackErr_ , msg )                                  ;
//  }                                                            ;
//}

static void JackErrorHandler(const char * msg)
{
  gPrintf ( ( "CIOS JACK : %s\n" , msg ) ) ;
}

static void JackOnShutdown(void * arg)
{
  if ( NULL == arg ) return                                                  ;
  JackHostApi * jackApi = (JackHostApi *)arg                                 ;
  JackStream  * stream  = jackApi->processQueue                              ;
  gPrintf                                                                  ( (
    "%s: JACK server is shutting down\n"                                     ,
    __FUNCTION__                                                         ) ) ;
  for ( ; stream ; stream = stream->next ) stream -> is_active = 0           ;
  /* Make sure that the main thread doesn't get stuck waiting on the condition */
  ASSERT_CALL ( pthread_mutex_lock   ( &jackApi -> mtx  ) , 0 )              ;
  jackApi -> jackIsDown = 1                                                  ;
  ASSERT_CALL ( pthread_cond_signal  ( &jackApi -> cond ) , 0 )              ;
  ASSERT_CALL ( pthread_mutex_unlock ( &jackApi -> mtx  ) , 0 )              ;
}

static int JackSrCb(jack_nframes_t nframes,void * arg)
{
  if ( NULL == arg ) return 0                                                ;
  JackHostApi * jackApi    = (JackHostApi *)arg                              ;
  JackStream  * stream     = jackApi->processQueue                           ;
  double        sampleRate = (double)nframes                                 ;
  /* Update all streams in process queue                                    */
  gPrintf                                                                  ( (
    "%s: Acting on change in JACK samplerate: %f\n"                          ,
    __FUNCTION__                                                             ,
    sampleRate                                                           ) ) ;
  for ( ; stream ; stream = stream->next )                                   {
    if ( stream -> sampleRate != sampleRate )                                {
      gPrintf                                                              ( (
        "%s: Updating samplerate\n"                                          ,
        __FUNCTION__                                                     ) ) ;
      stream->UpdateSampleRate ( sampleRate )                                ;
    }                                                                        ;
  }                                                                          ;
  return 0                                                                   ;
}

static int JackXRunCb(void * arg)
{
  if ( NULL == arg ) return 0                                 ;
  JackHostApi * hostApi = (JackHostApi *)arg                  ;
  hostApi->xrun = TRUE                                        ;
//  gPrintf ( ( "%s: JACK signalled xrun\n" , __FUNCTION__  ) ) ;
  return 0                                                    ;
}

/* Audio processing callback invoked periodically from JACK. */
static int JackCallback(jack_nframes_t frames,void * userData)
{
  CaError       result  = NoError                                            ;
  JackHostApi * hostApi = (JackHostApi *)userData                            ;
  JackStream  * stream  = NULL                                               ;
  int           xrun    = hostApi->xrun                                      ;
  ////////////////////////////////////////////////////////////////////////////
  hostApi -> xrun = 0                                                        ;
  ENSURE_PA ( hostApi -> UpdateQueue ( ) )                                   ;
  /* Process each stream                                                    */
  stream = hostApi -> processQueue                                           ;
  for ( ; CaNotNull ( stream ) ; stream = stream->next )                     {
    if ( xrun ) stream -> xrun = 1                                           ;
    /* See if this stream is to be started                                  */
    if ( stream->doStart )                                                   {
      /* If we can't obtain a lock, we'll try next time                     */
     int err = pthread_mutex_trylock ( &stream->hostApi->mtx )               ;
     if ( !err )                                                             {
       if ( stream->doStart )                                                {
         stream -> is_active = 1                                             ;
         stream -> doStart   = 0                                             ;
         gPrintf ( ( "JACK Audio Connection Kit started\n" ) )               ;
         ASSERT_CALL ( pthread_cond_signal( &stream->hostApi->cond ) , 0 )   ;
         stream -> callbackResult = Conduit::Continue                        ;
         stream -> isSilenced     = 0                                        ;
       }                                                                     ;
       ASSERT_CALL( pthread_mutex_unlock ( &stream->hostApi->mtx ) , 0 )     ;
     } else assert( err == EBUSY )                                           ;
   } else
   if ( stream->doStop || stream->doAbort )                                  {
     if ( stream->callbackResult == Conduit::Continue )                      {
       gPrintf ( ( "%s: Stopping stream\n" , __FUNCTION__ ) )                ;
       stream->callbackResult = stream->doStop ? Conduit::Complete           :
                                                 Conduit::Abort              ;
     }                                                                       ;
   }                                                                         ;
   ///////////////////////////////////////////////////////////////////////////
   if ( stream -> is_active )                                                {
     ENSURE_PA ( stream -> RealProcess ( frames ) )                          ;
   }                                                                         ;
   /* If we have just entered inactive state, silence output                */
   if ( ! stream -> is_active && ! stream -> isSilenced )                    {
     int i                                                                   ;
     /* Silence buffer after entering inactive state                        */
     gPrintf ( ( "Silencing the output\n" ) )                                ;
     for ( i = 0 ; i < stream->num_outgoing_connections ; ++i )              {
       jack_default_audio_sample_t * buffer                                  ;
       buffer = (jack_default_audio_sample_t *)jack_port_get_buffer          (
                  stream->local_output_ports[i]                              ,
                  frames                                                   ) ;
       memset( buffer, 0, sizeof (jack_default_audio_sample_t) * frames )    ;
     }                                                                       ;
     stream -> isSilenced = 1                                                ;
   }                                                                         ;
   ///////////////////////////////////////////////////////////////////////////
   if ( stream->doStop || stream->doAbort )                                  {
     if ( ! stream->is_active )                                              {
       int err = pthread_mutex_trylock( &stream->hostApi->mtx )              ;
       if ( !err )                                                           {
         stream->doStop = stream->doAbort = 0                                ;
         ASSERT_CALL( pthread_cond_signal  ( &stream->hostApi->cond ) , 0 )  ;
         ASSERT_CALL( pthread_mutex_unlock ( &stream->hostApi->mtx  ) , 0 )  ;
       } else assert( err == EBUSY )                                         ;
      }                                                                      ;
    }                                                                        ;
   ///////////////////////////////////////////////////////////////////////////
  }                                                                          ;
  return  0                                                                  ;
error                                                                        :
  return -1                                                                  ;
}

//////////////////////////////////////////////////////////////////////////////

CaError JackInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError       result      = NoError                                        ;
  JackHostApi * jackHostApi = NULL                                           ;
  int           activated   = 0                                              ;
  jack_status_t jackStatus  = JackFailure                                    ;
  (*hostApi) = NULL                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  jackHostApi = new JackHostApi ( )                                          ;
  UNLESS ( jackHostApi , InsufficientMemory )                                ;
  jackHostApi->deviceInfoMemory = new AllocationGroup ( )                    ;
  UNLESS ( jackHostApi->deviceInfoMemory , InsufficientMemory )              ;
  mainThread_ = pthread_self()                                               ;
  ASSERT_CALL ( pthread_mutex_init( & jackHostApi -> mtx  , NULL ) , 0 )     ;
  ASSERT_CALL ( pthread_cond_init ( & jackHostApi -> cond , NULL ) , 0 )     ;
  ////////////////////////////////////////////////////////////////////////////
  jackHostApi -> jack_client = jack_client_open ( clientName_                ,
                                                  JackNoStartServer          ,
                                                  &jackStatus              ) ;
  if ( CaIsNull ( jackHostApi -> jack_client ) )                             {
    gPrintf                                                                ( (
      "%s: Couldn't connect to JACK, status: %d\n"                           ,
      __FUNCTION__                                                           ,
      jackStatus                                                         ) ) ;
    result = NoError                                                         ;
    goto error                                                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  jackHostApi -> hostApiIndex         = hostApiIndex                         ;
  *hostApi                            = (HostApi *)jackHostApi               ;
  (*hostApi)  -> info . structVersion = 1                                    ;
  (*hostApi)  -> info . type          = JACK                                 ;
  (*hostApi)  -> info . index         = hostApiIndex                         ;
  (*hostApi)  -> info . name          = "JACK Audio Connection Kit"          ;
  /* Build a device list by querying the JACK server                        */
  ENSURE_PA( jackHostApi -> BuildDeviceList ( ) )                            ;
  ////////////////////////////////////////////////////////////////////////////
  jackHostApi -> inputBase    = jackHostApi->outputBase = 0                  ;
  jackHostApi -> xrun         = 0                                            ;
  jackHostApi -> toAdd        = jackHostApi->toRemove   = NULL               ;
  jackHostApi -> processQueue = NULL                                         ;
  jackHostApi -> jackIsDown   = 0                                            ;
  ////////////////////////////////////////////////////////////////////////////
  jack_on_shutdown( jackHostApi->jack_client, JackOnShutdown, jackHostApi )  ;
  jack_set_error_function ( JackErrorHandler )                               ;
  jack_set_info_function  ( JackErrorHandler )                               ;
  jackHostApi -> jack_buffer_size = jack_get_buffer_size ( jackHostApi -> jack_client ) ;
  jack_set_sample_rate_callback ( jackHostApi -> jack_client , JackSrCb , jackHostApi ) ;
  UNLESS( ! jack_set_xrun_callback    ( jackHostApi->jack_client , JackXRunCb   , jackHostApi ) , UnanticipatedHostError ) ;
  UNLESS( ! jack_set_process_callback ( jackHostApi->jack_client , JackCallback , jackHostApi ) , UnanticipatedHostError ) ;
  UNLESS( ! jack_activate             ( jackHostApi->jack_client                              ) , UnanticipatedHostError ) ;
  activated = 1                                                              ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
error                                                                        :
  ////////////////////////////////////////////////////////////////////////////
  if ( activated )                                                           {
    ASSERT_CALL ( jack_deactivate ( jackHostApi->jack_client ) , 0 )         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != jackHostApi )                                                 {
    if ( jackHostApi->jack_client )                                          {
      ASSERT_CALL ( jack_client_close ( jackHostApi->jack_client ) , 0 )     ;
    }                                                                        ;
    if ( NULL != jackHostApi->deviceInfoMemory )                             {
      jackHostApi -> deviceInfoMemory -> release ( )                         ;
      delete jackHostApi -> deviceInfoMemory                                 ;
      jackHostApi -> deviceInfoMemory = NULL                                 ;
    }                                                                        ;
    delete jackHostApi                                                       ;
  }                                                                          ;
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

JackStream:: JackStream (void)
           : Stream     (    )
{
  hostApi                  = NULL                ;
  local_input_ports        = NULL                ;
  local_output_ports       = NULL                ;
  remote_input_ports       = NULL                ;
  remote_output_ports      = NULL                ;
  jack_client              = NULL                ;
  stream_memory            = NULL                ;
  next                     = NULL                ;
  ////////////////////////////////////////////////
  num_incoming_connections = 0                   ;
  num_outgoing_connections = 0                   ;
  is_running               = 0                   ;
  is_active                = 0                   ;
  doStart                  = 0                   ;
  doStop                   = 0                   ;
  doAbort                  = 0                   ;
  t0                       = 0                   ;
  callbackResult           = Conduit::Continue   ;
  isSilenced               = 0                   ;
  xrun                     = 0                   ;
  isBlockingStream         = 0                   ;
  data_available           = 0                   ;
  bytesPerFrame            = 0                   ;
  samplesPerFrame          = 0                   ;
  ////////////////////////////////////////////////
  memset ( &data_semaphore , 0 , sizeof(sem_t) ) ;
}

JackStream::~JackStream(void)
{
}

bool JackStream::isRealTime(void)
{
  return false ;
}

CaError JackStream::Start(void)
{
  CaError result = NoError                                                   ;
  int     i                                                                  ;
  ////////////////////////////////////////////////////////////////////////////
  bufferProcessor . Reset ( )                                                ;
  if ( num_incoming_connections > 0 )                                        {
    for ( i = 0 ; i < num_incoming_connections ; i++ )                       {
      int r = jack_connect ( jack_client                                     ,
                             jack_port_name ( remote_output_ports[i] )       ,
                             jack_port_name ( local_input_ports[i]   )     ) ;
      UNLESS ( 0 == r || EEXIST == r , UnanticipatedHostError )              ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( num_outgoing_connections > 0 )                                        {
    for ( i = 0 ; i < num_outgoing_connections ; i++ )                       {
      int r = jack_connect ( jack_client                                     ,
                             jack_port_name ( local_output_ports[i] )        ,
                             jack_port_name ( remote_input_ports[i] )      ) ;
      UNLESS ( 0 == r || EEXIST == r , UnanticipatedHostError )              ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  xrun    = FALSE                                                            ;
  ASSERT_CALL ( pthread_mutex_lock ( &hostApi->mtx ) , 0 )                   ;
  doStart = 1                                                                ;
  result  = hostApi -> WaitCondition ( )                                     ;
  if ( CaIsWrong ( result ) )                                                {
    dPrintf ( ( "WaitCondition is wrong\n" ) )                               ;
    doStart   = 0                                                            ;
    is_active = 0                                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ASSERT_CALL ( pthread_mutex_unlock ( &hostApi->mtx ) , 0 )                 ;
  ENSURE_PA   ( result                                     )                 ;
  is_running = TRUE                                                          ;
  dPrintf ( ( "%s: JACK stream started\n", __FUNCTION__ ) )                  ;
error                                                                        :
  return result                                                              ;
}

CaError JackStream::RealStop(int abort)
{
  CaError result = NoError                                                   ;
  int     i                                                                  ;
  ////////////////////////////////////////////////////////////////////////////
  if ( isBlockingStream ) BlockingWaitEmpty ( )                              ;
  ASSERT_CALL ( pthread_mutex_lock( &hostApi->mtx ) , 0 )                    ;
  if ( abort ) doAbort = 1 ; else doStop = 1                                 ;
  /* Wait for stream to be stopped                                          */
  result = hostApi -> WaitCondition ( )                                      ;
  ASSERT_CALL ( pthread_mutex_unlock ( &hostApi->mtx ) , 0 )                 ;
  ENSURE_PA   ( result                                     )                 ;
  UNLESS      ( ! is_active , InternalError                )                 ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "%s: Stream stopped\n" , __FUNCTION__ ) )                      ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  is_running = FALSE                                                         ;
  /* Disconnect ports belonging to this stream                              */
  if ( ! hostApi->jackIsDown )                                               {
    //////////////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < num_incoming_connections ; i++ )                       {
      if ( jack_port_connected ( local_input_ports[i] ) )                    {
        UNLESS( ! jack_port_disconnect ( jack_client, local_input_ports[i] ) ,
                  UnanticipatedHostError                                   ) ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < num_outgoing_connections ; i++ )                       {
      if ( jack_port_connected ( local_output_ports[i] ) )                   {
         UNLESS( ! jack_port_disconnect ( jack_client,local_output_ports[i]) ,
                   UnanticipatedHostError                                  ) ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
  }                                                                          ;
  return result                                                              ;
}

CaError JackStream::Stop(void)
{
  return RealStop ( 0 ) ;
}

CaError JackStream::Close(void)
{
  CaError result = NoError           ;
  ENSURE_PA     ( RemoveStream ( ) ) ;
error                                :
  CleanUpStream ( 1 , 1            ) ;
  return result                      ;
}

CaError JackStream::Abort(void)
{
  return RealStop ( 1 ) ;
}

CaError JackStream::IsStopped(void)
{
  return ! is_running ;
}

CaError JackStream::IsActive(void)
{
  return is_active ;
}

CaTime JackStream::GetTime(void)
{
  return ( jack_frame_time( jack_client ) - t0 )     /
         (CaTime)jack_get_sample_rate( jack_client ) ;
}

double JackStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer . Value ( ) ;
}

CaInt32 JackStream::ReadAvailable(void)
{
  return 0 ;
}

CaInt32 JackStream::WriteAvailable(void)
{
  return 0 ;
}

CaError JackStream::Read(void * buffer,unsigned long frames)
{
  return 0 ;
}

CaError JackStream::Write(const void * buffer,unsigned long frames)
{
  return 0 ;
}

bool JackStream::hasVolume(void)
{
  return false ;
}

CaVolume JackStream::MinVolume(void)
{
  return 0.0 ;
}

CaVolume JackStream::MaxVolume(void)
{
  return 10000.0 ;
}

CaVolume JackStream::Volume(int atChannel)
{
  return 10000.0 ;
}

CaVolume JackStream::setVolume(CaVolume volume,int atChannel)
{
  return 10000.0 ;
}

CaError JackStream::BlockingBegin(int minimum_buffer_size)
{
  long    doRead  = 0                                                        ;
  long    doWrite = 0                                                        ;
  CaError result  = NoError                                                  ;
  long    numFrames                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  doRead          = local_input_ports  != NULL                               ;
  doWrite         = local_output_ports != NULL                               ;
  samplesPerFrame = 2                                                        ;
  bytesPerFrame   = sizeof(float) * samplesPerFrame                          ;
  numFrames       = 32                                                       ;
  while ( numFrames < minimum_buffer_size ) numFrames *= 2                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( doRead )                                                              {
    ENSURE_PA ( BlockingInitFIFO ( &inFIFO , numFrames , bytesPerFrame )   ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( doWrite )                                                             {
    long numBytes                                                            ;
    ENSURE_PA( BlockingInitFIFO ( &outFIFO , numFrames , bytesPerFrame )   ) ;
    /* Make Write FIFO appear full initially.                               */
    numBytes = outFIFO . WriteAvailable (          )                         ;
    outFIFO  . AdvanceWriteIndex        ( numBytes )                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  data_available = 0                                                         ;
  sem_init( & data_semaphore , 0 , 0 )                                       ;
error                                                                        :
  return result                                                              ;
}

void JackStream::BlockingEnd(void)
{
  BlockingTermFIFO ( &inFIFO         ) ;
  BlockingTermFIFO ( &outFIFO        ) ;
  sem_destroy      ( &data_semaphore ) ;
}

CaError JackStream::BlockingReadStream(void * data,unsigned long numFrames)
{
  CaError result   = NoError                             ;
  char  * p        = (char *) data                       ;
  long    numBytes = bytesPerFrame * numFrames           ;
  long    bytesRead                                      ;
  ////////////////////////////////////////////////////////
  while ( numBytes > 0 )                                 {
    bytesRead = inFIFO . Read ( p , numBytes )           ;
    numBytes -= bytesRead                                ;
    p        += bytesRead                                ;
    if ( numBytes > 0 )                                  {
      if ( data_available ) data_available = 0           ;
                       else sem_wait ( &data_semaphore ) ;
    }                                                    ;
  }                                                      ;
  ////////////////////////////////////////////////////////
  return result                                          ;
}

CaError JackStream::BlockingWriteStream(const void * data,unsigned long numFrames)
{
  CaError result   = NoError                             ;
  char  * p        = (char *) data                       ;
  long    numBytes = bytesPerFrame * numFrames           ;
  long    bytesWritten                                   ;
  ////////////////////////////////////////////////////////
  while ( numBytes > 0 )                                 {
    bytesWritten = outFIFO . Write ( p , numBytes )      ;
    numBytes    -= bytesWritten                          ;
    p           += bytesWritten                          ;
    if ( numBytes > 0 )                                  {
      if ( data_available ) data_available = 0           ;
                       else sem_wait( & data_semaphore ) ;
    }                                                    ;
  }                                                      ;
  ////////////////////////////////////////////////////////
  return result                                          ;
}

signed long JackStream::BlockingGetStreamReadAvailable(void)
{
  int bytesFull = inFIFO . ReadAvailable ( ) ;
  return bytesFull / bytesPerFrame           ;
}

signed long JackStream::BlockingGetStreamWriteAvailable(void)
{
  int bytesEmpty = outFIFO . WriteAvailable ( ) ;
  return bytesEmpty / bytesPerFrame             ;
}

CaError JackStream::BlockingWaitEmpty(void)
{
  while ( outFIFO . ReadAvailable ( ) > 0 ) {
    data_available = 0                      ;
    sem_wait ( &data_semaphore )            ;
  }                                         ;
  return 0                                  ;
}

void JackStream::UpdateSampleRate(double SampleRate)
{
  bufferProcessor . samplePeriod   = 1.0 / SampleRate             ;
  cpuLoadMeasurer . samplingPeriod = bufferProcessor.samplePeriod ;
  sampleRate                       = SampleRate                   ;
}

CaError JackStream::RealProcess(jack_nframes_t frames)
{
  CaRETURN ( CaNotEqual ( is_running , TRUE ) , NoError )                    ;
  CaError              result              = NoError                         ;
  const double         sr                  = jack_get_sample_rate ( jack_client ) ;
  CaStreamFlags        cbFlags             = 0                               ;
  CaTime               InputBufferAdcTime  = 0                               ;
  CaTime               CurrentTime         = 0                               ;
  CaTime               OutputBufferDacTime = 0                               ;
  int                  chn                                                   ;
  int                  framesProcessed                                       ;
  jack_latency_range_t jackrange                                             ;
  ////////////////////////////////////////////////////////////////////////////
  CaRETURN ( CaNOT ( CaIsGreater ( sr , 0 ) ) , NoError )                    ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( Conduit::Continue != callbackResult )                              &&
       bufferProcessor.isOutputEmpty()                                     ) {
    if ( Conduit::Postpone != callbackResult )                               {
      is_active  = 0                                                         ;
      is_running = FALSE                                                     ;
      if ( CaNotNull ( conduit ) )                                           {
        if ( num_incoming_connections > 0 )                                  {
          conduit -> finish ( Conduit::InputDirection  , Conduit::Correct )  ;
        }                                                                    ;
        if ( num_outgoing_connections > 0 )                                  {
          conduit -> finish ( Conduit::OutputDirection , Conduit::Correct )  ;
        }                                                                    ;
      }                                                                      ;
      dPrintf ( ( "%s: Callback finished\n" , __FUNCTION__  ) )              ;
      goto end                                                               ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  CurrentTime = ( jack_frame_time ( jack_client ) - t0 ) / sr                ;
  if ( num_incoming_connections > 0 )                                        {
    jack_port_get_latency_range(remote_output_ports[0],JackPlaybackLatency,&jackrange) ;
    InputBufferAdcTime  = CurrentTime - jackrange.max / sr                   ;
  }                                                                          ;
  if ( num_outgoing_connections > 0 )                                        {
    jack_port_get_latency_range(remote_input_ports[0],JackCaptureLatency,&jackrange) ;
    OutputBufferDacTime = CurrentTime + jackrange.max / sr                   ;
  }                                                                          ;
  cpuLoadMeasurer . Begin ( )                                                ;
  ////////////////////////////////////////////////////////////////////////////
  if ( xrun )                                                                {
    cbFlags = StreamIO::OutputUnderflow | StreamIO::InputOverflow            ;
    xrun    = FALSE                                                          ;
  }                                                                          ;
  if ( num_incoming_connections > 0 )                                        {
    conduit -> Input . StatusFlags  = cbFlags                                ;
    conduit -> Input . CurrentTime  = CurrentTime                            ;
    conduit -> Input . AdcDac       = InputBufferAdcTime                     ;
  }                                                                          ;
  if ( num_outgoing_connections > 0 )                                        {
    conduit -> Output . StatusFlags = cbFlags                                ;
    conduit -> Output . CurrentTime = CurrentTime                            ;
    conduit -> Output . AdcDac      = OutputBufferDacTime                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  bufferProcessor . Begin ( conduit )                                        ;
  if ( num_incoming_connections > 0 )                                        {
    bufferProcessor . setInputFrameCount  ( 0 , frames )                     ;
  }                                                                          ;
  if ( num_outgoing_connections > 0 )                                        {
    bufferProcessor . setOutputFrameCount ( 0 , frames )                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  for ( chn = 0 ; chn < num_incoming_connections ; chn++ )                   {
    jack_default_audio_sample_t *channel_buf                                 ;
    channel_buf = (jack_default_audio_sample_t *) jack_port_get_buffer       (
                    local_input_ports[chn]                                   ,
                    frames                                                 ) ;
    bufferProcessor . setNonInterleavedInputChannels ( 0,chn,channel_buf )   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  for ( chn = 0 ; chn < num_outgoing_connections ; chn++ )                   {
    jack_default_audio_sample_t * channel_buf                                ;
    channel_buf = (jack_default_audio_sample_t *) jack_port_get_buffer       (
                    local_output_ports[chn] , frames                       ) ;
    bufferProcessor.setNonInterleavedOutputChannel ( 0 , chn , channel_buf ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  framesProcessed = bufferProcessor . End ( &callbackResult )                ;
  assert ( framesProcessed == frames )                                       ;
  cpuLoadMeasurer . End ( framesProcessed )                                  ;
end                                                                          :
  return result                                                              ;
}

/* Remove stream from processing queue */

CaError JackStream::RemoveStream(void)
{
  CaError result = NoError                                   ;
  ASSERT_CALL ( pthread_mutex_lock   ( &hostApi->mtx ) , 0 ) ;
  if ( !hostApi->jackIsDown )                                {
    hostApi -> toRemove = this                               ;
    result = hostApi->WaitCondition  (                     ) ;
  }                                                          ;
  ASSERT_CALL ( pthread_mutex_unlock ( &hostApi->mtx ) , 0 ) ;
  ENSURE_PA   ( result                                     ) ;
error                                                        :
  return result                                              ;
}

CaError JackStream::AddStream(void)
{
  CaError result = NoError                                   ;
  /* Add to queue of streams that should be processed       */
  ASSERT_CALL ( pthread_mutex_lock( &hostApi->mtx ), 0 )     ;
  if ( !hostApi->jackIsDown )                                {
    hostApi->toAdd = this                                    ;
    result = hostApi -> WaitCondition ( )                    ;
  }                                                          ;
  ASSERT_CALL ( pthread_mutex_unlock ( &hostApi->mtx ) , 0 ) ;
  ENSURE_PA   ( result                                     ) ;
  UNLESS      ( !hostApi->jackIsDown , DeviceUnavailable   ) ;
error                                                        :
  return result                                              ;
}

/* Free resources associated with stream, and eventually stream itself.
 * Frees allocated memory, and closes opened pcms.                          */
void JackStream:: CleanUpStream          (
       int terminateStreamRepresentation ,
       int terminateBufferProcessor      )
{
  int i                                                                 ;
  if ( isBlockingStream ) BlockingEnd ( )                               ;
  for ( i = 0 ; i < num_incoming_connections ; ++i )                    {
    if ( local_input_ports[i] )                                         {
      ASSERT_CALL ( jack_port_unregister( jack_client                   ,
                                          local_input_ports [i] ) , 0 ) ;
    }                                                                   ;
  }                                                                     ;
  for ( i = 0; i < num_outgoing_connections; ++i )                      {
    if ( local_output_ports[i] )                                        {
      ASSERT_CALL ( jack_port_unregister( jack_client                   ,
                                          local_output_ports[i] ) , 0 ) ;
    }                                                                   ;
  }                                                                     ;
  bufferProcessor . Terminate ( )                                       ;
  Terminate                   ( )                                       ;
  if ( NULL != stream_memory )                                          {
    stream_memory -> release ( )                                        ;
    delete stream_memory                                                ;
    stream_memory = NULL                                                ;
  }                                                                     ;
}

//////////////////////////////////////////////////////////////////////////////

JackDeviceInfo:: JackDeviceInfo (void)
               : DeviceInfo     (    )
{
}

JackDeviceInfo::~JackDeviceInfo (void)
{
}

///////////////////////////////////////////////////////////////////////////////

JackHostApi:: JackHostApi (void)
            : HostApi     (    )
{
  deviceInfoMemory = NULL                                ;
  jack_client      = NULL                                ;
  toAdd            = NULL                                ;
  toRemove         = NULL                                ;
  processQueue     = NULL                                ;
  ////////////////////////////////////////////////////////
  jackIsDown       = 0                                   ;
  jack_buffer_size = 0                                   ;
  hostApiIndex     = 0                                   ;
  inputBase        = 0                                   ;
  outputBase       = 0                                   ;
  xrun             = 0                                   ;
  ////////////////////////////////////////////////////////
  ::memset ( &mtx        , 0 , sizeof(pthread_mutex_t) ) ;
  ::memset ( &cond       , 0 , sizeof(pthread_cond_t ) ) ;
}

JackHostApi::~JackHostApi(void)
{
}

HostApi::Encoding JackHostApi::encoding(void) const
{
  return UTF8 ;
}

bool JackHostApi::hasDuplex(void) const
{
  return true ;
}

CaError JackHostApi::Open                            (
          Stream                ** s                 ,
          const StreamParameters * inputParameters   ,
          const StreamParameters * outputParameters  ,
          double                   sampleRate        ,
          CaUint32                 framesPerCallback ,
          CaStreamFlags            streamFlags       ,
          Conduit                * conduit           )
{
  CaError        result             = NoError                                ;
  JackStream  *  stream             = NULL                                   ;
  char        *  port_string        = (char *) deviceInfoMemory -> alloc ( jack_port_name_size() ) ;
  unsigned long  regexSz            = jack_client_name_size ( ) + 3          ;
  char        *  regex_pattern      = (char *) deviceInfoMemory -> alloc ( regexSz               ) ;
  const char  ** jack_ports         = NULL                                   ;
  const double   jackSr             = jack_get_sample_rate ( jack_client )   ;
  CaSampleFormat inputSampleFormat  = cafNothing                             ;
  CaSampleFormat outputSampleFormat = cafNothing                             ;
  int            bpInitialized      = 0                                      ;
  int            srInitialized      = 0                                      ;
  unsigned long  ofs                                                         ;
  int            i                                                           ;
  int            inputChannelCount                                           ;
  int            outputChannelCount                                          ;
  jack_latency_range_t jackrange                                             ;
  ////////////////////////////////////////////////////////////////////////////
  /* validate platform specific flags                                       */
  if ( (streamFlags & csfPlatformSpecificFlags) != 0 )                       {
     /* unexpected platform specific flag                                   */
    return InvalidFlag                                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( (streamFlags & csfPrimeOutputBuffersUsingStreamCallback) != 0 )       {
    streamFlags &= ~csfPrimeOutputBuffersUsingStreamCallback                 ;
    /*return paInvalidFlag;*/   /* This implementation does not support buffer priming */
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( framesPerCallback != Stream::FramesPerBufferUnspecified )             {
    /* Jack operates with power of two buffers, and we don't support non-integer buffer adaption (yet) */
    /*UNLESS( !(framesPerBuffer & (framesPerBuffer - 1)), paBufferTooBig );*/  /* TODO: Add descriptive error code? */
  }                                                                          ;
  /* Preliminary checks                                                     */
  if ( NULL != inputParameters )                                             {
    inputChannelCount = inputParameters -> channelCount                      ;
    inputSampleFormat = inputParameters -> sampleFormat                      ;
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    if ( inputParameters->device==CaUseHostApiSpecificDeviceSpecification )  {
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that input device can support inputChannelCount                */
    if ( inputChannelCount > deviceInfos[ inputParameters->device ]->maxInputChannels ) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate inputStreamInfo                                             */
    if ( NULL != inputParameters->streamInfo )                {
      return IncompatibleStreamInfo                           ;
      /* this implementation doesn't use custom stream info                 */
    }                                                                        ;
  } else                                                                     {
    inputChannelCount = 0                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputChannelCount = outputParameters -> channelCount                    ;
    outputSampleFormat = outputParameters -> sampleFormat                    ;
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    if ( outputParameters->device==CaUseHostApiSpecificDeviceSpecification ) {
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that output device can support inputChannelCount               */
    if ( outputChannelCount > deviceInfos[outputParameters->device]->maxOutputChannels ) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* validate outputStreamInfo                                            */
    if ( NULL != outputParameters->streamInfo )               {
      return IncompatibleStreamInfo                           ;
      /* this implementation doesn't use custom stream info                 */
    }                                                                        ;
  } else                                                                     {
    outputChannelCount = 0                                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* ... check that the sample rate exactly matches the ONE acceptable rate
   * A: This rate isn't necessarily constant though?                        */
  #define ABS(x) ( (x) > 0 ? (x) : -(x) )
  if ( ABS ( sampleRate - jackSr ) > 1 ) return InvalidSampleRate            ;
  #undef ABS
  stream = new JackStream ( )                                                ;
  UNLESS ( stream , InsufficientMemory )                                     ;
  stream -> debugger = debugger                                              ;
  stream -> conduit  = conduit                                               ;
  ENSURE_PA( Initialize ( stream , inputChannelCount, outputChannelCount ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  /* the blocking emulation, if necessary                                   */
  stream -> isBlockingStream = ( NULL == conduit )                           ;
  if ( stream->isBlockingStream )                                            {
    #ifdef XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    float latency = 0.001; /* 1ms is the absolute minimum we support */
    int   minimum_buffer_frames = 0;
    if ( inputParameters && inputParameters->suggestedLatency > latency )
      latency = inputParameters->suggestedLatency;
    else
    if ( outputParameters && outputParameters->suggestedLatency > latency )
      latency = outputParameters->suggestedLatency;
    /* the latency the user asked for indicates the minimum buffer size in frames */
    minimum_buffer_frames = (int) (latency * jack_get_sample_rate( jackHostApi->jack_client ));
    /* we also need to be able to store at least three full jack buffers to avoid dropouts */
    if ( jackHostApi->jack_buffer_size * 3 > minimum_buffer_frames )
      minimum_buffer_frames = jackHostApi->jack_buffer_size * 3;
    /* setup blocking API data structures (FIXME: can fail) */
    BlockingBegin( stream, minimum_buffer_frames );
    /* install our own callback for the blocking API */
    streamCallback = BlockingCallback;
    userData = stream;
    #endif
  } else                                                                     {
    stream->conduit = conduit                                                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  srInitialized = 1                                                          ;
  stream -> cpuLoadMeasurer . Initialize ( jackSr )                          ;
  /* create the JACK ports.  We cannot connect them until audio processing begins */
  /* Register a unique set of ports for this stream
   * TODO: Robust allocation of new port names                              */
  ofs = inputBase                                                            ;
  for ( i = 0 ; i < inputChannelCount ; i++ )                                {
    ::snprintf ( port_string , jack_port_name_size() , "in_%lu" , ofs + i  ) ;
    stream -> local_input_ports[i] = jack_port_register                      (
                                       jack_client                           ,
                                       port_string                           ,
                                       JACK_DEFAULT_AUDIO_TYPE               ,
                                       JackPortIsInput                       ,
                                       0                                   ) ;
    UNLESS( stream->local_input_ports[i] , InsufficientMemory              ) ;
  }                                                                          ;
  inputBase += inputChannelCount                                             ;
  ofs        = outputBase                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < outputChannelCount ; i++ )                               {
    ::snprintf ( port_string,jack_port_name_size(),"out_%lu",ofs+i )         ;
    stream->local_output_ports[i] = jack_port_register                       (
                                      jack_client                            ,
                                      port_string                            ,
                                      JACK_DEFAULT_AUDIO_TYPE                ,
                                      JackPortIsOutput                       ,
                                      0                                    ) ;
    UNLESS ( stream->local_output_ports[i] , InsufficientMemory            ) ;
  }                                                                          ;
  outputBase += outputChannelCount                                           ;
  ////////////////////////////////////////////////////////////////////////////
  /* look up the jack_port_t's for the remote ports.  We could do this at
   * stream start time, but doing it here ensures the name lookup only
   *  happens once.                                                         */
  if ( inputChannelCount > 0 )                                               {
    int err = 0                                                              ;
    /* Get output ports of our capture device                               */
    ::snprintf ( regex_pattern                                               ,
                 regexSz                                                     ,
                 "%s:.*"                                                     ,
                 deviceInfos[ inputParameters->device ]->name              ) ;
    jack_ports = jack_get_ports ( jack_client                                ,
                                  regex_pattern                              ,
                                  JACK_PORT_TYPE_FILTER                      ,
                                  JackPortIsOutput                         ) ;
    UNLESS ( jack_ports , UnanticipatedHostError )                           ;
    for ( i = 0; i < inputChannelCount && jack_ports[i]; i++ )               {
      stream->remote_output_ports[i] = jack_port_by_name                     (
                                         jack_client                         ,
                                         jack_ports[i]                     ) ;
      if ( NULL == stream->remote_output_ports[i] )                          {
        err = 1                                                              ;
        break                                                                ;
      }                                                                      ;
    }                                                                        ;
    ::free ( jack_ports )                                                    ;
    UNLESS ( ! err                  , InsufficientMemory                   ) ;
    /* Fewer ports than expected?                                           */
    UNLESS ( i == inputChannelCount , InternalError                        ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( outputChannelCount > 0 )                                              {
    int err = 0                                                              ;
    /* Get input ports of our playback device                               */
    ::snprintf ( regex_pattern                                               ,
                 regexSz                                                     ,
                 "%s:.*"                                                     ,
                 deviceInfos[ outputParameters->device ]->name             ) ;
    jack_ports = jack_get_ports ( jack_client                                ,
                                  regex_pattern                              ,
                                  JACK_PORT_TYPE_FILTER                      ,
                                  JackPortIsInput                          ) ;
    UNLESS ( jack_ports , UnanticipatedHostError )                           ;
    for ( i = 0; i < outputChannelCount && jack_ports[i]; i++ )              {
      stream -> remote_input_ports[i] = jack_port_by_name                    (
                                          jack_client                        ,
                                          jack_ports[i]                    ) ;
      if ( 0 == stream -> remote_input_ports[i] )                            {
        err = 1                                                              ;
        break                                                                ;
      }                                                                      ;
    }                                                                        ;
    ::free ( jack_ports )                                                    ;
    UNLESS ( ! err                   , InsufficientMemory )                  ;
   /* Fewer ports than expected?                                            */
    UNLESS ( i == outputChannelCount , InternalError      )                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ENSURE_PA ( stream->bufferProcessor.Initialize                             (
                inputChannelCount                                            ,
                inputSampleFormat                                            ,
                (CaSampleFormat)(cafFloat32 | cafNonInterleaved)             , /* hostInputSampleFormat */
                outputChannelCount                                           ,
                outputSampleFormat                                           ,
                (CaSampleFormat)(cafFloat32 | cafNonInterleaved)             , /* hostOutputSampleFormat */
                jackSr                                                       ,
                streamFlags                                                  ,
                framesPerCallback                                            ,
                0                                                            ,
                cabUnknownHostBufferSize                                     , /* Buffer size may vary on JACK's discretion */
                conduit                                                  ) ) ;
  bpInitialized = 1                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( stream->num_incoming_connections > 0 )                                {
    jack_port_get_latency_range ( stream->remote_output_ports[0]             ,
                                  JackPlaybackLatency                        ,
                                  &jackrange                               ) ;
    stream -> inputLatency =(jackrange . max                                 -
                             jack_get_buffer_size( jack_client )             +
                             stream->bufferProcessor.InputLatencyFrames())   /
                             sampleRate                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( stream->num_outgoing_connections > 0 )                                {
    jack_port_get_latency_range ( stream->remote_input_ports[0]              ,
                                  JackCaptureLatency                         ,
                                  &jackrange                               ) ;
    stream -> outputLatency=(jackrange . max                                 -
                             jack_get_buffer_size( jack_client )             +
                             stream->bufferProcessor.OutputLatencyFrames())  /
                             sampleRate                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> sampleRate = jackSr                                              ;
  stream -> t0         = jack_frame_time ( jack_client )                     ;
  /* Add to queue of opened streams                                         */
  ENSURE_PA ( stream -> AddStream ( ) )                                      ;
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
  *s = (Stream *) stream                                                     ;
  return result                                                              ;
error                                                                        :
  if ( NULL != stream ) stream -> CleanUpStream(srInitialized,bpInitialized) ;
  return result                                                              ;
}

void JackHostApi::Terminate(void)
{
  ASSERT_CALL ( jack_deactivate       ( jack_client ) , 0 ) ;
  ASSERT_CALL ( pthread_mutex_destroy ( &mtx        ) , 0 ) ;
  ASSERT_CALL ( pthread_cond_destroy  ( &cond       ) , 0 ) ;
  ASSERT_CALL ( jack_client_close     ( jack_client ) , 0 ) ;
  if ( NULL != deviceInfoMemory )                           {
    deviceInfoMemory -> release ( )                         ;
    delete deviceInfoMemory                                 ;
    deviceInfoMemory = NULL                                 ;
  }                                                         ;
  free ( jackErr_ )                                         ;
  jackErr_ = NULL                                           ;
}

CaError JackHostApi::isSupported                     (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double                    sampleRate       )
{
  int            inputChannelCount  = 0                                      ;
  int            outputChannelCount = 0                                      ;
  CaSampleFormat inputSampleFormat                                           ;
  CaSampleFormat outputSampleFormat                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputParameters )                                             {
    inputChannelCount = inputParameters->channelCount                        ;
    inputSampleFormat = inputParameters->sampleFormat                        ;
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
    outputChannelCount = outputParameters -> channelCount                    ;
    outputSampleFormat = outputParameters -> sampleFormat                    ;
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
  #define ABS(x) ( (x) > 0 ? (x) : -(x) )
  if ( ABS ( sampleRate - jack_get_sample_rate(jack_client ) ) > 1         ) {
    return InvalidSampleRate                                                 ;
  }                                                                          ;
  #undef ABS
  return NoError                                                             ;
}

CaError JackHostApi::BuildDeviceList(void)
{
  CaError              result          = NoError                             ;
  const char        ** jack_ports      = NULL                                ;
  char              ** client_names    = NULL                                ;
  char              *  regex_pattern   = NULL                                ;
  char              *  tmp_client_name = NULL                                ;
  unsigned long        numClients      = 0                                   ;
  unsigned long        numPorts        = 0                                   ;
  int                  port_index                                            ;
  int                  client_index                                          ;
  int                  i                                                     ;
  double               globalSampleRate                                      ;
  regex_t              port_regex                                            ;
  jack_latency_range_t jackrange                                             ;
  ////////////////////////////////////////////////////////////////////////////
  info.defaultInputDevice  = CaNoDevice                                      ;
  info.defaultOutputDevice = CaNoDevice                                      ;
  info.deviceCount         = 0                                               ;
  /* Parse the list of ports, using a regex to grab the client names        */
  ASSERT_CALL ( regcomp ( &port_regex , "^[^:]*" , REG_EXTENDED ) , 0 )      ;
  deviceInfoMemory -> release ( )                                            ;
  regex_pattern   = (char *)deviceInfoMemory->alloc(jack_client_name_size()+3) ;
  tmp_client_name = (char *)deviceInfoMemory->alloc(jack_client_name_size()  ) ;
  jack_ports = jack_get_ports ( jack_client,"",JACK_PORT_TYPE_FILTER,0     ) ;
  UNLESS ( jack_ports && jack_ports[0] , NoError )                           ;
  while  ( jack_ports [ numPorts ] ) ++numPorts                              ;
  client_names = (char **)deviceInfoMemory->alloc( numPorts*sizeof(char *) ) ;
  UNLESS ( client_names , InsufficientMemory )                               ;
  ////////////////////////////////////////////////////////////////////////////
  /* Build a list of clients from the list of ports                         */
  for ( numClients = 0 , port_index = 0                                      ;
        jack_ports[port_index] != NULL                                       ;
        port_index++                                                       ) {
    //////////////////////////////////////////////////////////////////////////
    int          client_seen = FALSE                                         ;
    const char * port        = jack_ports [ port_index ]                     ;
    regmatch_t   match_info                                                  ;
    //////////////////////////////////////////////////////////////////////////
    /* extract the client name from the port name, using a regex that parses
     * the clientname:portname syntax                                       */
    UNLESS ( !regexec( &port_regex,port,1,&match_info,0 ),InternalError    ) ;
    assert ( match_info.rm_eo - match_info.rm_so < jack_client_name_size() ) ;
    memcpy ( tmp_client_name                                                 ,
             port             + match_info.rm_so                             ,
             match_info.rm_eo - match_info.rm_so                           ) ;
    tmp_client_name [ match_info.rm_eo - match_info.rm_so ] = '\0'           ;
    //////////////////////////////////////////////////////////////////////////
    /* do we know about this port's client yet?                             */
    for ( i = 0; i < numClients; i++ )                                       {
      if ( 0 == ::strcmp ( tmp_client_name, client_names[i] ) )              {
        client_seen = TRUE                                                   ;
      }                                                                      ;
    }                                                                        ;
    if ( client_seen ) continue                                              ;
    //////////////////////////////////////////////////////////////////////////
    client_names[numClients] = (char *)deviceInfoMemory->alloc               (
                                         strlen(tmp_client_name) + 1       ) ;
    UNLESS ( client_names[numClients] , InsufficientMemory )                 ;
    if ( strcmp( "alsa_pcm", tmp_client_name ) == 0 && numClients > 0 )      {
      /* alsa_pcm goes in spot 0                                            */
      ::strcpy ( client_names [ numClients ] , client_names [ 0 ] )          ;
      ::strcpy ( client_names [ 0          ] , tmp_client_name    )          ;
    } else                                                                   {
      /* put the new client at the end of the client list                   */
      ::strcpy ( client_names [ numClients ] , tmp_client_name )             ;
    }                                                                        ;
    ++numClients                                                             ;
  }                                                                          ;
  /* there is one global sample rate all clients must conform to            */
  globalSampleRate = jack_get_sample_rate ( jack_client )                    ;
  deviceInfos = new DeviceInfo * [ numClients ]                              ;
  UNLESS ( deviceInfos , InsufficientMemory )                                ;
  assert ( info . deviceCount == 0          )                                ;
  /* Create a PaDeviceInfo structure for every client                       */
  for ( client_index = 0; client_index < numClients; client_index++ )        {
    DeviceInfo *  curDevInfo                                                 ;
    const char ** clientPorts = NULL                                         ;
    curDevInfo = (DeviceInfo *)new JackDeviceInfo ( )                        ;
    UNLESS ( curDevInfo , InsufficientMemory )                               ;
    curDevInfo->name = (char *) deviceInfoMemory -> alloc                    (
                                  strlen(client_names[client_index]) + 1   ) ;
    UNLESS ( curDevInfo->name , InsufficientMemory )                         ;
    ::strcpy ( (char *)curDevInfo->name, client_names[client_index] )        ;
    curDevInfo -> structVersion = 2                                          ;
    curDevInfo -> hostApi       = hostApiIndex                               ;
    curDevInfo -> hostType      = JACK                                       ;
    //////////////////////////////////////////////////////////////////////////
    curDevInfo -> defaultSampleRate = globalSampleRate                       ;
    ::sprintf ( regex_pattern , "%s:.*" , client_names[client_index]       ) ;
    /* ... what are your output ports (that we could input from)?           */
    clientPorts = jack_get_ports ( jack_client                               ,
                                   regex_pattern                             ,
                                   JACK_PORT_TYPE_FILTER                     ,
                                   JackPortIsOutput                        ) ;
    curDevInfo -> maxInputChannels        = 0                                ;
    curDevInfo -> defaultLowInputLatency  = 0.0                              ;
    curDevInfo -> defaultHighInputLatency = 0.0                              ;
    if ( clientPorts )                                                       {
      jack_port_t * p                                                        ;
      p = jack_port_by_name ( jack_client , clientPorts [ 0 ] )              ;
      jack_port_get_latency_range ( p , JackCaptureLatency , &jackrange )    ;
      curDevInfo -> defaultHighInputLatency = jackrange.max/globalSampleRate ;
      curDevInfo -> defaultLowInputLatency  = curDevInfo->defaultHighInputLatency ;
      for ( i = 0; clientPorts[i] != NULL; i++)                              {
        curDevInfo->maxInputChannels++                                       ;
      }                                                                      ;
      free ( clientPorts )                                                   ;
    }                                                                        ;
    /* ... what are your input ports (that we could output to)?             */
    clientPorts = jack_get_ports ( jack_client                               ,
                                   regex_pattern                             ,
                                   JACK_PORT_TYPE_FILTER                     ,
                                   JackPortIsInput                         ) ;
    curDevInfo -> maxOutputChannels        = 0                               ;
    curDevInfo -> defaultLowOutputLatency  = 0.0                             ;
    curDevInfo -> defaultHighOutputLatency = 0.0                             ;
    //////////////////////////////////////////////////////////////////////////
    if ( clientPorts )                                                       {
      jack_port_t * p                                                        ;
      p = jack_port_by_name ( jack_client , clientPorts[0] )                 ;
      jack_port_get_latency_range ( p , JackPlaybackLatency , &jackrange )   ;
      curDevInfo->defaultHighOutputLatency = jackrange.max/globalSampleRate  ;
      curDevInfo->defaultLowOutputLatency = curDevInfo->defaultHighOutputLatency ;
      for ( i = 0; clientPorts[i] != NULL; i++)                              {
        curDevInfo -> maxOutputChannels ++                                   ;
      }                                                                      ;
      free ( clientPorts )                                                   ;
    }                                                                        ;
    /* Add this client to the list of devices                               */
    deviceInfos [ client_index ] = curDevInfo                                ;
    ++info.deviceCount                                                       ;
    if ( ( info . defaultInputDevice      == CaNoDevice                  )  &&
         ( curDevInfo -> maxInputChannels >  0                           ) ) {
      info . defaultInputDevice  = client_index                              ;
    }                                                                        ;
    if ( ( info .defaultOutputDevice     == CaNoDevice                   )  &&
         ( curDevInfo->maxOutputChannels >  0                            ) ) {
      info . defaultOutputDevice = client_index                              ;
    }                                                                        ;
  }                                                                          ;
error                                                                        :
  ::regfree ( &port_regex )                                                  ;
  ::free    ( jack_ports  )                                                  ;
  return result                                                              ;
}

/* Update the JACK callback's stream processing queue. */
CaError JackHostApi::UpdateQueue(void)
{
  CaError      result        = NoError                                       ;
  int          queueModified = 0                                             ;
  const double jackSr        = jack_get_sample_rate ( jack_client )          ;
  int          err                                                           ;
  ////////////////////////////////////////////////////////////////////////////
  err = pthread_mutex_trylock ( & mtx )                                      ;
  if ( 0 != err )                                                            {
    assert( err == EBUSY )                                                   ;
    return NoError                                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != toAdd )                                                       {
    if ( NULL != processQueue )                                              {
      JackStream * node = processQueue                                       ;
      while ( node->next ) node = node->next                                 ;
      node -> next = (JackStream *) toAdd                                    ;
    } else                                                                   {
      processQueue = (JackStream *) toAdd                                    ;
    }                                                                        ;
    /* If necessary, update stream state                                    */
    if ( toAdd -> sampleRate != jackSr )                                     {
      ((JackStream *)toAdd) -> UpdateSampleRate ( (double)jackSr )           ;
    }                                                                        ;
    toAdd         = NULL                                                     ;
    queueModified = 1                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != toRemove )                                                    {
    int          removed = 0                                                 ;
    JackStream * node    = processQueue                                      ;
    JackStream * prev    = NULL                                              ;
    assert ( processQueue )                                                  ;
    while ( node )                                                           {
      if ( node == toRemove )                                                {
        if ( prev ) prev -> next = node -> next                              ;
               else processQueue = (JackStream *) node -> next               ;
        removed = 1                                                          ;
        break                                                                ;
      }                                                                      ;
      prev = node                                                            ;
      node = node -> next                                                    ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    UNLESS ( removed , InternalError )                                       ;
    toRemove      = NULL                                                     ;
    queueModified = 1                                                        ;
    dPrintf ( ("%s: Removed stream from processing queue\n",__FUNCTION__) )  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  if ( queueModified )                                                       {
    /* Signal that we've done what was asked of us                          */
    ASSERT_CALL ( pthread_cond_signal  ( &cond ) , 0 )                       ;
  }                                                                          ;
  ASSERT_CALL   ( pthread_mutex_unlock ( &mtx  ) , 0 )                       ;
  return result                                                              ;
}

CaError JackHostApi ::WaitCondition(void)
{
  CaError         result = NoError                                           ;
  int             err    = 0                                                 ;
  CaTime          pt     = Timer::Time()                                     ;
  struct timespec ts                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  ts . tv_sec  = (time_t) floor( pt + 10 * 60 /* 10 minutes */ )             ;
  ts . tv_nsec = (long  ) ( ( pt - floor ( pt ) ) * 1000000000 )             ;
  err = pthread_cond_timedwait ( &cond , &mtx , &ts )                        ;
  /* Make sure we didn't time out                                           */
  UNLESS (   err != ETIMEDOUT , TimedOut      )                              ;
  UNLESS ( ! err              , InternalError )                              ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  return result                                                              ;
}

/* Basic stream initialization */
CaError JackHostApi :: Initialize        (
          JackStream * stream            ,
          int          numInputChannels  ,
          int          numOutputChannels )
{
  CaError result = NoError                                                   ;
  UNLESS (stream->stream_memory = new AllocationGroup(),InsufficientMemory)  ;
  stream -> jack_client = jack_client                                        ;
  stream -> hostApi     = this                                               ;
  ////////////////////////////////////////////////////////////////////////////
  if ( numInputChannels > 0 )                                                {
    UNLESS ( stream->local_input_ports                                       =
            (jack_port_t **) stream->stream_memory->alloc                    (
                               sizeof(jack_port_t*) * numInputChannels     ) ,
            InsufficientMemory                                             ) ;
    memset ( stream->local_input_ports                                       ,
             0                                                               ,
             sizeof(jack_port_t*) * numInputChannels                       ) ;
    UNLESS ( stream->remote_output_ports                                     =
             (jack_port_t**) stream->stream_memory->alloc                    (
                               sizeof(jack_port_t*) * numInputChannels     ) ,
             InsufficientMemory                                            ) ;
    memset ( stream->remote_output_ports                                     ,
             0                                                               ,
             sizeof(jack_port_t*) * numInputChannels                       ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( numOutputChannels > 0 )                                               {
    UNLESS ( stream->local_output_ports                                      =
             (jack_port_t **) stream->stream_memory->alloc                   (
                                sizeof(jack_port_t*) * numOutputChannels   ) ,
             InsufficientMemory                                            ) ;
    memset ( stream->local_output_ports                                      ,
             0                                                               ,
             sizeof(jack_port_t*) * numOutputChannels                      ) ;
    UNLESS ( stream->remote_input_ports                                      =
             (jack_port_t **) stream->stream_memory->alloc                   (
                                sizeof(jack_port_t*) * numOutputChannels   ) ,
             InsufficientMemory                                            ) ;
    memset ( stream -> remote_input_ports                                    ,
             0                                                               ,
             sizeof(jack_port_t*) * numOutputChannels                      ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> num_incoming_connections = numInputChannels                      ;
  stream -> num_outgoing_connections = numOutputChannels                     ;
error                                                                        :
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
