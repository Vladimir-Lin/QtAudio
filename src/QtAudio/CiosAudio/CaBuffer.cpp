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

#include "CiosAudioPrivate.hpp"

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

#define CA_FRAMES_PER_TEMP_BUFFER_WHEN_HOST_BUFFER_SIZE_IS_UNKNOWN_ 1024
#define CA_MIN_( a, b ) ( ( (a) < (b) ) ? (a) : (b) )
#define CA_MAX_( a, b ) ( ( (a) > (b) ) ? (a) : (b) )

BufferProcessor:: BufferProcessor(void)
{
  framesPerUserBuffer                 = 0                        ;
  framesPerHostBuffer                 = 0                        ;
  hostBufferSizeMode                  = cabUnknownHostBufferSize ;
  useNonAdaptingProcess               = 0                        ;
  userOutputSampleFormatIsEqualToHost = 0                        ;
  userInputSampleFormatIsEqualToHost  = 0                        ;
  framesPerTempBuffer                 = 0                        ;
  inputChannelCount                   = 0                        ;
  bytesPerHostInputSample             = 0                        ;
  bytesPerUserInputSample             = 0                        ;
  userInputIsInterleaved              = 0                        ;
  inputConverter                      = NULL                     ;
  inputZeroer                         = NULL                     ;
  outputChannelCount                  = 0                        ;
  bytesPerHostOutputSample            = 0                        ;
  bytesPerUserOutputSample            = 0                        ;
  userOutputIsInterleaved             = 0                        ;
  outputConverter                     = NULL                     ;
  outputZeroer                        = NULL                     ;
  initialFramesInTempInputBuffer      = 0                        ;
  initialFramesInTempOutputBuffer     = 0                        ;
  tempInputBuffer                     = NULL                     ;
  tempInputBufferPtrs                 = NULL                     ;
  framesInTempInputBuffer             = 0                        ;
  tempOutputBuffer                    = NULL                     ;
  tempOutputBufferPtrs                = NULL                     ;
  framesInTempOutputBuffer            = 0                        ;
  conduit                             = NULL                     ;
  hostInputIsInterleaved              = 0                        ;
  hostInputFrameCount  [ 0 ]          = 0                        ;
  hostInputFrameCount  [ 1 ]          = 0                        ;
  hostInputChannels    [ 0 ]          = NULL                     ;
  hostInputChannels    [ 1 ]          = NULL                     ;
  hostOutputIsInterleaved             = 0                        ;
  hostOutputFrameCount [ 0 ]          = 0                        ;
  hostOutputFrameCount [ 1 ]          = 0                        ;
  hostOutputChannels   [ 0 ]          = NULL                     ;
  hostOutputChannels   [ 1 ]          = NULL                     ;
  samplePeriod                        = 0                        ;
}

BufferProcessor::~BufferProcessor(void)
{
}

CaUint32 BufferProcessor::FrameShift(CaUint32 M,CaUint32 N)
{
  CaUint32 result = 0                     ;
  CaUint32 i                              ;
  CaUint32 lcm                            ;
  if ( M <= 0 ) return result             ;
  if ( N <= 0 ) return result             ;
  lcm = LCM ( M, N )                      ;
  for ( i = M; i < lcm; i += M )          {
    result = CA_MAX_( result, ( i % N ) ) ;
  }                                       ;
  return result;
}

CaError BufferProcessor::Initialize                   (
          int                  InputChannelCount      ,
          CaSampleFormat       UserInputSampleFormat  ,
          CaSampleFormat       HostInputSampleFormat  ,
          int                  OutputChannelCount     ,
          CaSampleFormat       UserOutputSampleFormat ,
          CaSampleFormat       HostOutputSampleFormat ,
          double               SampleRate             ,
          CaStreamFlags        StreamFlags            ,
          CaUint32             FramesPerUserBuffer    ,
          CaUint32             FramesPerHostBuffer    ,
          CaHostBufferSizeMode HostBufferSizeMode     ,
          Conduit            * CONDUIT                )
{
  CaError       result = NoError                                             ;
  CaError       bytesPerSample                                               ;
  CaUint32      tempInputBufferSize                                          ;
  CaUint32      tempOutputBufferSize                                         ;
  CaStreamFlags tempInputStreamFlags                                         ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != ( StreamFlags & csfNeverDropInput ) )                            {
    /* NeverDropInput is only valid for full-duplex callback streams         ,
     * with an unspecified number of frames per buffer.                     */
    if ( ! ( ( InputChannelCount > 0 ) && ( OutputChannelCount > 0 ) )     ) {
      return InvalidFlag                                                     ;
    }                                                                        ;
    if ( Stream::FramesPerBufferUnspecified != FramesPerUserBuffer         ) {
      return InvalidFlag                                                     ;
    }                                                                        ;
    if ( NULL == CONDUIT ) return InvalidFlag                                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  /* initialize buffer ptrs to zero so they can be freed if necessary in error */
  tempInputBuffer          = NULL                                            ;
  tempInputBufferPtrs      = NULL                                            ;
  tempOutputBuffer         = NULL                                            ;
  tempOutputBufferPtrs     = NULL                                            ;
  framesPerUserBuffer      = FramesPerUserBuffer                             ;
  framesPerHostBuffer      = FramesPerHostBuffer                             ;
  inputChannelCount        = InputChannelCount                               ;
  outputChannelCount       = OutputChannelCount                              ;
  hostBufferSizeMode       = HostBufferSizeMode                              ;
  hostInputChannels  [ 0 ] = NULL                                            ;
  hostInputChannels  [ 1 ] = NULL                                            ;
  hostOutputChannels [ 0 ] = NULL                                            ;
  hostOutputChannels [ 1 ] = NULL                                            ;
  if ( 0 == framesPerUserBuffer )                                            { /* streamCallback will accept any buffer size */
    useNonAdaptingProcess           = 1                                      ;
    initialFramesInTempInputBuffer  = 0                                      ;
    initialFramesInTempOutputBuffer = 0                                      ;
    if ( ( cabFixedHostBufferSize   == HostBufferSizeMode )                 ||
         ( cabBoundedHostBufferSize == HostBufferSizeMode )                ) {
      framesPerTempBuffer = FramesPerHostBuffer                              ;
    } else                                                                   { /* unknown host buffer size */
      framesPerTempBuffer = CA_FRAMES_PER_TEMP_BUFFER_WHEN_HOST_BUFFER_SIZE_IS_UNKNOWN_ ;
    }                                                                        ;
  } else                                                                     {
    framesPerTempBuffer = FramesPerUserBuffer                                ;
    if ( ( cabFixedHostBufferSize == HostBufferSizeMode   )                 &&
         ( ( framesPerHostBuffer % framesPerUserBuffer ) == 0 )            ) {
      useNonAdaptingProcess           = 1                                    ;
      initialFramesInTempInputBuffer  = 0                                    ;
      initialFramesInTempOutputBuffer = 0                                    ;
    } else                                                                   {
      useNonAdaptingProcess           = 0                                    ;
      if ( ( inputChannelCount > 0 ) && ( outputChannelCount > 0 )         ) {
        /* full duplex                                                      */
        if ( cabFixedHostBufferSize == HostBufferSizeMode )                  {
          CaUint32 frameShift                                                ;
          frameShift = FrameShift( FramesPerHostBuffer,FramesPerUserBuffer ) ;
          if ( FramesPerUserBuffer > FramesPerHostBuffer )                   {
            initialFramesInTempInputBuffer  = frameShift                     ;
            initialFramesInTempOutputBuffer = 0                              ;
          } else                                                             {
            initialFramesInTempInputBuffer  = 0                              ;
            initialFramesInTempOutputBuffer = frameShift                     ;
          }                                                                  ;
        }  else                                                              {
          /* variable host buffer size, add framesPerUserBuffer latency     */
          initialFramesInTempInputBuffer  = 0                                ;
          initialFramesInTempOutputBuffer = FramesPerUserBuffer              ;
        }                                                                    ;
      } else                                                                 {
        /* half duplex                                                      */
        initialFramesInTempInputBuffer  = 0                                  ;
        initialFramesInTempOutputBuffer = 0                                  ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  framesInTempInputBuffer  = initialFramesInTempInputBuffer                  ;
  framesInTempOutputBuffer = initialFramesInTempOutputBuffer                 ;
  ////////////////////////////////////////////////////////////////////////////
  if ( inputChannelCount > 0 )                                               {
    bytesPerSample = Core::SampleSize( HostInputSampleFormat )               ;
    if ( bytesPerSample > 0 )                                                {
      bytesPerHostInputSample = bytesPerSample                               ;
    }  else                                                                  {
      result = bytesPerSample                                                ;
      Terminate()                                                            ;
      return result                                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    bytesPerSample = Core::SampleSize( UserInputSampleFormat )               ;
    if ( bytesPerSample > 0 )                                                {
      bytesPerUserInputSample = bytesPerSample                               ;
    } else                                                                   {
      result = bytesPerSample                                                ;
      Terminate()                                                            ;
      return result                                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    /* Under the assumption that no ADC in existence delivers better than 24bits resolution,
       we disable dithering when host input format is paInt32 and user format is paInt24,
       since the host samples will just be padded with zeros anyway.        */
    tempInputStreamFlags = StreamFlags                                       ;
    if ( !  ( tempInputStreamFlags  & csfDitherOff )                        && /* dither is on */
            ( HostInputSampleFormat & cafInt32     )                        && /* host input format is int32 */
            ( UserInputSampleFormat & cafInt24     )                       ) { /* user requested format is int24 */
      tempInputStreamFlags = tempInputStreamFlags | csfDitherOff             ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    inputConverter = SelectConverter                                         (
                       HostInputSampleFormat                                 ,
                       UserInputSampleFormat                                 ,
                       tempInputStreamFlags                                ) ;
    inputZeroer    = SelectZeroCopier ( UserInputSampleFormat )              ;
    //////////////////////////////////////////////////////////////////////////
    userInputIsInterleaved = ( UserInputSampleFormat & cafNonInterleaved ) ? 0 : 1 ;
    hostInputIsInterleaved = ( HostInputSampleFormat & cafNonInterleaved ) ? 0 : 1 ;
    userInputSampleFormatIsEqualToHost                                       =
            ( ( UserInputSampleFormat & ~cafNonInterleaved )                ==
              ( HostInputSampleFormat & ~cafNonInterleaved )               ) ;
    tempInputBufferSize    = framesPerTempBuffer                             *
                             bytesPerUserInputSample                         *
                             InputChannelCount                               ;
    tempInputBuffer = Allocator::allocate( tempInputBufferSize )             ;
    //////////////////////////////////////////////////////////////////////////
    if ( NULL == tempInputBuffer )                                           {
      result = InsufficientMemory                                            ;
      Terminate()                                                            ;
      return result                                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( framesInTempInputBuffer > 0 )                                       {
      ::memset ( tempInputBuffer , 0 , tempInputBufferSize )                 ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( UserInputSampleFormat & cafNonInterleaved  )                        {
      tempInputBufferPtrs = (void **)Allocator::allocate ( sizeof(void*) * inputChannelCount ) ;
      if ( 0 == tempInputBufferPtrs )                                        {
        result = InsufficientMemory                                          ;
        Terminate()                                                          ;
        return result                                                        ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    hostInputChannels[0] = ( CaChannelDescriptor *) Allocator::allocate      (
                              sizeof(CaChannelDescriptor)                    *
                              inputChannelCount * 2                        ) ;
    if ( NULL == hostInputChannels[0] )                                      {
      result = InsufficientMemory                                            ;
      Terminate()                                                            ;
      return result                                                          ;
    }                                                                        ;
    hostInputChannels[1] = &hostInputChannels[0][inputChannelCount]          ;
    //////////////////////////////////////////////////////////////////////////
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( OutputChannelCount > 0 )                                              {
    bytesPerSample = Core::SampleSize( HostOutputSampleFormat )              ;
    if ( bytesPerSample > 0 )                                                {
      bytesPerHostOutputSample = bytesPerSample                              ;
    } else                                                                   {
      result = bytesPerSample                                                ;
      Terminate()                                                            ;
      return result                                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    bytesPerSample = Core::SampleSize( UserOutputSampleFormat )              ;
    if ( bytesPerSample > 0 )                                                {
      bytesPerUserOutputSample = bytesPerSample                              ;
    } else                                                                   {
      result = bytesPerSample                                                ;
      Terminate()                                                            ;
      return result                                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    outputConverter = SelectConverter                                        (
                        UserOutputSampleFormat                               ,
                        HostOutputSampleFormat                               ,
                        StreamFlags                                        ) ;
    outputZeroer    = SelectZeroCopier ( HostOutputSampleFormat )            ;
    userOutputIsInterleaved = ( UserOutputSampleFormat & cafNonInterleaved ) ? 0 : 1 ;
    hostOutputIsInterleaved = ( HostOutputSampleFormat & cafNonInterleaved ) ? 0 : 1 ;
    userOutputSampleFormatIsEqualToHost                                      =
                       ( ( UserOutputSampleFormat & ~cafNonInterleaved )    ==
                         ( HostOutputSampleFormat & ~cafNonInterleaved )   ) ;
    tempOutputBufferSize    = framesPerTempBuffer                            *
                              bytesPerUserOutputSample                       *
                              OutputChannelCount                             ;
    tempOutputBuffer = Allocator::allocate ( tempOutputBufferSize )          ;
    //////////////////////////////////////////////////////////////////////////
    if ( 0 == tempOutputBuffer )                                             {
      result = InsufficientMemory                                            ;
      Terminate()                                                            ;
      return result                                                          ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( framesInTempOutputBuffer > 0 )                                      {
      ::memset ( tempOutputBuffer , 0 , tempOutputBufferSize )               ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( UserOutputSampleFormat & cafNonInterleaved )                        {
      tempOutputBufferPtrs = (void **)Allocator::allocate( sizeof(void*) * outputChannelCount ) ;
      if ( 0 == tempOutputBufferPtrs )                                       {
        result = InsufficientMemory                                          ;
        Terminate()                                                          ;
        return result                                                        ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    hostOutputChannels[0] = (CaChannelDescriptor *)
                              Allocator::allocate                            (
                                sizeof(CaChannelDescriptor)                  *
                                outputChannelCount * 2                     ) ;
    if ( 0 == hostOutputChannels[0] )                                        {
      result = InsufficientMemory                                            ;
      Terminate()                                                            ;
      return result                                                          ;
    }                                                                        ;
    hostOutputChannels[1] = &hostOutputChannels [ 0 ] [ outputChannelCount ] ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  conduit      = CONDUIT                                                     ;
  samplePeriod = 1.0 / SampleRate                                            ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

void BufferProcessor::Terminate(void)
{
  Delete ( tempInputBuffer          ) ; tempInputBuffer          = NULL ;
  Delete ( tempInputBufferPtrs      ) ; tempInputBufferPtrs      = NULL ;
  Delete ( hostInputChannels  [ 0 ] ) ; hostInputChannels  [ 0 ] = NULL ;
  Delete ( tempOutputBuffer         ) ; tempOutputBuffer         = NULL ;
  Delete ( tempOutputBufferPtrs     ) ; tempOutputBufferPtrs     = NULL ;
  Delete ( hostOutputChannels [ 0 ] ) ; hostOutputChannels [ 0 ] = NULL ;
}

void BufferProcessor::Reset(void)
{
  unsigned long tempInputBufferSize                          ;
  unsigned long tempOutputBufferSize                         ;
  ////////////////////////////////////////////////////////////
  framesInTempInputBuffer  = initialFramesInTempInputBuffer  ;
  framesInTempOutputBuffer = initialFramesInTempOutputBuffer ;
  ////////////////////////////////////////////////////////////
  if ( framesInTempInputBuffer > 0 )                         {
    tempInputBufferSize = framesPerTempBuffer                *
                          bytesPerUserInputSample            *
                          inputChannelCount                  ;
    ::memset ( tempInputBuffer , 0 , tempInputBufferSize )   ;
  }                                                          ;
  ////////////////////////////////////////////////////////////
  if ( framesInTempOutputBuffer > 0 )                        {
    tempOutputBufferSize = framesPerTempBuffer               *
                           bytesPerUserOutputSample          *
                           outputChannelCount                ;
    ::memset ( tempOutputBuffer , 0 , tempOutputBufferSize ) ;
  }                                                          ;
}

CaUint32 BufferProcessor::InputLatencyFrames(void)
{
  return initialFramesInTempInputBuffer ;
}

CaUint32 BufferProcessor::OutputLatencyFrames(void)
{
  return initialFramesInTempOutputBuffer ;
}

void BufferProcessor::setNoInput(void)
{
  if ( NULL == hostInputChannels ) return       ;
  if ( NULL != hostInputChannels [ 0 ] )        {
    hostInputChannels [ 0 ] [ 0 ] . data = NULL ;
  }                                             ;
  if ( NULL != hostInputChannels [ 1 ] )        {
    hostInputChannels [ 1 ] [ 0 ] . data = NULL ;
  }                                             ;
}

void BufferProcessor::setInputFrameCount(int index,CaUint32 frameCount)
{
  if ( 0 == frameCount ) hostInputFrameCount [ index ] = framesPerHostBuffer ;
                    else hostInputFrameCount [ index ] = frameCount          ;
}

void BufferProcessor::setInputChannel (
       int      index                 ,
       CaUint32 channel               ,
       void   * data                  ,
       CaUint32 stride                )
{
  if ( index   <  0                  ) return               ;
  if ( channel >=  inputChannelCount ) return               ;
  if ( NULL    == hostInputChannels  ) return               ;
  ///////////////////////////////////////////////////////////
  hostInputChannels [ index ] [ channel ] . data   = data   ;
  hostInputChannels [ index ] [ channel ] . stride = stride ;
}

void BufferProcessor::setInterleavedInputChannels (
       int      index                             ,
       CaUint32 firstChannel                      ,
       void   * data                              ,
       CaUint32 channelCount                      )
{
  unsigned int    channel = firstChannel                                ;
  unsigned char * p       = (unsigned char *)data                       ;
  ///////////////////////////////////////////////////////////////////////
  if ( 0 == channelCount ) channelCount = inputChannelCount             ;
  ///////////////////////////////////////////////////////////////////////
  if ( firstChannel                    >= inputChannelCount ) return    ;
  if ( ( firstChannel + channelCount ) >  inputChannelCount ) return    ;
  if ( 0 == hostInputIsInterleaved                          ) return    ;
  ///////////////////////////////////////////////////////////////////////
  for ( CaUint32 i = 0 ; i < channelCount ; ++i )                       {
    hostInputChannels [ index ] [ channel + i ] . data = p              ;
    p += bytesPerHostInputSample                                        ;
    hostInputChannels [ index ] [ channel + i ] . stride = channelCount ;
  }                                                                     ;
}

void BufferProcessor::setNonInterleavedInputChannels (
       int      index                                ,
       CaUint32 channel                              ,
       void   * data                                 )
{
  if ( 0 != hostInputIsInterleaved ) return    ;
  setInputChannel ( index , channel, data, 1 ) ;
}

///////////////////////////////////////////////////////////////////////////////

void BufferProcessor::setNoOutput(void)
{
  if ( NULL == hostOutputChannels ) return       ;
  if ( NULL != hostOutputChannels [ 0 ] )        {
    hostOutputChannels [ 0 ] [ 0 ] . data = NULL ;
  }                                              ;
  if ( NULL != hostOutputChannels [ 1 ] )        {
    hostOutputChannels [ 1 ] [ 0 ] . data = NULL ;
  }                                              ;
}

void BufferProcessor::setOutputFrameCount (
       int      index                     ,
       CaUint32 frameCount                )
{
  if ( 0 == frameCount ) hostOutputFrameCount [ index ] = framesPerHostBuffer ;
                    else hostOutputFrameCount [ index ] = frameCount          ;
}

void BufferProcessor::setOutputChannel (
       int      index                  ,
       CaUint32 channel                ,
       void   * data                   ,
       CaUint32 stride                 )
{
  if ( index   <  0                  ) return                ;
  if ( channel >= outputChannelCount ) return                ;
  if ( NULL    == hostOutputChannels ) return                ;
  ////////////////////////////////////////////////////////////
  hostOutputChannels [ index ] [ channel ] . data   = data   ;
  hostOutputChannels [ index ] [ channel ] . stride = stride ;
}

void BufferProcessor::setInterleavedOutputChannels (
       int      index                              ,
       CaUint32 firstChannel                       ,
       void   * data                               ,
       CaUint32 channelCount                       )
{
  unsigned int channel = firstChannel                                ;
  unsigned char     * p = (unsigned char *) data                     ;
  ////////////////////////////////////////////////////////////////////
  if ( 0 == channelCount ) channelCount = outputChannelCount         ;
  ////////////////////////////////////////////////////////////////////
  if ( firstChannel >= outputChannelCount                   ) return ;
  if ( ( firstChannel + channelCount ) > outputChannelCount ) return ;
  if ( 0 == hostOutputIsInterleaved                         ) return ;
  ////////////////////////////////////////////////////////////////////
  for ( CaUint32 i = 0 ; i < channelCount ; ++i )                    {
    setOutputChannel ( index , channel + i , p , channelCount )      ;
    p += bytesPerHostOutputSample                                    ;
  }                                                                  ;
}

void BufferProcessor::setNonInterleavedOutputChannel (
       int      index                                ,
       CaUint32 channel                              ,
       void   * data                                 )
{
  if ( 0 != hostOutputIsInterleaved ) return     ;
  setOutputChannel ( index , channel , data, 1 ) ;
}

///////////////////////////////////////////////////////////////////////////////

void BufferProcessor::Begin(Conduit * CONDUIT)
{
  conduit  = CONDUIT                                                    ;
  if ( NULL == conduit ) return                                         ;
  conduit -> Input  . AdcDac -= framesInTempInputBuffer  * samplePeriod ;
  conduit -> Output . AdcDac += framesInTempOutputBuffer * samplePeriod ;
  hostInputFrameCount  [ 1 ] = 0                                        ;
  hostOutputFrameCount [ 1 ] = 0                                        ;
}

CaUint32 BufferProcessor::End(int * callbackResult)
{
  CaUint32 framesToProcess                                                  ;
  CaUint32 framesToGo                                                       ;
  CaUint32 framesProcessed = 0                                              ;
  ///////////////////////////////////////////////////////////////////////////
  if ( ( 0    != inputChannelCount             )                           &&
       ( 0    != outputChannelCount            )                           &&
       ( NULL != hostInputChannels [0][0].data )                           && /* input was supplied (see PaUtil_SetNoInput) */
       ( NULL != hostOutputChannels[0][0].data )                          ) { /* output was supplied (see PaUtil_SetNoOutput) */
    if ( ( hostInputFrameCount  [ 0 ] + hostInputFrameCount  [ 1 ] )       !=
         ( hostOutputFrameCount [ 0 ] + hostOutputFrameCount [ 1 ] )      ) {
      return 0                                                              ;
    }                                                                       ;
  }                                                                         ;
  ///////////////////////////////////////////////////////////////////////////
  if ( ( Conduit::Continue != (*callbackResult) )                          &&
       ( Conduit::Complete != (*callbackResult) )                          &&
       ( Conduit::Abort    != (*callbackResult) )                          &&
       ( Conduit::Postpone != (*callbackResult) )                         ) {
    return 0                                                                ;
  }                                                                         ;
  ///////////////////////////////////////////////////////////////////////////
  if ( 0 != useNonAdaptingProcess )                                         {
    if ( ( 0 != inputChannelCount ) && ( 0 != outputChannelCount ) )        {
      /* full duplex non-adapting process, splice buffers if they are different lengths */
      framesToGo = hostOutputFrameCount[0] + hostOutputFrameCount[1]        ; /* relies on assert above for input/output equivalence */
      do                                                                    {
        CaUint32              noInputInputFrameCount                        ;
        CaUint32            * hiFrameCount                                  ;
        CaChannelDescriptor * hic                                           ;
        CaUint32              noOutputOutputFrameCount                      ;
        CaUint32            * hoFrameCount                                  ;
        CaChannelDescriptor * hoc                                           ;
        CaUint32              framesProcessedThisIteration                  ;
        /////////////////////////////////////////////////////////////////////
        if ( NULL == hostInputChannels[0][0].data )                         {
          /* no input was supplied (see PaUtil_SetNoInput) NonAdapting knows how to deal with this */
          noInputInputFrameCount = framesToGo                               ;
          hiFrameCount           = &noInputInputFrameCount                  ;
          hic                    = NULL                                     ;
        } else
        if ( 0 != hostInputFrameCount[0] )                                  {
          hiFrameCount = &hostInputFrameCount [ 0 ]                         ;
          hic          =  hostInputChannels   [ 0 ]                         ;
        } else                                                              {
          hiFrameCount = &hostInputFrameCount [ 1 ]                         ;
          hic          =  hostInputChannels   [ 1 ]                         ;
        }                                                                   ;
        /////////////////////////////////////////////////////////////////////
        if ( NULL == hostOutputChannels[0][0].data )                        {
          /* no output was supplied (see setNoOutput) NonAdapting knows how to deal with this */
          noOutputOutputFrameCount = framesToGo                             ;
          hoFrameCount             = &noOutputOutputFrameCount              ;
          hoc                      = 0                                      ;
        } else
        if ( 0 != hostOutputFrameCount[0] )                                 {
          hoFrameCount = &hostOutputFrameCount [ 0 ]                        ;
          hoc          =  hostOutputChannels   [ 0 ]                        ;
        } else                                                              {
          hoFrameCount = &hostOutputFrameCount [ 1 ]                        ;
          hoc          =  hostOutputChannels   [ 1 ]                        ;
        }                                                                   ;
        /////////////////////////////////////////////////////////////////////
        framesToProcess = CA_MIN_( *hostInputFrameCount                     ,
                                   *hostOutputFrameCount                  ) ;
        if ( 0 == framesToProcess ) return 0                                ;
        framesProcessedThisIteration = NonAdapting                          (
                                         callbackResult                     ,
                                         hic                                ,
                                         hoc                                ,
                                         framesToProcess                  ) ;
        *hiFrameCount   -= framesProcessedThisIteration                     ;
        *hoFrameCount   -= framesProcessedThisIteration                     ;
        framesProcessed += framesProcessedThisIteration                     ;
        framesToGo      -= framesProcessedThisIteration                     ;
      } while ( framesToGo > 0 )                                            ;
    } else                                                                  {
      /* half duplex non-adapting process, just process 1st and 2nd buffer */
      /* process first buffer                                              */
      framesToProcess = ( 0 != inputChannelCount )                          ?
                        hostInputFrameCount  [ 0 ]                          :
                        hostOutputFrameCount [ 0 ]                          ;
      framesProcessed = NonAdapting                                         (
                          callbackResult                                    ,
                          hostInputChannels  [ 0 ]                          ,
                          hostOutputChannels [ 0 ]                          ,
                          framesToProcess                                 ) ;
      /* process second buffer if provided                                 */
      framesToProcess = ( 0 != inputChannelCount )                          ?
                               hostInputFrameCount  [ 1 ]                   :
                               hostOutputFrameCount [ 1 ]                   ;
      ///////////////////////////////////////////////////////////////////////
      if ( framesToProcess > 0 )                                            {
        framesProcessed += NonAdapting                                      (
                             callbackResult                                 ,
                             hostInputChannels  [ 1 ]                       ,
                             hostOutputChannels [ 1 ]                       ,
                             framesToProcess                              ) ;
      }                                                                     ;
    }                                                                       ;
  } else                                                                    { /* block adaption necessary*/
    if ( ( 0 != inputChannelCount ) && ( 0 != outputChannelCount ) )        {
      /* full duplex                                                       */
      if ( cabVariableHostBufferSizePartialUsageAllowed == hostBufferSizeMode ) {
        framesProcessed = Adapting ( callbackResult , 0 )                   ;
        /* dont process partial user buffers                               */
      } else                                                                {
        framesProcessed = Adapting ( callbackResult , 1 )                   ;
        /* process partial user buffers                                    */
      }                                                                     ;
    } else
    if ( 0 != inputChannelCount )                                           {
      /* input only                                                        */
      framesToProcess = hostInputFrameCount [ 0 ]                           ;
      framesProcessed = AdaptingInputOnly                                   (
                          callbackResult                                    ,
                          hostInputChannels [ 0 ]                           ,
                          framesToProcess                                 ) ;
      framesToProcess  = hostInputFrameCount [ 1 ]                          ;
      if ( framesToProcess > 0 )                                            {
        framesProcessed += AdaptingInputOnly                                (
                             callbackResult                                 ,
                             hostInputChannels [ 1 ]                        ,
                             framesToProcess                              ) ;
      }                                                                     ;
    } else                                                                  {
      /* output only                                                       */
      framesToProcess = hostOutputFrameCount [ 0 ]                          ;
      if ( framesToProcess > 0 )                                            {
        framesProcessed = AdaptingOutputOnly                                (
                            callbackResult                                  ,
                            hostOutputChannels [ 0 ]                        ,
                            framesToProcess                               ) ;
      } else framesProcessed = 0                                            ;
      framesToProcess = hostOutputFrameCount [ 1 ]                          ;
      if ( framesToProcess > 0 )                                            {
        framesProcessed += AdaptingOutputOnly                               (
                             callbackResult                                 ,
                             hostOutputChannels [ 1 ]                       ,
                             framesToProcess                              ) ;
      }                                                                     ;
    }                                                                       ;
  }                                                                         ;
  ///////////////////////////////////////////////////////////////////////////
  return framesProcessed                                                    ;
}

bool BufferProcessor::isOutputEmpty(void)
{
  return ( framesInTempOutputBuffer > 0 ) ? false : true ;
}

///////////////////////////////////////////////////////////////////////////////

CaUint32 BufferProcessor::CopyInput(void ** buffer,CaUint32 frameCount)
{
  CaChannelDescriptor * hic                     = NULL                   ;
  CaUint32              framesToCopy                                     ;
  unsigned char       * destBytePtr             = NULL                   ;
  void               ** nonInterleavedDestPtrs  = NULL                   ;
  CaUint32              destSampleStrideSamples ; /* stride from one sample to the next within a channel, in samples */
  CaUint32              destChannelStrideBytes  ; /* stride from one channel to the next, in bytes */
  CaUint32              i                                                ;
  ////////////////////////////////////////////////////////////////////////
  hic          = hostInputChannels [ 0 ]                                 ;
  framesToCopy = CA_MIN_( hostInputFrameCount [ 0 ] , frameCount )       ;
  ////////////////////////////////////////////////////////////////////////
  if ( 0 != userInputIsInterleaved )                                     {
    destBytePtr             = (unsigned char *) (*buffer)                ;
    destSampleStrideSamples = inputChannelCount                          ;
    destChannelStrideBytes  = bytesPerUserInputSample                    ;
    //////////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < inputChannelCount ; ++i )                          {
      inputConverter ( destBytePtr                                       ,
                       destSampleStrideSamples                           ,
                       hic [ i ] . data                                  ,
                       hic [ i ] . stride                                ,
                       framesToCopy                                      ,
                       &dither                                         ) ;
      destBytePtr += destChannelStrideBytes                              ; /* skip to next dest channel */
      /* advance source ptr for next iteration                          */
      hic [ i ] . data = ((unsigned char*)hic[i].data)                   +
                           framesToCopy                                  *
                           hic[i].stride                                 *
                           bytesPerHostInputSample                       ;
    }                                                                    ;
    //////////////////////////////////////////////////////////////////////
    /* advance callers dest pointer (buffer)                            */
    (*buffer) = ((unsigned char *)(*buffer))                             +
                  framesToCopy                                           *
                  inputChannelCount                                      *
                  bytesPerUserInputSample                                ;
  } else                                                                 {
    /* user input is not interleaved                                    */
    nonInterleavedDestPtrs  = (void**)(*buffer)                          ;
    destSampleStrideSamples = 1                                          ;
    for ( i = 0 ; i < inputChannelCount ; ++i )                          {
      destBytePtr = (unsigned char *)nonInterleavedDestPtrs[i]           ;
      inputConverter ( destBytePtr                                       ,
                       destSampleStrideSamples                           ,
                       hic [ i ] . data                                  ,
                       hic [ i ] . stride                                ,
                       framesToCopy                                      ,
                       &dither                                         ) ;
      /* advance callers dest pointer (nonInterleavedDestPtrs[i])       */
      destBytePtr                 += bytesPerUserInputSample             *
                                     framesToCopy                        ;
      nonInterleavedDestPtrs [ i ] = destBytePtr                         ;
      /* advance source ptr for next iteration                          */
      hic[i].data = ((unsigned char*)hic[i].data)                        +
                  framesToCopy * hic[i].stride * bytesPerHostInputSample ;
    }                                                                    ;
  }                                                                      ;
  ////////////////////////////////////////////////////////////////////////
  hostInputFrameCount [ 0 ] -= framesToCopy                              ;
  ////////////////////////////////////////////////////////////////////////
  return framesToCopy                                                    ;
}

CaUint32 BufferProcessor::CopyOutput (const void ** buffer,CaUint32 frameCount)
{
  CaChannelDescriptor * hoc                    = NULL               ;
  CaUint32              framesToCopy                                ;
  unsigned char       * srcBytePtr             = NULL               ;
  void               ** nonInterleavedSrcPtrs  = NULL               ;
  CaUint32              srcSampleStrideSamples                      ; /* stride from one sample to the next within a channel, in samples */
  CaUint32              srcChannelStrideBytes                       ; /* stride from one channel to the next, in bytes */
  CaUint32              i                                           ;
  ///////////////////////////////////////////////////////////////////
  hoc          = hostOutputChannels [ 0 ]                           ;
  framesToCopy = CA_MIN_ ( hostOutputFrameCount[0], frameCount    ) ;
  ///////////////////////////////////////////////////////////////////
  if ( 0 != userOutputIsInterleaved )                               {
    srcBytePtr             = (unsigned char *) (*buffer)            ;
    srcSampleStrideSamples = outputChannelCount                     ;
    srcChannelStrideBytes  = bytesPerUserOutputSample               ;
    /////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < outputChannelCount ; ++i )                    {
      outputConverter ( hoc [ i ] . data                            ,
                        hoc [ i ] . stride                          ,
                        srcBytePtr                                  ,
                        srcSampleStrideSamples                      ,
                        framesToCopy                                ,
                        &dither                                   ) ;
       /* skip to next source channel                              */
      srcBytePtr += srcChannelStrideBytes                           ;
      /* advance dest ptr for next iteration                       */
      hoc [ i ] . data = ((unsigned char *)hoc [ i ] . data)        +
                           framesToCopy                             *
                           hoc [ i ] . stride                       *
                           bytesPerHostOutputSample                 ;
    }                                                               ;
    /////////////////////////////////////////////////////////////////
    /* advance callers source pointer (buffer)                     */
    (*buffer) = ((unsigned char *)(*buffer))                        +
                  framesToCopy                                      *
                  outputChannelCount                                *
                  bytesPerUserOutputSample                          ;
  } else                                                            {
    /* user output is not interleaved                              */
    nonInterleavedSrcPtrs  = (void **)(*buffer)                     ;
    srcSampleStrideSamples = 1                                      ;
    for ( i = 0 ; i < outputChannelCount ; ++i )                    {
      srcBytePtr = (unsigned char *) nonInterleavedSrcPtrs [ i ]    ;
      outputConverter ( hoc [ i ] . data                            ,
                        hoc [ i ] . stride                          ,
                        srcBytePtr                                  ,
                        srcSampleStrideSamples                      ,
                        framesToCopy                                ,
                        &dither                                   ) ;
      ///////////////////////////////////////////////////////////////
      /* advance callers source pointer (nonInterleavedSrcPtrs[i]) */
      srcBytePtr              += bytesPerUserOutputSample           *
                                 framesToCopy                       ;
      nonInterleavedSrcPtrs[i] = srcBytePtr                         ;
      /* advance dest ptr for next iteration                       */
      hoc [ i ] . data = ((unsigned char *)hoc [ i ] . data)        +
                           framesToCopy                             *
                           hoc [ i ] . stride                       *
                           bytesPerHostOutputSample                 ;
    }                                                               ;
  }                                                                 ;
  ///////////////////////////////////////////////////////////////////
  hostOutputFrameCount [ 0 ] += framesToCopy                        ;
  ///////////////////////////////////////////////////////////////////
  return framesToCopy                                               ;
}

CaUint32 BufferProcessor::ZeroOutput(CaUint32 frameCount)
{
  CaChannelDescriptor * hoc                                          ;
  CaUint32              framesToZero                                 ;
  CaUint32              i                                            ;
  ////////////////////////////////////////////////////////////////////
  hoc          = hostOutputChannels [ 0 ]                            ;
  framesToZero = CA_MIN_ ( hostOutputFrameCount [ 0 ] , frameCount ) ;
  ////////////////////////////////////////////////////////////////////
  for ( i = 0 ; i < outputChannelCount ; ++i )                       {
    outputZeroer ( hoc[i].data , hoc[i].stride , framesToZero )      ;
    /* advance dest ptr for next iteration                          */
    hoc[i].data = ( (unsigned char *) hoc[i] . data )                +
                  ( framesToZero                                     *
                    hoc [ i ] . stride                               *
                    bytesPerHostOutputSample                       ) ;
  }                                                                  ;
  ////////////////////////////////////////////////////////////////////
  hostOutputFrameCount [ 0 ] += framesToZero                         ;
  return framesToZero                                                ;
}

///////////////////////////////////////////////////////////////////////////////

/*
    NonAdapting() is a simple buffer copying adaptor that can handle
    both full and half duplex copies. It processes framesToProcess frames,
    broken into blocks framesPerTempBuffer long.
    This routine can be used when the streamCallback doesn't care what length
    the buffers are, or when framesToProcess is an integer multiple of
    framesPerTempBuffer, in which case streamCallback will always be called
    with framesPerTempBuffer samples.
*/

CaUint32 BufferProcessor::NonAdapting              (
        int                 * streamCallbackResult ,
        CaChannelDescriptor * hic                  ,
        CaChannelDescriptor * hoc                  ,
        CaUint32              framesToProcess      )
{
  void          * userInput         = NULL                                   ;
  void          * userOutput        = NULL                                   ;
  unsigned char * srcBytePtr        = NULL                                   ;
  unsigned char * destBytePtr       = NULL                                   ;
  CaUint32        srcSampleStrideSamples                                     ; /* stride from one sample to the next within a channel, in samples */
  CaUint32        srcChannelStrideBytes                                      ; /* stride from one channel to the next, in bytes */
  CaUint32        destSampleStrideSamples                                    ; /* stride from one sample to the next within a channel, in samples */
  CaUint32        destChannelStrideBytes                                     ; /* stride from one channel to the next, in bytes */
  CaUint32        i                                                          ;
  CaUint32        frameCount                                                 ;
  CaUint32        framesToGo        = framesToProcess                        ;
  CaUint32        framesProcessed   = 0                                      ;
  int             skipOutputConvert = 0                                      ;
  int             skipInputConvert  = 0                                      ;
  ////////////////////////////////////////////////////////////////////////////
  if ( Conduit::Continue == (*streamCallbackResult)                        ) {
    do                                                                       {
      frameCount = CA_MIN_( framesPerTempBuffer , framesToGo )               ;
      /* configure user input buffer and convert input data (host -> user)  */
      if ( 0 == inputChannelCount )                                          {
        /* no input                                                         */
        userInput = NULL                                                     ;
      } else                                                                 { /* there are input channels */
        destBytePtr = (unsigned char *) tempInputBuffer                      ;
        if ( 0 != userInputIsInterleaved )                                   {
          destSampleStrideSamples = inputChannelCount                        ;
          destChannelStrideBytes  = bytesPerUserInputSample                  ;
          /* process host buffer directly, or use temp buffer if formats
           * differ or host buffer non-interleaved, or if the number of
           * channels differs between the host (set in stride) and the
           * user                                                           */
          if ( ( 0    != userInputSampleFormatIsEqualToHost )               &&
               ( 0    != hostInputIsInterleaved             )               &&
               ( NULL != hostInputChannels[0][0].data       )               &&
               ( inputChannelCount == hic[0].stride )       )                {
            userInput        = hic[0].data                                   ;
            destBytePtr      = (unsigned char *)hic[0].data                  ;
            skipInputConvert = 1                                             ;
          } else                                                             {
            userInput        = tempInputBuffer                               ;
          }                                                                  ;
        } else                                                               { /* user input is not interleaved */
          destSampleStrideSamples = 1                                        ;
          destChannelStrideBytes  = frameCount * bytesPerUserInputSample     ;
          /* setup non-interleaved ptrs                                     */
          if ( ( 0    != userInputSampleFormatIsEqualToHost )               &&
               ( 0    == hostInputIsInterleaved             )               &&
               ( NULL != hostInputChannels[0][0].data       )              ) {
            for ( i = 0 ; i < inputChannelCount ; ++i )                      {
              tempInputBufferPtrs[i] = hic[i].data                           ;
            }                                                                ;
            skipInputConvert = 1                                             ;
          } else                                                             {
            for ( i = 0 ; i < inputChannelCount ; ++i )                      {
              tempInputBufferPtrs[i] = ((unsigned char*)tempInputBuffer)     +
                                         i                                   *
                                         bytesPerUserInputSample             *
                                         frameCount                          ;
            }                                                                ;
          }                                                                  ;
          userInput = tempInputBufferPtrs                                    ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        if ( NULL == hostInputChannels[0][0].data )                          {
          /* no input was supplied (see PaUtil_SetNoInput), so zero the input buffer */
          for ( i = 0 ; i < inputChannelCount ; ++i )                        {
            inputZeroer ( destBytePtr, destSampleStrideSamples, frameCount ) ;
            destBytePtr += destChannelStrideBytes                            ; /* skip to next destination channel */
          }                                                                  ;
        } else                                                               {
          if ( 0 != skipInputConvert )                                       {
            for ( i = 0 ; i < inputChannelCount ; ++i )                      {
              /* advance src ptr for next iteration                         */
              hic[i].data = ((unsigned char*)hic[i].data)                    +
                              frameCount                                     *
                              hic[i].stride                                  *
                              bytesPerHostInputSample                        ;
            }                                                                ;
          } else                                                             {
            for ( i = 0 ; i < inputChannelCount ; ++i )                      {
              inputConverter                                                 (
                destBytePtr                                                  ,
                destSampleStrideSamples                                      ,
                hic [ i ] . data                                             ,
                hic [ i ] . stride                                           ,
                frameCount                                                   ,
                &dither                                                    ) ;
              /* skip to next destination channel                           */
              destBytePtr += destChannelStrideBytes                          ;
              /* advance src ptr for next iteration                         */
              hic[i].data = ((unsigned char*)hic[i].data)                    +
                              frameCount                                     *
                              hic [ i ] . stride                             *
                              bytesPerHostInputSample                        ;
            }                                                                ;
          }                                                                  ;
        }                                                                    ;
      }                                                                      ;
      /* configure user output buffer                                       */
      if ( 0 == outputChannelCount )                                         {
        /* no output                                                        */
        userOutput = 0                                                       ;
      } else                                                                 {
        /* there are output channels                                        */
        if ( 0 != userOutputIsInterleaved )                                  {
          /* process host buffer directly, or use temp buffer if formats
           * differ or host buffer non-interleaved                          */
          if ( ( 0 != userOutputSampleFormatIsEqualToHost                )  &&
               ( 0 != hostOutputIsInterleaved                            ) ) {
            userOutput        = hoc [ 0 ] . data                             ;
            skipOutputConvert = 1                                            ;
          } else                                                             {
            userOutput = tempOutputBuffer                                    ;
          }                                                                  ;
        } else                                                               {
          /* user output is not interleaved                                 */
          if ( ( 0 != userOutputSampleFormatIsEqualToHost                )  &&
               ( 0 == hostOutputIsInterleaved                            ) ) {
            for ( i = 0 ; i < outputChannelCount ; ++i )                     {
              tempOutputBufferPtrs [ i ] = hoc [ i ] . data                  ;
            }                                                                ;
            skipOutputConvert = 1                                            ;
          } else                                                             {
            for ( i = 0 ; i < outputChannelCount ; ++i )                     {
              tempOutputBufferPtrs [i] = ((unsigned char*)tempOutputBuffer)  +
                                           i                                 *
                                           bytesPerUserOutputSample          *
                                           frameCount                        ;
            }                                                                ;
          }                                                                  ;
          userOutput = tempOutputBufferPtrs                                  ;
        }                                                                    ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      conduit -> Output . Buffer     = userOutput                            ;
      conduit -> Output . FrameCount = frameCount                            ;
      conduit -> LockConduit   ( )                                           ;
      *streamCallbackResult   = conduit -> obtain ( )                        ;
      conduit -> UnlockConduit ( )                                           ;
      ////////////////////////////////////////////////////////////////////////
      if ( Conduit::Abort == (*streamCallbackResult) )                       {
        /* callback returned paAbort, don't advance framesProcessed and framesToGo, they will be handled below */
      } else                                                                 {
        conduit -> Input  . AdcDac += frameCount * samplePeriod              ;
        conduit -> Output . AdcDac += frameCount * samplePeriod              ;
        /* convert output data (user -> host)                               */
        if ( ( 0    != outputChannelCount                                )  &&
             ( NULL != hostOutputChannels[0][0].data                     ) ) {
          if ( 0 != skipOutputConvert )                                      {
            for ( i = 0 ; i < outputChannelCount ; ++i )                     {
              /* advance dest ptr for next iteration                        */
              hoc[i].data = ((unsigned char*)hoc[i].data)                    +
                              frameCount                                     *
                              hoc[i].stride                                  *
                              bytesPerHostOutputSample                       ;
            }                                                                ;
          } else                                                             {
            srcBytePtr = (unsigned char *) tempOutputBuffer                  ;
            if ( 0 != userOutputIsInterleaved )                              {
              srcSampleStrideSamples = outputChannelCount                    ;
              srcChannelStrideBytes  = bytesPerUserOutputSample              ;
            } else                                                           {
              /* user output is not interleaved                             */
              srcSampleStrideSamples = 1                                     ;
              srcChannelStrideBytes  = frameCount * bytesPerUserOutputSample ;
            }                                                                ;
            //////////////////////////////////////////////////////////////////
            for ( i = 0 ; i < outputChannelCount ; ++i )                     {
              outputConverter                                                (
                hoc [ i ] . data                                             ,
                hoc [ i ] . stride                                           ,
                srcBytePtr                                                   ,
                srcSampleStrideSamples                                       ,
                frameCount                                                   ,
                &dither                                                    ) ;
              /* skip to next source channel                                */
              srcBytePtr += srcChannelStrideBytes                            ;
              /* advance dest ptr for next iteration                        */
              hoc[i].data = ((unsigned char*)hoc[i].data)                    +
                              frameCount                                     *
                              hoc[i].stride                                  *
                              bytesPerHostOutputSample                       ;
            }                                                                ;
          }                                                                  ;
        }                                                                    ;
        //////////////////////////////////////////////////////////////////////
        framesProcessed += frameCount                                        ;
        framesToGo      -= frameCount                                        ;
      }                                                                      ;
    } while ( ( framesToGo > 0                             )                &&
              ( Conduit::Continue == *streamCallbackResult )               ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( framesToGo > 0 )                                                      {
    /* zero any remaining frames output. There will only be remaining frames.
       if the callback has returned Complete or Abort                       */
    frameCount = framesToGo                                                  ;
    if ( ( 0    != outputChannelCount                                     ) &&
         ( NULL != hostOutputChannels[0][0].data                         ) ) {
      for ( i = 0 ; i < outputChannelCount ; ++i )                           {
        outputZeroer ( hoc [ i ] . data , hoc [ i ] . stride , frameCount  ) ;
        /* advance dest ptr for next iteration                              */
        hoc [ i ] . data = ((unsigned char *)hoc[i].data)                    +
                             frameCount                                      *
                             hoc[i].stride                                   *
                             bytesPerHostOutputSample                        ;
      }                                                                      ;
    }                                                                        ;
    framesProcessed += frameCount                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return framesProcessed                                                     ;
}

CaUint32 BufferProcessor::AdaptingInputOnly           (
           int                 * streamCallbackResult ,
           CaChannelDescriptor * hic                  ,
           CaUint32              framesToProcess      )
{
  void          * userInput       = NULL                                     ;
  unsigned char * destBytePtr     = NULL                                     ;
  CaUint32        destSampleStrideSamples                                    ; /* stride from one sample to the next within a channel, in samples */
  CaUint32        destChannelStrideBytes                                     ; /* stride from one channel to the next, in bytes */
  CaUint32        i                                                          ;
  CaUint32        frameCount                                                 ;
  CaUint32        framesToGo      = framesToProcess                          ;
  CaUint32        framesProcessed = 0                                        ;
  ////////////////////////////////////////////////////////////////////////////
  do                                                                         {
    frameCount = ( ( framesInTempInputBuffer + framesToGo ) > framesPerUserBuffer ) ?
                   ( framesPerUserBuffer - framesInTempInputBuffer )         :
                     framesToGo                                              ;
    /* convert frameCount samples into temp buffer                          */
    if ( 0 != userInputIsInterleaved )                                       {
      destBytePtr = ((unsigned char*)tempInputBuffer)                        +
                      bytesPerUserInputSample                                *
                      inputChannelCount                                      *
                      framesInTempInputBuffer                                ;
      destSampleStrideSamples = inputChannelCount                            ;
      destChannelStrideBytes  = bytesPerUserInputSample                      ;
      userInput               = tempInputBuffer                              ;
    } else                                                                   {
      /* user input is not interleaved                                      */
      destBytePtr = ((unsigned char*)tempInputBuffer)                        +
                      bytesPerUserInputSample                                *
                      framesInTempInputBuffer                                ;
      destSampleStrideSamples = 1                                            ;
      destChannelStrideBytes  = framesPerUserBuffer*bytesPerUserInputSample  ;
      /* setup non-interleaved ptrs                                         */
      for ( i = 0 ; i < inputChannelCount ; ++i )                            {
        tempInputBufferPtrs[i] = ((unsigned char *)tempInputBuffer)          +
                                 i                                           *
                                 bytesPerUserInputSample                     *
                                 framesPerUserBuffer                         ;
      }                                                                      ;
      userInput = tempInputBufferPtrs                                        ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < inputChannelCount ; ++i )                              {
      inputConverter                                                         (
        destBytePtr                                                          ,
        destSampleStrideSamples                                              ,
        hic [ i ] . data                                                     ,
        hic [ i ] . stride                                                   ,
        frameCount                                                           ,
        &dither                                                            ) ;
      /* skip to next destination channel                                   */
      destBytePtr += destChannelStrideBytes                                  ;
      /* advance src ptr for next iteration                                 */
      hic[i].data = ((unsigned char*)hic[i].data)                            +
                      frameCount                                             *
                      hic[i].stride                                          *
                      bytesPerHostInputSample                                ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    framesInTempInputBuffer += frameCount                                    ;
    if ( framesInTempInputBuffer == framesPerUserBuffer )                    {
      if ( ( Conduit::Continue == (*streamCallbackResult) )                 ||
           ( Conduit::Postpone == (*streamCallbackResult) )                ) {
        conduit -> Output . AdcDac     = 0                                   ;
        conduit -> Input  . Buffer     = userInput                           ;
        conduit -> Input  . FrameCount = framesPerUserBuffer                 ;
        conduit -> LockConduit   ( )                                         ;
        *streamCallbackResult          = conduit -> put ( )                  ;
        conduit -> UnlockConduit ( )                                         ;
        conduit -> Input  . AdcDac    += framesPerUserBuffer * samplePeriod  ;
      }                                                                      ;
      if ( Conduit::Postpone == (*streamCallbackResult) )                    {
      } else                                                                 {
        framesInTempInputBuffer = 0                                          ;
      }                                                                      ;
    }                                                                        ;
    framesProcessed += frameCount                                            ;
    framesToGo      -= frameCount                                            ;
  } while ( framesToGo > 0 )                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  return framesProcessed                                                     ;
}

CaUint32 BufferProcessor::AdaptingOutputOnly          (
           int                 * streamCallbackResult ,
           CaChannelDescriptor * hoc                  ,
           CaUint32              framesToProcess      )
{
  void          * userInput       = NULL                                     ;
  void          * userOutput      = NULL                                     ;
  unsigned char * srcBytePtr      = NULL                                     ;
  CaUint32        srcSampleStrideSamples                                     ; /* stride from one sample to the next within a channel, in samples */
  CaUint32        srcChannelStrideBytes                                      ; /* stride from one channel to the next, in bytes */
  CaUint32        i                                                          ;
  CaUint32        frameCount                                                 ;
  CaUint32        framesToGo      = framesToProcess                          ;
  CaUint32        framesProcessed = 0                                        ;
  ////////////////////////////////////////////////////////////////////////////
  do                                                                         {
    if ( ( 0                 == framesInTempOutputBuffer                 )  &&
         ( Conduit::Continue == (*streamCallbackResult)                  ) ) {
      userInput = NULL                                                       ;
      /* setup userOutput                                                   */
      if ( 0 != userOutputIsInterleaved )                                    {
        userOutput = tempOutputBuffer                                        ;
      } else                                                                 { /* user output is not interleaved */
        for ( i = 0 ; i < outputChannelCount ; ++i )                         {
          tempOutputBufferPtrs [ i ] = ((unsigned char *)tempOutputBuffer)   +
                                         i                                   *
                                         framesPerUserBuffer                 *
                                         bytesPerUserOutputSample            ;
        }                                                                    ;
        userOutput = tempOutputBufferPtrs                                    ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      conduit -> Input  . AdcDac     = 0                                     ;
      conduit -> Output . Buffer     = userOutput                            ;
      conduit -> Output . FrameCount = framesPerUserBuffer                   ;
      conduit -> LockConduit   ( )                                           ;
      *streamCallbackResult          = conduit -> obtain ( )                 ;
      conduit -> UnlockConduit ( )                                           ;
      ////////////////////////////////////////////////////////////////////////
      if ( Conduit::Abort == (*streamCallbackResult) )                       {
        /* if the callback returned paAbort, we disregard its output        */
      } else                                                                 {
        conduit -> Output . AdcDac  += framesPerUserBuffer * samplePeriod    ;
        framesInTempOutputBuffer     = framesPerUserBuffer                   ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( framesInTempOutputBuffer > 0 )                                      {
      /* convert frameCount frames from user buffer to host buffer          */
      frameCount = CA_MIN_( framesInTempOutputBuffer, framesToGo )           ;
      if ( 0 != userOutputIsInterleaved )                                    {
        srcBytePtr = ((unsigned char *)tempOutputBuffer)                     +
                       bytesPerUserOutputSample                              *
                       outputChannelCount                                    *
                      ( framesPerUserBuffer - framesInTempOutputBuffer     ) ;
        srcSampleStrideSamples = outputChannelCount                          ;
        srcChannelStrideBytes  = bytesPerUserOutputSample                    ;
      } else                                                                 { /* user output is not interleaved */
        srcBytePtr = ((unsigned char *)tempOutputBuffer)                     +
                       bytesPerUserOutputSample                              *
                      ( framesPerUserBuffer - framesInTempOutputBuffer )     ;
        srcSampleStrideSamples = 1                                           ;
        srcChannelStrideBytes  = framesPerUserBuffer*bytesPerUserOutputSample;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      for ( i = 0 ; i < outputChannelCount ; ++i )                           {
        outputConverter                                                      (
          hoc[i].data                                                        ,
          hoc[i].stride                                                      ,
          srcBytePtr                                                         ,
          srcSampleStrideSamples                                             ,
          frameCount                                                         ,
          &dither                                                          ) ;
        /* skip to next source channel                                      */
        srcBytePtr += srcChannelStrideBytes                                  ;
        /* advance dest ptr for next iteration                              */
        hoc[i].data = ((unsigned char*)hoc[i].data)                          +
                        frameCount                                           *
                        hoc[i].stride                                        *
                        bytesPerHostOutputSample                             ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      framesInTempOutputBuffer -= frameCount                                 ;
    } else                                                                   {
      /* no more user data is available because the callback has returned
         Complete or Abort. Fill the remainder of the host buffer with zeros. */
      frameCount = framesToGo                                                ;
      for ( i = 0 ; i < outputChannelCount ; ++i )                           {
        outputZeroer ( hoc [ i ] . data , hoc [ i ] . stride , frameCount )  ;
        /* advance dest ptr for next iteration                              */
        hoc[i].data = ((unsigned char *)hoc[i].data)                         +
                        frameCount                                           *
                        hoc[i].stride                                        *
                        bytesPerHostOutputSample                             ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    framesProcessed += frameCount                                            ;
    framesToGo      -= frameCount                                            ;
  } while ( framesToGo > 0 )                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  return framesProcessed                                                     ;
}

/* TempToHost is called from AdaptingProcess to copy frames from
    tempOutputBuffer to hostOutputChannels. This includes data conversion
    and interleaving.
*/

void BufferProcessor::TempToHost(void)
{
  CaChannelDescriptor * hoc        = NULL                                    ;
  unsigned char       * srcBytePtr = NULL                                    ;
  unsigned long         maxFramesToCopy                                      ;
  unsigned int          frameCount                                           ;
  unsigned int          srcSampleStrideSamples                               ; /* stride from one sample to the next within a channel, in samples */
  unsigned int          srcChannelStrideBytes                                ; /* stride from one channel to the next, in bytes */
  unsigned int          i                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  /* copy frames from user to host output buffers                           */
  while ( ( framesInTempOutputBuffer > 0 )                                  &&
          ( ( hostOutputFrameCount[0] + hostOutputFrameCount[1] ) > 0 )    ) {
    maxFramesToCopy = framesInTempOutputBuffer                               ;
    /* select the output buffer set (1st or 2nd)                            */
    if ( hostOutputFrameCount[0] > 0 )                                       {
      hoc        = hostOutputChannels [ 0 ]                                  ;
      frameCount = CA_MIN_( hostOutputFrameCount[0] , maxFramesToCopy      ) ;
    } else                                                                   {
      hoc        = hostOutputChannels[1]                                     ;
      frameCount = CA_MIN_( hostOutputFrameCount[1] , maxFramesToCopy      ) ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( 0 != userOutputIsInterleaved )                                      {
      srcBytePtr = ((unsigned char *)tempOutputBuffer)                       +
                     bytesPerUserOutputSample                                *
                     outputChannelCount                                      *
                     ( framesPerUserBuffer - framesInTempOutputBuffer )      ;
      srcSampleStrideSamples = outputChannelCount                            ;
      srcChannelStrideBytes  = bytesPerUserOutputSample                      ;
    } else                                                                   { /* user output is not interleaved */
      srcBytePtr = ((unsigned char *)tempOutputBuffer)                       +
                     bytesPerUserOutputSample                                *
                     ( framesPerUserBuffer - framesInTempOutputBuffer )      ;
      srcSampleStrideSamples = 1                                             ;
      srcChannelStrideBytes  = framesPerUserBuffer * bytesPerUserOutputSample;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    for ( i = 0 ; i < outputChannelCount ; ++i )                             {
      outputConverter ( hoc [ i ] . data                                     ,
                        hoc [ i ] . stride                                   ,
                        srcBytePtr                                           ,
                        srcSampleStrideSamples                               ,
                        frameCount                                           ,
                        &dither                                            ) ;
      /* skip to next source channel                                        */
      srcBytePtr += srcChannelStrideBytes                                    ;
      /* advance dest ptr for next iteration                                */
      hoc[i].data = ((unsigned char *)hoc[i].data)                           +
                      frameCount                                             *
                      hoc[i].stride                                          *
                      bytesPerHostOutputSample                               ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( hostOutputFrameCount[0] > 0 ) hostOutputFrameCount[0] -= frameCount ;
                                  else hostOutputFrameCount[1] -= frameCount ;
     framesInTempOutputBuffer -= frameCount                                  ;
  }                                                                          ;
}

/*****************************************************************************

  AdaptingProcess is a full duplex adapting buffer processor. It converts
  data from the temporary output buffer into the host output buffers, then
  from the host input buffers into the temporary input buffers. Calling the
  streamCallback when necessary.

  When processPartialUserBuffers is 0, all available input data will be
  consumed and all available output space will be filled. When
  processPartialUserBuffers is non-zero, as many full user buffers
  as possible will be processed, but partial buffers will not be consumed.

 *****************************************************************************/

CaUint32 BufferProcessor::Adapting         (
           int * streamCallbackResult      ,
           int   processPartialUserBuffers )
{
  void                * userInput                  = NULL                    ;
  void                * userOutput                 = NULL                    ;
  unsigned char       * destBytePtr                = NULL                    ;
  CaChannelDescriptor * hic                        = NULL                    ;
  CaChannelDescriptor * hoc                        = NULL                    ;
  CaUint32              framesProcessed            = 0                       ;
  CaUint32              framesAvailable                                      ;
  CaUint32              endProcessingMinFrameCount                           ;
  CaUint32              maxFramesToCopy                                      ;
  CaUint32              frameCount                                           ;
  CaUint32              destSampleStrideSamples    ; /* stride from one sample to the next within a channel, in samples */
  CaUint32              destChannelStrideBytes     ; /* stride from one channel to the next, in bytes */
  CaUint32              i                                                    ;
  CaUint32              j                                                    ;
  ////////////////////////////////////////////////////////////////////////////
  /* this is assumed to be the same as the output buffer's frame count      */
  framesAvailable = hostInputFrameCount[0] + hostInputFrameCount[1]          ;
  if ( 0 != processPartialUserBuffers )                                      {
    endProcessingMinFrameCount = 0                                           ;
  } else                                                                     {
    endProcessingMinFrameCount = ( framesPerUserBuffer - 1 )                 ;
  }                                                                          ;
  /* Fill host output with remaining frames in user output (tempOutputBuffer) */
  TempToHost ( )                                                             ;
  while ( framesAvailable > endProcessingMinFrameCount )                     {
    if ( ( 0 == framesInTempOutputBuffer )                                  &&
         ( Conduit::Continue == (*streamCallbackResult) )                  ) {
      /* the callback will not be called any more, so zero what remains of the host output buffers */
      for ( i = 0 ; i < 2 ; ++i )                                            {
        frameCount = hostOutputFrameCount [ i ]                              ;
        if ( frameCount > 0 )                                                {
          hoc = hostOutputChannels [ i ]                                     ;
          for ( j = 0 ; j < outputChannelCount ; ++j )                       {
            outputZeroer ( hoc[j].data , hoc[j].stride , frameCount )        ;
            /* advance dest ptr for next iteration                          */
            hoc[j].data = ((unsigned char*)hoc[j].data)                      +
                            frameCount                                       *
                            hoc[j].stride                                    *
                            bytesPerHostOutputSample                         ;
          }                                                                  ;
          hostOutputFrameCount [ i ] = 0                                     ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
    /* copy frames from host to user input buffers                          */
    while ( ( framesInTempInputBuffer < framesPerUserBuffer )               &&
            ( ( hostInputFrameCount[0] + hostInputFrameCount[1] ) > 0 )    ) {
      maxFramesToCopy = framesPerUserBuffer - framesInTempInputBuffer        ;
      /* select the input buffer set (1st or 2nd)                           */
      if ( hostInputFrameCount[0] > 0 )                                      {
        hic        = hostInputChannels [ 0 ]                                 ;
        frameCount = CA_MIN_( hostInputFrameCount[0] , maxFramesToCopy )     ;
      } else                                                                 {
        hic        = hostInputChannels [ 1 ]                                 ;
        frameCount = CA_MIN_( hostInputFrameCount[1] , maxFramesToCopy )     ;
      }                                                                      ;
      /* configure conversion destination pointers                          */
      if ( 0 != userInputIsInterleaved )                                     {
        destBytePtr = ((unsigned char *)tempInputBuffer)                     +
                        bytesPerUserInputSample                              *
                        inputChannelCount                                    *
                        framesInTempInputBuffer                              ;
        destSampleStrideSamples = inputChannelCount                          ;
        destChannelStrideBytes  = bytesPerUserInputSample                    ;
      } else                                                                 { /* user input is not interleaved */
        destBytePtr = ((unsigned char *)tempInputBuffer)                     +
                        bytesPerUserInputSample                              *
                        framesInTempInputBuffer                              ;
        destSampleStrideSamples = 1                                          ;
        destChannelStrideBytes  = framesPerUserBuffer                        *
                                  bytesPerUserInputSample                    ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      for ( i = 0 ; i < inputChannelCount ; ++i )                            {
        inputConverter                                                       (
          destBytePtr                                                        ,
          destSampleStrideSamples                                            ,
          hic [ i ] . data                                                   ,
          hic [ i ] . stride                                                 ,
          frameCount                                                         ,
          &dither                                                          ) ;
        /* skip to next destination channel                                 */
        destBytePtr += destChannelStrideBytes                                ;
        /* advance src ptr for next iteration                               */
        hic[i].data = ((unsigned char *)hic[i].data)                         +
                        frameCount                                           *
                        hic[i].stride                                        *
                        bytesPerHostInputSample                              ;
      }                                                                      ;
      ////////////////////////////////////////////////////////////////////////
      if ( hostInputFrameCount[0] > 0 ) hostInputFrameCount[0] -= frameCount ;
                                   else hostInputFrameCount[1] -= frameCount ;
      framesInTempInputBuffer += frameCount                                  ;
      /* update framesAvailable and framesProcessed based on input consumed
         unless something is very wrong this will also correspond to the
         amount of output generated                                         */
      framesAvailable         -= frameCount                                  ;
      framesProcessed         += frameCount                                  ;
    }                                                                        ;
    /* call streamCallback                                                  */
    if ( ( framesInTempInputBuffer == framesPerUserBuffer                 ) &&
         ( 0 == framesInTempOutputBuffer                                 ) ) {
      if ( Conduit::Continue == (*streamCallbackResult) )                    {
        /* setup userInput                                                  */
        if ( 0 != userInputIsInterleaved )                                   {
          userInput = tempInputBuffer                                        ;
        } else                                                               {
          /* user input is not interleaved                                  */
          for ( i = 0 ; i < inputChannelCount ; ++i )                        {
            tempInputBufferPtrs[i] = ((unsigned char *)tempInputBuffer)      +
                                       i                                     *
                                       framesPerUserBuffer                   *
                                       bytesPerUserInputSample               ;
          }                                                                  ;
          userInput = tempInputBufferPtrs                                    ;
        }                                                                    ;
        /* setup userOutput                                                 */
        if ( 0 != userOutputIsInterleaved )                                  {
          userOutput = tempOutputBuffer                                      ;
        } else                                                               { /* user output is not interleaved */
          for ( i = 0 ; i < outputChannelCount ; ++i )                       {
             tempOutputBufferPtrs[i] = ((unsigned char *)tempOutputBuffer)   +
                                         i                                   *
                                         framesPerUserBuffer                 *
                                         bytesPerUserOutputSample            ;
          }                                                                  ;
          userOutput = tempOutputBufferPtrs                                  ;
        }                                                                    ;
        conduit -> Output . Buffer     = userOutput                          ;
        conduit -> Output . FrameCount = framesPerUserBuffer                 ;
        conduit -> LockConduit   ( )                                         ;
        *streamCallbackResult          = conduit -> obtain ( )               ;
        conduit -> UnlockConduit ( )                                         ;
        //////////////////////////////////////////////////////////////////////
        conduit -> Input  . AdcDac += framesPerUserBuffer * samplePeriod     ;
        conduit -> Output . AdcDac += framesPerUserBuffer * samplePeriod     ;
        framesInTempInputBuffer = 0                                          ;
        //////////////////////////////////////////////////////////////////////
        if ( Conduit::Abort == (*streamCallbackResult) )                     {
          framesInTempOutputBuffer = 0                                       ;
        } else                                                               {
          framesInTempOutputBuffer = framesPerUserBuffer                     ;
        }                                                                    ;
      } else                                                                 {
        /* Complete or Abort has already been called.                       */
        framesInTempInputBuffer = 0                                          ;
      }                                                                      ;
    }                                                                        ;
    /* copy frames from user (tempOutputBuffer) to host output buffers (hostOutputChannels)
       Means to process the user output provided by the callback. Has to be called after
       each callback.                                                       */
    TempToHost ( )                                                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return framesProcessed                                                     ;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
