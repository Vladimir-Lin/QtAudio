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

#ifndef CAVLC_HPP
#define CAVLC_HPP

#include "CiosAudioPrivate.hpp"

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

///////////////////////////////////////////////////////////////////////////////

class VlcStream : public Stream
{
  public:

    CpuLoad                               cpuLoadMeasurer                    ;
    bool                                  isInputChannel                     ;
    bool                                  isOutputChannel                    ;
    CaSampleFormat                        SampleFormat                       ;
    int                                   Channels                           ;
    int                                   Frames                             ;
    int                                   BytesPerSample                     ;
    volatile int                          isStopped                          ;
    volatile int                          isActive                           ;
    volatile int                          stopProcessing                     ; /* stop thread once existing buffers have been returned */
    volatile int                          abortProcessing                    ; /* stop thread immediately */
    #if defined(WIN32) || defined(_WIN32)
    unsigned                              ThreadId                           ;
    #else
    pthread_t                             ThreadId                           ;
    #endif

    explicit         VlcStream      (void) ;
    virtual         ~VlcStream      (void) ;

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

  protected:

    CaVolume FalseVolume[2] ;

  private:

};

class VlcDeviceInfo : public DeviceInfo
{
  public:

    explicit VlcDeviceInfo (void) ;
    virtual ~VlcDeviceInfo (void) ;

  protected:

  private:

} ;

class VlcHostApiInfo : public HostApiInfo
{
public:

    explicit VlcHostApiInfo (void) ;
    virtual ~VlcHostApiInfo (void) ;

  protected:

  private:

};

class VlcHostApi : public HostApi
{
  public:

    AllocationGroup * allocations       ;
    int               inputDeviceCount  ;
    int               outputDeviceCount ;

    explicit  VlcHostApi         (void) ;
    virtual  ~VlcHostApi         (void) ;

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

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError VlcInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAVLC_HPP
