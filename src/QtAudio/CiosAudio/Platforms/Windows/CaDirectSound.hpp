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

#ifndef CADIRECTSOUND_HPP
#define CADIRECTSOUND_HPP

#include "CaWindows.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <initguid.h> /* make sure ds guids get defined */
#include <objbase.h>

#define DIRECTSOUND_VERSION 0x0800
#include <dsound.h>
#include <dsconf.h>

#if (defined(WIN32) && (defined(_MSC_VER) && (_MSC_VER >= 1400)))

#pragma comment( lib, "dsound.lib" )
#pragma comment( lib, "winmm.lib" )
#pragma comment( lib, "kernel32.lib" )

#else

#error dsound.lib, winmm.lib, kernel32.lib are required.

#endif

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

#define CaWinDirectSoundUseLowLevelLatencyParameters (0x01)
#define CaWinDirectSoundUseChannelMask               (0x04)

typedef struct CaDirectSoundStreamInfo    {
  unsigned long           size            ;
  CaHostApiTypeId         hostApiType     ;
  unsigned long           version         ;
  unsigned long           flags           ;
  unsigned long           framesPerBuffer ;
  CaWaveFormatChannelMask channelMask     ;
} CaDirectSoundStreamInfo                 ;

class DirectSoundStream : public Stream
{
  public:

    typedef enum         {
      NoStream       = 0 ,
      CallbackStream = 1 ,
      BlockingStream = 2 }
      StreamDirection    ;

    CpuLoad                    cpuLoadMeasurer               ;
    BufferProcessor            bufferProcessor               ;
    // full duplex
    LPDIRECTSOUNDFULLDUPLEX8   pDirectSoundFullDuplex8       ;
    // Output
    LPDIRECTSOUND              pDirectSound                  ;
    LPDIRECTSOUNDBUFFER        pDirectSoundPrimaryBuffer     ;
    LPDIRECTSOUNDBUFFER        pDirectSoundOutputBuffer      ;
    DWORD                      outputBufferWriteOffsetBytes  ; // last write position
    INT                        outputBufferSizeBytes         ;
    INT                        outputFrameSizeBytes          ;
    // Try to detect play buffer underflows.
    LARGE_INTEGER              perfCounterTicksPerBuffer     ;
    // counter ticks it should take to play a full buffer
    LARGE_INTEGER              previousPlayTime              ;
    DWORD                      previousPlayCursor            ;
    UINT                       outputUnderflowCount          ;
    BOOL                       outputIsRunning               ;
    INT                        finalZeroBytesWritten         ;
    // used to determine when we've flushed the whole buffer
    // Input
    LPDIRECTSOUNDCAPTURE       pDirectSoundCapture           ;
    LPDIRECTSOUNDCAPTUREBUFFER pDirectSoundInputBuffer       ;
    INT                        inputFrameSizeBytes           ;
    UINT                       readOffset                    ; // last read position
    UINT                       inputBufferSizeBytes          ;
     // input and output host ringbuffers have the same number of frames
    unsigned long              hostBufferSizeFrames          ;
    double                     framesWritten                 ;
    double                     secondsPerHostByte            ; // Used to optimize latency calculation for outTime
    double                     pollingPeriodSeconds          ;
    //
    CaStreamFlags              callbackFlags                 ;
    CaStreamFlags              streamFlags                   ;
    int                        callbackResult                ;
    HANDLE                     processingCompleted           ;
    // FIXME - move all below to PaUtilStreamRepresentation
    volatile int               isStarted                     ;
    volatile int               isActive                      ;
    volatile int               stopProcessing                ; /* stop thread once existing buffers have been returned */
    volatile int               abortProcessing               ; /* stop thread immediately */
    //
    UINT                       systemTimerResolutionPeriodMs ; /* set to 0 if we were unable to set the timer period */
    MMRESULT                   timerID                       ;
    //
    StreamDirection            Direction                     ;
    bool                       timeSlicing                   ;

    explicit         DirectSoundStream (void) ;
    virtual         ~DirectSoundStream (void) ;

    virtual CaError  Start             (void) ;
    virtual CaError  Stop              (void) ;
    virtual CaError  Close             (void) ;
    virtual CaError  Abort             (void) ;
    virtual CaError  IsStopped         (void) ;
    virtual CaError  IsActive          (void) ;
    virtual CaTime   GetTime           (void) ;
    virtual double   GetCpuLoad        (void) ;
    virtual CaInt32  ReadAvailable     (void) ;
    virtual CaInt32  WriteAvailable    (void) ;
    virtual CaError  Read              (void       * buffer,unsigned long frames) ;
    virtual CaError  Write             (const void * buffer,unsigned long frames) ;
    virtual bool     hasVolume          (void) ;
    virtual CaVolume MinVolume         (void) ;
    virtual CaVolume MaxVolume         (void) ;
    virtual CaVolume Volume            (int atChannel = -1) ;
    virtual CaVolume setVolume         (CaVolume volume,int atChannel = -1) ;

  protected:

    HRESULT  ClearOutputBuffer         (void) ;
    CaVolume toVolume                  (CaVolume dB) ;
    CaVolume toDB                      (CaVolume volume) ;

  private:

};

class DirectSoundDeviceInfo : public DeviceInfo
{
  public:

    GUID   guid                            ;
    GUID * lpGUID                          ;
    double sampleRates [ 3 ]               ;
    char   deviceInputChannelCountIsKnown  ; /**<< if the system returns 0xFFFF then we don't really know the number of supported channels (1=>known, 0=>unknown)*/
    char   deviceOutputChannelCountIsKnown ; /**<< if the system returns 0xFFFF then we don't really know the number of supported channels (1=>known, 0=>unknown)*/

    explicit DirectSoundDeviceInfo (void) ;
    virtual ~DirectSoundDeviceInfo (void) ;

  protected:

  private:

} ;

class DirectSoundHostApiInfo : public HostApiInfo
{
  public:

    explicit DirectSoundHostApiInfo (void) ;
    virtual ~DirectSoundHostApiInfo (void) ;

  protected:

  private:

};

class DirectSoundHostApi : public HostApi
{
  public:

    AllocationGroup         * allocations             ;
    CaComInitializationResult comInitializationResult ;

    explicit  DirectSoundHostApi (void) ;
    virtual  ~DirectSoundHostApi (void) ;

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

CaError DirectSoundInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CADIRECTSOUND_HPP
