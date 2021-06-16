/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/24                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#ifndef CAALSA_HPP
#define CAALSA_HPP

#include "CaLinux.hpp"

// #define ENABLE_ALSA_BLUETOOTH

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <sys/poll.h>
#include <string.h> /* strlen() */
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <signal.h>
#include <dlfcn.h>
#include <alsa/asoundlib.h>
#include <alsa/error.h>

///////////////////////////////////////////////////////////////////////////////

typedef struct HwDevInfo   {
  const char * alsaName    ;
  char       * name        ;
  int          isPlug      ;
  int          hasPlayback ;
  int          hasCapture  ;
}              HwDevInfo   ;

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

typedef enum StreamDirection {
  StreamDirection_In         ,
  StreamDirection_Out        }
  StreamDirection            ;

///////////////////////////////////////////////////////////////////////////////

typedef struct    AlsaStreamInfo  {
  unsigned long   size            ;
  CaHostApiTypeId hostApiType     ;
  unsigned long   version         ;
  const char    * deviceString    ;
}                 AlsaStreamInfo  ;

///////////////////////////////////////////////////////////////////////////////

class AlsaStreamComponent ;
class AlsaDeviceInfo      ;
class AlsaHostApiInfo     ;
class AlsaStream          ;
class AlsaHostApi         ;

///////////////////////////////////////////////////////////////////////////////

class AlsaStreamComponent
{
  public:

    CaSampleFormat            hostSampleFormat    ;
    int                       numUserChannels     ;
    int                       numHostChannels     ;
    int                       userInterleaved     ;
    int                       hostInterleaved     ;
    int                       canMmap             ;
    void                   *  nonMmapBuffer       ;
    unsigned int              nonMmapBufferSize   ;
    CaDeviceIndex             device              ;
    int                       deviceIsPlug        ;
    int                       useReventFix        ;
    snd_pcm_t              *  pcm                 ;
    snd_pcm_uframes_t         framesPerPeriod     ;
    snd_pcm_uframes_t         alsaBufferSize      ;
    snd_pcm_format_t          nativeFormat        ;
    unsigned int              nfds                ;
    int                       ready               ;
    void                   ** userBuffers         ;
    snd_pcm_uframes_t         offset              ;
    StreamDirection           streamDir           ;
    snd_pcm_channel_area_t *  channelAreas        ;
    bool                      FixedFrameSize      ;

    explicit AlsaStreamComponent (void) ;
    virtual ~AlsaStreamComponent (void) ;

    virtual void    Reset (void) ;
    virtual CaError Initialize                                (
                      AlsaHostApi            * alsaApi        ,
                      const StreamParameters * params         ,
                      StreamDirection          streamDir      ,
                      int                      callbackMode ) ;
    virtual void Terminate(void) ;
    virtual CaError InitialConfigure                        (
                      const StreamParameters * params       ,
                      int                      primeBuffers ,
                      snd_pcm_hw_params_t    * hwParams     ,
                      double                 * sampleRate ) ;
    virtual CaError FinishConfigure                         (
                      snd_pcm_hw_params_t    * hwParams     ,
                      const StreamParameters * params       ,
                      int                      primeBuffers ,
                      double                   sampleRate   ,
                      CaTime                 * latency    ) ;
    virtual CaError DetermineFramesPerBuffer                       (
                      const StreamParameters * params              ,
                      unsigned long            framesPerUserBuffer ,
                      double                   sampleRate          ,
                      snd_pcm_hw_params_t    * hwParams            ,
                      int                    * accurate          ) ;
    virtual CaError EndProcessing(unsigned long numFrames,int * xrun) ;
    virtual CaError DoChannelAdaption(BufferProcessor * bp,int numFrames) ;
    virtual CaError AvailableFrames(CaUint32 * numFrames,int * xrunOccurred) ;
    virtual CaError BeginPolling(struct pollfd * pfds) ;
    virtual CaError EndPolling(struct pollfd * pfds,int * shouldPoll,int * xrun) ;
    virtual CaError RegisterChannels              (
                      BufferProcessor * bp        ,
                      unsigned long   * numFrames ,
                      int             * xrun    ) ;

  protected:

  private:

} ;

///////////////////////////////////////////////////////////////////////////////

class AlsaDeviceInfo  : public DeviceInfo
{
  public:

    char * alsaName          ;
    int    isPlug            ;
    int    minInputChannels  ;
    int    minOutputChannels ;

    explicit AlsaDeviceInfo     (void) ;
    virtual ~AlsaDeviceInfo     (void) ;

    virtual void    Initialize  (void) ;
    virtual CaError GropeDevice (snd_pcm_t     * pcm            ,
                                 int             isPlug         ,
                                 StreamDirection mode           ,
                                 int             openBlocking ) ;

  protected:

  private:

};

class AlsaHostApiInfo  : public HostApiInfo
{
  public:

    explicit AlsaHostApiInfo (void) ;
    virtual ~AlsaHostApiInfo (void) ;

  protected:

  private:

} ;

class AlsaStream : public Stream
{
  public:

    CpuLoad               cpuLoadMeasurer        ;
    BufferProcessor       bufferProcessor        ;
    unsigned long         framesPerUserBuffer    ;
    unsigned long         maxFramesPerHostBuffer ;
    int                   primeBuffers           ;
    int                   callbackMode           ;
    int                   pcmsSynced             ;
    struct pollfd       * pfds                   ;
    int                   pollTimeout            ;
    volatile sig_atomic_t callback_finished      ;
    volatile sig_atomic_t callbackAbort          ;
    volatile sig_atomic_t isActive               ;
    int                   neverDropInput         ;
    CaTime                underrun               ;
    CaTime                overrun                ;
    AlsaStreamComponent   capture                ;
    AlsaStreamComponent   playback               ;
    //////////////////////////////////////////////
    pthread_t             thread                 ;
    pthread_mutex_t       stateMtx               ;
    pthread_cond_t        threadCond             ;
    int                   rtSched                ;
    int                   parentWaiting          ;
    int                   stopRequested          ;
    int                   locked                 ;
    volatile sig_atomic_t stopRequest            ;

    explicit         AlsaStream      (void) ;
    virtual         ~AlsaStream      (void) ;

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
    virtual bool     hasVolume       (void) ;
    virtual CaVolume MinVolume       (void) ;
    virtual CaVolume MaxVolume       (void) ;
    virtual CaVolume Volume          (int atChannel = -1) ;
    virtual CaVolume setVolume       (CaVolume volume,int atChannel = -1) ;

    virtual void Terminate           (void) ;

    virtual int  CalculatePollTimeout     (unsigned long frames) ;
    virtual void EnableRealtimeScheduling (int enable) ;

    virtual CaError Configure        (const StreamParameters * inParams             ,
                                      const StreamParameters * outParams            ,
                                      double                   sampleRate           ,
                                      unsigned long            framesPerUserBuffer  ,
                                      double                 * inputLatency         ,
                                      double                 * outputLatency        ,
                                      CaHostBufferSizeMode   * hostBufferSizeMode ) ;
    virtual CaError SetUpBuffers     (unsigned long * numFrames      ,
                                      int           * xrunOccurred ) ;

    virtual CaError GetStreamInputCard  (int * card) ;
    virtual CaError GetStreamOutputCard (int * card) ;
    virtual CaError DetermineFramesPerBuffer                         (
                      double                    sampleRate           ,
                      const  StreamParameters * inputParameters      ,
                      const  StreamParameters * outputParameters     ,
                      unsigned long             framesPerUserBuffer  ,
                      snd_pcm_hw_params_t     * hwParamsCapture      ,
                      snd_pcm_hw_params_t     * hwParamsPlayback     ,
                      CaHostBufferSizeMode    * hostBufferSizeMode ) ;
    virtual CaError RealStop(int abort) ;
    virtual CaError Stop(int abort) ;

    virtual CaError AvailableFrames (int             queryCapture   ,
                                     int             queryPlayback  ,
                                     unsigned long * available      ,
                                     int           * xrunOccurred ) ;
    virtual CaError WaitForFrames   (unsigned long * framesAvail,int * xrunOccurred) ;
    virtual CaError EndProcessing   (unsigned long numFrames,int * xrunOccurred) ;
    virtual void CalculateTimeInfo  (Conduit * conduit) ;
    virtual CaError Restart         (void) ;
    virtual CaError HandleXrun      (void) ;
    virtual CaError ContinuePoll    (StreamDirection streamDir,int * pollTimeout,int * continuePoll) ;
    virtual void    SilenceBuffer   (void) ;
    virtual CaError Start           (int priming) ;
    virtual CaError Initialize              (
                       AlsaHostApi *alsaApi,
                       const StreamParameters *inParams,
                       const StreamParameters *outParams,
                       double sampleRate,
                       unsigned long framesPerUserBuffer,
                       Conduit * conduit) ;

    virtual void * Processor        (void) ;

  protected:

    CaError ThreadStart     (void) ;
    CaError ThreadTerminate (int wait,CaError * exitResult) ;

    CaError BoostPriority   (void) ;

    CaError PrepareNotify   (void) ;
    CaError NotifyParent    (void) ;

    CaError MutexInitialize (void) ;
    CaError MutexTerminate  (void) ;
    CaError MutexLock       (void) ;
    CaError MutexUnlock     (void) ;

  private:

    void OnExit             (void) ;

};

class AlsaHostApi : public HostApi
{
  public:

    AllocationGroup * allocations    ;
    CaHostApiIndex    hostApiIndex   ;
    CaUint32          alsaLibVersion ;

    explicit  AlsaHostApi        (void) ;
    virtual  ~AlsaHostApi        (void) ;

    virtual CaError  Open        (Stream                 ** stream            ,
                                  const StreamParameters  * inputParameters   ,
                                  const StreamParameters  * outputParameters  ,
                                  double                    sampleRate        ,
                                  CaUint32                  framesPerCallback ,
                                  CaStreamFlags             streamFlags       ,
                                  Conduit                 * conduit         ) ;
    virtual void     Terminate   (void) ;
    virtual CaError  isSupported (const  StreamParameters * inputParameters  ,
                                  const  StreamParameters * outputParameters ,
                                  double sampleRate                        ) ;
    virtual Encoding encoding    (void) const ;
    virtual bool     hasDuplex   (void) const ;

    virtual CaError  Open        (const StreamParameters  * params           ,
                                  StreamDirection           streamDir        ,
                                  snd_pcm_t              ** pcm            ) ;
    virtual const AlsaDeviceInfo * GetAlsaDeviceInfo (int device) ;
    virtual CaError ValidateParameters                                       (
                                  const StreamParameters *  parameters       ,
                                  StreamDirection           mode           ) ;
    virtual CaError  StrDup      (char ** dst,const char * src) ;
    virtual CaError  TestParameters                          (
                       const StreamParameters * parameters   ,
                       double                   sampleRate   ,
                       StreamDirection          streamDir  ) ;
    virtual CaError  BuildDeviceList (void) ;
    virtual CaError  FillInDevInfo                   (
                       HwDevInfo      * deviceHwInfo ,
                       int              blocking     ,
                       AlsaDeviceInfo * devInfo      ,
                       int            * devIdx     ) ;

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError AlsaInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAALSA_HPP
