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

#ifndef CACOREAUDIO_HPP
#define CACOREAUDIO_HPP

#include "CaMacOSX.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

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

typedef struct                   {
  unsigned long   size           ;
  CaHostApiTypeId hostApiType    ;
  unsigned long   version        ;
  unsigned long   flags          ;
  SInt32 const  * channelMap     ;
  unsigned long   channelMapSize ;
} CoreAudioStreamInfo            ;

///////////////////////////////////////////////////////////////////////////////

class MacBlio
{
  public:

    RingBuffer      inputRingBuffer        ;
    RingBuffer      outputRingBuffer       ;
    size_t          ringBufferFrames       ;
    CaSampleFormat  inputSampleFormat      ;
    size_t          inputSampleSizeActual  ;
    size_t          inputSampleSizePow2    ;
    CaSampleFormat  outputSampleFormat     ;
    size_t          outputSampleSizeActual ;
    size_t          outputSampleSizePow2   ;
    size_t          framesPerBuffer        ;
    int             inChan                 ;
    int             outChan                ;
    uint32_t        statusFlags            ;
    CaError         errors                 ;
    volatile bool   isInputEmpty           ;
    pthread_mutex_t inputMutex             ;
    pthread_cond_t  inputCond              ;
    volatile bool   isOutputFull           ;
    pthread_mutex_t outputMutex            ;
    pthread_cond_t  outputCond             ;

    explicit MacBlio (void) ;
    virtual ~MacBlio (void) ;

    CaError initializeBlioRingBuffers           (
              CaSampleFormat inputSampleFormat  ,
              CaSampleFormat outputSampleFormat ,
              size_t         framesPerBuffer    ,
              long           ringBufferSize     ,
              int            inChan             ,
              int            outChan            ) ;
    CaError blioSetIsInputEmpty(bool isEmpty) ;
    CaError blioSetIsOutputFull(bool isFull) ;
    CaError resetBlioRingBuffers(void) ;
    CaError destroyBlioRingBuffers(void) ;
    void waitUntilBlioWriteBufferIsFlushed(void) ;

  protected:

  private:

} ;

class CoreAudioDeviceInfo  : public DeviceInfo
{
  public:

    UInt32  safetyOffset    ;
    UInt32  bufferFrameSize ;
    UInt32  deviceLatency   ;
    Float64 sampleRate      ;
    Float64 samplePeriod    ;

    explicit CoreAudioDeviceInfo (void) ;
    virtual ~CoreAudioDeviceInfo (void) ;

    virtual void InitializeDeviceProperties (void) ;

  protected:

  private:

} ;

class CoreAudioHostApiInfo  : public HostApiInfo
{
  public:

    explicit CoreAudioHostApiInfo (void) ;
    virtual ~CoreAudioHostApiInfo (void) ;

  protected:

  private:

} ;

///////////////////////////////////////////////////////////////////////////////

class CoreAudioStream : public Stream
{
  public:

    CpuLoad              cpuLoadMeasurer                                     ;
    BufferProcessor      bufferProcessor                                     ;
    bool                 bufferProcessorIsInitialized                        ;
    MacBlio              blio                                                ;
    AudioUnit            inputUnit                                           ;
    AudioUnit            outputUnit                                          ;
    AudioDeviceID        inputDevice                                         ;
    AudioDeviceID        outputDevice                                        ;
    size_t               userInChan                                          ;
    size_t               userOutChan                                         ;
    size_t               inputFramesPerBuffer                                ;
    size_t               outputFramesPerBuffer                               ;
    RingBuffer           inputRingBuffer                                     ;
    AudioConverterRef    inputSRConverter                                    ;
    AudioBufferList      inputAudioBufferList                                ;
    AudioTimeStamp       startTime                                           ;
    volatile uint32_t    xrunFlags                                           ;
    double               sampleRate                                          ;
    CoreAudioDeviceInfo  inputProperties                                     ;
    CoreAudioDeviceInfo  outputProperties                                    ;
    int                  timingInformationMutexIsInitialized                 ;
    pthread_mutex_t      timingInformationMutex                              ;
    Float64              timestampOffsetCombined                             ;
    Float64              timestampOffsetInputDevice                          ;
    Float64              timestampOffsetOutputDevice                         ;
    Float64              timestampOffsetCombined_ioProcCopy                  ;
    Float64              timestampOffsetInputDevice_ioProcCopy               ;
    Float64              timestampOffsetOutputDevice_ioProcCopy              ;
    volatile enum                                                            {
       STOPPED          = 0                                                  ,
       CALLBACK_STOPPED = 1                                                  ,
       STOPPING         = 2                                                  ,
       ACTIVE           = 3                                                  }
                         state                                               ;

    explicit         CoreAudioStream (void) ;
    virtual         ~CoreAudioStream (void) ;

    virtual CaError  Start           (void) ;
    virtual CaError  Stop            (void) ;
    virtual CaError  Close           (void) ;
    virtual CaError  Abort           (void) ;
    virtual CaError  IsStopped       (void) ;
    virtual CaError  IsActive        (void) ;
    virtual CaTime   GetTime         (void) ;
    virtual double   GetCpuLoad      (void) ;
    virtual CaInt32  ReadAvailable   (void) ;
    virtual CaInt32  WriteAvailable  (void) ;
    virtual CaError  Read            (void       * buffer,unsigned long frames) ;
    virtual CaError  Write           (const void * buffer,unsigned long frames) ;
    virtual bool     hasVolume        (void) ;
    virtual CaVolume MinVolume       (void) ;
    virtual CaVolume MaxVolume       (void) ;
    virtual CaVolume Volume          (int atChannel = -1) ;
    virtual CaVolume setVolume       (CaVolume volume,int atChannel = -1) ;

    virtual OSStatus SetupDevicePropertyListeners(AudioDeviceID deviceID,Boolean isInput) ;
    virtual void CleanupDevicePropertyListeners(AudioDeviceID deviceID,Boolean isInput) ;
    virtual AudioDeviceID GetStreamInputDevice(void) ;
    virtual AudioDeviceID GetStreamOutputDevice(void) ;
    virtual Float64 CalculateSoftwareLatencyFromProperties(CoreAudioDeviceInfo *deviceProperties ) ;
    virtual Float64 CalculateHardwareLatencyFromProperties(CoreAudioDeviceInfo *deviceProperties ) ;
    virtual void UpdateTimeStampOffsets(void) ;
    virtual OSStatus UpdateSampleRateFromDeviceProperty(AudioDeviceID deviceID, Boolean isInput, AudioDevicePropertyID sampleRatePropertyID) ;

  protected:

  private:

};

class CoreAudioHostApi : public HostApi
{
  public:

    AllocationGroup * allocations ;
    AudioDeviceID   * devIds      ;
    AudioDeviceID     defaultIn   ;
    AudioDeviceID     defaultOut  ;
    long              devCount    ;

    explicit  CoreAudioHostApi   (void) ;
    virtual  ~CoreAudioHostApi   (void) ;

    virtual CaError  Open        (Stream                ** stream            ,
                                  const StreamParameters * inputParameters   ,
                                  const StreamParameters * outputParameters  ,
                                  double                   sampleRate        ,
                                  CaUint32                 framesPerCallback ,
                                  CaStreamFlags            streamFlags       ,
                                  Conduit                * conduit         ) ;
    virtual void     Terminate   (void) ;
    virtual CaError  isSupported (const  StreamParameters * inputParameters  ,
                                  const  StreamParameters * outputParameters ,
                                  double sampleRate                        ) ;
    virtual Encoding encoding    (void) const ;
    virtual bool     hasDuplex   (void) const ;

    virtual CaError gatherDeviceInfo(void) ;
    virtual CaError GetChannelInfo                   (
                      DeviceInfo  * deviceInfo       ,
                      AudioDeviceID macCoreDeviceId  ,
                      int           isInput        ) ;
    virtual CaError InitializeDeviceInfo             (
                      DeviceInfo   * deviceInfo      ,
                      AudioDeviceID  macCoreDeviceId ,
                      CaHostApiIndex hostApiIndex  ) ;
    virtual CaError OpenAndSetupOneAudioUnit(
                      const CoreAudioStream *stream,
                      const StreamParameters *inStreamParams,
                      const StreamParameters *outStreamParams,
                      const UInt32 requestedFramesPerBuffer,
                      UInt32 *actualInputFramesPerBuffer,
                      UInt32 *actualOutputFramesPerBuffer,
                      AudioUnit *audioUnit,
                      AudioConverterRef *srConverter,
                      AudioDeviceID *audioDevice,
                      const double sampleRate,
                      void *refCon ) ;
    virtual UInt32 CalculateOptimalBufferSize(
             const  StreamParameters *inputParameters,
             const  StreamParameters *outputParameters,
             UInt32 fixedInputLatency,
             UInt32 fixedOutputLatency,
             double sampleRate,
             UInt32 requestedFramesPerBuffer ) ;

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError CoreAudioInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CACOREAUDIO_HPP
