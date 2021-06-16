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

#include "CaCoreAudio.hpp"

#if defined(__APPLE__)
#else
#error "Core Audio works only on Mac OS X and iPhone OS platforms"
#endif

///////////////////////////////////////////////////////////////////////////////

#ifndef MIN
#define MIN(a, b)  (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a, b)  (((a)<(b))?(b):(a))
#endif

#define ERR(mac_error)     MacCoreSetError(mac_error, __LINE__, 1 )
#define WARNING(mac_error) MacCoreSetError(mac_error, __LINE__, 0 )
#define UNIX_ERR(err)      SetMacOSXError( err, __LINE__ )
#define CA_AUHAL_SET_LAST_HOST_ERROR( errorCode, errorText ) \
                           SetLastHostErrorInfo( CoreAudio , errorCode , errorText )

#define INPUT_ELEMENT  (1)
#define OUTPUT_ELEMENT (0)

#define CA_MAC_BLIO_BUSY_WAIT_SLEEP_INTERVAL (5)
#define CA_MAC_BLIO_BUSY_WAIT
#define RING_BUFFER_ADVANCE_DENOMINATOR (4)
#define CA_MAC_SMALL_BUFFER_SIZE    (64)
#define RING_BUFFER_EMPTY (1000)

#define HOST_TIME_TO_CA_TIME( x ) ( AudioConvertHostTimeToNanos( (x) ) * 1.0E-09) /* convert to nanoseconds and then to seconds */

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#define caMacCoreChangeDeviceParameters   (0x0001)
#define caMacCoreFailIfConversionRequired (0x0002)
#define caMacCoreConversionQualityMin     (0x0100)
#define caMacCoreConversionQualityMedium  (0x0200)
#define caMacCoreConversionQualityLow     (0x0300)
#define caMacCoreConversionQualityHigh    (0x0400)
#define caMacCoreConversionQualityMax     (0x0000)
#define caMacCorePlayNice                 (0x0000)
#define caMacCorePro                      (0x0001)
#define caMacCoreMinimizeCPUButPlayNice   (0x0100)
#define caMacCoreMinimizeCPU              (0x0101)

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

struct CaMacXRunListNode_s            {
  CoreAudioStream            * stream ;
  struct CaMacXRunListNode_s * next   ;
}                                     ;

typedef struct CaMacXRunListNode_s CaMacXRunListNode;

static CaMacXRunListNode firstXRunListNode      ;
static int               xRunListSize           ;
static pthread_mutex_t   xrunMutex              ;
static char            * channelName     = NULL ;
static int               channelNameSize = 0    ;

//////////////////////////////////////////////////////////////////////////////

static ComponentResult BlockWhileAudioUnitIsRunning (
                         AudioUnit        audioUnit ,
                         AudioUnitElement element   )
{
  Boolean isRunning = 1                        ;
  while ( isRunning )                          {
    UInt32 s = sizeof(isRunning)               ;
    ComponentResult err                        ;
    err = AudioUnitGetProperty                 (
            audioUnit                          ,
            kAudioOutputUnitProperty_IsRunning ,
            kAudioUnitScope_Global             ,
            element                            ,
            &isRunning                         ,
            &s                               ) ;
    if ( err ) return err                      ;
    Timer :: Sleep ( 100 )                     ;
  }                                            ;
  return noErr                                 ;
}

CaError SetMacOSXError(int err,int line)
{
  CaError      ret       = NoError                                  ;
  const char * errorText = NULL                                     ;
  if ( 0 == err ) return ret                                        ;
  errorText = strerror ( err )                                      ;
  if ( ENOMEM == err ) ret = InsufficientMemory                     ;
                  else ret = InternalError                          ;
  gPrintf ( ( "%d on line %d: msg='%s'\n", err, line, errorText ) ) ;
  SetLastHostErrorInfo ( CoreAudio , err , errorText )              ;
  return ret                                                        ;
}

//////////////////////////////////////////////////////////////////////////////

CaError MacCoreSetError(OSStatus error,int line,int isError)
{
  CaError      result    = NoError                                           ;
  const char * errorType = NULL                                              ;
  const char * errorText = NULL                                              ;
  ////////////////////////////////////////////////////////////////////////////
  switch ( error )                                                           {
    case kAudioHardwareNoError                                               :
    return NoError                                                           ;
    case kAudioHardwareNotRunningError                                       :
      errorText = "Audio Hardware Not Running"                               ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioHardwareUnspecifiedError                                      :
      errorText = "Unspecified Audio Hardware Error"                         ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioHardwareUnknownPropertyError                                  :
      errorText = "Audio Hardware: Unknown Property"                         ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioHardwareBadPropertySizeError                                  :
      errorText = "Audio Hardware: Bad Property Size"                        ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioHardwareIllegalOperationError                                 :
      errorText = "Audio Hardware: Illegal Operation"                        ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioHardwareBadDeviceError                                        :
      errorText = "Audio Hardware: Bad Device"                               ;
      result    = InvalidDevice                                              ;
    break                                                                    ;
    case kAudioHardwareBadStreamError                                        :
      errorText = "Audio Hardware: BadStream"                                ;
      result    = BadStreamPtr                                               ;
    break                                                                    ;
    case kAudioHardwareUnsupportedOperationError                             :
      errorText = "Audio Hardware: Unsupported Operation"                    ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioDeviceUnsupportedFormatError                                  :
      errorText = "Audio Device: Unsupported Format"                         ;
      result    = SampleFormatNotSupported                                   ;
    break                                                                    ;
    case kAudioDevicePermissionsError                                        :
      errorText = "Audio Device: Permissions Error"                          ;
      result    = DeviceUnavailable                                          ;
    break                                                                    ;
    case kAudioUnitErr_InvalidProperty                                       :
      errorText = "Audio Unit: Invalid Property"                             ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_InvalidParameter                                      :
      errorText = "Audio Unit: Invalid Parameter"                            ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_NoConnection                                          :
      errorText = "Audio Unit: No Connection"                                ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_FailedInitialization                                  :
      errorText = "Audio Unit: Initialization Failed"                        ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_TooManyFramesToProcess                                :
      errorText = "Audio Unit: Too Many Frames"                              ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_IllegalInstrument                                     :
      errorText = "Audio Unit: Illegal Instrument"                           ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_InstrumentTypeNotFound                                :
      errorText = "Audio Unit: Instrument Type Not Found"                    ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_InvalidFile                                           :
      errorText = "Audio Unit: Invalid File"                                 ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_UnknownFileType                                       :
      errorText = "Audio Unit: Unknown File Type"                            ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_FileNotSpecified                                      :
      errorText = "Audio Unit: File Not Specified"                           ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_FormatNotSupported                                    :
      errorText = "Audio Unit: Format Not Supported"                         ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_Uninitialized                                         :
      errorText = "Audio Unit: Unitialized"                                  ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_InvalidScope                                          :
      errorText = "Audio Unit: Invalid Scope"                                ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_PropertyNotWritable                                   :
      errorText = "Audio Unit: PropertyNotWritable"                          ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_InvalidPropertyValue                                  :
      errorText = "Audio Unit: Invalid Property Value"                       ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_PropertyNotInUse                                      :
      errorText = "Audio Unit: Property Not In Use"                          ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_Initialized                                           :
        errorText = "Audio Unit: Initialized"                                ;
        result    = InternalError                                            ;
    break                                                                    ;
    case kAudioUnitErr_InvalidOfflineRender                                  :
      errorText = "Audio Unit: Invalid Offline Render"                       ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_Unauthorized                                          :
      errorText = "Audio Unit: Unauthorized"                                 ;
      result    = InternalError                                              ;
    break                                                                    ;
    case kAudioUnitErr_CannotDoInCurrentContext                              :
      errorText = "Audio Unit: cannot do in current context"                 ;
      result    = InternalError                                              ;
    break                                                                    ;
    default                                                                  :
      errorText = "Unknown Error"                                            ;
      result    = InternalError                                              ;
    break                                                                    ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( isError ) errorType = "Error"                                         ;
            else errorType = "Warning"                                       ;
  ////////////////////////////////////////////////////////////////////////////
  char str [ 20 ]                                                            ;
  // see if it appears to be a 4-char-code
  *(UInt32 *)(str + 1) = CFSwapInt32HostToBig(error)                         ;
  if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
    str [ 0 ] = str[5] = '\''                                                ;
    str [ 6 ] = '\0'                                                         ;
  } else                                                                     {
    sprintf ( str , "%d", (int)error )                                       ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "%s on line %d: err='%s', msg=%s\n"                            ,
              errorType, line, str, errorText                            ) ) ;
  ////////////////////////////////////////////////////////////////////////////
  SetLastHostErrorInfo ( CoreAudio, error, errorText )                       ;
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////

static void startStopCallback               (
              void *              inRefCon  ,
              AudioUnit           ci        ,
              AudioUnitPropertyID inID      ,
              AudioUnitScope      inScope   ,
              AudioUnitElement    inElement )
{
   CoreAudioStream * stream = (CoreAudioStream *) inRefCon                   ;
   UInt32            isRunning                                               ;
   UInt32            size = sizeof( isRunning )                              ;
   OSStatus          err                                                     ;
   err = AudioUnitGetProperty ( ci                                           ,
                                kAudioOutputUnitProperty_IsRunning           ,
                                inScope                                      ,
                                inElement                                    ,
                                &isRunning                                   ,
                                &size                                      ) ;
   assert ( !err )                                                           ;
   if ( err ) isRunning = false                                              ;
   if ( isRunning ) return                                                   ;
   if ( ( 0 != stream->inputUnit                                          ) &&
        ( 0 != stream->outputUnit                                         ) &&
        ( stream->inputUnit != stream->outputUnit                         ) &&
        ( ci == stream->inputUnit                                 ) ) return ;
   if ( CoreAudioStream::STOPPING == stream->state )                         {
     stream->state = CoreAudioStream::STOPPED                                ;
   }                                                                         ;
   if ( NULL != stream->conduit )                                            {
     if ( 0 != stream->inputUnit  )                                          {
       stream->conduit->finish(Conduit::InputDirection  ,Conduit::Correct)   ;
     }                                                                       ;
     if ( 0 != stream->outputUnit )                                          {
       stream->conduit->finish(Conduit::OutputDirection ,Conduit::Correct)   ;
     }                                                                       ;
   }                                                                         ;
}

long computeRingBufferSize( const StreamParameters * inputParameters       ,
                            const StreamParameters * outputParameters      ,
                            long                     inputFramesPerBuffer  ,
                            long                     outputFramesPerBuffer ,
                            double                   sampleRate            )
{
  long   ringSize                                                            ;
  int    index                                                               ;
  int    i                                                                   ;
  double latency                                                             ;
  long   framesPerBuffer                                                     ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "%s : %d\n" , __FUNCTION__ , __LINE__ ) )                      ;
  assert ( inputParameters || outputParameters )                             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaAND ( CaNotNull(outputParameters) , CaNotNull(inputParameters) )  ) {
    latency = MAX( inputParameters->suggestedLatency, outputParameters->suggestedLatency );
    framesPerBuffer = MAX( inputFramesPerBuffer, outputFramesPerBuffer )     ;
  } else
  if ( CaNotNull ( outputParameters ) )                                      {
    latency         = outputParameters->suggestedLatency                     ;
    framesPerBuffer = outputFramesPerBuffer                                  ;
  } else                                                                     {
    /* we have inputParameters                                              */
    latency         = inputParameters->suggestedLatency                      ;
    framesPerBuffer = inputFramesPerBuffer                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ringSize = (long) ( latency * sampleRate * 2 + .5)                         ;
  gPrintf ( ( "suggested latency : %d\n", (int) ( latency * sampleRate ) ) ) ;
  if( ringSize < framesPerBuffer * 3 ) ringSize = framesPerBuffer * 3        ;
  gPrintf ( ( "framesPerBuffer:%d\n",(int)framesPerBuffer ) )                ;
  gPrintf ( ( "Ringbuffer size (1): %d\n", (int)ringSize  ) )                ;
  /* make sure it's at least 4                                              */
  ringSize = MAX ( ringSize , 4 )                                            ;
  /* round up to the next power of 2                                        */
  index = -1                                                                 ;
  for ( i=0; i<sizeof(long)*8; ++i ) if ( ringSize >> i & 0x01 ) index = i   ;
  assert ( index > 0 )                                                       ;
  if ( ringSize <= ( 0x01 << index ) ) ringSize = 0x01 <<   index            ;
                                  else ringSize = 0x01 << ( index + 1 )      ;
  gPrintf ( ( "Final Ringbuffer size (2): %d\n" , (int)ringSize ) )          ;
  return ringSize                                                            ;
}

OSStatus propertyProc                         (
           AudioDeviceID         inDevice     ,
           UInt32                inChannel    ,
           Boolean               isInput      ,
           AudioDevicePropertyID inPropertyID ,
           void                * inClientData )
{
  return noErr ;
}

CaError AudioDeviceSetPropertyNowAndWaitForChange  (
          AudioDeviceID         inDevice           ,
          UInt32                inChannel          ,
          Boolean               isInput            ,
          AudioDevicePropertyID inPropertyID       ,
          UInt32                inPropertyDataSize ,
          const void          * inPropertyData     ,
          void                * outPropertyData    )
{
   OSStatus macErr;
   UInt32 outPropertyDataSize = inPropertyDataSize;

   /* First, see if it already has that value. If so, return. */
   macErr = AudioDeviceGetProperty( inDevice, inChannel,
                                 isInput, inPropertyID,
                                 &outPropertyDataSize, outPropertyData );
   if( macErr ) {
      memset( outPropertyData, 0, inPropertyDataSize );
      goto failMac;
   }
   if( inPropertyDataSize!=outPropertyDataSize )
      return InternalError;
   if( 0==memcmp( outPropertyData, inPropertyData, outPropertyDataSize ) )
      return NoError;

   macErr = AudioDeviceAddPropertyListener( inDevice, inChannel, isInput,
                                   inPropertyID, propertyProc,
                                   NULL );
   if( macErr )
      /* we couldn't add a listener. */
      goto failMac;

   /* set property */
   macErr  = AudioDeviceSetProperty( inDevice, NULL, inChannel,
                                 isInput, inPropertyID,
                                 inPropertyDataSize, inPropertyData );
   if( macErr )
      goto failMac;

   struct timeval tv1, tv2;
   gettimeofday( &tv1, NULL );
   memcpy( &tv2, &tv1, sizeof( struct timeval ) );
   while( tv2.tv_sec - tv1.tv_sec < 30 ) {
      /* now read the property back out */
      macErr = AudioDeviceGetProperty( inDevice, inChannel,
                                    isInput, inPropertyID,
                                    &outPropertyDataSize, outPropertyData );
      if( macErr ) {
         memset( outPropertyData, 0, inPropertyDataSize );
         goto failMac;
      }
      /* and compare... */
      if( 0==memcmp( outPropertyData, inPropertyData, outPropertyDataSize ) ) {
         AudioDeviceRemovePropertyListener( inDevice, inChannel, isInput, inPropertyID, propertyProc );
         return NoError ;
      }
      /* No match yet, so let's sleep and try again. */
      Timer::Sleep( 100 ) ;
      gettimeofday( &tv2, NULL );
   }
  gPrintf ( ( "Timeout waiting for device setting.\n" ) )                    ;
  AudioDeviceRemovePropertyListener( inDevice, inChannel, isInput, inPropertyID, propertyProc );
  return NoError                                                             ;
failMac                                                                      :
  AudioDeviceRemovePropertyListener( inDevice, inChannel, isInput, inPropertyID, propertyProc );
  return ERR( macErr )                                                       ;
}

CaError setBestSampleRateForDevice         (
          const AudioDeviceID device       ,
          const bool          isOutput     ,
          const bool          requireExact ,
          const Float64       desiredSrate )
{
  const bool isInput = isOutput ? 0 : 1;
  Float64 srate;
  UInt32 propsize = sizeof( Float64 );
  OSErr err;
  AudioValueRange *ranges;
  int i=0;
  Float64 max  = -1; /*the maximum rate available*/
  Float64 best = -1; /*the lowest sample rate still greater than desired rate*/
  gPrintf ( ( "Setting sample rate for device %ld to %g.\n",device,(float)desiredSrate ) ) ;
   /* -- try setting the sample rate -- */
   srate = 0;
   err = AudioDeviceSetPropertyNowAndWaitForChange(
                                 device, 0, isInput,
                                 kAudioDevicePropertyNominalSampleRate,
                                 propsize, &desiredSrate, &srate );

   /* -- if the rate agrees, and was changed, we are done -- */
   if( srate != 0 && srate == desiredSrate )
      return NoError;
   /* -- if the rate agrees, and we got no errors, we are done -- */
   if( !err && srate == desiredSrate )
      return NoError;
   /* -- we've failed if the rates disagree and we are setting input -- */
   if( requireExact )
      return InvalidSampleRate;

   /* -- generate a list of available sample rates -- */
   err = AudioDeviceGetPropertyInfo( device, 0, isInput,
                                kAudioDevicePropertyAvailableNominalSampleRates,
                                &propsize, NULL );
   if( err )
      return ERR( err );
   ranges = (AudioValueRange *)calloc( 1, propsize );
   if( !ranges ) return InsufficientMemory;
   err = AudioDeviceGetProperty( device, 0, isInput,
                                kAudioDevicePropertyAvailableNominalSampleRates,
                                &propsize, ranges );
   if( err )
   {
      free( ranges );
      return ERR( err );
   }
  gPrintf ( ( "Requested sample rate of %g was not available.\n", (float)desiredSrate) ) ;
  gPrintf ( ( "%lu Available Sample Rates are:\n",propsize/sizeof(AudioValueRange)) ) ;
  gPrintf ( ( "-----\n"                                    ) ) ;
  /* -- now pick the best available sample rate -- */
   for( i=0; i<propsize/sizeof(AudioValueRange); ++i )
   {
      if( ranges[i].mMaximum > max ) max = ranges[i].mMaximum;
      if( ranges[i].mMinimum > desiredSrate ) {
         if( best < 0 )
            best = ranges[i].mMinimum;
         else if( ranges[i].mMinimum < best )
            best = ranges[i].mMinimum;
      }
   }
  if ( best < 0 ) best = max                                                 ;
  gPrintf ( ( "Maximum Rate %g. best is %g.\n", max, best  ) )               ;
   free( ranges );

   /* -- set the sample rate -- */
   propsize = sizeof( best );
   srate = 0;
   err = AudioDeviceSetPropertyNowAndWaitForChange(
                                 device, 0, isInput,
                                 kAudioDevicePropertyNominalSampleRate,
                                 propsize, &best, &srate );

   /* -- if the set rate matches, we are done -- */
   if( srate != 0 && srate == best )
      return NoError;

   if( err )
      return ERR( err );
   /* -- otherwise, something wierd happened: we didn't set the rate, and we got no errors. Just bail. */
   return InternalError;
}

CaError setBestFramesPerBuffer( const AudioDeviceID device,
                                const bool isOutput,
                                UInt32 requestedFramesPerBuffer,
                                UInt32 *actualFramesPerBuffer )
{
    UInt32 afpb;
    const bool isInput = !isOutput;
    UInt32 propsize = sizeof(UInt32);
    OSErr err;
    AudioValueRange range;

    if( actualFramesPerBuffer == NULL )
    {
        actualFramesPerBuffer = &afpb;
    }

    /* -- try and set exact FPB -- */
    err = AudioDeviceSetProperty( device, NULL, 0, isInput,
                                 kAudioDevicePropertyBufferFrameSize,
                                 propsize, &requestedFramesPerBuffer);
    err = AudioDeviceGetProperty( device, 0, isInput,
                           kAudioDevicePropertyBufferFrameSize,
                           &propsize, actualFramesPerBuffer);
    if( err )
    {
        return ERR( err );
    }
    // Did we get the size we asked for?
    if( *actualFramesPerBuffer == requestedFramesPerBuffer )
    {
        return NoError; /* we are done */
    }

    // Clip requested value against legal range for the device.
    propsize = sizeof(AudioValueRange);
    err = AudioDeviceGetProperty( device, 0, isInput,
                                kAudioDevicePropertyBufferFrameSizeRange,
                                &propsize, &range );
    if( err )
    {
      return ERR( err );
    }
    if( requestedFramesPerBuffer < range.mMinimum )
    {
        requestedFramesPerBuffer = range.mMinimum;
    }
    else if( requestedFramesPerBuffer > range.mMaximum )
    {
        requestedFramesPerBuffer = range.mMaximum;
    }

   /* --- set the buffer size (ignore errors) -- */
    propsize = sizeof( UInt32 );
   err = AudioDeviceSetProperty( device, NULL, 0, isInput,
                                 kAudioDevicePropertyBufferFrameSize,
                                 propsize, &requestedFramesPerBuffer );
   /* --- read the property to check that it was set -- */
   err = AudioDeviceGetProperty( device, 0, isInput,
                                 kAudioDevicePropertyBufferFrameSize,
                                 &propsize, actualFramesPerBuffer );

   if( err )
      return ERR( err );
   return NoError;
}

OSStatus xrunCallback(
    AudioDeviceID inDevice,
    UInt32 inChannel,
    Boolean isInput,
    AudioDevicePropertyID inPropertyID,
    void* inClientData)
{
   CaMacXRunListNode *node = (CaMacXRunListNode *) inClientData;

   int ret = pthread_mutex_trylock( &xrunMutex ) ;

   if( ret == 0 ) {

      node = node->next ; //skip the first node

      for( ; node; node=node->next ) {
         CoreAudioStream *stream = node->stream;

         if( stream->state != CoreAudioStream::ACTIVE )
            continue; //if the stream isn't active, we don't care if the device is dropping

         if( isInput ) {
            if( stream->inputDevice == inDevice )
               OSAtomicOr32( StreamIO::InputOverflow, &stream->xrunFlags );
         } else {
            if( stream->outputDevice == inDevice )
               OSAtomicOr32( StreamIO::OutputUnderflow, &stream->xrunFlags );
         }
      }

      pthread_mutex_unlock( &xrunMutex );
   }
   return 0;
}

int initializeXRunListenerList(void)
{
  xRunListSize = 0                                                  ;
  ::bzero( (void *) &firstXRunListNode, sizeof(firstXRunListNode) ) ;
  return ::pthread_mutex_init ( &xrunMutex , NULL )                 ;
}

int destroyXRunListenerList(void)
{
  CaMacXRunListNode * node = firstXRunListNode . next ;
  while ( NULL != node )                              {
    CaMacXRunListNode * tmp = node                    ;
    node = node->next                                 ;
    free ( tmp )                                      ;
  }                                                   ;
  xRunListSize = 0                                    ;
  return ::pthread_mutex_destroy ( &xrunMutex )       ;
}

void * addToXRunListenerList(void * stream)
{
  ::pthread_mutex_lock   ( &xrunMutex )                                 ;
  CaMacXRunListNode * newNode                                           ;
  newNode = (CaMacXRunListNode *) malloc( sizeof( CaMacXRunListNode ) ) ;
  newNode->stream = (CoreAudioStream *) stream                          ;
  newNode->next   = firstXRunListNode.next                              ;
  firstXRunListNode.next = newNode                                      ;
  ::pthread_mutex_unlock ( &xrunMutex )                                 ;
  return &firstXRunListNode                                             ;
}

int removeFromXRunListenerList(void * stream)
{
   pthread_mutex_lock( &xrunMutex );
   CaMacXRunListNode *node, *prev;
   prev = &firstXRunListNode;
   node = firstXRunListNode.next;
   while( node ) {
      if( node->stream == stream ) {
         //found it:
         --xRunListSize;
         prev->next = node->next;
         free( node );
         pthread_mutex_unlock( &xrunMutex );
         return xRunListSize;
      }
      prev = prev->next;
      node = node->next;
   }

   pthread_mutex_unlock( &xrunMutex );
   // failure
   return xRunListSize;
}

void SetupStreamInfo(  CoreAudioStreamInfo *data, const unsigned long flags )
{
  ::bzero( data, sizeof( CoreAudioStreamInfo ) ) ;
  data->size = sizeof( CoreAudioStreamInfo )     ;
  data->hostApiType    = CoreAudio               ;
  data->version        = 0x01                    ;
  data->flags          = flags                   ;
  data->channelMap     = NULL                    ;
  data->channelMapSize = 0                       ;
}

void SetupChannelMap( CoreAudioStreamInfo * data           ,
                      const SInt32        * channelMap     ,
                      const unsigned long   channelMapSize )
{
  data->channelMap     = channelMap     ;
  data->channelMapSize = channelMapSize ;
}

static bool ensureChannelNameSize(int size)
{
  if ( size < channelNameSize ) return true                   ;
  Allocator :: free ( channelName )                           ;
  channelNameSize = size                                      ;
  channelName     = (char *) Allocator::allocate ( size + 1 ) ;
  if ( CaIsNull ( channelName ) )                             {
    channelNameSize = 0                                       ;
    return false                                              ;
  }                                                           ;
  return true                                                 ;
}

const char * GetChannelName( int device, int channelIndex, bool input )
{
   struct HostApi *hostApi;
   CaError err;
   OSStatus error;
   err = Core::GetHostApi( &hostApi, CoreAudio );
   assert(err == NoError);
   if( err != NoError )
      return NULL;
   CoreAudioHostApi *macCoreHostApi = (CoreAudioHostApi *)hostApi;
   AudioDeviceID hostApiDevice = macCoreHostApi->devIds[device];

   UInt32 size = 0;

   error = AudioDeviceGetPropertyInfo( hostApiDevice,
                                       channelIndex + 1,
                                       input,
                                       kAudioDevicePropertyChannelName,
                                       &size,
                                       NULL );
   if( error ) {
      //try the CFString
      CFStringRef name;
      bool isDeviceName = false;
      size = sizeof( name );
      error = AudioDeviceGetProperty( hostApiDevice,
                                      channelIndex + 1,
                                      input,
                                      kAudioDevicePropertyChannelNameCFString,
                                      &size,
                                      &name );
      if( error ) { //as a last-ditch effort, get the device name. Later we'll append the channel number.
         size = sizeof( name );
         error = AudioDeviceGetProperty( hostApiDevice,
                                      channelIndex + 1,
                                      input,
                                      kAudioDevicePropertyDeviceNameCFString,
                                      &size,
                                      &name );
         if( error )
            return NULL;
         isDeviceName = true;
      }
      if( isDeviceName ) {
         name = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%@: %d"), name, channelIndex + 1 );
      }

      CFIndex length = CFStringGetLength(name);
      while( ensureChannelNameSize( length * sizeof(UniChar) + 1 ) ) {
         if( CFStringGetCString( name, channelName, channelNameSize, kCFStringEncodingUTF8 ) ) {
            if( isDeviceName )
               CFRelease( name );
            return channelName;
         }
         if( length == 0 )
            ++length;
         length *= 2;
      }
      if( isDeviceName )
         CFRelease( name );
      return NULL;
   }

   //continue with C string:
   if( !ensureChannelNameSize( size ) )
      return NULL;

   error = AudioDeviceGetProperty( hostApiDevice,
                                   channelIndex + 1,
                                   input,
                                   kAudioDevicePropertyChannelName,
                                   &size,
                                   channelName );

   if( error ) {
      ERR( error );
      return NULL;
   }
   return channelName;
}

CaError GetBufferSizeRange                  (
          CaDeviceIndex device              ,
          long        * minBufferSizeFrames ,
          long        * maxBufferSizeFrames )
{
  CaError            result                                                  ;
  CoreAudioHostApi * hostApi                                                 ;
  result = Core :: GetHostApi ( (HostApi **)&hostApi , CoreAudio )           ;
  if ( CaIsWrong ( result ) ) return result                                  ;
  CaDeviceIndex hostApiDeviceIndex                                           ;
  result =  hostApi -> ToHostDeviceIndex ( &hostApiDeviceIndex , device )    ;
  if ( CaIsCorrect ( result ) )                                              {
    AudioDeviceID   macCoreDeviceId = hostApi->devIds[hostApiDeviceIndex]    ;
    AudioValueRange audioRange                                               ;
    UInt32          propSize        = sizeof( audioRange )                   ;
    Boolean         isInput         = 0                                      ;
    // return the size range for the output scope unless we only have inputs
    if ( hostApi->deviceInfos[hostApiDeviceIndex]->maxOutputChannels == 0 )  {
      isInput = 1                                                            ;
    }                                                                        ;
    result = WARNING ( AudioDeviceGetProperty                                (
                         macCoreDeviceId                                     ,
                         0                                                   ,
                         isInput                                             ,
                         kAudioDevicePropertyBufferFrameSizeRange            ,
                         &propSize                                           ,
                         &audioRange                                     ) ) ;
    *minBufferSizeFrames = audioRange.mMinimum                               ;
    *maxBufferSizeFrames = audioRange.mMaximum                               ;
  }                                                                          ;
  return result                                                              ;
}

static CaError ClipToDeviceBufferSize( AudioDeviceID macCoreDeviceId,
                                    int isInput, UInt32 desiredSize, UInt32 *allowedSize )
{
    UInt32 resultSize = desiredSize;
    AudioValueRange audioRange;
    UInt32 propSize = sizeof( audioRange );
    CaError err = WARNING(AudioDeviceGetProperty( macCoreDeviceId, 0, isInput, kAudioDevicePropertyBufferFrameSizeRange, &propSize, &audioRange ) );
    resultSize = MAX( resultSize, audioRange.mMinimum );
    resultSize = MIN( resultSize, audioRange.mMaximum );
    *allowedSize = resultSize;
    return err;
}

static CaError CalculateFixedDeviceLatency( AudioDeviceID macCoreDeviceId, int isInput, UInt32 *fixedLatencyPtr )
{
    CaError err;
    UInt32 propSize;
    UInt32 deviceLatency;
    UInt32 streamLatency;
    UInt32 safetyOffset;
    AudioStreamID streamIDs[1];

    propSize = sizeof(streamIDs);
    err  = WARNING(AudioDeviceGetProperty(macCoreDeviceId, 0, isInput, kAudioDevicePropertyStreams, &propSize, &streamIDs[0]));
    if( err != NoError ) goto error;
    if( propSize == sizeof(AudioStreamID) )
    {
        propSize = sizeof(UInt32);
        err  = WARNING(AudioStreamGetProperty(streamIDs[0], 0, kAudioStreamPropertyLatency, &propSize, &streamLatency));
    }

    propSize = sizeof(UInt32);
    err = WARNING(AudioDeviceGetProperty(macCoreDeviceId, 0, isInput, kAudioDevicePropertySafetyOffset, &propSize, &safetyOffset));
    if( err != NoError ) goto error;

    propSize = sizeof(UInt32);
    err = WARNING(AudioDeviceGetProperty(macCoreDeviceId, 0, isInput, kAudioDevicePropertyLatency, &propSize, &deviceLatency));
    if( err != NoError ) goto error;

    *fixedLatencyPtr = deviceLatency + streamLatency + safetyOffset;
    return err;
error:
    return err;
}

/* =================================================================================================== */
static CaError CalculateDefaultDeviceLatencies( AudioDeviceID macCoreDeviceId,
                                               int isInput, UInt32 *lowLatencyFramesPtr,
                                               UInt32 *highLatencyFramesPtr )
{
    UInt32 propSize;
    UInt32 bufferFrames = 0;
    UInt32 fixedLatency = 0;
    UInt32 clippedMinBufferSize = 0;

    CaError err = CalculateFixedDeviceLatency( macCoreDeviceId, isInput, &fixedLatency );
    if( err != NoError ) goto error;

    err = ClipToDeviceBufferSize( macCoreDeviceId, isInput, CA_MAC_SMALL_BUFFER_SIZE, &clippedMinBufferSize );
    if( err != NoError ) goto error;

    propSize = sizeof(UInt32);
    err = WARNING(AudioDeviceGetProperty(macCoreDeviceId, 0, isInput, kAudioDevicePropertyBufferFrameSize, &propSize, &bufferFrames));
    if( err != NoError ) goto error;

    *lowLatencyFramesPtr = fixedLatency + clippedMinBufferSize;
    *highLatencyFramesPtr = fixedLatency + bufferFrames;

    return err;
error:
    return err;
}

/* =================================================================================================== */

static OSStatus AudioDevicePropertyActualSampleRateListenerProc( AudioDeviceID inDevice, UInt32 inChannel, Boolean isInput, AudioDevicePropertyID inPropertyID, void *inClientData )
{
  CoreAudioStream * stream = (CoreAudioStream *)inClientData                 ;
  assert ( stream->isValid() )                                               ;
  OSStatus osErr                                                             ;
  osErr = stream -> UpdateSampleRateFromDeviceProperty                       (
            inDevice                                                         ,
            isInput                                                          ,
            kAudioDevicePropertyActualSampleRate                           ) ;
  if ( osErr == noErr )                                                      {
    stream -> UpdateTimeStampOffsets ( )                                     ;
  }                                                                          ;
  return osErr                                                               ;
}

/* ================================================================================= */
static OSStatus QueryUInt32DeviceProperty( AudioDeviceID deviceID, Boolean isInput, AudioDevicePropertyID propertyID, UInt32 *outValue )
{
    UInt32 propertyValue = 0;
    UInt32 propertySize = sizeof(UInt32);
    OSStatus osErr = AudioDeviceGetProperty( deviceID, 0, isInput, propertyID, &propertySize, &propertyValue);
    if( osErr == noErr )
    {
        *outValue = propertyValue;
    }
    return osErr;
}

static OSStatus AudioDevicePropertyGenericListenerProc( AudioDeviceID inDevice, UInt32 inChannel, Boolean isInput, AudioDevicePropertyID inPropertyID, void *inClientData )
{
    OSStatus osErr = noErr;
    CoreAudioStream *stream = (CoreAudioStream*)inClientData;

    // Make sure the callback is operating on a stream that is still valid!
    assert( stream->magic == Stream::STREAM_MAGIC );

    CoreAudioDeviceInfo *deviceProperties = isInput ? &stream->inputProperties : &stream->outputProperties;
    UInt32 *valuePtr = NULL;
    switch( inPropertyID )
    {
        case kAudioDevicePropertySafetyOffset:
            valuePtr = &deviceProperties->safetyOffset;
            break;

        case kAudioDevicePropertyLatency:
            valuePtr = &deviceProperties->deviceLatency;
            break;

        case kAudioDevicePropertyBufferFrameSize:
            valuePtr = &deviceProperties->bufferFrameSize;
            break;
    }
    if( valuePtr != NULL )
    {
        osErr = QueryUInt32DeviceProperty( inDevice, isInput, inPropertyID, valuePtr );
        if( osErr == noErr )
        {
            stream->UpdateTimeStampOffsets(  );
        }
    }
    return osErr;
}

static OSStatus ringBufferIOProc( AudioConverterRef inAudioConverter,
                             UInt32*ioDataSize,
                             void** outData,
                             void*inUserData )
{
  void       * dummyData                                                     ;
  CaInt32      dummySize                                                     ;
  RingBuffer * rb = (RingBuffer *) inUserData                                ;
  if ( rb -> ReadAvailable ( ) == 0 ) {
      *outData = NULL;
      *ioDataSize = 0;
      return RING_BUFFER_EMPTY;
   }
   assert(sizeof(UInt32) == sizeof(CaUint32));
   assert( ( (*ioDataSize) / rb->elementSizeBytes ) * rb->elementSizeBytes == (*ioDataSize) ) ;
   (*ioDataSize) /= rb->elementSizeBytes ;
   rb->ReadRegions(*ioDataSize,
                                    outData, (CaInt32 *)ioDataSize,
                                    &dummyData, &dummySize );
   assert( *ioDataSize );
   rb->AdvanceReadIndex(*ioDataSize );
   (*ioDataSize) *= rb->elementSizeBytes ;
   return noErr;
}

static OSStatus AudioIOProc                                   (
                  void                       * inRefCon       ,
                  AudioUnitRenderActionFlags * ioActionFlags  ,
                  const AudioTimeStamp       * inTimeStamp    ,
                  UInt32                       inBusNumber    ,
                  UInt32                       inNumberFrames ,
                  AudioBufferList            * ioData         )
{
  unsigned long     framesProcessed       = 0                                ;
  CoreAudioStream * stream                = (CoreAudioStream*)inRefCon       ;
  const bool        isRender              = inBusNumber == OUTPUT_ELEMENT    ;
  int               callbackResult        = Conduit::Continue                ;
  double            hostTimeStampInPaTime = HOST_TIME_TO_CA_TIME(inTimeStamp->mHostTime);
  stream -> cpuLoadMeasurer . Begin ( )                                      ;

    if( pthread_mutex_trylock( &stream->timingInformationMutex ) == 0 ){
        /* snapshot the ioproc copy of timing information */
        stream->timestampOffsetCombined_ioProcCopy = stream->timestampOffsetCombined;
        stream->timestampOffsetInputDevice_ioProcCopy = stream->timestampOffsetInputDevice;
        stream->timestampOffsetOutputDevice_ioProcCopy = stream->timestampOffsetOutputDevice;
        pthread_mutex_unlock( &stream->timingInformationMutex );
    }

    if( isRender )
    {
        if( stream->inputUnit ) /* full duplex */
        {
            if( stream->inputUnit == stream->outputUnit ) /* full duplex AUHAL IOProc */
            {
                stream->conduit->Input . AdcDac = hostTimeStampInPaTime -
                    (stream->timestampOffsetCombined_ioProcCopy + stream->timestampOffsetInputDevice_ioProcCopy);
                stream->conduit->Output . AdcDac      = hostTimeStampInPaTime + stream->timestampOffsetOutputDevice_ioProcCopy;
                stream->conduit->Input  . CurrentTime = HOST_TIME_TO_CA_TIME( AudioGetCurrentHostTime() );
                stream->conduit->Output . CurrentTime = stream->conduit->Input . CurrentTime ;
            }
            else /* full duplex with ring-buffer from a separate input AUHAL ioproc */
            {
                stream->conduit->Input . AdcDac = hostTimeStampInPaTime -
                    (stream->timestampOffsetCombined_ioProcCopy + stream->timestampOffsetInputDevice_ioProcCopy);
                stream->conduit->Output . AdcDac      = hostTimeStampInPaTime + stream->timestampOffsetOutputDevice_ioProcCopy;
                stream->conduit->Input  . CurrentTime = HOST_TIME_TO_CA_TIME( AudioGetCurrentHostTime() );
                stream->conduit->Output . CurrentTime = stream->conduit->Input . CurrentTime ;
            }
        }
        else /* output only */
        {
            stream->conduit->Input  . AdcDac      = 0;
            stream->conduit->Output . CurrentTime = HOST_TIME_TO_CA_TIME( AudioGetCurrentHostTime() );
            stream->conduit->Output . AdcDac      = hostTimeStampInPaTime + stream->timestampOffsetOutputDevice_ioProcCopy;
        }
    }
    else /* input only */
    {
        stream->conduit->Input  . AdcDac      = hostTimeStampInPaTime - stream->timestampOffsetInputDevice_ioProcCopy;
        stream->conduit->Input  . CurrentTime = HOST_TIME_TO_CA_TIME( AudioGetCurrentHostTime() );
        stream->conduit->Output . AdcDac      = 0;
    }

   //printf( "---%g, %g, %g\n", timeInfo.inputBufferAdcTime, timeInfo.currentTime, timeInfo.outputBufferDacTime );

   if( isRender && stream->inputUnit == stream->outputUnit
                && !stream->inputSRConverter )
   {
      OSStatus err = 0;
       unsigned long frames;
       long bytesPerFrame = sizeof( float ) * ioData->mBuffers[0].mNumberChannels;

      /* -- start processing -- */
      stream->conduit->Input  . StatusFlags = stream->xrunFlags ;
      stream->conduit->Output . StatusFlags = stream->xrunFlags ;
      stream->bufferProcessor.Begin(stream->conduit) ;
      stream->xrunFlags = 0; //FIXME: this flag also gets set outside by a callback, which calls the xrunCallback function. It should be in the same thread as the main audio callback, but the apple docs just use the word "usually" so it may be possible to loose an xrun notification, if that callback happens here.

      /* -- compute frames. do some checks -- */
      assert( ioData->mNumberBuffers == 1 );
      assert( ioData->mBuffers[0].mNumberChannels == stream->userOutChan );

      frames = ioData->mBuffers[0].mDataByteSize / bytesPerFrame;
      /* -- copy and process input data -- */
      err= AudioUnitRender(stream->inputUnit,
                    ioActionFlags,
                    inTimeStamp,
                    INPUT_ELEMENT,
                    inNumberFrames,
                    &stream->inputAudioBufferList );
      /* FEEDBACK: I'm not sure what to do when this call fails. There's nothing in the PA API to
       * do about failures in the callback system. */
      assert( !err );

      stream->bufferProcessor.setInputFrameCount(0,frames );
      stream->bufferProcessor.setInterleavedInputChannels(
                          0,
                          0,
                          stream->inputAudioBufferList.mBuffers[0].mData,
                          stream->inputAudioBufferList.mBuffers[0].mNumberChannels);
      /* -- Copy and process output data -- */
      stream->bufferProcessor.setOutputFrameCount(0,frames );
      stream->bufferProcessor.setInterleavedOutputChannels(
                                        0,
                                        0,
                                        ioData->mBuffers[0].mData,
                                        ioData->mBuffers[0].mNumberChannels);
      /* -- complete processing -- */
      framesProcessed = stream->bufferProcessor.End(&callbackResult) ;
   }
   else if( isRender )
   {
       unsigned long frames;
       long bytesPerFrame = sizeof( float ) * ioData->mBuffers[0].mNumberChannels;
      int xrunFlags = stream->xrunFlags;
      if( stream->state == CoreAudioStream::STOPPING || stream->state == CoreAudioStream::CALLBACK_STOPPED )
         xrunFlags = 0;

      /* -- start processing -- */
      stream->conduit->Output . StatusFlags = xrunFlags ;
      stream->bufferProcessor.Begin( stream->conduit ) ;
      stream->xrunFlags = 0; /* FEEDBACK: we only send flags to Buf Proc once */

      /* -- Copy and process output data -- */
      assert( ioData->mNumberBuffers == 1 );
      frames = ioData->mBuffers[0].mDataByteSize / bytesPerFrame;
      assert( ioData->mBuffers[0].mNumberChannels == stream->userOutChan );
      stream->bufferProcessor.setOutputFrameCount(0,frames );
      stream->bufferProcessor.setInterleavedOutputChannels(
                                     0,
                                     0,
                                     ioData->mBuffers[0].mData,
                                     ioData->mBuffers[0].mNumberChannels);

      /* -- copy and process input data, and complete processing -- */
      if( stream->inputUnit ) {
         const int flsz = sizeof( float );
         /* Here, we read the data out of the ring buffer, through the
            audio converter. */
         int inChan = stream->inputAudioBufferList.mBuffers[0].mNumberChannels;
         long bytesPerFrame = flsz * inChan;

         if( stream->inputSRConverter )
         {
               OSStatus err;
               UInt32 size;
               float data[ inChan * frames ];
               size = sizeof( data );
               err = AudioConverterFillBuffer(
                             stream->inputSRConverter,
                             ringBufferIOProc,
                             &stream->inputRingBuffer,
                             &size,
                             (void *)&data );
               if( err == RING_BUFFER_EMPTY )
               { /*the ring buffer callback underflowed */
                  err = 0;
                  bzero( ((char *)data) + size, sizeof(data)-size );
                  /* The ring buffer can underflow normally when the stream is stopping.
                   * So only report an error if the stream is active. */
                  if( stream->state == CoreAudioStream::ACTIVE )
                  {
                      stream->xrunFlags |= StreamIO::InputUnderflow;
                  }
               }
               ERR( err );
               assert( !err );

               stream->bufferProcessor.setInputFrameCount(0, frames );
               stream->bufferProcessor.setInterleavedInputChannels(
                                   0,
                                   0,
                                   data,
                                   inChan );
               framesProcessed = stream->bufferProcessor.End(&callbackResult );
         }
         else
         {
            /* Without the AudioConverter is actually a bit more complex
               because we have to do a little buffer processing that the
               AudioConverter would otherwise handle for us. */
            void *data1, *data2;
            CaInt32 size1, size2;
            CaInt32 framesReadable = stream->inputRingBuffer.ReadRegions(
                                             frames,
                                             &data1, &size1,
                                             &data2, &size2 );
            if( size1 == frames ) {
               /* simplest case: all in first buffer */
               stream->bufferProcessor.setInputFrameCount(0, frames );
               stream->bufferProcessor.setInterleavedInputChannels(
                                   0,
                                   0,
                                   data1,
                                   inChan );
               framesProcessed = stream->bufferProcessor.End(&callbackResult );
               stream->inputRingBuffer.AdvanceReadIndex(size1 );
            } else if( framesReadable < frames ) {

                long sizeBytes1 = size1 * bytesPerFrame;
                long sizeBytes2 = size2 * bytesPerFrame;
               /*we underflowed. take what data we can, zero the rest.*/
               unsigned char data[ frames * bytesPerFrame ];
               if( size1 > 0 )
               {
                   memcpy( data, data1, sizeBytes1 );
               }
               if( size2 > 0 )
               {
                   memcpy( data+sizeBytes1, data2, sizeBytes2 );
               }
               bzero( data+sizeBytes1+sizeBytes2, (frames*bytesPerFrame) - sizeBytes1 - sizeBytes2 );

               stream->bufferProcessor.setInputFrameCount(0, frames );
               stream->bufferProcessor.setInterleavedInputChannels(
                                   0,
                                   0,
                                   data,
                                   inChan );
               framesProcessed = stream->bufferProcessor.End(&callbackResult );
               stream->inputRingBuffer.AdvanceReadIndex(framesReadable) ;
               /* flag underflow */
               stream->xrunFlags |= StreamIO::InputUnderflow;
            } else {
               /*we got all the data, but split between buffers*/
               stream->bufferProcessor.setInputFrameCount(0,size1 );
               stream->bufferProcessor.setInterleavedInputChannels(
                                   0,
                                   0,
                                   data1,
                                   inChan );
               stream->bufferProcessor.setInputFrameCount(1,size2);
               stream->bufferProcessor.setInterleavedInputChannels(
                                   1,
                                   0,
                                   data2,
                                   inChan );
               framesProcessed = stream->bufferProcessor.End(&callbackResult );
               stream->inputRingBuffer.AdvanceReadIndex(framesReadable) ;
            }
         }
      } else {
         framesProcessed = stream->bufferProcessor.End(&callbackResult );
      }
   } else {
      /* ------------------ Input
       *
       * First, we read off the audio data and put it in the ring buffer.
       * if this is an input-only stream, we need to process it more,
       * otherwise, we let the output case deal with it.
       */
      OSStatus err = 0;
      int chan = stream->inputAudioBufferList.mBuffers[0].mNumberChannels ;
      /* FIXME: looping here may not actually be necessary, but it was something I tried in testing. */
      do {
         err= AudioUnitRender(stream->inputUnit,
                 ioActionFlags,
                 inTimeStamp,
                 INPUT_ELEMENT,
                 inNumberFrames,
                 &stream->inputAudioBufferList );
         if( err == -10874 )
            inNumberFrames /= 2;
      } while( err == -10874 && inNumberFrames > 1 );
      /* FEEDBACK: I'm not sure what to do when this call fails */
      ERR( err );
      assert( !err );
      if( stream->inputSRConverter || stream->outputUnit )
      {
         /* If this is duplex or we use a converter, put the data
            into the ring buffer. */
          CaInt32 framesWritten = stream->inputRingBuffer.Write(
                                            stream->inputAudioBufferList.mBuffers[0].mData,
                                            inNumberFrames );
         if( framesWritten != inNumberFrames )
         {
             stream->xrunFlags |= StreamIO::InputOverflow ;
         }
      }
      else
      {
         /* for simplex input w/o SR conversion,
            just pop the data into the buffer processor.*/
         stream->conduit->Input . StatusFlags = stream->xrunFlags ;
         stream->bufferProcessor.Begin(stream->conduit );
         stream->xrunFlags = 0;

         stream->bufferProcessor.setInputFrameCount(0,inNumberFrames);
         stream->bufferProcessor.setInterleavedInputChannels(0,
                             0,
                             stream->inputAudioBufferList.mBuffers[0].mData,
                             chan );
         framesProcessed = stream->bufferProcessor.End(&callbackResult );
      }
      if( !stream->outputUnit && stream->inputSRConverter )
      {
         float data[ chan * inNumberFrames ];
         OSStatus err;
         do { /*Run the buffer processor until we are out of data*/
            UInt32 size;
            long f;

            size = sizeof( data );
            err = AudioConverterFillBuffer(
                          stream->inputSRConverter,
                          ringBufferIOProc,
                          &stream->inputRingBuffer,
                          &size,
                          (void *)data );
            if( err != RING_BUFFER_EMPTY )
               ERR( err );
            assert( err == 0 || err == RING_BUFFER_EMPTY );

            f = size / ( chan * sizeof(float) );
            stream->bufferProcessor.setInputFrameCount(0,f);
            if( f )
            {
                stream->conduit->Input . StatusFlags = stream->xrunFlags ;
               stream->bufferProcessor.Begin(stream->conduit) ;
               stream->xrunFlags = 0;

               stream->bufferProcessor.setInterleavedInputChannels(
                                0,
                                0,
                                data,
                                chan );
               framesProcessed = stream->bufferProcessor.End(&callbackResult );
            }
         } while( ( ( Conduit::Continue == callbackResult )                 ||
                    ( Conduit::Postpone == callbackResult )               ) &&
                    ( ! err                               )               )  ;
      }
   }

   switch ( callbackResult ) {
   case Conduit::Continue: break;
   case Conduit::Postpone: break;
   case Conduit::Complete:
   case Conduit::Abort:
      stream->state = CoreAudioStream::CALLBACK_STOPPED ;
      if( stream->outputUnit )
         AudioOutputUnitStop(stream->outputUnit);
      if( stream->inputUnit )
         AudioOutputUnitStop(stream->inputUnit);
      break;
   }

   stream->cpuLoadMeasurer.End(framesProcessed) ;

   return noErr;
}

//////////////////////////////////////////////////////////////////////////////

CaError CoreAudioInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{
  CaError            result       = NoError                                  ;
  int                i                                                       ;
  CoreAudioHostApi * auhalHostApi = NULL                                     ;
  DeviceInfo       * deviceInfoArray                                         ;
  int                unixErr                                                 ;
  SInt32             major                                                   ;
  SInt32             minor                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "CoreAudioInitialize(): hostApiIndex=%d\n" , hostApiIndex ) )  ;
  ////////////////////////////////////////////////////////////////////////////
  Gestalt ( gestaltSystemVersionMajor , &major )                             ;
  Gestalt ( gestaltSystemVersionMinor , &minor )                             ;
  ////////////////////////////////////////////////////////////////////////////
  // Starting with 10.6 systems, the HAL notification thread is created internally
  if ( major == 10 && minor >= 6 )                                           {
    CFRunLoopRef               theRunLoop = NULL                             ;
    AudioObjectPropertyAddress theAddress = { kAudioHardwarePropertyRunLoop, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    OSStatus osErr = AudioObjectSetPropertyData (kAudioObjectSystemObject, &theAddress, 0, NULL, sizeof(CFRunLoopRef), &theRunLoop);
    if ( osErr != noErr ) goto error                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  unixErr = initializeXRunListenerList ( )                                   ;
  if ( 0 != unixErr )                                                        {
    return UNIX_ERR ( unixErr )                                              ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  auhalHostApi = new CoreAudioHostApi()                                      ;
  CaExprCorrect ( CaIsNull ( auhalHostApi ) , InsufficientMemory )           ;
  ////////////////////////////////////////////////////////////////////////////
  auhalHostApi->allocations = new AllocationGroup()                          ;
  CaExprCorrect ( CaIsNull(auhalHostApi->allocations) , InsufficientMemory ) ;
  ////////////////////////////////////////////////////////////////////////////
  auhalHostApi->devIds   = NULL                                              ;
  auhalHostApi->devCount = 0                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  result = auhalHostApi->gatherDeviceInfo( )                                 ;
  if ( result != NoError ) goto error                                        ;
  ////////////////////////////////////////////////////////////////////////////
  *hostApi                                 = (HostApi *)auhalHostApi         ;
  (*hostApi) -> info . structVersion       = 1                               ;
  (*hostApi) -> info . type                = CoreAudio                       ;
  (*hostApi) -> info . index               = hostApiIndex                    ;
  (*hostApi) -> info . name                = "Core Audio"                    ;
  (*hostApi) -> info . defaultInputDevice  = CaNoDevice                      ;
  (*hostApi) -> info . defaultOutputDevice = CaNoDevice                      ;
  (*hostApi) -> info . deviceCount         = 0                               ;
  ////////////////////////////////////////////////////////////////////////////
  if ( auhalHostApi->devCount > 0 )                                          {
    (*hostApi)->deviceInfos = new DeviceInfo * [ auhalHostApi->devCount ]    ;
    CaExprCorrect ( CaIsNull( (*hostApi)->deviceInfos ),InsufficientMemory ) ;
    /* allocate all device info structs in a contiguous block               */
    deviceInfoArray = (CoreAudioDeviceInfo *)new CoreAudioDeviceInfo [ auhalHostApi->devCount ] ;
    CaExprCorrect ( CaIsNull(deviceInfoArray) , InsufficientMemory )         ;
    for ( i = 0 ; i < auhalHostApi -> devCount ; ++i )                       {
      int err = auhalHostApi -> InitializeDeviceInfo                         (
                  & deviceInfoArray      [ i ]                               ,
                  auhalHostApi -> devIds [ i ]                               ,
                  hostApiIndex                                             ) ;
      if ( NoError == err )                                                  {
        /* copy some info and set the defaults                              */
        (*hostApi)->deviceInfos[(*hostApi)->info.deviceCount] = &deviceInfoArray[i];
        if (auhalHostApi->devIds[i] == auhalHostApi->defaultIn)              {
          (*hostApi)->info.defaultInputDevice = (*hostApi)->info.deviceCount ;
        }                                                                    ;
        if (auhalHostApi->devIds[i] == auhalHostApi->defaultOut)             {
          (*hostApi)->info.defaultOutputDevice = (*hostApi)->info.deviceCount;
        }                                                                    ;
        (*hostApi) -> info . deviceCount ++                                  ;
      } else                                                                 {
        /* there was an error. we need to shift the devices down, so we ignore this one */
        int j                                                                ;
        auhalHostApi -> devCount --                                          ;
        for ( j = i ; j < auhalHostApi -> devCount ; ++j )                   {
          auhalHostApi->devIds[j] = auhalHostApi->devIds[j+1]                ;
        }                                                                    ;
        i--                                                                  ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  return result                                                              ;
error                                                                        :
  if ( NULL != auhalHostApi )                                                {
    delete auhalHostApi                                                      ;
    auhalHostApi = NULL                                                      ;
  }                                                                          ;
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

MacBlio:: MacBlio(void)
{
  ringBufferFrames       = 0          ;
  inputSampleFormat      = cafNothing ;
  inputSampleSizeActual  = 0          ;
  inputSampleSizePow2    = 0          ;
  outputSampleFormat     = cafNothing ;
  outputSampleSizeActual = 0          ;
  outputSampleSizePow2   = 0          ;
  framesPerBuffer        = 0          ;
  inChan                 = 0          ;
  outChan                = 0          ;
  statusFlags            = 0          ;
  errors                 = NoError    ;
  isInputEmpty           = true       ;
  isOutputFull           = false      ;
}

MacBlio::~MacBlio(void)
{
}

CaError MacBlio::initializeBlioRingBuffers  (
          CaSampleFormat inputSampleFormat  ,
          CaSampleFormat outputSampleFormat ,
          size_t         framesPerBuffer    ,
          long           ringBufferSize     ,
          int            inChan             ,
          int            outChan            )
{
  void   * data                                                              ;
  int      result                                                            ;
  OSStatus err                                                               ;
  ////////////////////////////////////////////////////////////////////////////
  this->inputRingBuffer.buffer  = NULL                                       ;
  this->outputRingBuffer.buffer = NULL                                       ;
  this->ringBufferFrames        = ringBufferSize                             ;
  this->inputSampleFormat       = inputSampleFormat                          ;
  this->inputSampleSizeActual   = Core::SampleSize ( inputSampleFormat  )    ;
  this->inputSampleSizePow2     = Core::SampleSize ( inputSampleFormat  )    ;
  this->outputSampleFormat      = outputSampleFormat                         ;
  this->outputSampleSizeActual  = Core::SampleSize ( outputSampleFormat )    ;
  this->outputSampleSizePow2    = Core::SampleSize ( outputSampleFormat )    ;
  this->framesPerBuffer         = framesPerBuffer                            ;
  this->inChan                  = inChan                                     ;
  this->outChan                 = outChan                                    ;
  this->statusFlags             = 0                                          ;
  this->errors                  = NoError                                    ;
  this->isInputEmpty            = false                                      ;
  this->isOutputFull            = false                                      ;
  ////////////////////////////////////////////////////////////////////////////
  /* setup ring buffers                                                     */
  result = SetMacOSXError( pthread_mutex_init(&(this->inputMutex),NULL), 0 ) ;
  if ( NoError != result ) goto error                                        ;
  result = UNIX_ERR( pthread_cond_init( &(this->inputCond), NULL )         ) ;
  if ( NoError != result ) goto error                                        ;
  result = UNIX_ERR( pthread_mutex_init(&(this->outputMutex),NULL)         ) ;
  if ( NoError != result ) goto error                                        ;
  result = UNIX_ERR( pthread_cond_init( &(this->outputCond), NULL )        ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != inChan )                                                         {
    data = calloc ( ringBufferSize, this->inputSampleSizePow2*inChan )       ;
    if ( NULL == data )                                                      {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    err = inputRingBuffer . Initialize                                       (
            1                                                                ,
            ringBufferSize*this->inputSampleSizePow2*inChan                  ,
            data                                                           ) ;
    assert ( !err )                                                          ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != outChan )                                                        {
    data = calloc ( ringBufferSize, this->outputSampleSizePow2*outChan )     ;
    if ( NULL == data )                                                      {
      result = InsufficientMemory                                            ;
      goto error                                                             ;
    }                                                                        ;
    err = outputRingBuffer . Initialize                                      (
            1                                                                ,
            ringBufferSize*this->outputSampleSizePow2*outChan                ,
            data                                                           ) ;
    assert ( !err )                                                          ;
  }                                                                          ;
  result = resetBlioRingBuffers ( )                                          ;
  if ( result ) goto error                                                   ;
  return 0                                                                   ;
error                                                                        :
  destroyBlioRingBuffers ( )                                                 ;
  return result                                                              ;
}

CaError MacBlio::blioSetIsInputEmpty(bool isEmpty)
{
   CaError result = NoError ;
   if ( isEmpty == isInputEmpty ) goto done ;
   result = UNIX_ERR( pthread_mutex_lock( &inputMutex ) ) ;
   if ( result ) goto done ;
   isInputEmpty = isEmpty ;
   result = UNIX_ERR( pthread_mutex_unlock( &inputMutex ) ) ;
   if ( result ) goto done ;
   result = UNIX_ERR( pthread_cond_broadcast( &inputCond ) ) ;
   if ( result ) goto done ;
done:
   return result;
}

CaError MacBlio::blioSetIsOutputFull(bool isFull)
{
   CaError result = NoError ;
   if ( isFull == isOutputFull ) goto done;
   result = UNIX_ERR( pthread_mutex_lock( &outputMutex ) );
   if( result )
      goto done;
   isOutputFull = isFull;
   result = UNIX_ERR( pthread_mutex_unlock( &outputMutex ) );
   if( result ) goto done;
   result = UNIX_ERR( pthread_cond_broadcast( &outputCond ) );
   if( result )
      goto done;
 done:
   return result;
}

CaError MacBlio::resetBlioRingBuffers(void)
{
   int result;
   statusFlags = 0;
   if ( outputRingBuffer.buffer ) {
      outputRingBuffer.Flush();
      bzero( outputRingBuffer.buffer,outputRingBuffer.bufferSize );
      outputRingBuffer.AdvanceWriteIndex(ringBufferFrames*outputSampleSizeActual*outChan ) ;
      #warning what the hell is toAdvance in MacBlio::resetBlioRingBuffers
//      result = blioSetIsOutputFull( toAdvance == outputRingBuffer.bufferSize ) ;
//      if( result ) goto error;
   }
   if( inputRingBuffer.buffer ) {
      inputRingBuffer.Flush();
      bzero( inputRingBuffer.buffer,
             inputRingBuffer.bufferSize );
      result = blioSetIsInputEmpty( true );
      if( result )
         goto error;
   }
   return NoError;
 error:
   return result;
}

CaError MacBlio::destroyBlioRingBuffers(void)
{
  CaError result = NoError                                                   ;
  if ( NULL != inputRingBuffer.buffer  )                                     {
    ::free ( inputRingBuffer.buffer  )                                       ;
    result = UNIX_ERR ( pthread_mutex_destroy ( & inputMutex ) )             ;
    if ( 0 != result ) return result                                         ;
    result = UNIX_ERR ( pthread_cond_destroy  ( & inputCond  ) )             ;
    if ( 0 != result ) return result                                         ;
  }                                                                          ;
  inputRingBuffer.buffer = NULL                                              ;
  if ( NULL != outputRingBuffer.buffer )                                     {
    ::free ( outputRingBuffer.buffer )                                       ;
    result = UNIX_ERR ( pthread_mutex_destroy ( & outputMutex ) )            ;
    if ( 0 != result ) return result                                         ;
    result = UNIX_ERR ( pthread_cond_destroy  ( & outputCond  ) )            ;
    if ( 0 != result ) return result                                         ;
  }                                                                          ;
  outputRingBuffer.buffer = NULL                                             ;
  return result                                                              ;
}

void MacBlio::waitUntilBlioWriteBufferIsFlushed(void)
{
  if ( NULL == outputRingBuffer.buffer ) return               ;
  long avail = outputRingBuffer.WriteAvailable()              ;
  if ( avail <= 0 ) return                                    ;
  while ( avail != outputRingBuffer.bufferSize )              {
    if ( avail == 0 )                                         {
      Timer :: Sleep ( CA_MAC_BLIO_BUSY_WAIT_SLEEP_INTERVAL ) ;
    }                                                         ;
    avail = outputRingBuffer . WriteAvailable ( )             ;
    if ( avail <= 0 ) return                                  ;
  }                                                           ;
}

///////////////////////////////////////////////////////////////////////////////

CoreAudioStream:: CoreAudioStream (void)
                : Stream          (    )
{
  bufferProcessorIsInitialized           = 0                                 ;
  inputUnit                              = 0                                 ;
  outputUnit                             = 0                                 ;
  inputDevice                            = 0                                 ;
  outputDevice                           = 0                                 ;
  userInChan                             = 0                                 ;
  userOutChan                            = 0                                 ;
  inputFramesPerBuffer                   = 0                                 ;
  outputFramesPerBuffer                  = 0                                 ;
  xrunFlags                              = 0                                 ;
  state                                  = STOPPED                           ;
  sampleRate                             = 0                                 ;
  timingInformationMutexIsInitialized    = 0                                 ;
  timestampOffsetCombined                = 0                                 ;
  timestampOffsetInputDevice             = 0                                 ;
  timestampOffsetOutputDevice            = 0                                 ;
  timestampOffsetCombined_ioProcCopy     = 0                                 ;
  timestampOffsetInputDevice_ioProcCopy  = 0                                 ;
  timestampOffsetOutputDevice_ioProcCopy = 0                                 ;
  ////////////////////////////////////////////////////////////////////////////
  ::bzero ( &inputSRConverter       , sizeof(AudioConverterRef)            ) ;
  ::bzero ( &inputAudioBufferList   , sizeof(AudioBufferList)              ) ;
  ::bzero ( &startTime              , sizeof(AudioTimeStamp)               ) ;
  ::bzero ( &timingInformationMutex , sizeof(pthread_mutex_t)              ) ;
}

CoreAudioStream::~CoreAudioStream(void)
{
}

CaError CoreAudioStream::Start(void)
{
  OSStatus result = noErr                                                    ;
  dPrintf ( ( "Starting Core Audio\n" ) )                                    ;
  #define ERR_WRAP(mac_err) result = mac_err ; if ( result != noErr ) return ERR(result)
  /*FIXME: maybe want to do this on close/abort for faster start?           */
  bufferProcessor . Reset ( )                                                ;
  if ( CaNotNull ( inputSRConverter ) )                                      {
    ERR_WRAP ( AudioConverterReset ( inputSRConverter ) )                    ;
  }                                                                          ;
  /* -- start --                                                            */
  state = ACTIVE                                                             ;
  if ( CaNotNull ( inputUnit ) )                                             {
    ERR_WRAP( AudioOutputUnitStart(inputUnit) )                              ;
  }                                                                          ;
  if ( CaAND ( CaNotNull  ( outputUnit             )                         ,
               CaNotEqual ( outputUnit , inputUnit )                     ) ) {
    ERR_WRAP ( AudioOutputUnitStart(outputUnit) )                            ;
  }                                                                          ;
  #undef ERR_WRAP
  return NoError                                                             ;
}

CaError CoreAudioStream::Stop(void)
{
  OSStatus result = noErr                                                    ;
  CaError  paErr                                                             ;
  dPrintf ( ( "Stop Core Audio Stream\n" ) )                                 ;
  dPrintf ( ( "Waiting for BLIO.\n"      ) )                                 ;
  blio . waitUntilBlioWriteBufferIsFlushed ( )                               ;
  dPrintf ( ( "Stopping stream.\n" ) )                                       ;
  state = STOPPING                                                           ;
  #define ERR_WRAP(mac_err)                                                  \
    result = mac_err                                                       ; \
    if ( result != noErr ) return ERR(result)
  /* -- stop and reset --                                                   */
  if ( ( inputUnit == outputUnit ) && ( NULL != inputUnit ) )                {
     ERR_WRAP( AudioOutputUnitStop          ( inputUnit     )              ) ;
     ERR_WRAP( BlockWhileAudioUnitIsRunning ( inputUnit , 0 )              ) ;
     ERR_WRAP( BlockWhileAudioUnitIsRunning ( inputUnit , 1 )              ) ;
     ERR_WRAP( AudioUnitReset ( inputUnit , kAudioUnitScope_Global , 1 )   ) ;
     ERR_WRAP( AudioUnitReset ( inputUnit , kAudioUnitScope_Global , 0 )   ) ;
  } else                                                                     {
    if ( NULL != inputUnit  )                                                {
      ERR_WRAP ( AudioOutputUnitStop ( inputUnit )                         ) ;
      ERR_WRAP ( BlockWhileAudioUnitIsRunning ( inputUnit , 1 )            ) ;
      ERR_WRAP ( AudioUnitReset ( inputUnit , kAudioUnitScope_Global , 1 ) ) ;
    }                                                                        ;
    if ( NULL != outputUnit )                                                {
      ERR_WRAP ( AudioOutputUnitStop ( outputUnit )                        ) ;
      ERR_WRAP ( BlockWhileAudioUnitIsRunning ( outputUnit , 0 )           ) ;
      ERR_WRAP ( AudioUnitReset ( outputUnit, kAudioUnitScope_Global , 0 ) ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != inputRingBuffer.buffer )                                      {
    inputRingBuffer . Flush ( )                                              ;
    bzero ( (void *)inputRingBuffer.buffer , inputRingBuffer.bufferSize    ) ;
    if ( NULL != outputUnit )                                                {
      inputRingBuffer . AdvanceWriteIndex                                    (
        inputRingBuffer.bufferSize                                           /
        RING_BUFFER_ADVANCE_DENOMINATOR                                    ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  xrunFlags = 0                                                              ;
  state     = STOPPED                                                        ;
  paErr     = blio . resetBlioRingBuffers ( )                                ;
  if ( NoError != paErr ) return paErr                                       ;
  dPrintf ( ( "Core Audio Stream Stopped.\n" ) )                             ;
  #undef ERR_WRAP
  return NoError                                                             ;
}

CaError CoreAudioStream::Close(void)
{
  CaError result = NoError;

       if( outputUnit )
        {
            Boolean isInput = FALSE;
            CleanupDevicePropertyListeners( outputDevice, isInput );
        }

        if( inputUnit )
        {
            Boolean isInput = TRUE;
            CleanupDevicePropertyListeners( inputDevice, isInput );
        }

       if( outputUnit ) {
          int count = removeFromXRunListenerList( this );
          if( count == 0 )
             AudioDeviceRemovePropertyListener( outputDevice,
                                                0,
                                                false,
                                                kAudioDeviceProcessorOverload,
                                                xrunCallback );
       }
       if( inputUnit && outputUnit != inputUnit ) {
          int count = removeFromXRunListenerList( this ) ;
          if( count == 0 )
             AudioDeviceRemovePropertyListener( inputDevice,
                                                0,
                                                true,
                                                kAudioDeviceProcessorOverload,
                                                xrunCallback );
       }
       if( outputUnit && outputUnit != inputUnit ) {
          AudioUnitUninitialize( outputUnit );
          CloseComponent( outputUnit );
       }
       outputUnit = NULL;
       if( inputUnit )
       {
          AudioUnitUninitialize( inputUnit );
          CloseComponent( inputUnit );
          inputUnit = NULL;
       }
       if( inputRingBuffer.buffer )
          free( (void *) inputRingBuffer.buffer );
       inputRingBuffer.buffer = NULL;
       /*TODO: is there more that needs to be done on error
               from AudioConverterDispose?*/
       if( inputSRConverter )
          ERR( AudioConverterDispose( inputSRConverter ) );
       inputSRConverter = NULL;
       if( inputAudioBufferList.mBuffers[0].mData )
          free( inputAudioBufferList.mBuffers[0].mData );
       inputAudioBufferList.mBuffers[0].mData = NULL;

       result = blio.destroyBlioRingBuffers( );
       if( result )
          return result;
       if( bufferProcessorIsInitialized )
         bufferProcessor.Terminate();

       if( timingInformationMutexIsInitialized )
          pthread_mutex_destroy( &timingInformationMutex );

       Terminate() ;
  return result                                                              ;
}

CaError CoreAudioStream::Abort(void)
{
  return Stop ( ) ;
}

CaError CoreAudioStream::IsStopped(void)
{
  return ( STOPPED == state ) ? 1 : 0 ;
}

CaError CoreAudioStream::IsActive(void)
{
  return ( state == ACTIVE || state == STOPPING ) ;
}

CaTime CoreAudioStream::GetTime(void)
{
  return HOST_TIME_TO_CA_TIME ( AudioGetCurrentHostTime() ) ;
}

double CoreAudioStream::GetCpuLoad(void)
{
  return cpuLoadMeasurer . Value ( ) ;
}

CaInt32 CoreAudioStream::ReadAvailable(void)
{
  return blio . inputRingBuffer       . ReadAvailable( ) /
       ( blio . inputSampleSizeActual * blio . inChan  ) ;
}

CaInt32 CoreAudioStream::WriteAvailable(void)
{
  return blio . outputRingBuffer       . WriteAvailable ( ) /
       ( blio . outputSampleSizeActual * blio . outChan   ) ;
}

CaError CoreAudioStream::Read(void * buffer,unsigned long frames)
{
  CaError ret  = NoError                                                     ;
  char *  cbuf = (char *) buffer                                             ;
  dPrintf ( ( "%s : %d\n" , __FUNCTION__ , __LINE__ ) )                      ;
  while ( CaIsGreater ( frames , 0 ) )                                       {
    long avail                                                               ;
    long toRead                                                              ;
    do                                                                       {
      avail = blio.inputRingBuffer.ReadAvailable()                           ;
          if( avail == 0 ) {
             /**block when empty*/
             ret = UNIX_ERR( pthread_mutex_lock( &blio.inputMutex ) );
             if( ret )
                return ret;
             while( blio.isInputEmpty ) {
                ret = UNIX_ERR( pthread_cond_wait( &blio.inputCond, &blio.inputMutex ) );
                if( ret )
                   return ret;
             }
             ret = UNIX_ERR( pthread_mutex_unlock( &blio.inputMutex ) );
             if( ret )
                return ret;
          }
       } while( avail == 0 );
       toRead = MIN( avail, frames * blio.inputSampleSizeActual * blio.inChan );
       toRead -= toRead % blio.inputSampleSizeActual * blio.inChan ;
       blio.inputRingBuffer .Read( (void *)cbuf, toRead );
       cbuf += toRead;
       frames -= toRead / ( blio.inputSampleSizeActual * blio.inChan );

       if( toRead == avail ) {
          /* we just emptied the buffer, so we need to mark it as empty. */
          ret = blio.blioSetIsInputEmpty( true );
          if( ret )
             return ret;
          /* of course, in the meantime, the callback may have put some sats
             in, so
             so check for that, too, to avoid a race condition. */
          if( blio.inputRingBuffer.ReadAvailable() ) {
             blio.blioSetIsInputEmpty( false ) ;
             if( ret )
                return ret;
          }
       }
    }

    /*   Report either NoError or paInputOverflowed. */
    /*   may also want to report other errors, but this is non-standard. */
    ret = blio.statusFlags & StreamIO::InputOverflow;

    /* report underflow only once: */
    if( ret ) {
       OSAtomicAnd32( (uint32_t)(~StreamIO::InputOverflow), &blio.statusFlags );
       ret = InputOverflowed;
    }
  return ret                                                                 ;
}

CaError CoreAudioStream::Write(const void * buffer,unsigned long frames)
{
  CaError ret  = NoError                                                     ;
  char *  cbuf = (char *) buffer                                             ;
  dPrintf ( ( "%s : %d\n" , __FUNCTION__ , __LINE__ ) )                      ;
  while ( frames > 0 )                                                       {
    long avail = 0                                                           ;
    long toWrite                                                             ;
    do                                                                       {
          avail = blio.outputRingBuffer.WriteAvailable() ;
          if( avail == 0 ) {
             /*block while full*/
             ret = UNIX_ERR( pthread_mutex_lock( &blio.outputMutex ) );
             if( ret )
                return ret;
             while( blio.isOutputFull ) {
                ret = UNIX_ERR( pthread_cond_wait( &blio.outputCond, &blio.outputMutex ) );
                if( ret )
                   return ret;
             }
             ret = UNIX_ERR( pthread_mutex_unlock( &blio.outputMutex ) );
             if( ret )
                return ret;
          }
       } while( avail == 0 );

       toWrite = MIN( avail, frames * blio.outputSampleSizeActual * blio.outChan );
       toWrite -= toWrite % blio.outputSampleSizeActual * blio.outChan ;
       blio.outputRingBuffer.Write((void *)cbuf, toWrite );
       cbuf += toWrite;
       frames -= toWrite / ( blio.outputSampleSizeActual * blio.outChan );

       if( toWrite == avail ) {
          /* we just filled up the buffer, so we need to mark it as filled. */
          ret = blio . blioSetIsOutputFull( true );
          if( ret )
             return ret;
          /* of course, in the meantime, we may have emptied the buffer, so
             so check for that, too, to avoid a race condition. */
          if( blio.outputRingBuffer.WriteAvailable() ) {
             blio.blioSetIsOutputFull(false) ;
             if( ret )
                return ret;
          }
       }
    }

    /*   Report either NoError or paOutputUnderflowed. */
    /*   may also want to report other errors, but this is non-standard. */
    ret = blio.statusFlags & StreamIO::OutputUnderflow;

    /* report underflow only once: */
    if( ret ) {
      OSAtomicAnd32( (uint32_t)(~StreamIO::OutputUnderflow), &blio.statusFlags );
      ret = OutputUnderflowed ;
    }
  return ret                                                                 ;
}

OSStatus CoreAudioStream::SetupDevicePropertyListeners(AudioDeviceID deviceID,Boolean isInput)
{
  OSStatus osErr = noErr;
  CoreAudioDeviceInfo *deviceProperties = isInput ? &inputProperties : &outputProperties;
  ////////////////////////////////////////////////////////////////////////////
  if ( (osErr = QueryUInt32DeviceProperty( deviceID, isInput,
                                           kAudioDevicePropertyLatency, &deviceProperties->deviceLatency )) != noErr ) return osErr;
  if ( (osErr = QueryUInt32DeviceProperty( deviceID, isInput,
                                           kAudioDevicePropertyBufferFrameSize, &deviceProperties->bufferFrameSize )) != noErr ) return osErr;
  if ( (osErr = QueryUInt32DeviceProperty( deviceID, isInput,
                                           kAudioDevicePropertySafetyOffset, &deviceProperties->safetyOffset )) != noErr ) return osErr;
  ////////////////////////////////////////////////////////////////////////////
  AudioDeviceAddPropertyListener( deviceID, 0, isInput, kAudioDevicePropertyActualSampleRate,
                                  AudioDevicePropertyActualSampleRateListenerProc, this );

  AudioDeviceAddPropertyListener( deviceID, 0, isInput, kAudioStreamPropertyLatency,
                                  AudioDevicePropertyGenericListenerProc, this );
  AudioDeviceAddPropertyListener( deviceID, 0, isInput, kAudioDevicePropertyBufferFrameSize,
                                  AudioDevicePropertyGenericListenerProc, this );
  AudioDeviceAddPropertyListener( deviceID, 0, isInput, kAudioDevicePropertySafetyOffset,
                                  AudioDevicePropertyGenericListenerProc, this );
  return osErr                                                               ;
}

void CoreAudioStream::CleanupDevicePropertyListeners(AudioDeviceID deviceID,Boolean isInput)
{
  AudioDeviceRemovePropertyListener( deviceID, 0, isInput, kAudioDevicePropertyActualSampleRate,
                                     AudioDevicePropertyActualSampleRateListenerProc );
  AudioDeviceRemovePropertyListener( deviceID, 0, isInput, kAudioDevicePropertyLatency,
                                     AudioDevicePropertyGenericListenerProc ) ;
  AudioDeviceRemovePropertyListener( deviceID, 0, isInput, kAudioDevicePropertyBufferFrameSize,
                                     AudioDevicePropertyGenericListenerProc ) ;
  AudioDeviceRemovePropertyListener( deviceID, 0, isInput, kAudioDevicePropertySafetyOffset,
                                     AudioDevicePropertyGenericListenerProc ) ;
}

AudioDeviceID CoreAudioStream::GetStreamInputDevice(void)
{
 return inputDevice ;
}

AudioDeviceID CoreAudioStream::GetStreamOutputDevice(void)
{
  return outputDevice ;
}

Float64 CoreAudioStream::CalculateSoftwareLatencyFromProperties(CoreAudioDeviceInfo *deviceProperties )
{
    UInt32 latencyFrames = deviceProperties->bufferFrameSize + deviceProperties->deviceLatency + deviceProperties->safetyOffset;
    return latencyFrames * deviceProperties->samplePeriod; // same as dividing by sampleRate but faster
}

Float64 CoreAudioStream::CalculateHardwareLatencyFromProperties(CoreAudioDeviceInfo *deviceProperties )
{
  // same as dividing by sampleRate but faster
  return deviceProperties -> deviceLatency *
         deviceProperties -> samplePeriod  ;
}

void CoreAudioStream::UpdateTimeStampOffsets(void)
{
    Float64 inputSoftwareLatency = 0.0;
    Float64 inputHardwareLatency = 0.0;
    Float64 outputSoftwareLatency = 0.0;
    Float64 outputHardwareLatency = 0.0;

    if( inputUnit != NULL )
    {
        inputSoftwareLatency = CalculateSoftwareLatencyFromProperties( &inputProperties );
        inputHardwareLatency = CalculateHardwareLatencyFromProperties( &inputProperties );
    }
    if( outputUnit != NULL )
    {
        outputSoftwareLatency = CalculateSoftwareLatencyFromProperties( &outputProperties );
        outputHardwareLatency = CalculateHardwareLatencyFromProperties( &outputProperties );
    }

    /* We only need a mutex around setting these variables as a group. */
    pthread_mutex_lock( &timingInformationMutex );
    timestampOffsetCombined = inputSoftwareLatency + outputSoftwareLatency;
    timestampOffsetInputDevice = inputHardwareLatency;
    timestampOffsetOutputDevice = outputHardwareLatency;
    pthread_mutex_unlock( &timingInformationMutex );
}

/* ================================================================================= */

/* can be used to update from nominal or actual sample rate */
OSStatus CoreAudioStream::UpdateSampleRateFromDeviceProperty(AudioDeviceID deviceID, Boolean isInput, AudioDevicePropertyID sampleRatePropertyID)
{
    CoreAudioDeviceInfo * deviceProperties = isInput ? &inputProperties : &outputProperties;

    Float64 sampleRate = 0.0;
    UInt32 propSize = sizeof(Float64);
    OSStatus osErr = AudioDeviceGetProperty( deviceID, 0, isInput, sampleRatePropertyID, &propSize, &sampleRate) ;
    if( (osErr == noErr) && (sampleRate > 1000.0) ) /* avoid divide by zero if there's an error */
    {
        deviceProperties->sampleRate = sampleRate;
        deviceProperties->samplePeriod = 1.0 / sampleRate;
    }
    return osErr;
}

bool CoreAudioStream::hasVolume(void)
{
  return CaNotNull ( outputUnit ) ;
}

CaVolume CoreAudioStream::MinVolume(void)
{
  return 0 ;
}

CaVolume CoreAudioStream::MaxVolume(void)
{
  return 10000.0 ;
}

CaVolume CoreAudioStream::Volume(int atChannel)
{
  if ( CaIsNull ( outputUnit ) ) return 10000.0             ;
  OSStatus status                                           ;
  UInt32   propertySize                                     ;
  UInt32   channels [ 2 ]                                   ;
  float    volumes  [ 2 ]                                   ;
  propertySize = sizeof(channels)                           ;
  status = AudioDeviceGetProperty                           (
             outputDevice                                   ,
             NULL                                           ,
             0                                              ,
             kAudioDevicePropertyPreferredChannelsForStereo ,
             &propertySize                                  ,
             &channels                                    ) ;
  if ( noErr != status ) return 10000.0                     ;
  propertySize = sizeof(float)                              ;
  status = AudioDeviceGetProperty                           (
              outputDevice                                  ,
              channels[0]                                   ,
              false                                         ,
              kAudioDevicePropertyVolumeScalar              ,
              &propertySize                                 ,
              &volumes[0]                                 ) ;
  status = AudioDeviceGetProperty                           (
              outputDevice                                  ,
              channels[1]                                   ,
              false                                         ,
              kAudioDevicePropertyVolumeScalar              ,
              &propertySize                                 ,
              &volumes[1]                                 ) ;
  volumes[0] *= 10000.0                                     ;
  volumes[1] *= 10000.0                                     ;
  if ( 0 == atChannel ) return volumes [ 0 ]                ;
  if ( 1 == atChannel ) return volumes [ 1 ]                ;
  return ( volumes[0] + volumes[1] ) / 2                    ;
}

CaVolume CoreAudioStream::setVolume(CaVolume volume,int atChannel)
{
  if ( CaIsNull ( outputUnit ) ) return 10000.0             ;
  ///////////////////////////////////////////////////////////
  if ( volume <       0 ) volume =       0                  ;
  if ( volume > 10000.0 ) volume = 10000.0                  ;
  ///////////////////////////////////////////////////////////
  OSStatus status                                           ;
  UInt32   propertySize                                     ;
  UInt32   channels [ 2 ]                                   ;
  float    volumes  [ 2 ]                                   ;
  bool     setv     [ 2 ]                                   ;
  ///////////////////////////////////////////////////////////
  setv [ 0 ] = false                                        ;
  setv [ 1 ] = false                                        ;
  ///////////////////////////////////////////////////////////
  propertySize = sizeof(channels)                           ;
  status = AudioDeviceGetProperty                           (
             outputDevice                                   ,
             NULL                                           ,
             0                                              ,
             kAudioDevicePropertyPreferredChannelsForStereo ,
             &propertySize                                  ,
             &channels                                    ) ;
  if ( noErr != status ) return 10000.0                     ;
  ///////////////////////////////////////////////////////////
  if ( CaOR ( ( 0 == atChannel )                            ,
              ( 1 == atChannel )                        ) ) {
    volumes [ atChannel ]  = volume                         ;
    volumes [ atChannel ] /= 10000.0                        ;
    setv    [ atChannel ] = true                            ;
  } else                                                    {
    volumes [ 0         ]  = volume                         ;
    volumes [ 0         ] /= 10000.0                        ;
    volumes [ 1         ]  = volumes [ 0 ]                  ;
    setv    [ 0         ]  = true                           ;
    setv    [ 1         ]  = true                           ;
  }                                                         ;
  ///////////////////////////////////////////////////////////
  for (int i=0;i<2;i++) if ( setv [ i ] )                   {
    AudioDeviceSetProperty                                  (
      outputDevice                                          ,
      0                                                     ,
      channels[i]                                           ,
      0                                                     ,
      kAudioDevicePropertyVolumeScalar                      ,
      propertySize                                          ,
      &volumes[i]                                         ) ;
  }                                                         ;
  ///////////////////////////////////////////////////////////
  return volume                                             ;
}

///////////////////////////////////////////////////////////////////////////////

CoreAudioDeviceInfo:: CoreAudioDeviceInfo (void)
                    : DeviceInfo          (    )
{
  safetyOffset    = 0 ;
  bufferFrameSize = 0 ;
  deviceLatency   = 0 ;
  sampleRate      = 0 ;
  samplePeriod    = 0 ;
}

CoreAudioDeviceInfo::~CoreAudioDeviceInfo (void)
{
}

void CoreAudioDeviceInfo::InitializeDeviceProperties(void)
{
  structVersion            = 2                ;
  name                     = NULL             ;
  hostApi                  = -1               ;
  maxInputChannels         = 0                ;
  maxOutputChannels        = 0                ;
  defaultLowInputLatency   = 0                ;
  defaultLowOutputLatency  = 0                ;
  defaultHighInputLatency  = 0                ;
  defaultHighOutputLatency = 0                ;
  defaultSampleRate        = 0                ;
  safetyOffset             = 0                ;
  bufferFrameSize          = 0                ;
  deviceLatency            = 0                ;
  sampleRate               = 1.0              ;
  samplePeriod             = 1.0 / sampleRate ;
}

///////////////////////////////////////////////////////////////////////////////

CoreAudioHostApiInfo:: CoreAudioHostApiInfo (void)
                     : HostApiInfo          (    )
{
}

CoreAudioHostApiInfo::~CoreAudioHostApiInfo (void)
{
}

///////////////////////////////////////////////////////////////////////////////

CoreAudioHostApi:: CoreAudioHostApi (void)
                 : HostApi          (    )
{
  allocations = NULL ;
  devIds      = NULL ;
  defaultIn   = 0    ;
  defaultOut  = 0    ;
  devCount    = 0    ;
}

CoreAudioHostApi::~CoreAudioHostApi(void)
{
  CaEraseAllocation ;
}

CoreAudioHostApi::Encoding CoreAudioHostApi::encoding(void) const
{
  return UTF8 ;
}

bool CoreAudioHostApi::hasDuplex(void) const
{
  return true ;
}

CaError CoreAudioHostApi ::         Open              (
          Stream                 ** s                 ,
          const StreamParameters *  inputParameters   ,
          const StreamParameters *  outputParameters  ,
          double                    sampleRate        ,
          CaUint32                  framesPerCallback ,
          CaStreamFlags             streamFlags       ,
          Conduit                *  conduit           )
{
  CaError           result                          = NoError                ;
  CoreAudioStream * stream                          = 0                      ;
  int               inputChannelCount                                        ;
  int               outputChannelCount                                       ;
  CaSampleFormat    inputSampleFormat                                        ;
  CaSampleFormat    outputSampleFormat                                       ;
  CaSampleFormat    hostInputSampleFormat                                    ;
  CaSampleFormat    hostOutputSampleFormat                                   ;
  UInt32            fixedInputLatency               = 0                      ;
  UInt32            fixedOutputLatency              = 0                      ;
  UInt32            inputLatencyFrames              = 0                      ;
  UInt32            outputLatencyFrames             = 0                      ;
  UInt32            suggestedLatencyFramesPerBuffer = framesPerCallback      ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf                                                                  ( (
      "OpenStream(): in chan=%d, in fmt=%ld, out chan=%d, out fmt=%ld SR=%g, FPB=%ld\n",
      inputParameters  ? inputParameters->channelCount  : -1                 ,
      inputParameters  ? inputParameters->sampleFormat  : -1                 ,
      outputParameters ? outputParameters->channelCount : -1                 ,
      outputParameters ? outputParameters->sampleFormat : -1                 ,
      (float) sampleRate                                                     ,
      framesPerCallback                                                  ) ) ;
  dPrintf ( ( "Opening Stream.\n" ) )                                        ;
  ////////////////////////////////////////////////////////////////////////////
  /* These first few bits of code are from paSkeleton with few modifications. */
  if ( NULL != inputParameters )                                             {
    inputChannelCount = inputParameters->channelCount                        ;
    inputSampleFormat = inputParameters->sampleFormat                        ;
    /* @todo Blocking read/write on Mac is not yet supported.               */
    if ( ( NULL != conduit ) && ( inputSampleFormat & cafNonInterleaved ) )  {
      return SampleFormatNotSupported                                        ;
    }                                                                        ;
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    if ( inputParameters->device==CaUseHostApiSpecificDeviceSpecification )  {
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that input device can support inputChannelCount                */
    if ( inputChannelCount > deviceInfos[ inputParameters->device ]->maxInputChannels ) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* Host supports interleaved float32                                    */
    hostInputSampleFormat = cafFloat32                                       ;
  } else                                                                     {
    inputChannelCount     = 0                                                ;
    inputSampleFormat     = cafFloat32                                       ;
    hostInputSampleFormat = cafFloat32                                       ;
    /* Surpress 'uninitialised var' warnings.                               */
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != outputParameters )                                            {
    outputChannelCount = outputParameters->channelCount                      ;
    outputSampleFormat = outputParameters->sampleFormat                      ;
    /* @todo Blocking read/write on Mac is not yet supported.               */
    if ( ( NULL != conduit ) && ( outputSampleFormat & cafNonInterleaved ) ) {
      return SampleFormatNotSupported                                        ;
    }                                                                        ;
    /* unless alternate device specification is supported, reject the use of
       CaUseHostApiSpecificDeviceSpecification                              */
    if ( outputParameters->device==CaUseHostApiSpecificDeviceSpecification ) {
      return InvalidDevice                                                   ;
    }                                                                        ;
    /* check that output device can support inputChannelCount               */
    if ( outputChannelCount > deviceInfos[ outputParameters->device ]->maxOutputChannels ) {
      return InvalidChannelCount                                             ;
    }                                                                        ;
    /* Host supports interleaved float32                                    */
    hostOutputSampleFormat = cafFloat32                                      ;
  } else                                                                     {
    outputChannelCount     = 0                                               ;
    outputSampleFormat     = cafFloat32                                      ;
    hostOutputSampleFormat = cafFloat32                                      ;
    /* Surpress 'uninitialized var' warnings.                               */
  }                                                                          ;
  /* validate platform specific flags                                       */
  if ( (streamFlags & csfPlatformSpecificFlags) != 0 )                       {
    return InvalidFlag                                                       ;
    /* unexpected platform specific flag                                    */
  }                                                                          ;
  stream = new CoreAudioStream ( )                                           ;
  if ( CaIsNull ( stream ) )                                                 {
    result = InsufficientMemory                                              ;
    goto error                                                               ;
  }                                                                          ;
  stream -> debugger = debugger                                              ;
  stream -> conduit  = conduit                                               ;
  stream -> cpuLoadMeasurer . Initialize ( sampleRate )                      ;
  if ( CaNotNull ( inputParameters ) )                                       {
    CalculateFixedDeviceLatency ( devIds [ inputParameters  -> device ]      ,
                                  true                                       ,
                                  &fixedInputLatency                       ) ;
    inputLatencyFrames  += fixedInputLatency                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( outputParameters ) )                                      {
    CalculateFixedDeviceLatency ( devIds [ outputParameters -> device ]      ,
                                  false                                      ,
                                  &fixedOutputLatency                      ) ;
    outputLatencyFrames += fixedOutputLatency                                ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  suggestedLatencyFramesPerBuffer = CalculateOptimalBufferSize               (
                                      inputParameters                        ,
                                      outputParameters                       ,
                                      fixedInputLatency                      ,
                                      fixedOutputLatency                     ,
                                      sampleRate                             ,
                                      framesPerCallback                    ) ;
  if ( framesPerCallback == Stream::FramesPerBufferUnspecified )             {
    framesPerCallback = suggestedLatencyFramesPerBuffer                      ;
  }                                                                          ;
  /* -- Now we actually open and setup streams. --                          */
  if ( ( NULL != inputParameters                                          ) &&
       ( NULL != outputParameters                                         ) &&
       ( outputParameters->device == inputParameters->device )            )  {
    /* full duplex. One device.                                             */
    UInt32 inputFramesPerBuffer  = (UInt32) stream->inputFramesPerBuffer     ;
    UInt32 outputFramesPerBuffer = (UInt32) stream->outputFramesPerBuffer    ;
    result = OpenAndSetupOneAudioUnit ( stream                               ,
                                        inputParameters                      ,
                                        outputParameters                     ,
                                        suggestedLatencyFramesPerBuffer      ,
                                        &inputFramesPerBuffer                ,
                                        &outputFramesPerBuffer               ,
                                        &(stream->inputUnit)                 ,
                                        &(stream->inputSRConverter)          ,
                                        &(stream->inputDevice)               ,
                                        sampleRate                           ,
                                        stream                             ) ;
    stream->inputFramesPerBuffer  = inputFramesPerBuffer                     ;
    stream->outputFramesPerBuffer = outputFramesPerBuffer                    ;
    stream->outputUnit            = stream->inputUnit                        ;
    stream->outputDevice          = stream->inputDevice                      ;
    if ( result != NoError ) goto error                                      ;
  } else                                                                     {
    /* full duplex, different devices OR simplex                            */
    UInt32 outputFramesPerBuffer = (UInt32) stream->outputFramesPerBuffer    ;
    UInt32 inputFramesPerBuffer  = (UInt32) stream->inputFramesPerBuffer     ;
    result = OpenAndSetupOneAudioUnit ( stream                               ,
                                        NULL                                 ,
                                        outputParameters                     ,
                                        suggestedLatencyFramesPerBuffer      ,
                                        NULL                                 ,
                                        &outputFramesPerBuffer               ,
                                        &(stream->outputUnit)                ,
                                        NULL                                 ,
                                        &(stream->outputDevice)              ,
                                        sampleRate                           ,
                                        stream                             ) ;
    if ( result != NoError ) goto error                                      ;
    result = OpenAndSetupOneAudioUnit ( stream                               ,
                                        inputParameters                      ,
                                        NULL                                 ,
                                        suggestedLatencyFramesPerBuffer      ,
                                        &inputFramesPerBuffer                ,
                                        NULL                                 ,
                                        &(stream->inputUnit)                 ,
                                        &(stream->inputSRConverter)          ,
                                        &(stream->inputDevice)               ,
                                        sampleRate                           ,
                                        stream                             ) ;
    if ( result != NoError ) goto error                                      ;
    stream->inputFramesPerBuffer  = inputFramesPerBuffer                     ;
    stream->outputFramesPerBuffer = outputFramesPerBuffer                    ;
  }                                                                          ;
  inputLatencyFrames  += stream->inputFramesPerBuffer                        ;
  outputLatencyFrames += stream->outputFramesPerBuffer                       ;

    if( stream->inputUnit ) {
       const size_t szfl = sizeof(float);
       /* setup the AudioBufferList used for input */
       bzero( &stream->inputAudioBufferList, sizeof( AudioBufferList ) );
       stream->inputAudioBufferList.mNumberBuffers = 1;
       stream->inputAudioBufferList.mBuffers[0].mNumberChannels
                 = inputChannelCount;
       stream->inputAudioBufferList.mBuffers[0].mDataByteSize
                 = stream->inputFramesPerBuffer*inputChannelCount*szfl;
       stream->inputAudioBufferList.mBuffers[0].mData
                 = (float *) calloc(
                               stream->inputFramesPerBuffer*inputChannelCount,
                               szfl );
       if( !stream->inputAudioBufferList.mBuffers[0].mData )
       {
          result = InsufficientMemory;
          goto error;
       }

       if( (stream->outputUnit && (stream->inputUnit != stream->outputUnit))
           || stream->inputSRConverter )
       {
          /* May want the ringSize ot initial position in
             ring buffer to depend somewhat on sample rate change */

          void *data;
          long ringSize;

          ringSize = computeRingBufferSize( inputParameters,
                                            outputParameters,
                                            stream->inputFramesPerBuffer,
                                            stream->outputFramesPerBuffer,
                                            sampleRate );
          /*ringSize <<= 4; *//*16x bigger, for testing */


          /*now, we need to allocate memory for the ring buffer*/
          data = calloc( ringSize, szfl*inputParameters->channelCount );
          if( !data )
          {
             result = InsufficientMemory;
             goto error;
          }

          /* now we can initialize the ring buffer */
          stream->inputRingBuffer.Initialize(szfl*inputParameters->channelCount, ringSize, data ) ;
          /* advance the read point a little, so we are reading from the
             middle of the buffer */
          if( stream->outputUnit )
            stream->inputRingBuffer.AdvanceWriteIndex(ringSize / RING_BUFFER_ADVANCE_DENOMINATOR );

           // Just adds to input latency between input device and PA full duplex callback.
           inputLatencyFrames += ringSize;
       }
    }

    /* -- initialize Blio Buffer Processors -- */
    if ( NULL != conduit ) {
       long ringSize;

       ringSize = computeRingBufferSize( inputParameters,
                                         outputParameters,
                                         stream->inputFramesPerBuffer,
                                         stream->outputFramesPerBuffer,
                                         sampleRate );
       result = stream->blio.initializeBlioRingBuffers(
              (inputParameters?inputParameters->sampleFormat:cafNothing) ,
              (outputParameters?outputParameters->sampleFormat:cafNothing) ,
              MAX(stream->inputFramesPerBuffer,stream->outputFramesPerBuffer),
              ringSize,
              inputParameters?inputChannelCount:0 ,
              outputParameters?outputChannelCount:0 ) ;
       if( result != NoError )
          goto error;

        inputLatencyFrames += ringSize;
        outputLatencyFrames += ringSize;

    }

    /* -- initialize Buffer Processor -- */
    {
       unsigned long maxHostFrames = stream->inputFramesPerBuffer;
       if( stream->outputFramesPerBuffer > maxHostFrames )
          maxHostFrames = stream->outputFramesPerBuffer;
       result = stream->bufferProcessor.Initialize(
                 inputChannelCount, inputSampleFormat,
                 hostInputSampleFormat,
                 outputChannelCount, outputSampleFormat,
                 hostOutputSampleFormat,
                 sampleRate,
                 streamFlags,
                 framesPerCallback,
                 /* If sample rate conversion takes place, the buffer size
                    will not be known. */
                 maxHostFrames,
                 stream->inputSRConverter
                              ? cabUnknownHostBufferSize
                              : cabBoundedHostBufferSize,
                 conduit ) ;
#warning how to deal with BlioCallback
//                 streamCallback ? streamCallback : BlioCallback,
//                 streamCallback ? userData : &stream->blio );
       if( result != NoError )
           goto error;
    }
    stream->bufferProcessorIsInitialized = TRUE;

    if( inputParameters )
    {
        inputLatencyFrames += stream->bufferProcessor.InputLatencyFrames();
        stream->inputLatency = inputLatencyFrames / sampleRate;
    }
    else
    {
        stream->inputLatency = 0.0;
    }

    if( outputParameters )
    {
        outputLatencyFrames += stream->bufferProcessor.OutputLatencyFrames();
        stream->outputLatency = outputLatencyFrames / sampleRate;
    }
    else
    {
        stream->outputLatency = 0.0;
    }
  ////////////////////////////////////////////////////////////////////////////
  stream -> sampleRate  = sampleRate                                         ;
  stream -> userInChan  = inputChannelCount                                  ;
  stream -> userOutChan = outputChannelCount                                 ;
  // Setup property listeners for timestamp and latency calculations.
  ::pthread_mutex_init ( &stream->timingInformationMutex , NULL )            ;
  stream -> timingInformationMutexIsInitialized = 1                          ;
  stream -> inputProperties  . InitializeDeviceProperties ( )                ;
  stream -> outputProperties . InitializeDeviceProperties ( )                ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( stream->outputUnit ) )                                    {
    Boolean isInput = FALSE                                                  ;
    result = ERR ( stream -> UpdateSampleRateFromDeviceProperty              (
                     stream -> outputDevice                                  ,
                     isInput                                                 ,
                     kAudioDevicePropertyNominalSampleRate               ) ) ;
    if ( CaIsWrong ( result ) ) goto error                                   ;
    stream -> UpdateSampleRateFromDeviceProperty                             (
                stream -> outputDevice                                       ,
                isInput                                                      ,
                kAudioDevicePropertyActualSampleRate                       ) ;
    stream -> SetupDevicePropertyListeners( stream->outputDevice , isInput ) ;
    if ( CaNotNull ( conduit ) )                                             {
      conduit -> Output . setSample ( outputSampleFormat                     ,
                                      outputChannelCount                   ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( stream -> inputUnit ) )                                   {
    Boolean isInput = TRUE                                                   ;
    result = ERR ( stream -> UpdateSampleRateFromDeviceProperty              (
                     stream -> inputDevice                                   ,
                     isInput                                                 ,
                     kAudioDevicePropertyNominalSampleRate               ) ) ;
    if ( CaIsWrong ( result ) ) goto error                                   ;
    stream -> UpdateSampleRateFromDeviceProperty(
                stream->inputDevice                                          ,
                isInput                                                      ,
                kAudioDevicePropertyActualSampleRate                       ) ;
    stream -> SetupDevicePropertyListeners ( stream->inputDevice , isInput ) ;
    if ( CaNotNull ( conduit ) )                                             {
      conduit -> Input  . setSample ( inputSampleFormat                      ,
                                      inputChannelCount                    ) ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  stream -> UpdateTimeStampOffsets ( )                                       ;
  // Setup timestamp copies to be used by audio callback.
  stream->timestampOffsetCombined_ioProcCopy     = stream->timestampOffsetCombined     ;
  stream->timestampOffsetInputDevice_ioProcCopy  = stream->timestampOffsetInputDevice  ;
  stream->timestampOffsetOutputDevice_ioProcCopy = stream->timestampOffsetOutputDevice ;
  stream->state     = CoreAudioStream::STOPPED                               ;
  stream->xrunFlags = 0                                                      ;
  ////////////////////////////////////////////////////////////////////////////
  *s = (Stream *)stream                                                      ;
  return result                                                              ;
error                                                                        :
  stream -> Close ( )                                                        ;
  return result                                                              ;
}

CaError CoreAudioHostApi::isSupported                (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double                    sampleRate       )
{
  int            inputChannelCount                                           ;
  int            outputChannelCount                                          ;
  CaSampleFormat inputSampleFormat                                           ;
  CaSampleFormat outputSampleFormat                                          ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf                                                                  ( (
    "IsFormatSupported(): in chan=%d, in fmt=%ld, out chan=%d, out fmt=%ld sampleRate=%g\n",
    inputParameters  ? inputParameters->channelCount  : -1                   ,
    inputParameters  ? inputParameters->sampleFormat  : -1                   ,
    outputParameters ? outputParameters->channelCount : -1                   ,
    outputParameters ? outputParameters->sampleFormat : -1                   ,
    (float) sampleRate                                                   ) ) ;
  /* These first checks are standard PA checks. We do some fancier checks later. */
  if ( NULL != inputParameters )                                             {
        inputChannelCount = inputParameters->channelCount;
        inputSampleFormat = inputParameters->sampleFormat;

        if( inputSampleFormat & cafCustomFormat )
            return SampleFormatNotSupported;

        if( inputParameters->device == CaUseHostApiSpecificDeviceSpecification )
            return InvalidDevice;

        if( inputChannelCount > deviceInfos[ inputParameters->device ]->maxInputChannels )
            return InvalidChannelCount;
    }
    else
    {
        inputChannelCount = 0;
    }

    if( outputParameters )
    {
        outputChannelCount = outputParameters->channelCount;
        outputSampleFormat = outputParameters->sampleFormat;

        if( outputSampleFormat & cafCustomFormat )
            return SampleFormatNotSupported;

        if( outputParameters->device == CaUseHostApiSpecificDeviceSpecification )
            return InvalidDevice;

        if( outputChannelCount > deviceInfos[ outputParameters->device ]->maxOutputChannels )
            return InvalidChannelCount;

    }
    else
    {
        outputChannelCount = 0;
    }

    {
       CaError err;
       Stream * s;
       err = Open( &s, inputParameters, outputParameters,
                           sampleRate, 1024, 0, NULL ) ;
       if( err != NoError && err != InvalidSampleRate ) {
       };
       if( err )
          return err;
       err = s->Close();
       if( err ) {
       }
    }
  return NoError ;
}

CaError CoreAudioHostApi::gatherDeviceInfo(void)
{
  UInt32 size                                                                ;
  UInt32 propsize                                                            ;
  dPrintf ( ( "gatherDeviceInfo()\n" ) )                                     ;
  /* -- free any previous allocations --                                    */
  if ( CaNotNull ( devIds ) ) allocations -> free ( devIds )                 ;
  devIds = NULL                                                              ;
  /* -- figure out how many devices there are --                            */
  AudioHardwareGetPropertyInfo ( kAudioHardwarePropertyDevices               ,
                                 &propsize                                   ,
                                 NULL                                      ) ;
  devCount = propsize / sizeof ( AudioDeviceID )                             ;
  dPrintf ( ( "Found %ld device(s).\n" , devCount) )                         ;
  /* -- copy the device IDs                                              -- */
  devIds = (AudioDeviceID *) allocations -> alloc ( propsize )               ;
  CaRETURN ( ( CaIsNull ( devIds ) ) , InsufficientMemory )                  ;
  ////////////////////////////////////////////////////////////////////////////
  AudioHardwareGetProperty ( kAudioHardwarePropertyDevices                   ,
                             &propsize                                       ,
                             devIds                                        ) ;
  size       = sizeof(AudioDeviceID)                                         ;
  defaultIn  = kAudioDeviceUnknown                                           ;
  defaultOut = kAudioDeviceUnknown                                           ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotEqual ( AudioHardwareGetProperty                                 (
                      kAudioHardwarePropertyDefaultInputDevice               ,
                      &size                                                  ,
                      &defaultIn                                           ) ,
                    0                                                    ) ) {
    int i                                                                    ;
    defaultIn  = kAudioDeviceUnknown                                         ;
    dPrintf ( ( "Failed to get default input device from OS.\n"          ) ) ;
    dPrintf ( ( " I will substitute the first available input Device.\n" ) ) ;
    for ( i = 0 ; i < devCount ; ++i )                                       {
      DeviceInfo devInfo                                                     ;
      if ( CaNotEqual ( GetChannelInfo( &devInfo, devIds[i], TRUE ) , 0 ) )  {
        if ( CaIsGreater ( devInfo.maxInputChannels , 0 ) )                  {
          defaultIn = devIds[i]                                              ;
          break                                                              ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != AudioHardwareGetProperty                                         (
              kAudioHardwarePropertyDefaultOutputDevice                      ,
              &size                                                          ,
              &defaultOut                                                ) ) {
    int i                                                                    ;
    defaultIn  = kAudioDeviceUnknown                                         ;
    dPrintf ( ("Failed to get default output device from OS.\n"         )  ) ;
    dPrintf ( (" I will substitute the first available output Device.\n")  ) ;
    for ( i=0; i<devCount; ++i )                                             {
      DeviceInfo devInfo                                                     ;
      if ( CaNotEqual ( GetChannelInfo( &devInfo,devIds[i], FALSE ) , 0 ) )  {
        if ( CaIsGreater ( devInfo.maxOutputChannels , 0 ) )                 {
          defaultOut = devIds[i]                                             ;
          break                                                              ;
        }                                                                    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "Default in : %ld\n" , defaultIn  ) )                          ;
  dPrintf ( ( "Default out: %ld\n" , defaultOut ) )                          ;
  return NoError                                                             ;
}

CaError CoreAudioHostApi::GetChannelInfo (
          DeviceInfo  * deviceInfo       ,
          AudioDeviceID macCoreDeviceId  ,
          int           isInput          )
{
  UInt32            propSize                                                 ;
  CaError           err         = NoError                                    ;
  UInt32            i                                                        ;
  int               numChannels = 0                                          ;
  AudioBufferList * buflist     = NULL                                       ;
  dPrintf ( ( "GetChannelInfo()\n" ) )                                       ;

    err = ERR(AudioDeviceGetPropertyInfo(macCoreDeviceId, 0, isInput, kAudioDevicePropertyStreamConfiguration, &propSize, NULL));
    if (err)
        return err;

    buflist = (AudioBufferList *)Allocator::allocate(propSize);
    if( !buflist )
       return InsufficientMemory;
    err = ERR(AudioDeviceGetProperty(macCoreDeviceId, 0, isInput, kAudioDevicePropertyStreamConfiguration, &propSize, buflist));
    if (err)
        goto error;

    for (i = 0; i < buflist->mNumberBuffers; ++i)
        numChannels += buflist->mBuffers[i].mNumberChannels;

    if (isInput)
        deviceInfo->maxInputChannels = numChannels;
    else
        deviceInfo->maxOutputChannels = numChannels;

    if (numChannels > 0) /* do not try to retrieve the latency if there are no channels. */
    {
        deviceInfo->defaultLowInputLatency = .01;
        deviceInfo->defaultHighInputLatency = .10;
        deviceInfo->defaultLowOutputLatency = .01;
        deviceInfo->defaultHighOutputLatency = .10;
        UInt32 lowLatencyFrames = 0;
        UInt32 highLatencyFrames = 0;
        err = CalculateDefaultDeviceLatencies( macCoreDeviceId, isInput, &lowLatencyFrames, &highLatencyFrames );
        if( err == 0 )
        {

            double lowLatencySeconds = lowLatencyFrames / deviceInfo->defaultSampleRate;
            double highLatencySeconds = highLatencyFrames / deviceInfo->defaultSampleRate;
            if (isInput)
            {
                deviceInfo->defaultLowInputLatency = lowLatencySeconds;
                deviceInfo->defaultHighInputLatency = highLatencySeconds;
            }
            else
            {
                deviceInfo->defaultLowOutputLatency = lowLatencySeconds;
                deviceInfo->defaultHighOutputLatency = highLatencySeconds;
            }
        }
    }
    Allocator::free( buflist );
    return NoError;
 error:
    Allocator::free( buflist );
    return err;
}

CaError CoreAudioHostApi::InitializeDeviceInfo (
          DeviceInfo   * deviceInfo            ,
          AudioDeviceID  macCoreDeviceId       ,
          CaHostApiIndex hostApiIndex          )
{
  Float64 sampleRate                                                         ;
  char  * name                                                               ;
  CaError err = NoError                                                      ;
  UInt32  propSize                                                           ;
  dPrintf                                                                  ( (
    "InitializeDeviceInfo(): macCoreDeviceId=%ld\n"                          ,
    macCoreDeviceId                                                      ) ) ;
  deviceInfo->structVersion = 2                                              ;
  deviceInfo->hostApi       = hostApiIndex                                   ;
  deviceInfo->hostType      = CoreAudio                                      ;
  err = ERR ( AudioDeviceGetPropertyInfo                                     (
                macCoreDeviceId                                              ,
                0                                                            ,
                0                                                            ,
                kAudioDevicePropertyDeviceName                               ,
                &propSize                                                    ,
                NULL                                                     ) ) ;
  if ( 0 != err ) return err                                                 ;
  name = (char *)allocations->alloc(propSize)                                ;
  if ( NULL == name ) return InsufficientMemory                              ;
  err = ERR ( AudioDeviceGetProperty                                         (
                macCoreDeviceId                                              ,
                0                                                            ,
                0                                                            ,
                kAudioDevicePropertyDeviceName                               ,
                &propSize                                                    ,
                name                                                     ) ) ;
  if ( 0 != err ) return err                                                 ;
  deviceInfo->name = name                                                    ;
  propSize = sizeof(Float64)                                                 ;
  err = ERR ( AudioDeviceGetProperty                                         (
                macCoreDeviceId                                              ,
                0                                                            ,
                0                                                            ,
                kAudioDevicePropertyNominalSampleRate                        ,
                &propSize                                                    ,
                &sampleRate                                              ) ) ;
  if ( 0 != err) deviceInfo->defaultSampleRate = 0.0                         ;
            else deviceInfo->defaultSampleRate = sampleRate                  ;
  err = GetChannelInfo ( deviceInfo , macCoreDeviceId , 1 )                  ;
  if ( 0 != err ) return err                                                 ;
  err = GetChannelInfo ( deviceInfo , macCoreDeviceId , 0 )                  ;
  if ( 0 != err ) return err                                                 ;
  return NoError                                                             ;
}

void CoreAudioHostApi::Terminate(void)
{
  int unixErr                           ;
  unixErr = destroyXRunListenerList ( ) ;
  if ( 0 != unixErr ) UNIX_ERR(unixErr) ;
  CaEraseAllocation                     ;
}

CaError CoreAudioHostApi::OpenAndSetupOneAudioUnit             (
          const CoreAudioStream  * stream                      ,
          const StreamParameters * inStreamParams              ,
          const StreamParameters * outStreamParams             ,
          const UInt32             requestedFramesPerBuffer    ,
          UInt32                 * actualInputFramesPerBuffer  ,
          UInt32                 * actualOutputFramesPerBuffer ,
          AudioUnit              * audioUnit                   ,
          AudioConverterRef      * srConverter                 ,
          AudioDeviceID          * audioDevice                 ,
          const double             sampleRate                  ,
          void                   * refCon                      )
{
  ComponentDescription desc;
  Component comp;
  AudioStreamBasicDescription desiredFormat;
  OSStatus result = noErr;
  CaError paResult = NoError;
  int line = 0;
  UInt32 callbackKey;
  AURenderCallbackStruct rcbs;
  unsigned long macInputStreamFlags  = caMacCorePlayNice;
  unsigned long macOutputStreamFlags = caMacCorePlayNice;
  SInt32 const *inChannelMap = NULL;
  SInt32 const *outChannelMap = NULL;
  unsigned long inChannelMapSize = 0;
  unsigned long outChannelMapSize = 0;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf                                                                  ( (
    "OpenAndSetupOneAudioUnit(): in chan=%d, in fmt=%ld, out chan=%d, out fmt=%ld, requestedFramesPerBuffer=%ld\n",
    inStreamParams  ? inStreamParams->channelCount  : -1                     ,
    inStreamParams  ? inStreamParams->sampleFormat  : -1                     ,
    outStreamParams ? outStreamParams->channelCount : -1                     ,
    outStreamParams ? outStreamParams->sampleFormat : -1                     ,
    requestedFramesPerBuffer                                             ) ) ;
  /* -- handle the degenerate case                                       -- */
    if( !inStreamParams && !outStreamParams ) {
       *audioUnit = NULL;
       *audioDevice = kAudioDeviceUnknown;
       return NoError;
    }

    /* -- get the user's api specific info, if they set any -- */
    if( inStreamParams && inStreamParams->streamInfo )
    {
       macInputStreamFlags=
            ((CoreAudioStreamInfo*)inStreamParams->streamInfo)
                  ->flags;
       inChannelMap = ((CoreAudioStreamInfo*)inStreamParams->streamInfo)
                  ->channelMap;
       inChannelMapSize = ((CoreAudioStreamInfo*)inStreamParams->streamInfo)
                  ->channelMapSize;
    }
    if( outStreamParams && outStreamParams->streamInfo )
    {
       macOutputStreamFlags=
            ((CoreAudioStreamInfo*)outStreamParams->streamInfo)
                  ->flags;
       outChannelMap = ((CoreAudioStreamInfo*)outStreamParams->streamInfo)
                  ->channelMap;
       outChannelMapSize = ((CoreAudioStreamInfo*)outStreamParams->streamInfo)
                  ->channelMapSize;
    }
    desc.componentType         = kAudioUnitType_Output;
    desc.componentSubType      = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags        = 0;
    desc.componentFlagsMask    = 0;
    /* -- find the component -- */
    comp = FindNextComponent( NULL, &desc );
    if( !comp )
    {
      dPrintf ( ( "AUHAL component not found." ) ) ;
       *audioUnit = NULL;
       *audioDevice = kAudioDeviceUnknown;
       return UnanticipatedHostError;
    }
    /* -- open it -- */
    result = OpenAComponent( comp, audioUnit );
    if( result )
    {
      dPrintf ( ( "Failed to open AUHAL component." ) ) ;
       *audioUnit = NULL;
       *audioDevice = kAudioDeviceUnknown;
       return ERR( result );
    }
    /* -- prepare a little error handling logic / hackery -- */
#define ERR_WRAP(mac_err) do { result = mac_err ; line = __LINE__ ; if ( result != noErr ) goto error ; } while(0)

    /* -- if there is input, we have to explicitly enable input -- */
    if( inStreamParams )
    {
       UInt32 enableIO = 1;
       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
                 kAudioOutputUnitProperty_EnableIO,
                 kAudioUnitScope_Input,
                 INPUT_ELEMENT,
                 &enableIO,
                 sizeof(enableIO) ) );
    }
    /* -- if there is no output, we must explicitly disable output -- */
    if( !outStreamParams )
    {
       UInt32 enableIO = 0;
       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
                 kAudioOutputUnitProperty_EnableIO,
                 kAudioUnitScope_Output,
                 OUTPUT_ELEMENT,
                 &enableIO,
                 sizeof(enableIO) ) );
    }

    if( inStreamParams && outStreamParams )
    {
       assert( outStreamParams->device == inStreamParams->device );
    }
    if( inStreamParams )
    {
       *audioDevice = devIds[inStreamParams->device] ;
       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
                    kAudioOutputUnitProperty_CurrentDevice,
                    kAudioUnitScope_Global,
                    INPUT_ELEMENT,
                    audioDevice,
                    sizeof(AudioDeviceID) ) );
    }
    if( outStreamParams && outStreamParams != inStreamParams )
    {
       *audioDevice = devIds[outStreamParams->device] ;
       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
                    kAudioOutputUnitProperty_CurrentDevice,
                    kAudioUnitScope_Global,
                    OUTPUT_ELEMENT,
                    audioDevice,
                    sizeof(AudioDeviceID) ) );
    }
    result = AudioDeviceAddPropertyListener( *audioDevice,
                                             0,
                                             outStreamParams ? false : true,
                                             kAudioDeviceProcessorOverload,
                                             xrunCallback,
                                             addToXRunListenerList( (void *)stream ) ) ;
    if( result == kAudioHardwareIllegalOperationError ) {
    } else {
       ERR_WRAP( result );
    }
    ERR_WRAP( AudioUnitAddPropertyListener( *audioUnit,
                                            kAudioOutputUnitProperty_IsRunning,
                                            startStopCallback,
                                            (void *)stream ) );

    bzero( &desiredFormat, sizeof(desiredFormat) );
    desiredFormat.mFormatID         = kAudioFormatLinearPCM ;
    desiredFormat.mFormatFlags      = kAudioFormatFlagsNativeFloatPacked;
    desiredFormat.mFramesPerPacket  = 1;
    desiredFormat.mBitsPerChannel   = sizeof( float ) * 8;

    result = 0;
    if( inStreamParams ) {
       paResult = setBestFramesPerBuffer( *audioDevice, FALSE,
                                          requestedFramesPerBuffer,
                                          actualInputFramesPerBuffer );
       if( paResult ) goto error;
       if( macInputStreamFlags & caMacCoreChangeDeviceParameters ) {
          bool requireExact;
          requireExact=macInputStreamFlags & caMacCoreFailIfConversionRequired;
          paResult = setBestSampleRateForDevice( *audioDevice, FALSE,
                                                 requireExact, sampleRate );
          if( paResult ) goto error;
       }
       if( actualInputFramesPerBuffer && actualOutputFramesPerBuffer )
          *actualOutputFramesPerBuffer = *actualInputFramesPerBuffer ;
    }
    if( outStreamParams && !inStreamParams ) {
       paResult = setBestFramesPerBuffer( *audioDevice, TRUE,
                                          requestedFramesPerBuffer,
                                          actualOutputFramesPerBuffer );
       if( paResult ) goto error;
       if( macOutputStreamFlags & caMacCoreChangeDeviceParameters ) {
          bool requireExact;
          requireExact=macOutputStreamFlags & caMacCoreFailIfConversionRequired;
          paResult = setBestSampleRateForDevice( *audioDevice, TRUE,
                                                 requireExact, sampleRate );
          if( paResult ) goto error;
       }
    }

    /* -- set the quality of the output converter -- */
    if( outStreamParams ) {
       UInt32 value = kAudioConverterQuality_Max;
       switch( macOutputStreamFlags & 0x0700 ) {
       case 0x0100: /*paMacCore_ConversionQualityMin:*/
          value=kRenderQuality_Min;
          break;
       case 0x0200: /*paMacCore_ConversionQualityLow:*/
          value=kRenderQuality_Low;
          break;
       case 0x0300: /*paMacCore_ConversionQualityMedium:*/
          value=kRenderQuality_Medium;
          break;
       case 0x0400: /*paMacCore_ConversionQualityHigh:*/
          value=kRenderQuality_High;
          break;
       }
       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
                    kAudioUnitProperty_RenderQuality,
                    kAudioUnitScope_Global,
                    OUTPUT_ELEMENT,
                    &value,
                    sizeof(value) ) );
    }
    /* now set the format on the Audio Units. */
    if( outStreamParams )
    {
       desiredFormat.mSampleRate    =sampleRate;
       desiredFormat.mBytesPerPacket=sizeof(float)*outStreamParams->channelCount;
       desiredFormat.mBytesPerFrame =sizeof(float)*outStreamParams->channelCount;
       desiredFormat.mChannelsPerFrame = outStreamParams->channelCount;
       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
                            kAudioUnitProperty_StreamFormat,
                            kAudioUnitScope_Input,
                            OUTPUT_ELEMENT,
                            &desiredFormat,
                            sizeof(AudioStreamBasicDescription) ) );
    }
    if( inStreamParams )
    {
       AudioStreamBasicDescription sourceFormat;
       UInt32 size = sizeof( AudioStreamBasicDescription );

       /* keep the sample rate of the device, or we confuse AUHAL */
       ERR_WRAP( AudioUnitGetProperty( *audioUnit,
                            kAudioUnitProperty_StreamFormat,
                            kAudioUnitScope_Input,
                            INPUT_ELEMENT,
                            &sourceFormat,
                            &size ) );
       desiredFormat.mSampleRate = sourceFormat.mSampleRate;
       desiredFormat.mBytesPerPacket=sizeof(float)*inStreamParams->channelCount;
       desiredFormat.mBytesPerFrame =sizeof(float)*inStreamParams->channelCount;
       desiredFormat.mChannelsPerFrame = inStreamParams->channelCount;
       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
                            kAudioUnitProperty_StreamFormat,
                            kAudioUnitScope_Output,
                            INPUT_ELEMENT,
                            &desiredFormat,
                            sizeof(AudioStreamBasicDescription) ) );
    }
    if( outStreamParams ) {
       UInt32 size = sizeof( *actualOutputFramesPerBuffer );
       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
                            kAudioUnitProperty_MaximumFramesPerSlice,
                            kAudioUnitScope_Input,
                            OUTPUT_ELEMENT,
                            actualOutputFramesPerBuffer,
                            sizeof(*actualOutputFramesPerBuffer) ) );
       ERR_WRAP( AudioUnitGetProperty( *audioUnit,
                            kAudioUnitProperty_MaximumFramesPerSlice,
                            kAudioUnitScope_Global,
                            OUTPUT_ELEMENT,
                            actualOutputFramesPerBuffer,
                            &size ) );
    }
    if( inStreamParams ) {
       /*UInt32 size = sizeof( *actualInputFramesPerBuffer );*/
       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
                            kAudioUnitProperty_MaximumFramesPerSlice,
                            kAudioUnitScope_Output,
                            INPUT_ELEMENT,
                            actualInputFramesPerBuffer,
                            sizeof(*actualInputFramesPerBuffer) ) );
    }

    if( inStreamParams ) {
       AudioStreamBasicDescription desiredFormat;
       AudioStreamBasicDescription sourceFormat;
       UInt32 sourceSize = sizeof( sourceFormat );
       bzero( &desiredFormat, sizeof(desiredFormat) );
       desiredFormat.mSampleRate       = sampleRate;
       desiredFormat.mFormatID         = kAudioFormatLinearPCM ;
       desiredFormat.mFormatFlags      = kAudioFormatFlagsNativeFloatPacked;
       desiredFormat.mFramesPerPacket  = 1;
       desiredFormat.mBitsPerChannel   = sizeof( float ) * 8;
       desiredFormat.mBytesPerPacket=sizeof(float)*inStreamParams->channelCount;
       desiredFormat.mBytesPerFrame =sizeof(float)*inStreamParams->channelCount;
       desiredFormat.mChannelsPerFrame = inStreamParams->channelCount;

       /* get the source format */
       ERR_WRAP( AudioUnitGetProperty(
                         *audioUnit,
                         kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Output,
                         INPUT_ELEMENT,
                         &sourceFormat,
                         &sourceSize ) );

       if( desiredFormat.mSampleRate != sourceFormat.mSampleRate )
       {
          UInt32 value = kAudioConverterQuality_Max;
          switch( macInputStreamFlags & 0x0700 ) {
          case 0x0100: /*paMacCore_ConversionQualityMin:*/
             value=kAudioConverterQuality_Min;
             break;
          case 0x0200: /*paMacCore_ConversionQualityLow:*/
             value=kAudioConverterQuality_Low;
             break;
          case 0x0300: /*paMacCore_ConversionQualityMedium:*/
             value=kAudioConverterQuality_Medium;
             break;
          case 0x0400: /*paMacCore_ConversionQualityHigh:*/
             value=kAudioConverterQuality_High;
             break;
          }
        dPrintf                                             ( (
          "Creating sample rate converter for input"
          " to convert from %g to %g\n"                                      ,
          (float)sourceFormat.mSampleRate                                    ,
          (float)desiredFormat.mSampleRate                               ) ) ;
          /* create our converter */
          ERR_WRAP( AudioConverterNew(
                             &sourceFormat,
                             &desiredFormat,
                             srConverter ) );
          /* Set quality */
          ERR_WRAP( AudioConverterSetProperty(
                             *srConverter,
                             kAudioConverterSampleRateConverterQuality,
                             sizeof( value ),
                             &value ) );
       }
    }
    /* -- set IOProc (callback) -- */
    callbackKey = outStreamParams ? kAudioUnitProperty_SetRenderCallback
                                  : kAudioOutputUnitProperty_SetInputCallback ;
    rcbs.inputProc = AudioIOProc;
    rcbs.inputProcRefCon = refCon;
    ERR_WRAP( AudioUnitSetProperty(
                               *audioUnit,
                               callbackKey,
                               kAudioUnitScope_Output,
                               outStreamParams ? OUTPUT_ELEMENT : INPUT_ELEMENT,
                               &rcbs,
                               sizeof(rcbs)) );

    if( inStreamParams && outStreamParams && *srConverter )
           ERR_WRAP( AudioUnitSetProperty(
                               *audioUnit,
                               kAudioOutputUnitProperty_SetInputCallback,
                               kAudioUnitScope_Output,
                               INPUT_ELEMENT,
                               &rcbs,
                               sizeof(rcbs)) );

    /* channel mapping. */
    if(inChannelMap)
    {
        UInt32 mapSize = inChannelMapSize *sizeof(SInt32);

        //for each channel of desired input, map the channel from
        //the device's output channel.
        ERR_WRAP( AudioUnitSetProperty(*audioUnit,
                                kAudioOutputUnitProperty_ChannelMap,
                                kAudioUnitScope_Output,
                                INPUT_ELEMENT,
                                inChannelMap,
                                mapSize));
    }
  ////////////////////////////////////////////////////////////////////////////
  if ( outChannelMap )                                                       {
    UInt32 mapSize = outChannelMapSize * sizeof(SInt32)                      ;
    // for each channel of desired output, map the channel from
    // the device's output channel.
    ERR_WRAP ( AudioUnitSetProperty                                          (
                 *audioUnit                                                  ,
                 kAudioOutputUnitProperty_ChannelMap                         ,
                 kAudioUnitScope_Output                                      ,
                 OUTPUT_ELEMENT                                              ,
                 outChannelMap                                               ,
                 mapSize                                                 ) ) ;
  }                                                                          ;
  /* initialize the audio unit                                              */
  ERR_WRAP ( AudioUnitInitialize(*audioUnit) )                               ;
  if ( CaAND ( CaNotNull(inStreamParams) , CaNotNull(outStreamParams) )    ) {
    dPrintf ( ( "Opened device %ld for input and output.\n", *audioDevice) ) ;
  } else
  if ( CaNotNull ( inStreamParams ) )                                        {
    dPrintf ( ( "Opened device %ld for input.\n"  , *audioDevice ) )         ;
  } else
  if ( CaNotNull ( outStreamParams ) )                                       {
    dPrintf ( ( "Opened device %ld for output.\n" , *audioDevice ) )         ;
  }                                                                          ;
  return NoError                                                             ;
  #undef ERR_WRAP
  error                                                                      :
    CloseComponent ( *audioUnit )                                            ;
    *audioUnit = NULL                                                        ;
    CaRETURN ( ( CaIsWrong     ( result            ) )                       ,
               MacCoreSetError ( result , line , 1 )                       ) ;
  return paResult                                                            ;
}

UInt32 CoreAudioHostApi::CalculateOptimalBufferSize         (
         const  StreamParameters * inputParameters          ,
         const  StreamParameters * outputParameters         ,
         UInt32                    fixedInputLatency        ,
         UInt32                    fixedOutputLatency       ,
         double                    sampleRate               ,
         UInt32                    requestedFramesPerBuffer )
{
  UInt32 resultBufferSizeFrames = 0;
  // Use maximum of suggested input and output latencies.
  if ( CaNotNull ( inputParameters  ) )                                      {
    UInt32 suggestedLatencyFrames = inputParameters->suggestedLatency * sampleRate;
    // Calculate a buffer size assuming we are double buffered.
    SInt32 variableLatencyFrames = suggestedLatencyFrames - fixedInputLatency;
    // Prevent negative latency.
    variableLatencyFrames  = MAX( variableLatencyFrames  , 0               ) ;
    resultBufferSizeFrames = MAX( resultBufferSizeFrames                     ,
                                  (UInt32) variableLatencyFrames           ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( outputParameters ) )                                      {
    UInt32 suggestedLatencyFrames = outputParameters->suggestedLatency * sampleRate;
    SInt32 variableLatencyFrames = suggestedLatencyFrames - fixedOutputLatency;
    variableLatencyFrames  = MAX( variableLatencyFrames , 0                ) ;
    resultBufferSizeFrames = MAX( resultBufferSizeFrames                     ,
                                  (UInt32) variableLatencyFrames           ) ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  // can't have zero frames. code to round up to next user buffer requires non-zero
  resultBufferSizeFrames = MAX( resultBufferSizeFrames , 1 )                 ;
  if ( requestedFramesPerBuffer != Stream::FramesPerBufferUnspecified )      {
    // make host buffer the next highest integer multiple of user frames per buffer
    UInt32 n = ( resultBufferSizeFrames + requestedFramesPerBuffer - 1 )     /
                 requestedFramesPerBuffer                                    ;
    resultBufferSizeFrames = n * requestedFramesPerBuffer                    ;
  } else                                                                     {
    dPrintf                                                                ( (
      "Block Size unspecified. Based on Latency, the user wants a Block Size near: %ld.\n",
      resultBufferSizeFrames                                             ) ) ;
  }                                                                          ;
  // Clip to the capabilities of the device.
  if ( CaNotNull ( inputParameters  ) )                                      {
    ClipToDeviceBufferSize( devIds[inputParameters->device],
                            true, // In the old code isInput was false!
                             resultBufferSizeFrames, &resultBufferSizeFrames );
  }                                                                          ;
  if ( CaNotNull ( outputParameters ) )                                      {
    ClipToDeviceBufferSize ( devIds[outputParameters->device]                ,
                             false                                           ,
                             resultBufferSizeFrames                          ,
                             &resultBufferSizeFrames                       ) ;
  }                                                                          ;
  dPrintf ( ( "After querying hardware, setting block size to %ld.\n"        ,
              resultBufferSizeFrames                                     ) ) ;
  return resultBufferSizeFrames                                              ;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
