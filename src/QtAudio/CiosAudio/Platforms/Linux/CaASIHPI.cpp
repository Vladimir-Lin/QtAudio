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

#include "CaASIHPI.hpp"

#warning !!! ASI HPI is expertimental !!!
#warning you should not include this into your official release.
#warning The author does not have AudioScience Sound card.
#warning HPI API code was written only by reading the document.
#warning The code for HPI API might highly possible go wrong.

//////////////////////////////////////////////////////////////////////////////

/* Error reporting and assertions                                           */

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

/* Evaluate expression, and return on any PortAudio errors                  */

#define CA_ENSURE_(expr) \
    do { \
        CaError caError = (expr); \
        if( UNLIKELY( caError < NoError ) ) \
        { \
            if ( NULL != globalDebugger ) { \
              globalDebugger -> printf ( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n"  ); \
            } ; \
            result = caError; \
            goto error; \
        } \
    } while (0);

/* Assert expression, else return the provided PaError                      */

#define CA_UNLESS_(expr, caError) \
    do { \
        if( UNLIKELY( (expr) == 0 ) ) \
        { \
            if ( NULL != globalDebugger ) { \
              globalDebugger -> printf ( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ) ; \
            } ; \
            result = (caError); \
            goto error; \
        } \
    } while( 0 );

/* Check return value of HPI function, and map it to CaError */

#define CA_ASIHPI_UNLESS_(expr, caError) \
    do { \
        hpi_err_t hpiError = (expr); \
        /* If HPI error occurred */ \
        if( UNLIKELY( hpiError ) ) \
        { \
        char szError[256]; \
        HPI_GetErrorText( hpiError, szError ); \
        if ( NULL != globalDebugger ) { \
          globalDebugger -> printf ( "HPI error %d occurred: %s\n", hpiError, szError ) ; \
        } ; \
        /* This message will always be displayed, even if debug info is disabled */ \
        if ( NULL != globalDebugger ) { \
          globalDebugger -> printf ( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ) ; \
        } ; \
            if( (caError) == UnanticipatedHostError ) \
        { \
            if ( NULL != globalDebugger ) { \
              globalDebugger -> printf ( "Host error description: %s\n", szError ) ; \
            } ; \
            /* PaUtil_SetLastHostErrorInfo should only be used in the main thread */ \
            if( pthread_equal( pthread_self(), caUnixMainThread ) ) \
                { \
                  SetLastHostErrorInfo( ASIHPI, hpiError, szError ); \
                } \
        } \
        /* If paNoError is specified, continue as usual */ \
            /* (useful if you only want to print out the debug messages above) */ \
        if( (caError) < 0 ) \
        { \
            result = (caError); \
            goto error; \
        } \
        } \
    } while( 0 );

/** Report HPI error code and text */
#define CA_ASIHPI_REPORT_ERROR_(hpiErrorCode) \
    do { \
        char szError[256]; \
        HPI_GetErrorText( hpiError, szError ); \
        if ( NULL != globalDebugger ) { \
          globalDebugger->printf("HPI error %d occurred: %s\n", hpiError, szError) ; \
        } ; \
        if( pthread_equal( pthread_self(), caUnixMainThread ) ) \
    { \
        SetLastHostErrorInfo( ASIHPI , (hpiErrorCode), szError ); \
    } \
    } while( 0 );

/* Defaults */

/* Sample formats available natively on AudioScience hardware */
#define CA_ASIHPI_AVAILABLE_FORMATS_ (cafFloat32 | cafInt32 | cafInt24 | cafInt16 | cafUint8)
/* Enable background bus mastering (BBM) for buffer transfers, if available (see HPI docs) */
#define CA_ASIHPI_USE_BBM_ 1
/* Minimum number of frames in HPI buffer (for either data or available space).
 If buffer contains less data/space, it indicates xrun or completion. */
#define CA_ASIHPI_MIN_FRAMES_ 1152
/* Minimum polling interval in milliseconds, which determines minimum host buffer size */
#define CA_ASIHPI_MIN_POLLING_INTERVAL_ 10

///////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////

/* Converts PortAudio sample format to equivalent HPI format.               */

static uint16_t CaToHpiFormat(CaSampleFormat caFormat)
{
  /* Ignore interleaving flag             */
  switch ( caFormat & ~cafNonInterleaved ) {
    case   cafFloat32                      :
    return HPI_FORMAT_PCM32_FLOAT          ;
    case   cafInt32                        :
    return HPI_FORMAT_PCM32_SIGNED         ;
    case   cafInt24                        :
    return HPI_FORMAT_PCM24_SIGNED         ;
    case   cafInt16                        :
    return HPI_FORMAT_PCM16_SIGNED         ;
    case   cafUint8                        :
    return HPI_FORMAT_PCM8_UNSIGNED        ;
    case   cafInt8                         :
    default                                :
    return HPI_FORMAT_PCM16_SIGNED         ;
  }                                        ;
}

//////////////////////////////////////////////////////////////////////////////

/* Converts HPI sample format to equivalent PortAudio format.               */

static CaSampleFormat HpiToCaFormat(uint16_t hpiFormat)
{
  switch ( hpiFormat )              {
    case   HPI_FORMAT_PCM32_FLOAT   :
    return cafFloat32               ;
    case   HPI_FORMAT_PCM32_SIGNED  :
    return cafInt32                 ;
    case   HPI_FORMAT_PCM24_SIGNED  :
    return cafInt24                 ;
    case   HPI_FORMAT_PCM16_SIGNED  :
    return cafInt16                 ;
    case   HPI_FORMAT_PCM8_UNSIGNED :
    return cafUint8                 ;
    default                         :
    return cafCustomFormat          ;
  }                                 ;
}

//////////////////////////////////////////////////////////////////////////////

/** Exit routine which is called when callback thread quits.
 This takes care of stopping the HPI streams (either waiting for output to finish, or
 abruptly). It also calls the user-supplied StreamFinished callback, and sets the
 stream state to CallbackFinished if it was reached via a non-paContinue return from
 the user callback function.                                                */

static void OnThreadExit(void * userData)
{
  if ( NULL == userData ) return                                             ;
  AsiHpiStream * stream = (AsiHpiStream *) userData                          ;
  assert ( stream )                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> cpuLoadMeasurer . Reset ( )                                      ;
  if ( NULL != globalDebugger )                                              {
    globalDebugger -> printf ( "%s: Stopping HPI streams\n" , __FUNCTION__ ) ;
  }                                                                          ;
  stream -> StopStream ( stream -> callbackAbort )                           ;
  if ( NULL != globalDebugger )                                              {
    globalDebugger -> printf ( "%s: Stoppage\n" , __FUNCTION__  )            ;
  }                                                                          ;
  /* Eventually notify user all buffers have played                         */
  if ( NULL != stream->conduit  ) stream -> conduit -> finish ( )            ;
  if ( stream->callbackFinished )                                            {
    stream -> state = AsiHpiCallbackFinishedState                            ;
  }                                                                          ;
}

//////////////////////////////////////////////////////////////////////////////

/* Main callback engine.
   This function runs in a separate thread and does all the work of fetching
   audio data from the AudioScience card via the HPI interface, feeding it to
   the user callback via the buffer processor, and delivering the resulting
   output data back to the card via HPI calls. It is started and terminated
   when the PortAudio stream is started and stopped, and starts the HPI
   streams on startup.                                                      */

static void * CallbackThreadFunc(void * userData)
{
  CaError        result         = NoError                                    ;
  AsiHpiStream * stream         = (AsiHpiStream *) userData                  ;
  int            callbackResult = Conduit::Continue                          ;
  /* Cleanup routine stops streams on thread exit                           */
  pthread_cleanup_push( &OnThreadExit , stream )                             ;
  /* Start HPI streams and notify parent when we're done                    */
  #warning CallbackThreadFunc need to implement PaUnixThread_PrepareNotify and PaUnixThread_NotifyParent
//  CA_ENSURE_( PaUnixThread_PrepareNotify( &stream->thread ) );
  /* Buffer will be primed with silence */
  CA_ENSURE_( stream->StartStream( 0 ) );
//  CA_ENSURE_( PaUnixThread_NotifyParent( &stream->thread ) );
    /* MAIN LOOP */
    while( 1 )
    {
        CaStreamFlags cbFlags = 0;
        unsigned long framesAvail, framesGot;
        pthread_testcancel();
        #warning CallbackThreadFunc need to implement PaUnixThread_StopRequested
//        if( PaUnixThread_StopRequested( &stream->thread ) && (callbackResult == Conduit::Continue) )
        {
            if ( NULL != globalDebugger ) {
              globalDebugger->printf( "Setting callbackResult to paComplete\n" ) ;
            } ;
            callbackResult = Conduit::Complete;
        }

        /* Start winding down thread if requested */
        if( callbackResult != Conduit::Continue )
        {
            stream->callbackAbort = (callbackResult == Conduit::Abort);
            if( stream->callbackAbort ||
                    /** @concern BlockAdaption: Go on if adaption buffers are empty */
               stream->bufferProcessor.isOutputEmpty() )
            {
                goto end;
            }
            if ( NULL != globalDebugger ) {
              globalDebugger->printf( "%s: Flushing buffer processor\n", __FUNCTION__ ) ;
            };
            /* There is still buffered output that needs to be processed */
        }
        /* SLEEP */
        /* Wait for data (or buffer space) to become available. This basically sleeps and
        polls the HPI interface until a full block of frames can be moved. */
        CA_ENSURE_( stream->WaitForFrames(&framesAvail, &cbFlags ) ) ;
        /* Consume buffer space. Once we have a number of frames available for consumption we
        must retrieve the data from the HPI interface and pass it to the PA buffer processor.
        We should be prepared to process several chunks successively. */
        while( framesAvail > 0 )
        {
            pthread_testcancel();
            framesGot = framesAvail;
            if( stream->bufferProcessor.hostBufferSizeMode == cabFixedHostBufferSize )
            {
                /* We've committed to a fixed host buffer size, stick to that */
                framesGot = framesGot >= stream->maxFramesPerHostBuffer ? stream->maxFramesPerHostBuffer : 0;
            }
            else
            {
                /* We've committed to an upper bound on the size of host buffers */
                assert( stream->bufferProcessor.hostBufferSizeMode == cabBoundedHostBufferSize ) ;
                framesGot = CA_MIN( framesGot, stream->maxFramesPerHostBuffer );
            }
            /* Obtain buffer timestamps */
            stream->CalculateTimeInfo(stream->conduit) ;
            #warning Status Flags requires to separate into input and output in ASIHPI
            stream->conduit->Input.StatusFlags  = cbFlags ;
            stream->conduit->Output.StatusFlags = cbFlags ;
            stream->bufferProcessor.Begin(stream->conduit);
            /* CPU load measurement should include processing activivity external to the stream callback */
            stream->cpuLoadMeasurer.Begin();
            if( framesGot > 0 )
            {
                /* READ FROM HPI INPUT STREAM */
                CA_ENSURE_( stream->BeginProcessing( &framesGot, &cbFlags ) ) ;
                /* Input overflow in a full-duplex stream makes for interesting times */
                if ( stream->input && stream->output && (cbFlags & StreamIO::InputOverflow) )
                {
                    /* Special full-duplex paNeverDropInput mode */
                    if ( stream->neverDropInput )
                    {
                        stream->bufferProcessor.setNoInput();
                        cbFlags |= StreamIO::OutputOverflow;
                    }
                }
                /* CALL USER CALLBACK WITH INPUT DATA, AND OBTAIN OUTPUT DATA */
                stream->bufferProcessor.End(&callbackResult) ;
                /* Clear overflow and underflow information (but PaAsiHpi_EndProcessing might
                still show up output underflow that will carry over to next round) */
                cbFlags = 0;
                /*  WRITE TO HPI OUTPUT STREAM */
                CA_ENSURE_( stream->EndProcessing(framesGot, &cbFlags ) );
                /* Advance frame counter */
                framesAvail -= framesGot;
            }
            stream->cpuLoadMeasurer.End(framesGot);
            if( framesGot == 0 )
            {
                /* Go back to polling for more frames */
                break;

            }
            if( callbackResult != Conduit::Continue )
                break;
        }
    }
  pthread_cleanup_pop ( 1 )                                                  ;
end                                                                          :
  /* Indicates normal exit of callback, as opposed to the thread getting killed explicitly */
  stream->callbackFinished = 1                                               ;
  if ( NULL != globalDebugger )                                              {
    globalDebugger->printf                                                   (
      "%s: Thread %d exiting (callbackResult = %d)\n "                       ,
      __FUNCTION__                                                           ,
      pthread_self()                                                         ,
      callbackResult                                                       ) ;
  }                                                                          ;
  #warning In CallbackThreadFunc, PaUnixThreading_EXIT need to implement
//  PaUnixThreading_EXIT( result )                                             ;
error                                                                        :
  goto end                                                                   ;
  return NULL                                                                ;
}

///////////////////////////////////////////////////////////////////////////////

CaError AsiHpiInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError         result      = NoError                                      ;
  HostApiInfo   * baseApiInfo = NULL                                         ;
  AsiHpiHostApi * hpiHostApi  = NULL                                         ;
  ////////////////////////////////////////////////////////////////////////////
  /* Try to initialize HPI subsystem                                        */
  if ( ! HPI_SubSysCreate ( ) )                                              {
    if ( NULL != globalDebugger )                                            {
      globalDebugger -> printf ( "Could not open HPI interface\n" )          ;
    }                                                                        ;
    *hostApi = NULL                                                          ;
    return NoError                                                           ;
  } else                                                                     {
    uint32_t hpiVersion                                                      ;
    CA_ASIHPI_UNLESS_( HPI_SubSysGetVersionEx ( NULL, &hpiVersion )          ,
                       UnanticipatedHostError                              ) ;
    if ( NULL != globalDebugger )                                            {
      globalDebugger -> printf                                               (
        "HPI interface v%d.%02d.%02d\n"                                      ,
        hpiVersion >> 16                                                     ,
        (hpiVersion >> 8) & 0x0F                                             ,
        (hpiVersion & 0x0F                                               ) ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* Allocate host API structure                                            */
  hpiHostApi  = new AsiHpiHostApi ( )                                        ;
  CA_UNLESS_ ( hpiHostApi                , InsufficientMemory              ) ;
  hpiHostApi->allocations = new AllocationGroup ( )                          ;
  CA_UNLESS_ ( hpiHostApi -> allocations , InsufficientMemory              ) ;
  ////////////////////////////////////////////////////////////////////////////
  hpiHostApi -> hostApiIndex = hostApiIndex                                  ;
  (*hostApi)  = (HostApi *)hpiHostApi                                        ;
  baseApiInfo = &((*hostApi)->info)                                          ;
  /* Fill in common API details                                             */
  baseApiInfo -> structVersion       = 1                                     ;
  baseApiInfo -> type                = ASIHPI                                ;
  baseApiInfo -> index               = hostApiIndex                          ;
  baseApiInfo -> name                = "AudioScience HPI"                    ;
  baseApiInfo -> deviceCount         = 0                                     ;
  baseApiInfo -> defaultInputDevice  = CaNoDevice                            ;
  baseApiInfo -> defaultOutputDevice = CaNoDevice                            ;
  ////////////////////////////////////////////////////////////////////////////
  CA_ENSURE_ ( hpiHostApi -> BuildDeviceList ( ) )                           ;
  /* Store identity of main thread                                          */
  #warning In AsiHpiInitialize, PaUnixThreading_Initialize need to implement
//  CA_ENSURE_ ( PaUnixThreading_Initialize() )                                ;
  return result                                                              ;
error                                                                        :
  if ( NULL != hpiHostApi ) delete hpiHostApi                                ;
  hpiHostApi = NULL                                                          ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

AsiHpiStream:: AsiHpiStream (void)
             : Stream       (    )
{
}

AsiHpiStream::~AsiHpiStream(void)
{
}

CaError AsiHpiStream::Start(void)
{
  CaError result = NoError                                      ;
  /* Ready the processor                                       */
  bufferProcessor . Reset ( )                                   ;
  if ( callbackMode )                                           {
    #warning Thread still requires to implement
//    PA_ENSURE_( PaUnixThread_New( &stream->thread, &CallbackThreadFunc, stream, 1., 0 /*rtSched*/ ) );
  } else                                                        {
    CA_ENSURE_ ( StartStream ( 0 ) )                            ;
  }                                                             ;
error                                                           :
  return result                                                 ;
}

CaError AsiHpiStream::Stop(void)
{
  return ExplicitStop ( 0 ) ;
}

CaError AsiHpiStream::Close(void)
{
  CaError result = NoError                                                   ;
  /* If stream is already gone, all is well                                 */
  /* Generic stream cleanup                                                 */
  bufferProcessor . Terminate ( )                                            ;
  Terminate                   ( )                                            ;
  /* Implementation-specific details - close internal streams               */
  if ( input )                                                               {
    /* Close HPI stream (freeing BBM host buffer in the process, if used)   */
    if ( input->hpiStream )                                                  {
      CA_ASIHPI_UNLESS_( HPI_InStreamClose                                   (
                           NULL                                              ,
                           input->hpiStream                                ) ,
                         UnanticipatedHostError                            ) ;
    }                                                                        ;
    /* Free temp buffer and stream component                                */
    Allocator :: free ( input -> tempBuffer )                                ;
    Allocator :: free ( input               )                                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( output )                                                              {
    /* Close HPI stream (freeing BBM host buffer in the process, if used)   */
    if ( output->hpiStream )                                                 {
      CA_ASIHPI_UNLESS_( HPI_OutStreamClose                                  (
                           NULL                                              ,
                           output->hpiStream                               ) ,
                         UnanticipatedHostError                            ) ;
    }                                                                        ;
    /* Free temp buffer and stream component                                */
    Allocator :: free ( output -> tempBuffer )                               ;
    Allocator :: free ( output               )                               ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  Allocator :: free ( blockingUserBufferCopy )                               ;
error                                                                        :
  return result                                                              ;
}

CaError AsiHpiStream::Abort(void)
{
  return ExplicitStop ( 1 ) ;
}

CaError AsiHpiStream::IsStopped(void)
{
  return ( AsiHpiStoppedState == state ) ? 1 : 0 ;
}

CaError AsiHpiStream::IsActive(void)
{
  return ( AsiHpiActiveState == state ) ? 1 : 0 ;
}

CaTime AsiHpiStream::GetTime(void)
{
  return Timer :: Time ( ) ;
}

double AsiHpiStream::GetCpuLoad(void)
{
  return callbackMode ? cpuLoadMeasurer . Value ( ) : 0.0 ;
}

CaInt32 AsiHpiStream::ReadAvailable(void)
{
  return 0 ;
}

CaError AsiHpiStream::PrimeOutputWithSilence(void)
{
  CaError                 result = NoError                                   ;
  AsiHpiStreamComponent * out    = output                                    ;
  ZeroCopier            * zeroer                                             ;
  CaSampleFormat          outputFormat                                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( !out ) return result                                                  ;
  /* Clear all existing data in hardware playback buffer                    */
  CA_ASIHPI_UNLESS_ ( HPI_OutStreamReset                                     (
                        NULL                                                 ,
                        out->hpiStream                                     ) ,
                      UnanticipatedHostError                               ) ;
  /* Fill temp buffer with silence                                          */
  outputFormat = HpiToCaFormat    ( out->hpiFormat.wFormat )                 ;
  zeroer       = SelectZeroCopier ( outputFormat           )                 ;
  zeroer ( out->tempBuffer                                                   ,
           1                                                                 ,
           out->tempBufferSize / Core::SampleSize(outputFormat)            ) ;
  /* Write temp buffer to hardware fifo twice, to get started               */
  CA_ASIHPI_UNLESS_ ( HPI_OutStreamWriteBuf                                  (
                        NULL                                                 ,
                        out->hpiStream                                       ,
                        out->tempBuffer                                      ,
                        out->tempBufferSize                                  ,
                        &out->hpiFormat                                    ) ,
                      UnanticipatedHostError                               ) ;
  CA_ASIHPI_UNLESS_ ( HPI_OutStreamWriteBuf                                  (
                        NULL                                                 ,
                        out->hpiStream                                       ,
                        out->tempBuffer                                      ,
                        out->tempBufferSize                                  ,
                        &out->hpiFormat                                    ) ,
                      UnanticipatedHostError                               ) ;
error                                                                        :
  return result                                                              ;
}

CaError AsiHpiStream::StartStream(int outputPrimed)
{
  CaError result = NoError                                                   ;
  if ( NULL != input )                                                       {
    CA_ASIHPI_UNLESS_ ( HPI_InStreamStart  ( NULL , input  -> hpiStream    ) ,
                        UnanticipatedHostError                             ) ;
  }                                                                          ;
  if ( NULL != output )                                                      {
    if ( !outputPrimed )                                                     {
      CA_ENSURE_( PrimeOutputWithSilence ( ) )                               ;
    }                                                                        ;
    CA_ASIHPI_UNLESS_ ( HPI_OutStreamStart ( NULL , output -> hpiStream    ) ,
                        UnanticipatedHostError                             ) ;
  }                                                                          ;
  state            = AsiHpiActiveState                                       ;
  callbackFinished = 0                                                       ;
  /* Report stream info for debugging purposes                              */
error                                                                        :
  return result                                                              ;
}

CaError AsiHpiStream::StopStream(int abort)
{
  CaError result = NoError                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( input )                                                               {
    CA_ASIHPI_UNLESS_ ( HPI_InStreamReset                                    (
                          NULL                                               ,
                          input -> hpiStream                               ) ,
                        UnanticipatedHostError                             ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( output )                                                              {
    if ( !abort )                                                            {
      while ( 1 )                                                            {
        AsiHpiStreamInfo streamInfo                                          ;
        CaTime           timeLeft                                            ;
        /* Obtain number of samples waiting to be played                    */
        CA_ENSURE_ ( output -> GetStreamInfo ( &streamInfo ) )               ;
        /* Check if stream is drained                                       */
        if ( ( streamInfo . state != HPI_STATE_PLAYING                    ) &&
             ( streamInfo . dataSize < ( output->bytesPerFrame * CA_ASIHPI_MIN_FRAMES_ ) ) ) {
          break                                                              ;
        }                                                                    ;
        /* Sleep amount of time represented by remaining samples            */
        timeLeft = 1000.0 * streamInfo.dataSize                              /
                   output->bytesPerFrame                                     /
                   sampleRate                                                ;
        Timer :: Sleep ( (long)ceil( timeLeft ) )                            ;
      }                                                                      ;
    }                                                                        ;
    CA_ASIHPI_UNLESS_ ( HPI_OutStreamReset                                   (
                          NULL                                               ,
                          output->hpiStream                                ) ,
                        UnanticipatedHostError                             ) ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError AsiHpiStream::ExplicitStop(int abort)
{
  CaError result = NoError                                                   ;
  /* First deal with the callback thread, cancelling and/or joining it if necessary */
  if ( callbackMode )                                                        {
    CaError threadRes                                                        ;
    callbackAbort = abort                                                    ;
    if ( abort )                                                             {
            globalDebugger->printf(( "Aborting callback\n" ));
    } else                                                                   {
            globalDebugger->printf(( "Stopping callback\n" ));
    }                                                                        ;
    #warning Thread Terminate requires to implement
//    CA_ENSURE_( PaUnixThread_Terminate( &stream->thread, !abort, &threadRes ) );
    if ( threadRes != NoError )                                              {
      if ( NULL != globalDebugger )                                          {
        globalDebugger->printf("Callback thread returned: %d\n",threadRes)   ;
      }                                                                      ;
    }                                                                        ;
  } else                                                                     {
    CA_ENSURE_ ( StopStream ( abort ) )                                      ;
  }                                                                          ;
  state = AsiHpiStoppedState                                                 ;
error                                                                        :
  return result                                                              ;
}

CaInt32 AsiHpiStream::WriteAvailable(void)
{
  CaError          result = NoError                           ;
  AsiHpiStreamInfo info                                       ;
  CA_UNLESS_ ( output , CanNotWriteToAnInputOnlyStream )      ;
  CA_ENSURE_ ( output -> GetStreamInfo ( &info )       )      ;
  /* Round down to the nearest host buffer multiple          */
  result = ( info . availableFrames / maxFramesPerHostBuffer) *
                                      maxFramesPerHostBuffer  ;
  if ( info.underflow ) result = OutputUnderflowed            ;
error                                                         :
  return result                                               ;
}

CaError AsiHpiStream::Read(void * buffer,unsigned long frames)
{
  CaError          result = NoError                                          ;
  AsiHpiStreamInfo info                                                      ;
  void           * userBuffer                                                ;
  ////////////////////////////////////////////////////////////////////////////
  CA_UNLESS_ ( input , CanNotReadFromAnOutputOnlyStream )                    ;
  /* Check for input overflow since previous call to ReadStream             */
  CA_ENSURE_ ( input -> GetStreamInfo ( &info ) )                            ;
  if ( info.overflow ) result = InputOverflowed                              ;
  /* NB Make copy of user buffer pointers, since they are advanced by buffer processor */
  if ( bufferProcessor . userInputIsInterleaved )                            {
    userBuffer = buffer                                                      ;
  } else                                                                     {
    /* Copy channels into local array                                       */
    userBuffer = blockingUserBufferCopy                                      ;
    memcpy ( userBuffer                                                      ,
             buffer                                                          ,
             sizeof (void *) * input->hpiFormat.wChannels                  ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  while ( frames > 0 )                                                       {
    unsigned long framesGot                                                  ;
    unsigned long framesAvail                                                ;
    CaStreamFlags cbFlags = 0                                                ;
    //////////////////////////////////////////////////////////////////////////
    CA_ENSURE_ ( WaitForFrames   ( &framesAvail , &cbFlags ) )               ;
    framesGot = CA_MIN ( framesAvail, frames )                               ;
    CA_ENSURE_ ( BeginProcessing ( &framesGot   , &cbFlags ) )               ;
    if ( framesGot > 0 )                                                     {
      framesGot = bufferProcessor . CopyInput ( &userBuffer , framesGot    ) ;
      CA_ENSURE_ ( EndProcessing ( framesGot , &cbFlags ) )                  ;
      frames -= framesGot                                                    ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
error                                                                        :
  return result                                                              ;
}

CaError AsiHpiStream::Write(const void * buffer,unsigned long frames)
{
  CaError          result = NoError                                          ;
  AsiHpiStreamInfo info                                                      ;
  const void     * userBuffer                                                ;
  ////////////////////////////////////////////////////////////////////////////
  CA_UNLESS_ ( output , CanNotWriteToAnInputOnlyStream )                     ;
  /* Check for output underflow since previous call to WriteStream          */
  CA_ENSURE_ ( output -> GetStreamInfo ( &info ) )                           ;
  if ( info.underflow ) result = OutputUnderflowed                           ;
  /* NB Make copy of user buffer pointers, since they are advanced by buffer processor */
  if ( bufferProcessor.userOutputIsInterleaved )                             {
    userBuffer = buffer                                                      ;
  } else                                                                     {
    /* Copy channels into local array                                       */
    userBuffer = blockingUserBufferCopy                                      ;
    memcpy ( (void *)userBuffer                                              ,
             buffer                                                          ,
             sizeof (void *) * output->hpiFormat.wChannels                 ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  while ( frames > 0 )                                                       {
    unsigned long framesGot                                                  ;
    unsigned long framesAvail                                                ;
    CaStreamFlags cbFlags = 0                                                ;
    //////////////////////////////////////////////////////////////////////////
    CA_ENSURE_ ( WaitForFrames   ( &framesAvail , &cbFlags ) )               ;
    framesGot = CA_MIN ( framesAvail, frames )                               ;
    CA_ENSURE_ ( BeginProcessing ( &framesGot   , &cbFlags ) )               ;
    if ( framesGot > 0 )                                                     {
      framesGot = bufferProcessor . CopyOutput ( &userBuffer , framesGot )   ;
      CA_ENSURE_ ( EndProcessing ( framesGot , &cbFlags ) )                  ;
      /* Advance frame counter                                              */
      frames -= framesGot                                                    ;
    }                                                                        ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

signed long AsiHpiStream::GetStreamReadAvailable(void)
{
  CaError          result = NoError                            ;
  AsiHpiStreamInfo info                                        ;
  CA_UNLESS_ ( input , CanNotReadFromAnOutputOnlyStream )      ;
  CA_ENSURE_ ( input -> GetStreamInfo ( &info )         )      ;
  /* Round down to the nearest host buffer multiple           */
  result = ( info . availableFrames / maxFramesPerHostBuffer ) *
                                      maxFramesPerHostBuffer   ;
  if ( info.overflow ) result = InputOverflowed                ;
error                                                          :
  return result                                                ;
}

bool AsiHpiStream::hasVolume(void)
{
  return true ;
}

CaVolume AsiHpiStream::MinVolume(void)
{
  return 0.0 ;
}

CaVolume AsiHpiStream::MaxVolume(void)
{
  return 10000.0 ;
}

#warning Do not forget ASIHPI Volume Control functionalities

CaVolume AsiHpiStream::Volume(int atChannel)
{
  return 0 ;
}

CaVolume AsiHpiStream::setVolume(CaVolume volume,int atChannel)
{
  return 0 ;
}

CaError AsiHpiStream::WaitForFrames(unsigned long * framesAvail,CaStreamFlags * cbFlags)
{
  CaError       result       = NoError                                       ;
  double        SampleRate   = sampleRate                                    ;
  unsigned long framesTarget = 0                                             ;
  uint32_t      outputData   = 0                                             ;
  uint32_t      outputSpace  = 0                                             ;
  uint32_t      inputData    = 0                                             ;
  uint32_t      framesLeft   = 0                                             ;
  /* We have to come up with this much frames on both input and output      */
  framesTarget = bufferProcessor . framesPerHostBuffer                       ;
  while ( 1 )                                                                {
    AsiHpiStreamInfo info                                                    ;
    /* Check output first, as this takes priority in the default full-duplex mode */
    if ( output )                                                            {
      CA_ENSURE_ ( output -> GetStreamInfo ( &info ) )                       ;
      /* Wait until enough space is available in output buffer to receive a full block */
      if ( info.availableFrames < framesTarget )                             {
        framesLeft = framesTarget - info.availableFrames                     ;
        Timer :: Sleep ( (long)ceil( 1000 * framesLeft / sampleRate ) )      ;
        continue                                                             ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      if ( ! input && ( output->outputBufferCap > 0 )                       &&
           ( info.totalBufferedData > output->outputBufferCap / output->bytesPerFrame ) ) {
        framesLeft = info.totalBufferedData                                  -
                     output->outputBufferCap                                 /
                     output->bytesPerFrame                                   ;
        Timer :: Sleep ( (long)ceil( 1000 * framesLeft / sampleRate ) )      ;
        continue                                                             ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      outputData  = info.totalBufferedData                                   ;
      outputSpace = info.availableFrames                                     ;
      if ( info.underflow ) *cbFlags |= StreamIO::OutputUnderflow             ;
    }                                                                        ;
    /* Now check input side                                                 */
    if ( input )                                                             {
      CA_ENSURE_ ( input -> GetStreamInfo ( &info ) )                        ;
      /* If a full block of samples hasn't been recorded yet, wait for it if possible */
      if ( info.availableFrames < framesTarget )                             {
        framesLeft = framesTarget - info.availableFrames                     ;
        /* As long as output is not disrupted in the process, wait for a full block of input samples */
        if ( ! output || (outputData > framesLeft) )                         {
          Timer :: Sleep ( (long)ceil( 1000 * framesLeft / sampleRate )    ) ;
          continue                                                           ;
        }                                                                    ;
      }                                                                      ;
      inputData = info.availableFrames                                       ;
      if ( info.overflow ) *cbFlags |= StreamIO::InputOverflow                ;
    }                                                                        ;
    break                                                                    ;
  }                                                                          ;
  /* Full-duplex stream                                                     */
  if ( input && output )                                                     {
    if ( outputSpace >= framesTarget ) *framesAvail = outputSpace            ;
     if ( (inputData >= framesTarget) && (inputData < outputSpace) )         {
       *framesAvail = inputData                                              ;
     }                                                                       ;
  } else                                                                     {
    *framesAvail = input ? inputData : outputSpace                           ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

void AsiHpiStream::CalculateTimeInfo(Conduit * CONDUIT)
{
  AsiHpiStreamInfo streamInfo                                                ;
  double           SampleRate = sampleRate                                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( input )                                                               {
    CONDUIT->Input.CurrentTime  = GetTime ( )                                 ;
    CONDUIT->Input.AdcDac = CONDUIT->Input.CurrentTime                    ;
    input->GetStreamInfo(&streamInfo)                                        ;
    CONDUIT->Input.AdcDac -= streamInfo.totalBufferedData/sampleRate   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( output )                                                              {
    CONDUIT->Output.AdcDac = CONDUIT->Output.CurrentTime                ;
    CONDUIT->Output.CurrentTime = GetTime ( )                                 ;
    output ->GetStreamInfo(&streamInfo)                                      ;
    CONDUIT->Output.AdcDac += streamInfo.totalBufferedData/sampleRate  ;
  }                                                                          ;
}

CaError AsiHpiStream::BeginProcessing(unsigned long * numFrames,CaStreamFlags * cbFlags)
{
  CaError result = NoError                                                   ;
  if ( *numFrames > maxFramesPerHostBuffer )                                 {
    *numFrames = maxFramesPerHostBuffer                                      ;
  }                                                                          ;
  if ( input )                                                               {
    AsiHpiStreamInfo info                                                    ;
    uint32_t         framesToGet = *numFrames                                ;
    /* Check for overflows and underflows yet again                         */
    CA_ENSURE_ ( input -> GetStreamInfo ( &info ) )                          ;
    if ( info.overflow ) (*cbFlags) |= StreamIO::InputOverflow                ;
    /* Input underflow if less than expected number of samples pitch up     */
    if ( framesToGet > info.availableFrames )                                {
      CaSampleFormat inputFormat                                             ;
      ZeroCopier   * zeroer                                                  ;
      /* Never call an input-only stream with InputUnderflow set            */
      if ( output ) *cbFlags |= StreamIO::InputUnderflow                      ;
      framesToGet = info . availableFrames                                   ;
      /* Fill temp buffer with silence (to make up for missing input samples) */
      inputFormat = HpiToCaFormat    ( input -> hpiFormat . wFormat )        ;
      zeroer      = SelectZeroCopier ( inputFormat                  )        ;
      zeroer ( input->tempBuffer                                             ,
               1                                                             ,
               input->tempBufferSize / Core::SampleSize(inputFormat)       ) ;
    }                                                                        ;
    /* Read block of data into temp buffer                                  */
    CA_ASIHPI_UNLESS_( HPI_InStreamReadBuf                                   (
                         NULL                                                ,
                         input -> hpiStream                                  ,
                         input -> tempBuffer                                 ,
                         framesToGet * input->bytesPerFrame                ) ,
                         UnanticipatedHostError                            ) ;
    /* Register temp buffer with buffer processor (always FULL buffer)      */
    bufferProcessor . setInputFrameCount ( 0 , *numFrames )                  ;
    /* HPI interface only allows interleaved channels                       */
    bufferProcessor . setInterleavedInputChannels                            (
      0                                                                      ,
      0                                                                      ,
      input -> tempBuffer                                                    ,
      input -> hpiFormat . wChannels                                       ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( output )                                                              {
    /* Register temp buffer with buffer processor                           */
    bufferProcessor . setOutputFrameCount ( 0 , *numFrames )                 ;
    /* HPI interface only allows interleaved channels                       */
    bufferProcessor . setInterleavedOutputChannels                           (
      0                                                                      ,
      0                                                                      ,
      output -> tempBuffer                                                   ,
      output -> hpiFormat . wChannels                                      ) ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError AsiHpiStream::EndProcessing(unsigned long numFrames,CaStreamFlags * cbFlags)
{
  CaError result = NoError                                                   ;
  if ( output )                                                              {
    AsiHpiStreamInfo info                                                    ;
    CA_ENSURE_ ( output -> GetStreamInfo ( &info ) )                         ;
    if ( info.underflow ) *cbFlags |= StreamIO::OutputUnderflow               ;
    CA_ASIHPI_UNLESS_ ( HPI_OutStreamWriteBuf                                (
                          NULL                                               ,
                          output  -> hpiStream                               ,
                          output  -> tempBuffer                              ,
                          numFrames * output->bytesPerFrame                  ,
                          &output -> hpiFormat                             ) ,
                          UnanticipatedHostError                           ) ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

void AsiHpiStream::StreamDump(void)
{
  if ( NULL == globalDebugger ) return ;
  globalDebugger->printf ( "\n------------------------- STREAM INFO FOR %p ---------------------------\n", this );
  /* General stream info (input+output) */
  if ( NULL != conduit ) {
    globalDebugger->printf( "[callback] " );
  } else {
    globalDebugger->printf( "[blocking] " );
  }
  globalDebugger->printf( "sr=%d Hz, poll=%d ms, max %d frames/buf ",
               (int)sampleRate,
               pollingInterval,
               maxFramesPerHostBuffer ) ;
  switch ( state ) {
    case AsiHpiStoppedState:
      globalDebugger->printf( "[stopped]\n") ;
    break;
    case AsiHpiActiveState:
      globalDebugger->printf( "[active]\n") ;
    break;
    case AsiHpiCallbackFinishedState:
      globalDebugger->printf( "[cb fin]\n") ;
    break;
    default:
      globalDebugger->printf( "[unknown state]\n") ;
    break;
  }
  if ( callbackMode ) {
    globalDebugger->printf( "cb info: thread=%p, cbAbort=%d, cbFinished=%d\n",
                   thread, callbackAbort, callbackFinished ) ;
  }
  globalDebugger->printf("----------------------------------- Input  ------------------------------------\n") ;
  if ( input ) {
    input->StreamComponentDump(this) ;
  } else {
    globalDebugger->printf("*none*\n") ;
  }
  globalDebugger->printf("----------------------------------- Output ------------------------------------\n") ;
  if ( output ) {
    output->StreamComponentDump(this) ;
  } else {
    globalDebugger->printf("*none*\n");
  } ;
  globalDebugger->printf("-------------------------------------------------------------------------------\n\n" ) ;
}

///////////////////////////////////////////////////////////////////////////////

AsiHpiDeviceInfo:: AsiHpiDeviceInfo (void)
                 : DeviceInfo       (    )
{
}

AsiHpiDeviceInfo::~AsiHpiDeviceInfo (void)
{
}

///////////////////////////////////////////////////////////////////////////////

AsiHpiStreamComponent:: AsiHpiStreamComponent (void)
{
}

AsiHpiStreamComponent::~AsiHpiStreamComponent (void)
{
}

CaError AsiHpiStreamComponent::GetStreamInfo(AsiHpiStreamInfo * info)
{
  CaError result = DeviceUnavailable ;
  uint16_t state;
  uint32_t bufferSize, dataSize, frameCounter, auxDataSize, threshold;
  uint32_t hwBufferSize, hwDataSize;
  /* First blank the stream info struct, in case something goes wrong below.
       This saves the caller from initializing the struct. */
    info->state = 0;
    info->bufferSize = 0;
    info->dataSize = 0;
    info->frameCounter = 0;
    info->auxDataSize = 0;
    info->totalBufferedData = 0;
    info->availableFrames = 0;
    info->underflow = 0;
    info->overflow = 0;

  if ( hpiDevice && hpiStream )  {
        /* Obtain detailed stream info (either input or output) */
        if ( hpiDevice->streamIsOutput )
        {
            CA_ASIHPI_UNLESS_( HPI_OutStreamGetInfoEx( NULL,
                               hpiStream,
                               &state, &bufferSize, &dataSize, &frameCounter,
                               &auxDataSize ), UnanticipatedHostError );
        }
        else
        {
            CA_ASIHPI_UNLESS_( HPI_InStreamGetInfoEx( NULL,
                               hpiStream,
                               &state, &bufferSize, &dataSize, &frameCounter,
                               &auxDataSize ), UnanticipatedHostError );
        }
        /* Load stream info */
        info->state = state;
        info->bufferSize = bufferSize;
        info->dataSize = dataSize;
        info->frameCounter = frameCounter;
        info->auxDataSize = auxDataSize;
        /* Determine total buffered data */
        info->totalBufferedData = dataSize;
        if( hostBufferSize > 0 )
            info->totalBufferedData += auxDataSize;
        info->totalBufferedData /= bytesPerFrame;
        /* Determine immediately available frames */
        info->availableFrames = hpiDevice->streamIsOutput ?
                                bufferSize - dataSize : dataSize;
        info->availableFrames /= bytesPerFrame;
        /* Minimum space/data required in buffers */
        threshold = CA_MIN( tempBufferSize,
                            bytesPerFrame * CA_ASIHPI_MIN_FRAMES_ );
        /* Obtain hardware buffer stats first, to simplify things */
        hwBufferSize = hardwareBufferSize;
        hwDataSize   = hostBufferSize > 0 ? auxDataSize : dataSize;
        /* Underflow is a bit tricky */
        info->underflow = hpiDevice->streamIsOutput ?
                          /* Stream seems to start in drained state sometimes, so ignore initial underflow */
                          (frameCounter > 0) && ( (state == HPI_STATE_DRAINED) || (hwDataSize == 0) ) :
                          /* Input streams check the first-level (host) buffer for underflow */
                          (state != HPI_STATE_STOPPED) && (dataSize < threshold);
        /* Check for overflow in second-level (hardware) buffer for both input and output */
        info->overflow = (state != HPI_STATE_STOPPED) && (hwBufferSize - hwDataSize < threshold);

    return NoError                                                           ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

void AsiHpiStreamComponent::StreamComponentDump(AsiHpiStream * stream)
{
  if ( NULL == globalDebugger ) return ;
  AsiHpiStreamInfo streamInfo ;
    /* Name of soundcard/device used by component */
    globalDebugger->printf( "device: %s\n", hpiDevice->name);
    /* Unfortunately some overlap between input and output here */
    if ( hpiDevice->streamIsOutput ) {
        /* Settings on the user side (as experienced by user callback) */
        globalDebugger->printf( "user: %d-bit, %d ",
                   8*stream->bufferProcessor.bytesPerUserOutputSample,
                   stream->bufferProcessor.outputChannelCount);
        if( stream->bufferProcessor.userOutputIsInterleaved )
        {
            globalDebugger->printf( "interleaved channels, " ) ;
        } else {
            globalDebugger->printf( "non-interleaved channels, " ) ;
        }
        globalDebugger->printf( "%d frames/buffer, latency = %5.1f ms\n",
                   stream->bufferProcessor.framesPerUserBuffer,
                   1000*stream->outputLatency);
        /* Settings on the host side (internal to PortAudio host API) */
        globalDebugger->printf( "host: %d-bit, %d interleaved channels, %d frames/buffer ",
                   8*stream->bufferProcessor.bytesPerHostOutputSample,
                   stream->bufferProcessor.outputChannelCount,
                   stream->bufferProcessor.framesPerHostBuffer );
    } else {
        /* Settings on the user side (as experienced by user callback) */
        globalDebugger->printf( "user: %d-bit, %d ",
                   8*stream->bufferProcessor.bytesPerUserInputSample,
                   stream->bufferProcessor.inputChannelCount) ;
        if( stream->bufferProcessor.userInputIsInterleaved )
        {
            globalDebugger->printf("interleaved channels, " ) ;
        } else {
            globalDebugger->printf("non-interleaved channels, ") ;
        }
        globalDebugger->printf( "%d frames/buffer, latency = %5.1f ms\n",
                   stream->bufferProcessor.framesPerUserBuffer,
                   1000*stream->inputLatency ) ;
        /* Settings on the host side (internal to PortAudio host API) */
        globalDebugger->printf( "host: %d-bit, %d interleaved channels, %d frames/buffer ",
                   8*stream->bufferProcessor.bytesPerHostInputSample,
                   stream->bufferProcessor.inputChannelCount,
                   stream->bufferProcessor.framesPerHostBuffer ) ;
    }
    switch( stream->bufferProcessor.hostBufferSizeMode )
    {
    case cabFixedHostBufferSize:
        globalDebugger->printf( "[fixed] " );
        break;
    case cabBoundedHostBufferSize:
        globalDebugger->printf( "[bounded] " );
        break;
    case cabUnknownHostBufferSize:
        globalDebugger->printf( "[unknown] " );
        break;
    case cabVariableHostBufferSizePartialUsageAllowed:
        globalDebugger->printf( "[variable] " );
        break;
    }
    globalDebugger->printf( "(%d max)\n", tempBufferSize / bytesPerFrame );
    /* HPI hardware settings */
    globalDebugger->printf( "HPI: adapter %d stream %d, %d-bit, %d-channel, %d Hz\n",
               hpiDevice->adapterIndex, hpiDevice->streamIndex,
               8 * bytesPerFrame / hpiFormat.wChannels,
               hpiFormat.wChannels,
               hpiFormat.dwSampleRate ) ;
    /* Stream state and buffer levels */
    globalDebugger->printf( "HPI: " );
    GetStreamInfo( &streamInfo ) ;
    switch( streamInfo.state )
    {
    case HPI_STATE_STOPPED:
        globalDebugger->printf( "[STOPPED] " );
        break;
    case HPI_STATE_PLAYING:
        globalDebugger->printf( "[PLAYING] " );
        break;
    case HPI_STATE_RECORDING:
        globalDebugger->printf( "[RECORDING] " );
        break;
    case HPI_STATE_DRAINED:
        globalDebugger->printf( "[DRAINED] " );
        break;
    default:
        globalDebugger->printf( "[unknown state] " );
        break;
    }
    if ( hostBufferSize )
    {
        globalDebugger->printf( "host = %d/%d B, ", streamInfo.dataSize, hostBufferSize );
        globalDebugger->printf( "hw = %d/%d (%d) B, ", streamInfo.auxDataSize,
                   hardwareBufferSize, outputBufferCap ) ;
    } else {
        globalDebugger->printf( "hw = %d/%d B, ", streamInfo.dataSize, hardwareBufferSize ) ;
    }
    globalDebugger->printf( "count = %d", streamInfo.frameCounter ) ;
    if ( streamInfo.overflow ) {
        globalDebugger->printf( " [overflow]" ) ;
    }
    else if( streamInfo.underflow )
    {
        globalDebugger->printf( " [underflow]" ) ;
    }
    globalDebugger->printf("\n") ;
}

CaError AsiHpiStreamComponent::SetupBuffers   (
          uint32_t      pollingInterval       ,
          unsigned long framesPerPaHostBuffer ,
          CaTime        suggestedLatency      )
{
  CaError          result = NoError                                          ;
  AsiHpiStreamInfo streamInfo                                                ;
  unsigned long    hpiBufferSize    = 0                                      ;
  unsigned long    paHostBufferSize = 0                                      ;
  ////////////////////////////////////////////////////////////////////////////
  CA_ENSURE_ ( GetStreamInfo ( &streamInfo ) )                               ;
  hardwareBufferSize = streamInfo . bufferSize                               ;
  hpiBufferSize      = streamInfo . bufferSize                               ;
  /* Check if BBM (background bus mastering) is to be enabled               */
  if ( CA_ASIHPI_USE_BBM_ )                                                  {
    uint32_t  bbmBufferSize = 0, preLatencyBufferSize = 0;
    hpi_err_t hpiError = 0;
    CaTime    pollingOverhead;
    /* Check overhead of Pa_Sleep() call (minimum sleep duration in ms -> OS dependent) */
    pollingOverhead = Timer::Time() ;
    Timer :: Sleep ( 0 )  ;
    pollingOverhead = 1000*( Timer::Time() - pollingOverhead) ;
    if ( NULL != globalDebugger ) {
      globalDebugger->printf( "polling overhead = %f ms (length of 0-second sleep)\n", pollingOverhead ) ;
    } ;
        /* Obtain minimum recommended size for host buffer (in bytes) */
        CA_ASIHPI_UNLESS_( HPI_StreamEstimateBufferSize( &hpiFormat,
                           pollingInterval + (uint32_t)ceil( pollingOverhead ),
                           &bbmBufferSize ), UnanticipatedHostError ) ;
        /* BBM places more stringent requirements on buffer size (see description */
        /* of HPI_StreamEstimateBufferSize in HPI API document) */
        bbmBufferSize *= 3;
        /* Make sure the BBM buffer contains multiple PA host buffers */
        if( bbmBufferSize < 3 * bytesPerFrame * framesPerPaHostBuffer )
            bbmBufferSize = 3 * bytesPerFrame * framesPerPaHostBuffer;
        /* Try to honor latency suggested by user by growing buffer (no decrease possible) */
        if( suggestedLatency > 0.0 )
        {
            CaTime bufferDuration = ((CaTime)bbmBufferSize) / bytesPerFrame
                                    / hpiFormat.dwSampleRate;
            /* Don't decrease buffer */
            if( bufferDuration < suggestedLatency )
            {
                /* Save old buffer size, to be retried if new size proves too big */
                preLatencyBufferSize = bbmBufferSize;
                bbmBufferSize = (uint32_t)ceil( suggestedLatency * bytesPerFrame
                                            * hpiFormat.dwSampleRate );
            }
        }
        /* Choose closest memory block boundary (HPI API document states that
        "a buffer size of Nx4096 - 20 makes the best use of memory"
        (under the entry for HPI_StreamEstimateBufferSize)) */
        bbmBufferSize = ((uint32_t)ceil((bbmBufferSize + 20)/4096.0))*4096 - 20;
        hostBufferSize = bbmBufferSize;
        /* Allocate BBM host buffer (this enables bus mastering transfers in background) */
        if ( hpiDevice->streamIsOutput )
            hpiError = HPI_OutStreamHostBufferAllocate( NULL,
                       hpiStream,
                       bbmBufferSize );
        else
            hpiError = HPI_InStreamHostBufferAllocate( NULL,
                       hpiStream,
                       bbmBufferSize );
        if ( hpiError ) {
            /* Indicate that BBM is disabled */
            hostBufferSize = 0;
            /* Retry with smaller buffer size (transfers will still work, but not via BBM) */
            if( hpiError == HPI_ERROR_INVALID_DATASIZE )
            {
                /* Retry BBM allocation with smaller size if requested latency proved too big */
                if( preLatencyBufferSize > 0 )
                {
                    if ( NULL != globalDebugger ) {
                      globalDebugger->printf("Retrying BBM allocation with smaller size (%d vs. %d bytes)\n",
                               preLatencyBufferSize, bbmBufferSize) ;
                    } ;
                    bbmBufferSize = preLatencyBufferSize;
                    if( hpiDevice->streamIsOutput )
                        hpiError = HPI_OutStreamHostBufferAllocate( NULL,
                                   hpiStream,
                                   bbmBufferSize );
                    else
                        hpiError = HPI_InStreamHostBufferAllocate( NULL,
                                   hpiStream,
                                   bbmBufferSize );
                    /* Another round of error checking */
                    if ( hpiError ) {
                        CA_ASIHPI_REPORT_ERROR_( hpiError );
                        /* No escapes this time */
                        if( hpiError == HPI_ERROR_INVALID_DATASIZE )
                        {
                            result = BufferTooBig;
                            goto error;
                        }
                        else if( hpiError != HPI_ERROR_INVALID_OPERATION )
                        {
                            result = UnanticipatedHostError;
                            goto error;
                        }
                    } else {
                        hostBufferSize = bbmBufferSize;
                        hpiBufferSize = bbmBufferSize;
                    }
                } else  {
                    result = BufferTooBig;
                    goto error;
                }
            }
            /* If BBM not supported, foreground transfers will be used, but not a show-stopper */
            /* Anything else is an error */
            else if (( hpiError != HPI_ERROR_INVALID_OPERATION ) &&
             ( hpiError != HPI_ERROR_INVALID_FUNC ))
            {
                CA_ASIHPI_REPORT_ERROR_( hpiError );
                result = UnanticipatedHostError;
                goto error;
            }
        } else{
            hpiBufferSize = bbmBufferSize;
        }
  }                                                                          ;
  /* Final check of buffer size                                             */
  paHostBufferSize = bytesPerFrame * framesPerPaHostBuffer                   ;
  if ( hpiBufferSize < 3*paHostBufferSize )                                  {
    result = BufferTooBig                                                    ;
    goto error                                                               ;
  }                                                                          ;
  /* Set cap on output buffer size, based on latency suggestions            */
  if ( hpiDevice->streamIsOutput )                                           {
    CaTime latency = suggestedLatency > 0.0 ? suggestedLatency               :
                                         hpiDevice->defaultHighOutputLatency ;
    outputBufferCap = (uint32_t)ceil( latency                                *
                                      bytesPerFrame                          *
                                      hpiFormat.dwSampleRate               ) ;
    /* The cap should not be too small, to prevent underflow                */
    if ( outputBufferCap < 4 * paHostBufferSize )                            {
      outputBufferCap = 4 * paHostBufferSize                                 ;
    }                                                                        ;
  } else                                                                     {
    outputBufferCap = 0                                                      ;
  }                                                                          ;
  /* Temp buffer size should be multiple of PA host buffer size (or 1x, if using fixed blocks) */
  tempBufferSize = paHostBufferSize                                          ;
  /* Allocate temp buffer                                                   */
  CA_UNLESS_ ( tempBuffer = (uint8_t *)Allocator::allocate(tempBufferSize  ) ,
               InsufficientMemory                                          ) ;
error                                                                        :
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

AsiHpiHostApi:: AsiHpiHostApi (void)
              : HostApi       (    )
{
}

AsiHpiHostApi::~AsiHpiHostApi(void)
{
}

HostApi::Encoding AsiHpiHostApi::encoding(void) const
{
  return UTF8 ;
}

bool AsiHpiHostApi::hasDuplex(void) const
{
  return true ;
}

CaError AsiHpiHostApi ::           Open              (
          Stream                ** s                 ,
          const StreamParameters * inputParameters   ,
          const StreamParameters * outputParameters  ,
          double                   sampleRate        ,
          CaUint32                 framesPerCallback ,
          CaStreamFlags            streamFlags       ,
          Conduit                * conduit           )
{
  CaError result = NoError ;
  AsiHpiStream *stream = NULL;
  unsigned long framesPerHostBuffer = framesPerCallback;
  int inputChannelCount = 0, outputChannelCount = 0;
  CaSampleFormat inputSampleFormat = cafNothing, outputSampleFormat = cafNothing;
  CaSampleFormat hostInputSampleFormat = cafNothing, hostOutputSampleFormat = cafNothing;
  CaTime maxSuggestedLatency = 0.0;
  /* Validate platform-specific flags -> none expected for HPI */
    if( (streamFlags & csfPlatformSpecificFlags) != 0 )
        return InvalidFlag; /* unexpected platform-specific flag */
    /* Create blank stream structure */
    CA_UNLESS_( stream = new AsiHpiStream(),InsufficientMemory );
    /* If the number of frames per buffer is unspecified, we have to come up with one. */
    if( framesPerHostBuffer == Stream::FramesPerBufferUnspecified )
    {
        if( inputParameters )
            maxSuggestedLatency = inputParameters->suggestedLatency;
        if( outputParameters && (outputParameters->suggestedLatency > maxSuggestedLatency) )
            maxSuggestedLatency = outputParameters->suggestedLatency;
        /* Use suggested latency if available */
        if( maxSuggestedLatency > 0.0 )
            framesPerHostBuffer = (unsigned long)ceil( maxSuggestedLatency * sampleRate );
        else
            /* AudioScience cards like BIG buffers by default */
            framesPerHostBuffer = 4096;
    }
    /* Lower bounds on host buffer size, due to polling and HPI constraints */
    if( 1000.0*framesPerHostBuffer/sampleRate < CA_ASIHPI_MIN_POLLING_INTERVAL_ )
        framesPerHostBuffer = (unsigned long)ceil( sampleRate * CA_ASIHPI_MIN_POLLING_INTERVAL_ / 1000.0 );
    /*    if( framesPerHostBuffer < PA_ASIHPI_MIN_FRAMES_ )
            framesPerHostBuffer = PA_ASIHPI_MIN_FRAMES_; */
    /* Efficient if host buffer size is integer multiple of user buffer size */
    if( framesPerCallback > 0 )
        framesPerHostBuffer = (unsigned long)ceil( (double)framesPerHostBuffer / framesPerCallback ) * framesPerCallback ;
    /* Buffer should always be a multiple of 4 bytes to facilitate 32-bit PCI transfers.
     By keeping the frames a multiple of 4, this is ensured even for 8-bit mono sound. */
    framesPerHostBuffer = (framesPerHostBuffer / 4) * 4;
    /* Polling is based on time length (in milliseconds) of user-requested block size */
    stream->pollingInterval = (uint32_t)ceil( 1000.0*framesPerHostBuffer/sampleRate );
    /* Open underlying streams, check formats and allocate buffers */
    if( inputParameters )
    {
        /* Create blank stream component structure */
        CA_UNLESS_( stream->input = new AsiHpiStreamComponent(),InsufficientMemory );
        #warning AsiHpiStreamComponent need to add an initializer
//        memset( stream->input, 0, sizeof(PaAsiHpiStreamComponent) );
        /* Create/validate format */
        CA_ENSURE_ ( CreateFormat( inputParameters, sampleRate,
                                   &stream->input->hpiDevice, &stream->input->hpiFormat ) ) ;
        /* Open stream and set format */
        CA_ENSURE_( OpenInput( stream->input->hpiDevice, &stream->input->hpiFormat,
                                        &stream->input->hpiStream ) );
        inputChannelCount = inputParameters->channelCount;
        inputSampleFormat = inputParameters->sampleFormat;
        hostInputSampleFormat = HpiToCaFormat( stream->input->hpiFormat.wFormat );
        stream->input->bytesPerFrame = inputChannelCount * Core::SampleSize(hostInputSampleFormat) ;
        assert( stream->input->bytesPerFrame > 0 );
        /* Allocate host and temp buffers of appropriate size */
        CA_ENSURE_( stream->input->SetupBuffers(stream->pollingInterval,
                                           framesPerHostBuffer, inputParameters->suggestedLatency ) );
    }
    if( outputParameters )
    {
        /* Create blank stream component structure */
        CA_UNLESS_( stream->output = new AsiHpiStreamComponent(),InsufficientMemory ) ;
        #warning AsiHpiStreamComponent need to add an initializer
//        memset( stream->output, 0, sizeof(PaAsiHpiStreamComponent) );
        /* Create/validate format */
        CA_ENSURE_( CreateFormat( outputParameters, sampleRate,
                                           &stream->output->hpiDevice, &stream->output->hpiFormat ) );
        /* Open stream and check format */
        CA_ENSURE_( OpenOutput( stream->output->hpiDevice,
                                         &stream->output->hpiFormat,
                                         &stream->output->hpiStream ) );
        outputChannelCount = outputParameters->channelCount;
        outputSampleFormat = outputParameters->sampleFormat;
        hostOutputSampleFormat = HpiToCaFormat( stream->output->hpiFormat.wFormat );
        stream->output->bytesPerFrame = outputChannelCount * Core::SampleSize( hostOutputSampleFormat ) ;
        /* Allocate host and temp buffers of appropriate size */
        CA_ENSURE_( stream->output->SetupBuffers( stream->pollingInterval,
                                           framesPerHostBuffer, outputParameters->suggestedLatency ) );
    }
    /* Determine maximum frames per host buffer (least common denominator of input/output) */
    if( inputParameters && outputParameters )
    {
        stream->maxFramesPerHostBuffer = CA_MIN( stream->input->tempBufferSize / stream->input->bytesPerFrame,
                                         stream->output->tempBufferSize / stream->output->bytesPerFrame );
    }
    else
    {
        stream->maxFramesPerHostBuffer = inputParameters ? stream->input->tempBufferSize / stream->input->bytesPerFrame
                                         : stream->output->tempBufferSize / stream->output->bytesPerFrame;
    }
    assert( stream->maxFramesPerHostBuffer > 0 );
    /* Initialize various other stream parameters */
    stream->neverDropInput = streamFlags & csfNeverDropInput;
    stream->state = AsiHpiStoppedState;

    /* Initialize either callback or blocking interface */
    stream->conduit = conduit ;
    if ( NULL != conduit ) {
        stream->callbackMode = 1;
    } else {
        /* Pre-allocate non-interleaved user buffer pointers for blocking interface */
        stream->blockingUserBufferCopy = (void **)Allocator::allocate( sizeof(void *) * CA_MAX( inputChannelCount, outputChannelCount  ) ) ;
        CA_UNLESS_( stream->blockingUserBufferCopy , InsufficientMemory ) ;
        stream->callbackMode = 0 ;
    }
    stream->cpuLoadMeasurer.Initialize(sampleRate) ;
    /* Following pa_linux_alsa's lead, we operate with fixed host buffer size by default, */
    /* since other modes will invariably lead to block adaption (maybe Bounded better?) */
    CA_ENSURE_( stream->bufferProcessor.Initialize(
                inputChannelCount, inputSampleFormat, hostInputSampleFormat,
                outputChannelCount, outputSampleFormat, hostOutputSampleFormat,
                sampleRate, streamFlags,
                framesPerCallback, framesPerHostBuffer, cabFixedHostBufferSize,
                conduit) );
    stream->structVersion = 1;
    stream->sampleRate = sampleRate;
    /* Determine input latency from buffer processor and buffer sizes */
    if( stream->input )
    {
        CaTime bufferDuration = ( stream->input->hostBufferSize + stream->input->hardwareBufferSize )
                                / sampleRate / stream->input->bytesPerFrame;
        stream->inputLatency =
            bufferDuration +
            ((CaTime) stream->bufferProcessor.InputLatencyFrames() -
                stream->maxFramesPerHostBuffer) / sampleRate ;
        assert( stream->inputLatency > 0.0 );
    }
    /* Determine output latency from buffer processor and buffer sizes */
    if( stream->output )
    {
        CaTime bufferDuration = ( stream->output->hostBufferSize + stream->output->hardwareBufferSize )
                                / sampleRate / stream->output->bytesPerFrame;
        /* Take buffer size cap into account (see PaAsiHpi_WaitForFrames) */
        if( !stream->input && (stream->output->outputBufferCap > 0) )
        {
            bufferDuration = CA_MIN( bufferDuration,
                                     stream->output->outputBufferCap / sampleRate / stream->output->bytesPerFrame ) ;
        }
        stream->outputLatency =
            bufferDuration +
            ((CaTime)stream->bufferProcessor.OutputLatencyFrames() -
                stream->maxFramesPerHostBuffer) / sampleRate;
        assert( stream->outputLatency > 0.0 );
    }
    /* Report stream info, for debugging purposes */
    stream->StreamDump();
    /* Save initialized stream to PA stream list */
    *s = (Stream*)stream;
    return result;
error:
  stream->Close() ;
  return result                                                              ;
}

void AsiHpiHostApi::Terminate(void)
{
  int      i                                                                 ;
  CaError  result           = NoError                                        ;
  uint16_t lastAdapterIndex = HPI_MAX_ADAPTERS                               ;
  /* Get rid of HPI-specific structures                                     */
  ////////////////////////////////////////////////////////////////////////////
  /* Iterate through device list and close adapters                         */
  for ( i = 0 ; i < info . deviceCount ; ++i )                               {
    AsiHpiDeviceInfo * hpiDevice = (AsiHpiDeviceInfo *) deviceInfos [ i ]    ;
    /* Close adapter only if it differs from previous one                   */
    if ( hpiDevice->adapterIndex != lastAdapterIndex )                       {
      /* Ignore errors (report only during debugging)                       */
      CA_ASIHPI_UNLESS_ ( HPI_AdapterClose                                   (
                            NULL                                             ,
                            hpiDevice->adapterIndex                        ) ,
                          NoError                                          ) ;
      lastAdapterIndex = hpiDevice->adapterIndex                             ;
    }                                                                        ;
  }                                                                          ;
  /* Finally dismantle HPI subsystem                                        */
  HPI_SubSysFree ( NULL )                                                    ;
  CaEraseAllocation                                                          ;
error                                                                        :
  return                                                                     ;
}

CaError AsiHpiHostApi ::            isSupported      (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double                    sampleRate       )
{
  CaError            result    = NoError                                     ;
  AsiHpiDeviceInfo * hpiDevice = NULL                                        ;
  struct hpi_format  hpiFormat                                               ;
  hpi_handle_t       hpiStream                                               ;
  /* Input stream                                                           */
  if ( NULL != inputParameters )                                             {
    if ( NULL != globalDebugger )                                            {
      globalDebugger -> printf                                               (
        "%s: Checking input params: dev=%d, sr=%d, chans=%d, fmt=%d\n"       ,
        __FUNCTION__                                                         ,
        inputParameters -> device                                            ,
        (int)sampleRate                                                      ,
        inputParameters -> channelCount                                      ,
        inputParameters -> sampleFormat                                    ) ;
    }                                                                        ;
    /* Create and validate format */
    CA_ENSURE_( CreateFormat ( inputParameters                               ,
                               sampleRate                                    ,
                               &hpiDevice                                    ,
                               &hpiFormat                                ) ) ;
    /* Open stream to further check format                                  */
    CA_ENSURE_( OpenInput ( hpiDevice , &hpiFormat , &hpiStream )          ) ;
    /* Close stream again                                                   */
    CA_ASIHPI_UNLESS_( HPI_InStreamClose( NULL, hpiStream ),NoError )        ;
  }                                                                          ;
  /* Output stream                                                          */
  if ( NULL != outputParameters )                                            {
    if ( NULL != globalDebugger )                                            {
      globalDebugger -> printf                                               (
        "%s: Checking output params: dev=%d, sr=%d, chans=%d, fmt=%d\n"      ,
        __FUNCTION__                                                         ,
        outputParameters -> device                                           ,
        (int)sampleRate                                                      ,
        outputParameters -> channelCount                                     ,
        outputParameters -> sampleFormat                                   ) ;
    }                                                                        ;
    /* Create and validate format                                           */
    CA_ENSURE_( CreateFormat( outputParameters                               ,
                              sampleRate                                     ,
                              &hpiDevice                                     ,
                              &hpiFormat                                 ) ) ;
    /* Open stream to further check format                                  */
    CA_ENSURE_( OpenOutput( hpiDevice, &hpiFormat, &hpiStream )            ) ;
    /* Close stream again                                                   */
    CA_ASIHPI_UNLESS_ ( HPI_OutStreamClose(NULL,hpiStream),NoError         ) ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError AsiHpiHostApi ::            CreateFormat (
          const StreamParameters *  parameters   ,
          double                    sampleRate   ,
          AsiHpiDeviceInfo       ** hpiDevice    ,
          struct hpi_format      *  hpiFormat    )
{
  int            maxChannelCount  = 0                                        ;
  CaSampleFormat hostSampleFormat = cafNothing                               ;
  hpi_err_t      hpiError         = 0                                        ;
  /* Unless alternate device specification is supported, reject the use of
     CaUseHostApiSpecificDeviceSpecification                                */
  if ( parameters->device == CaUseHostApiSpecificDeviceSpecification )       {
    return InvalidDevice                                                     ;
  } else                                                                     {
    assert( parameters->device < info.deviceCount )                          ;
    *hpiDevice = (AsiHpiDeviceInfo *)deviceInfos[ parameters->device ]       ;
  }                                                                          ;
  /* Validate streamInfo - this implementation doesn't use custom stream info */
  if ( NULL != parameters->streamInfo )                       {
    return IncompatibleStreamInfo                             ;
  }                                                                          ;
  /* Check that device can support channel count                            */
  if ( (*hpiDevice)->streamIsOutput )                                        {
    maxChannelCount = (*hpiDevice)->maxOutputChannels                        ;
  } else                                                                     {
    maxChannelCount = (*hpiDevice)->maxInputChannels                         ;
  }                                                                          ;
  if ( (maxChannelCount==0) || (parameters->channelCount>maxChannelCount) )  {
    return InvalidChannelCount                                               ;
  }                                                                          ;
  /* All standard sample formats are supported by the buffer adapter,
     and this implementation doesn't support any custom sample formats      */
  if ( parameters->sampleFormat & cafCustomFormat )                          {
    return SampleFormatNotSupported                                          ;
  }                                                                          ;
  /* Switch to closest HPI native format                                    */
  hostSampleFormat = ClosestFormat ( CA_ASIHPI_AVAILABLE_FORMATS_            ,
                                     parameters->sampleFormat              ) ;
  /* Setup format + info objects                                            */
  hpiError = HPI_FormatCreate                                                (
               hpiFormat                                                     ,
               (uint16_t)parameters->channelCount                            ,
               CaToHpiFormat(hostSampleFormat)                               ,
               (uint32_t)sampleRate                                          ,
               0                                                             ,
               0                                                           ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( hpiError )                                                            {
    CA_ASIHPI_REPORT_ERROR_ ( hpiError )                                     ;
    switch ( hpiError )                                                      {
      case   HPI_ERROR_INVALID_FORMAT                                        :
      return SampleFormatNotSupported                                        ;
      case   HPI_ERROR_INVALID_SAMPLERATE                                    :
      case   HPI_ERROR_INCOMPATIBLE_SAMPLERATE                               :
      return InvalidSampleRate                                               ;
      case   HPI_ERROR_INVALID_CHANNELS                                      :
      return InvalidChannelCount                                             ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

CaError AsiHpiHostApi ::            OpenInput (
          const AsiHpiDeviceInfo  * hpiDevice ,
          const struct hpi_format * hpiFormat ,
          hpi_handle_t            * hpiStream )
{
  CaError   result   = NoError                                               ;
  hpi_err_t hpiError = 0                                                     ;
  /* Catch misplaced output devices, as they typically have 0 input channels */
  CA_UNLESS_ ( !hpiDevice->streamIsOutput , InvalidChannelCount )            ;
  /* Try to open input stream                                               */
  CA_ASIHPI_UNLESS_ ( HPI_InStreamOpen                                       (
                        NULL                                                 ,
                        hpiDevice -> adapterIndex                            ,
                        hpiDevice -> streamIndex                             ,
                        hpiStream                                          ) ,
                      DeviceUnavailable                                    ) ;
  /* Set input format (checking it in the process)                          */
  /* Could also use HPI_InStreamQueryFormat, but this economizes the process */
  hpiError = HPI_InStreamSetFormat                                           (
               NULL                                                          ,
               *hpiStream                                                    ,
               (struct hpi_format*)hpiFormat                               ) ;
  if ( hpiError )                                                            {
    CA_ASIHPI_REPORT_ERROR_ ( hpiError )                                     ;
    CA_ASIHPI_UNLESS_ ( HPI_InStreamClose(NULL,*hpiStream),NoError )         ;
    switch ( hpiError )                                                      {
      case   HPI_ERROR_INVALID_FORMAT                                        :
      return SampleFormatNotSupported                                        ;
      case   HPI_ERROR_INVALID_SAMPLERATE                                    :
      case   HPI_ERROR_INCOMPATIBLE_SAMPLERATE                               :
      return InvalidSampleRate                                               ;
      case   HPI_ERROR_INVALID_CHANNELS                                      :
      return InvalidChannelCount                                             ;
      default                                                                :
      return InvalidDevice                                                   ;
    }                                                                        ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError AsiHpiHostApi ::            OpenOutput (
          const AsiHpiDeviceInfo  * hpiDevice  ,
          const struct hpi_format * hpiFormat  ,
          hpi_handle_t            * hpiStream  )
{
  CaError   result   = NoError                                               ;
  hpi_err_t hpiError = 0                                                     ;
  /* Catch misplaced input devices, as they typically have 0 output channels */
  CA_UNLESS_ ( hpiDevice->streamIsOutput , InvalidChannelCount )             ;
  /* Try to open output stream                                              */
  CA_ASIHPI_UNLESS_ ( HPI_OutStreamOpen                                      (
                        NULL                                                 ,
                        hpiDevice->adapterIndex                              ,
                        hpiDevice->streamIndex                               ,
                        hpiStream                                          ) ,
                      DeviceUnavailable                                    ) ;
  /* Check output format (format is set on first write to output stream)    */
  hpiError = HPI_OutStreamQueryFormat                                        (
               NULL                                                          ,
               *hpiStream                                                    ,
               (struct hpi_format*)hpiFormat                               ) ;
  if ( hpiError )                                                            {
    CA_ASIHPI_REPORT_ERROR_ ( hpiError )                                     ;
    CA_ASIHPI_UNLESS_ ( HPI_OutStreamClose(NULL,*hpiStream) , NoError      ) ;
    switch ( hpiError )                                                      {
      case   HPI_ERROR_INVALID_FORMAT                                        :
      return SampleFormatNotSupported                                        ;
      case   HPI_ERROR_INVALID_SAMPLERATE                                    :
      case   HPI_ERROR_INCOMPATIBLE_SAMPLERATE                               :
      return InvalidSampleRate                                               ;
      case   HPI_ERROR_INVALID_CHANNELS                                      :
      return InvalidChannelCount                                             ;
      default                                                                :
      return InvalidDevice                                                   ;
    }                                                                        ;
  }                                                                          ;
error                                                                        :
  return result                                                              ;
}

CaError AsiHpiHostApi::BuildDeviceList(void)
{
  CaError result = NoError                                                   ;
  HostApiInfo *baseApiInfo = &info ;
  AsiHpiDeviceInfo *hpiDeviceList;
  int numAdapters;
  hpi_err_t hpiError = 0;
  int i, j, deviceCount = 0, deviceIndex = 0;
  /* Errors not considered critical here (subsystem may report 0 devices), but report them */
  /* in debug mode. */
    CA_ASIHPI_UNLESS_( HPI_SubSysGetNumAdapters( NULL, &numAdapters), NoError );
    for( i=0; i < numAdapters; ++i )
    {
        uint16_t inStreams, outStreams;
        uint16_t version;
        uint32_t serial;
        uint16_t type;
        uint32_t idx;

        hpiError = HPI_SubSysGetAdapter(NULL, i, &idx, &type);
        if (hpiError)
            continue;

        /* Try to open adapter */
        hpiError = HPI_AdapterOpen( NULL, idx );
        /* Report error and skip to next device on failure */
        if( hpiError )
        {
            CA_ASIHPI_REPORT_ERROR_( hpiError );
            continue;
        }
        hpiError = HPI_AdapterGetInfo( NULL, idx, &outStreams, &inStreams,
                    &version, &serial, &type );
        /* Skip to next device on failure */
        if( hpiError )
        {
            CA_ASIHPI_REPORT_ERROR_( hpiError );
            continue;
        }
        else
        {
            /* Assign default devices if available and increment device count */
            if( (baseApiInfo->defaultInputDevice == CaNoDevice) && (inStreams > 0) )
                baseApiInfo->defaultInputDevice = deviceCount;
            deviceCount += inStreams;
            if( (baseApiInfo->defaultOutputDevice == CaNoDevice) && (outStreams > 0) )
                baseApiInfo->defaultOutputDevice = deviceCount;
            deviceCount += outStreams;
        }
    }
    /* Register any discovered devices */
    if( deviceCount > 0 )
    {
        /* Memory allocation */
        deviceInfos = new DeviceInfo * [deviceCount] ;
        CA_UNLESS_( deviceInfos , InsufficientMemory ) ;
        /* Allocate all device info structs in a contiguous block */
        hpiDeviceList = new AsiHpiDeviceInfo [ deviceCount ] ;
        CA_UNLESS_( hpiDeviceList , InsufficientMemory );
        /* Now query devices again for information */
        for( i=0; i < numAdapters; ++i )
        {
            uint16_t inStreams, outStreams;
            uint16_t version;
            uint32_t serial;
            uint16_t type;
            uint32_t idx;

            hpiError = HPI_SubSysGetAdapter( NULL, i, &idx, &type );
            if (hpiError)
                continue;

            /* Assume adapter is still open from previous round */
            hpiError = HPI_AdapterGetInfo( NULL, idx,
                                           &outStreams, &inStreams, &version, &serial, &type );
            /* Report error and skip to next device on failure */
            if( hpiError )
            {
                CA_ASIHPI_REPORT_ERROR_( hpiError );
                continue;
            }
            else
            {
                if ( NULL != globalDebugger ) {
                globalDebugger->printf( "Found HPI Adapter ID=%4X Idx=%d #In=%d #Out=%d S/N=%d HWver=%c%d DSPver=%03d\n",
                           type, idx, inStreams, outStreams, serial,
                           ((version>>3)&0xf)+'A',                  /* Hw version major */
                           version&0x7,                             /* Hw version minor */
                           ((version>>13)*100)+((version>>7)&0x3f)  /* DSP code version */
                         );
                } ;
            }
            /* First add all input streams as devices */
            for( j=0; j < inStreams; ++j )
            {
                AsiHpiDeviceInfo * hpiDevice       = &hpiDeviceList[deviceIndex] ;
                DeviceInfo       * baseDeviceInfo  = (DeviceInfo *)hpiDevice;
                char srcName[72];
                char *deviceName;
                #warning hpiDevice requires a initialization
                /* Set implementation-specific device details */
                hpiDevice->adapterIndex = idx;
                hpiDevice->adapterType = type;
                hpiDevice->adapterVersion = version;
                hpiDevice->adapterSerialNumber = serial;
                hpiDevice->streamIndex = j;
                hpiDevice->streamIsOutput = 0;
                /* Set common PortAudio device stats */
                baseDeviceInfo->structVersion = 2;
                /* Make sure name string is owned by API info structure */
                sprintf( srcName,
                         "Adapter %d (%4X) - Input Stream %d", i+1, type, j+1 );
                CA_UNLESS_( deviceName = (char *)allocations->alloc( strlen(srcName) + 1 ),InsufficientMemory ) ;
                strcpy( deviceName, srcName );
                baseDeviceInfo->name = deviceName;
                baseDeviceInfo->hostApi = hostApiIndex;
                baseDeviceInfo->hostType = ASIHPI ;
                baseDeviceInfo->maxInputChannels = HPI_MAX_CHANNELS;
                baseDeviceInfo->maxOutputChannels = 0;
                /* Default latency values for interactive performance */
                baseDeviceInfo->defaultLowInputLatency = 0.01;
                baseDeviceInfo->defaultLowOutputLatency = -1.0;
                /* Default latency values for robust non-interactive applications (eg. playing sound files) */
                baseDeviceInfo->defaultHighInputLatency = 0.2;
                baseDeviceInfo->defaultHighOutputLatency = -1.0;
                /* HPI interface can actually handle any sampling rate to 1 Hz accuracy,
                * so this default is as good as any */
                baseDeviceInfo->defaultSampleRate = 44100;
                /* Store device in global PortAudio list */
                deviceInfos[deviceIndex++] = (DeviceInfo *) hpiDevice;
            }
            /* Now add all output streams as devices (I know, the repetition is painful) */
            for( j=0; j < outStreams; ++j )
            {
                AsiHpiDeviceInfo * hpiDevice      = &hpiDeviceList[deviceIndex];
                DeviceInfo       * baseDeviceInfo = (DeviceInfo *)hpiDevice;
                char srcName[72];
                char *deviceName;
                #warning hpiDevice requires a initialization
                /* Set implementation-specific device details */
                hpiDevice->adapterIndex = idx;
                hpiDevice->adapterType = type;
                hpiDevice->adapterVersion = version;
                hpiDevice->adapterSerialNumber = serial;
                hpiDevice->streamIndex = j;
                hpiDevice->streamIsOutput = 1;
                /* Set common PortAudio device stats */
                baseDeviceInfo->structVersion = 2;
                /* Make sure name string is owned by API info structure */
                sprintf( srcName,
                         "Adapter %d (%4X) - Output Stream %d", i+1, type, j+1 );
                CA_UNLESS_( deviceName = (char *)allocations->alloc(strlen(srcName) + 1 ),InsufficientMemory );
                strcpy( deviceName, srcName );
                baseDeviceInfo->name = deviceName;
                baseDeviceInfo->hostApi = hostApiIndex;
                baseDeviceInfo->maxInputChannels = 0;
                baseDeviceInfo->maxOutputChannels = HPI_MAX_CHANNELS;
                /* Default latency values for interactive performance. */
                baseDeviceInfo->defaultLowInputLatency = -1.0;
                baseDeviceInfo->defaultLowOutputLatency = 0.01;
                /* Default latency values for robust non-interactive applications (eg. playing sound files). */
                baseDeviceInfo->defaultHighInputLatency = -1.0;
                baseDeviceInfo->defaultHighOutputLatency = 0.2;
                /* HPI interface can actually handle any sampling rate to 1 Hz accuracy,
                * so this default is as good as any */
                baseDeviceInfo->defaultSampleRate = 44100;
                /* Store device in global PortAudio list */
                deviceInfos[deviceIndex++] = (DeviceInfo *) hpiDevice;
            }
        }
    }
    /* Finally acknowledge checked devices */
    baseApiInfo->deviceCount = deviceIndex ;
error:
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
