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

#ifndef CAOPENAL_HPP
#define CAOPENAL_HPP

#include "CiosAudioPrivate.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

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

class OpenALStream : public Stream
{
  public:

    CpuLoad            cpuLoadMeasurer ;
    bool               isInputChannel  ;
    bool               isOutputChannel ;
    ALenum             alFormat        ;
    CaSampleFormat     SampleFormat    ;
    int                Channels        ;
    int                Frames          ;
    int                BytesPerSample  ;
    volatile int       isStopped       ;
    volatile int       isActive        ;
    volatile int       stopProcessing  ;
    volatile int       abortProcessing ;
    OpenALStreamInfo * spec            ;
    #if defined(WIN32) || defined(_WIN32)
    unsigned           ThreadId        ;
    #else
    pthread_t          ThreadId        ;
    #endif

    explicit         OpenALStream  (void) ;
    virtual         ~OpenALStream  (void) ;

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
    virtual bool     hasVolume      (void) ;
    virtual CaVolume MinVolume      (void) ;
    virtual CaVolume MaxVolume      (void) ;
    virtual CaVolume Volume         (int atChannel = -1) ;
    virtual CaVolume setVolume      (CaVolume volume,int atChannel = -1) ;

    virtual CaError  Processor      (void) ;
    virtual CaError  Obtain         (void) ;
    virtual CaError  Put            (void) ;

    virtual bool     VerifyInfo     (void * spec) ;

  protected:

  private:

};

class OpenALDeviceInfo : public DeviceInfo
{
  public:

    bool            canInput  ;
    bool            canOutput ;
    int             MaxMono   ;
    int             MaxStereo ;
    int             TotalConf ;
    int             MaxConf   ;
    CaDeviceConf ** CONFs     ;
    ALenum        * Formats   ;

    explicit OpenALDeviceInfo  (void) ;
    virtual ~OpenALDeviceInfo  (void) ;

    virtual bool ObtainDetails (ALCdevice * device) ;
    virtual int  SupportIndex  (int            channels ,
                                CaSampleFormat format   ,
                                double         rate   ) ;
    virtual ALenum GetFormat   (int            channels ,
                                CaSampleFormat format   ,
                                double         rate   ) ;
    virtual bool VerifyCapture (int index) ;

  protected:

    virtual bool ObtainInput   (void) ;
    virtual bool ObtainOutput  (ALCdevice * device) ;

  private:

    int          addConf       (CaDeviceConf * conf,ALenum format) ;
    void         SuitableConf  (void) ;

} ;

class OpenALHostApiInfo : public HostApiInfo
{
public:

    explicit OpenALHostApiInfo (void) ;
    virtual ~OpenALHostApiInfo (void) ;

  protected:

  private:

};

class OpenALHostApi : public HostApi
{
  public:

    AllocationGroup * allocations       ;
    int               inputDeviceCount  ;
    int               outputDeviceCount ;

    explicit  OpenALHostApi       (void) ;
    virtual  ~OpenALHostApi       (void) ;

    virtual CaError  Open         (Stream                ** stream            ,
                                   const StreamParameters * inputParameters   ,
                                   const StreamParameters * outputParameters  ,
                                   double                   sampleRate        ,
                                   CaUint32                 framesPerCallback ,
                                   CaStreamFlags            streamFlags       ,
                                   Conduit                * conduit         ) ;
    virtual void     Terminate    (void) ;
    virtual CaError  isSupported  (const  StreamParameters * inputParameters  ,
                                   const  StreamParameters * outputParameters ,
                                   double sampleRate                        ) ;
    virtual Encoding encoding     (void) const ;
    virtual bool     hasDuplex    (void) const ;

    virtual bool     FindDevices  (void) ;
    virtual bool     FindPlayback (void) ;
    virtual bool     FindCapture  (void) ;

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError OpenALInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAOPENAL_HPP
