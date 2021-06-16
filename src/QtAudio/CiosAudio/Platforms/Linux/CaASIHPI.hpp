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

#ifndef CAASIHPI_HPP
#define CAASIHPI_HPP

#include "CaLinux.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <asihpi/hpi.h>

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

typedef enum AsiHpiStreamState    {
  AsiHpiStoppedState          = 0 ,
  AsiHpiActiveState           = 1 ,
  AsiHpiCallbackFinishedState = 2 }
  AsiHpiStreamState               ;

/* Stream state information, collected together for convenience */

typedef struct AsiHpiStreamInfo  {
  uint16_t     state             ;
  uint32_t     bufferSize        ;
  uint32_t     dataSize          ;
  uint32_t     frameCounter      ;
  uint32_t     auxDataSize       ;
  uint32_t     totalBufferedData ;
  uint32_t     availableFrames   ;
  int          overflow          ;
  int          underflow         ;
}              AsiHpiStreamInfo  ;

class AsiHpiStream ;

class AsiHpiDeviceInfo : public DeviceInfo
{
  public:

    uint16_t adapterIndex        ;
    uint16_t adapterType         ;
    uint16_t adapterVersion      ;
    uint32_t adapterSerialNumber ;
    uint16_t streamIndex         ;
    uint16_t streamIsOutput      ;

    explicit AsiHpiDeviceInfo (void) ;
    virtual ~AsiHpiDeviceInfo (void) ;

  protected:

  private:

};

/* Stream component data (associated with one direction, i.e. either input or output) */

class AsiHpiStreamComponent
{
  public:

    AsiHpiDeviceInfo * hpiDevice          ;
    hpi_handle_t       hpiStream          ;
    struct hpi_format  hpiFormat          ;
    uint32_t           bytesPerFrame      ;
    uint32_t           hardwareBufferSize ;
    uint32_t           hostBufferSize     ;
    uint32_t           outputBufferCap    ;
    uint8_t          * tempBuffer         ;
    uint32_t           tempBufferSize     ;

    explicit AsiHpiStreamComponent (void) ;
    virtual ~AsiHpiStreamComponent (void) ;

    virtual CaError GetStreamInfo       (AsiHpiStreamInfo * info  ) ;
    virtual void    StreamComponentDump (AsiHpiStream     * stream) ;
    virtual CaError SetupBuffers        (uint32_t      pollingInterval       ,
                                         unsigned long framesPerPaHostBuffer ,
                                         CaTime        suggestedLatency    ) ;

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

class AsiHpiStream : public Stream
{
  public:

    CpuLoad                  cpuLoadMeasurer        ;
    BufferProcessor          bufferProcessor        ;
    AllocationGroup       *  allocations            ;
    AsiHpiStreamComponent *  input                  ;
    AsiHpiStreamComponent *  output                 ;
    uint32_t                 pollingInterval        ;
    int                      callbackMode           ;
    unsigned long            maxFramesPerHostBuffer ;
    int                      neverDropInput         ;
    void                  ** blockingUserBufferCopy ;
    pthread_t                thread                 ;
    volatile sig_atomic_t    state                  ;
    volatile sig_atomic_t    callbackAbort          ;
    volatile sig_atomic_t    callbackFinished       ;

    explicit         AsiHpiStream    (void) ;
    virtual         ~AsiHpiStream    (void) ;

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

    virtual void     StreamDump      (void) ;
    virtual CaError  BeginProcessing (unsigned long * numFrames,CaStreamFlags * cbFlags) ;
    virtual CaError  EndProcessing   (unsigned long   numFrames,CaStreamFlags * cbFlags) ;
    virtual CaError  WaitForFrames   (unsigned long * framesAvail,CaStreamFlags * cbFlags) ;
    virtual CaError  StartStream     (int outputPrimed) ;
    virtual CaError  StopStream      (int abort) ;
    virtual CaError  ExplicitStop    (int abort) ;

    virtual signed long GetStreamReadAvailable (void) ;
    virtual CaError     PrimeOutputWithSilence (void) ;
    virtual void        CalculateTimeInfo      (Conduit * conduit) ;

  protected:

  private:

};

class AsiHpiHostApi : public HostApi
{
  public:

    AllocationGroup * allocations  ;
    CaHostApiIndex    hostApiIndex ;

    explicit  AsiHpiHostApi      (void) ;
    virtual  ~AsiHpiHostApi      (void) ;

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

    virtual CaError BuildDeviceList (void) ;

    virtual CaError CreateFormat (const StreamParameters  *  parameters  ,
                                  double                     sampleRate  ,
                                  AsiHpiDeviceInfo        ** hpiDevice   ,
                                  struct hpi_format       *  hpiFormat ) ;
    virtual CaError OpenInput    (const AsiHpiDeviceInfo  *  hpiDevice   ,
                                  const struct hpi_format *  hpiFormat   ,
                                  hpi_handle_t            *  hpiStream ) ;
    virtual CaError OpenOutput   (const AsiHpiDeviceInfo  *  hpiDevice   ,
                                  const struct hpi_format *  hpiFormat   ,
                                  hpi_handle_t            *  hpiStream ) ;

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError AsiHpiInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAASIHPI_HPP
