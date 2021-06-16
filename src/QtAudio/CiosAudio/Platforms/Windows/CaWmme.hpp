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

#ifndef CAWMME_HPP
#define CAWMME_HPP

#include "CaWindows.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <mmsystem.h>

#if (defined(WIN32) && (defined(_MSC_VER) && (_MSC_VER >= 1400)))

#pragma comment( lib, "winmm.lib" )
#pragma comment( lib, "kernel32.lib" )

#else

#error winmm.lib, kernel32.lib are required.

#endif

/* use CreateThread for CYGWIN, _beginthreadex for all others */
#if !defined(__CYGWIN__) && !defined(_WIN32_WCE)
#define CA_THREAD_ID unsigned
#else
#define CA_THREAD_ID DWORD
#endif

typedef struct                           {
  HANDLE       bufferEvent               ;
  void      *  waveHandles               ;
  unsigned int deviceCount               ;
  unsigned int channelCount              ;
  WAVEHDR   ** waveHeaders               ;
  unsigned int bufferCount               ;
  unsigned int currentBufferIndex        ;
  unsigned int framesPerBuffer           ;
  unsigned int framesUsedInCurrentBuffer ;
} CaMmeSingleDirectionHandlesAndBuffers  ;

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

typedef struct CaMmeDeviceAndChannelCount {
  CaDeviceIndex device                    ;
  int           channelCount              ;
}              CaMmeDeviceAndChannelCount ;

typedef struct CaWmmeStreamInfo                {
  unsigned long                size            ;
  CaHostApiTypeId              hostApiType     ;
  unsigned long                version         ;
  unsigned long                flags           ;
  unsigned long                framesPerBuffer ;
  unsigned long                bufferCount     ;  /* formerly numBuffers */
  unsigned long                deviceCount     ;
  CaMmeDeviceAndChannelCount * devices         ;
  CaWaveFormatChannelMask      channelMask     ;
} CaWmmeStreamInfo                             ;

class WmmeStream : public Stream
{
  public:

    CpuLoad                               cpuLoadMeasurer                    ;
    BufferProcessor                       bufferProcessor                    ;
    int                                   primeStreamUsingCallback           ;
    CaMmeSingleDirectionHandlesAndBuffers input                              ;
    CaMmeSingleDirectionHandlesAndBuffers output                             ;
    HANDLE                                abortEvent                         ;
    HANDLE                                processingThread                   ;
    CA_THREAD_ID                          processingThreadId                 ;
    char                                  throttleProcessingThreadOnOverload ; /* 0 -> don't throtte, non-0 -> throttle */
    int                                   processingThreadPriority           ;
    int                                   highThreadPriority                 ;
    int                                   throttledThreadPriority            ;
    unsigned long                         throttledSleepMsecs                ;
    volatile int                          isStopped                          ;
    volatile int                          isActive                           ;
    volatile int                          stopProcessing                     ; /* stop thread once existing buffers have been returned */
    volatile int                          abortProcessing                    ; /* stop thread immediately */
    DWORD                                 allBuffersDurationMs               ;

    explicit         WmmeStream         (void) ;
    virtual         ~WmmeStream         (void) ;

    virtual CaError  Start              (void) ;
    virtual CaError  Stop               (void) ;
    virtual CaError  Close              (void) ;
    virtual CaError  Abort              (void) ;
    virtual CaError  IsStopped          (void) ;
    virtual CaError  IsActive           (void) ;
    virtual CaTime   GetTime            (void) ;
    virtual double   GetCpuLoad         (void) ;
    virtual CaInt32  ReadAvailable      (void) ;
    virtual CaInt32  WriteAvailable     (void) ;
    virtual CaError  Read               (void       * buffer,unsigned long frames) ;
    virtual CaError  Write              (const void * buffer,unsigned long frames) ;
    virtual bool     hasVolume           (void) ;
    virtual CaVolume MinVolume          (void) ;
    virtual CaVolume MaxVolume          (void) ;
    virtual CaVolume Volume             (int atChannel = -1) ;
    virtual CaVolume setVolume          (CaVolume volume,int atChannel = -1) ;

    CaError AdvanceToNextInputBuffer    (void) ;
    CaError AdvanceToNextOutputBuffer   (void) ;
    CaError CatchUpInputBuffers         (void) ;
    CaError CatchUpOutputBuffers        (void) ;
    int     CurrentInputBuffersAreDone  (void) ;
    int     CurrentOutputBuffersAreDone (void) ;

  protected:

  private:

};

class WmmeDeviceInfo : public DeviceInfo
{
  public:

    DWORD dwFormats                       ; /**<< standard formats bitmask from the WAVEINCAPS and WAVEOUTCAPS structures */
    char  deviceInputChannelCountIsKnown  ; /**<< if the system returns 0xFFFF then we don't really know the number of supported channels (1=>known, 0=>unknown)*/
    char  deviceOutputChannelCountIsKnown ; /**<< if the system returns 0xFFFF then we don't really know the number of supported channels (1=>known, 0=>unknown)*/

    explicit WmmeDeviceInfo (void) ;
    virtual ~WmmeDeviceInfo (void) ;

    CaError IsInputChannelCountSupported  (int channelCount) ;
    CaError IsOutputChannelCountSupported (int channelCount) ;

  protected:

  private:

} ;

class WmmeHostApiInfo : public HostApiInfo
{
  public:

    explicit WmmeHostApiInfo (void) ;
    virtual ~WmmeHostApiInfo (void) ;

  protected:

  private:

};

class WmmeHostApi : public HostApi
{
  public:

    AllocationGroup * allocations       ;
    UINT            * winMmeDeviceIds   ;
    int               inputDeviceCount  ;
    int               outputDeviceCount ;

    explicit  WmmeHostApi        (void) ;
    virtual  ~WmmeHostApi        (void) ;

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

    CaError RetrieveDevicesFromStreamParameters             (
              const StreamParameters     * streamParameters ,
              const CaWmmeStreamInfo     * streamInfo       ,
              CaMmeDeviceAndChannelCount * devices          ,
              unsigned long                deviceCount    ) ;
    CaError ValidateInputChannelCounts                      (
              CaMmeDeviceAndChannelCount * devices          ,
              unsigned long                deviceCount    ) ;
    CaError ValidateOutputChannelCounts                     (
              CaMmeDeviceAndChannelCount * devices          ,
              unsigned long                deviceCount    ) ;

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError WmmeInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAWMME_HPP
