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

#ifndef CAOSS_HPP
#define CAOSS_HPP

#include "CaLinux.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <limits.h>
#include <semaphore.h>

#ifdef HAVE_SYS_SOUNDCARD_H
# include <sys/soundcard.h>
# ifdef __NetBSD__
#  define DEVICE_NAME_BASE           "/dev/audio"
# else
#  define DEVICE_NAME_BASE           "/dev/dsp"
# endif
#elif defined(HAVE_LINUX_SOUNDCARD_H)
# include <linux/soundcard.h>
# define DEVICE_NAME_BASE            "/dev/dsp"
#elif defined(HAVE_MACHINE_SOUNDCARD_H)
# include <machine/soundcard.h> /* JH20010905 */
# define DEVICE_NAME_BASE            "/dev/audio"
#else
# error No sound card header file
#endif

#define MIN_OSS_VERSION 0x040100

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

typedef enum     {
  StreamMode_In  ,
  StreamMode_Out }
  StreamMode     ;

/** Per-direction structure for PaOssStream.
 *
 * Aspect StreamChannels: In case the user requests to open the same device for both capture and playback,
 * but with different number of channels we will have to adapt between the number of user and host
 * channels for at least one direction, since the configuration space is the same for both directions
 * of an OSS device.                                                        */

class OssStreamComponent
{
  public:

    int            fd               ;
    const char  *  devName          ;
    int            version          ;
    int            userChannelCount ;
    int            hostChannelCount ;
    int            userInterleaved  ;
    void        *  buffer           ;
    CaSampleFormat userFormat       ;
    CaSampleFormat hostFormat       ;
    double         latency          ;
    unsigned long  hostFrames       ;
    unsigned long  numBufs          ;
    void        ** userBuffers      ; /* For non-interleaved blocking */

    explicit OssStreamComponent (void) ;
    virtual ~OssStreamComponent (void) ;

    CaError  Initialize         (const StreamParameters * parameters   ,
                                 int                      callbackMode ,
                                 int                      fd           ,
                                 const char             * deviceName ) ;
    CaError  Configure          (double                   sampleRate      ,
                                 unsigned long            framesPerBuffer ,
                                 StreamMode               streamMode      ,
                                 OssStreamComponent     * master        ) ;
    unsigned int  FrameSize     (void) ;
    unsigned long BufferSize    (void) ;
    CaError  Read               (unsigned long * frames) ;
    CaError  Write              (unsigned long * frames) ;
    CaError  AvailableFormats   (CaSampleFormat * availableFormats) ;
    void     Terminate          (void) ;

    int      Major              (void) ;
    int      Minor              (void) ;

  protected:

  private:

} ;

///////////////////////////////////////////////////////////////////////////////

class OssStream : public Stream
{
  public:

    CpuLoad              cpuLoadMeasurer     ;
    BufferProcessor      bufferProcessor     ;
    pthread_t            threading           ;
    int                  sharedDevice        ;
    unsigned long        framesPerHostBuffer ;
    int                  triggered           ;
    int                  isActive            ;
    int                  isStopped           ;
    int                  lastPosPtr          ;
    double               lastStreamBytes     ;
    int                  framesProcessed     ;
    double               sampleRate          ;
    int                  callbackMode        ;
    volatile int         callbackStop        ;
    volatile int         callbackAbort       ;
    OssStreamComponent * capture             ;
    OssStreamComponent * playback            ;
    unsigned long        pollTimeout         ;
    sem_t              * semaphore           ;

    explicit         OssStream      (void) ;
    virtual         ~OssStream      (void) ;

    virtual CaError  Start          (void) ;
    virtual CaError  Stop           (void) ;
    virtual CaError  Close          (void) ;
    virtual CaError  Abort          (void) ;
    virtual CaError  IsStopped      (void) ;
    virtual CaError  IsActive       (void) ;
    virtual CaTime   GetTime        (void) ;
    virtual double   GetCpuLoad     (void) ;
    virtual CaInt32  ReadAvailable  (void) ;
    virtual CaInt32  WriteAvailable (void) ;
    virtual CaError  Read           (void       * buffer,unsigned long frames) ;
    virtual CaError  Write          (const void * buffer,unsigned long frames) ;
    virtual bool     hasVolume       (void) ;
    virtual CaVolume MinVolume      (void) ;
    virtual CaVolume MaxVolume      (void) ;
    virtual CaVolume Volume         (int atChannel = -1) ;
    virtual CaVolume setVolume      (CaVolume volume,int atChannel = -1) ;

    virtual CaError  Prepare        (void) ;
    virtual void     Terminate      (void) ;
    virtual CaError  RealStop       (int abort) ;
    virtual CaError  StreamStop     (int abort) ;
    virtual CaError  SetUpBuffers   (unsigned long framesAvail) ;
    virtual CaError  WaitForFrames  (unsigned long * frames) ;
    virtual CaError  Configure      (double        sampleRate      ,
                                     unsigned long framesPerBuffer ,
                                     double      * inputLatency    ,
                                     double      * outputLatency ) ;

  protected:

  private:

};

class OssDeviceInfo : public DeviceInfo
{
  public:

    int version ;

    explicit OssDeviceInfo (void) ;
    virtual ~OssDeviceInfo (void) ;

    virtual CaError ValidateParameters                    (
                      const StreamParameters * parameters ,
                      StreamMode               mode     ) ;
    virtual CaError Initialize                                   (
                      const char      * name                     ,
                      CaHostApiIndex    hostApiIndex             ,
                      int               maxInputChannels         ,
                      int               maxOutputChannels        ,
                      CaTime            defaultLowInputLatency   ,
                      CaTime            defaultLowOutputLatency  ,
                      CaTime            defaultHighInputLatency  ,
                      CaTime            defaultHighOutputLatency ,
                      double            defaultSampleRate        ,
                      AllocationGroup * allocations            ) ;

  protected:

  private:

};

class OssHostApi : public HostApi
{
  public:

    AllocationGroup * allocations  ;
    CaHostApiIndex    hostApiIndex ;

    explicit  OssHostApi         (void) ;
    virtual  ~OssHostApi         (void) ;

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

    virtual CaError  Initialize  (OssStream              * stream            ,
                                  const StreamParameters * inputParameters   ,
                                  const StreamParameters * outputParameters  ,
                                  Conduit                * conduit           ,
                                  CaStreamFlags            streamFlags     ) ;

    virtual CaError QueryDevice  (char * deviceName,OssDeviceInfo ** deviceInfo) ;

    virtual CaError BuildDeviceList (void) ;

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError OssInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAOSS_HPP
