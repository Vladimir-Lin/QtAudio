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

#ifndef CAWDMKS_HPP
#define CAWDMKS_HPP

#include "CaWindows.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__
#include <initguid.h>
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501
#endif

#include <setupapi.h>

#if defined(__GNUC__)

/* For MinGW we reference mingw-include files supplied with WASAPI */
#define WINBOOL BOOL

#include "../MinGW/ks.h"
#include "../MinGW/ksmedia.h"

#else

#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>

#endif

#if (defined(WIN32) && (defined(_MSC_VER) && (_MSC_VER >= 1400)))

#pragma comment( lib, "setupapi.lib" )
#pragma comment( lib, "kernel32.lib" )

#else

#error setupapi.lib, kernel32.lib are required.

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

class WdmKsStream ;

typedef struct __CaWdmFilter         CaWdmFilter         ;
typedef struct __CaWdmPin            CaWdmPin            ;
typedef struct __CaProcessThreadInfo CaProcessThreadInfo ;

typedef CaError (*FunctionGetPinAudioPosition)(CaWdmPin *, unsigned long*);
typedef CaError (*FunctionPinHandler)(CaProcessThreadInfo * pInfo, unsigned eventIndex);

/* Function prototype for memory barrier */
typedef void (*FunctionMemoryBarrier)(void);

///////////////////////////////////////////////////////////////////////////////

typedef enum CaWdmKsType {
  Type_kNotUsed          ,
  Type_kWaveCyclic       ,
  Type_kWaveRT           ,
  Type_kCnt              }
  CaWdmKsType            ;

typedef enum CaWdmKsSubType {
  SubType_kUnknown          ,
  SubType_kNotification     ,
  SubType_kPolled           ,
  SubType_kCnt              }
  CaWdmKsSubType            ;

typedef struct    CaWdmKsInfo {
  unsigned long   size        ; /* sizeof(CaWdmKsInfo) */
  CaHostApiTypeId hostApiType ; /* WDMKS */
  unsigned long   version     ; /* < 1 */
  unsigned        noOfPackets ;
  /* The number of packets to use for WaveCyclic devices, range is [2, 8]. Set to zero for default value of 2. */
}                 CaWdmKsInfo ;

typedef struct CaWdmKsDirectionSpecificStreamInfo {
  CaDeviceIndex  device                           ;
  unsigned       channels                         ; /* No of channels the device is opened with */
  unsigned       framesPerHostBuffer              ; /* No of frames of the device buffer */
  int            endpointPinId                    ; /* Endpoint pin ID (on topology filter if topologyName is not empty) */
  int            muxNodeId                        ; /* Only valid for input */
  CaWdmKsSubType streamingSubType                 ; /* Not known until device is opened for streaming */
}              CaWdmKsDirectionSpecificStreamInfo ;

typedef struct CaWdmKsSpecificStreamInfo    {
  CaWdmKsDirectionSpecificStreamInfo input  ;
  CaWdmKsDirectionSpecificStreamInfo output ;
}              CaWdmKsSpecificStreamInfo    ;

typedef enum CaStreamStartEnum {
  StreamStart_kOk              ,
  StreamStart_kFailed          ,
  StreamStart_kCnt             }
  CaStreamStartEnum            ;

typedef struct CaWdmMuxedInput      {
  wchar_t friendlyName [ MAX_PATH ] ;
  ULONG   muxPinId                  ;
  ULONG   muxNodeId                 ;
  ULONG   endpointPinId             ;
} CaWdmMuxedInput                   ;

typedef struct __DATAPACKET {
  KSSTREAM_HEADER Header    ;
  OVERLAPPED      Signal    ;
} DATAPACKET                ;

typedef struct __CaIOPacket {
  DATAPACKET * packet       ;
  unsigned     startByte    ;
  unsigned     lengthBytes  ;
} CaIOPacket                ;

class WdmKsDeviceInfo : public DeviceInfo
{
  public:

    char          compositeName[MAX_PATH] ; /* Composite name consists of pin name + device name in utf8 */
    wchar_t       filterPath   [MAX_PATH] ; /* KS filter path in Unicode! */
    wchar_t       topologyPath [MAX_PATH] ; /* Topology filter path in Unicode! */
    CaWdmFilter * filter                  ;
    CaWdmKsType   streamingType           ;
    GUID          deviceProductGuid       ; /* The product GUID of the device (if supported) */
    unsigned long pin                     ;
    int           muxPosition             ; /* Used only for input devices */
    int           endpointPinId           ;

    explicit WdmKsDeviceInfo (void) ;
    virtual ~WdmKsDeviceInfo (void) ;

    void     Initialize      (void) ;

  protected:

  private:

} ;

class WdmKsHostApiInfo : public HostApiInfo
{
  public:

    explicit WdmKsHostApiInfo (void) ;
    virtual ~WdmKsHostApiInfo (void) ;

  protected:

  private:

};

typedef struct __CaWdmIOInfo   {
  CaWdmPin   * pPin            ;
  char       * hostBuffer      ;
  unsigned     hostBufferSize  ;
  unsigned     framesPerBuffer ;
  unsigned     bytesPerFrame   ;
  unsigned     bytesPerSample  ;
  unsigned     noOfPackets     ;
  HANDLE     * events          ;
  DATAPACKET * packets         ;
  unsigned     lastPosition    ;
  unsigned     pollCntr        ;
} CaWdmIOInfo                  ;

/* Gather all processing variables in a struct */
struct __CaProcessThreadInfo
{
  WdmKsStream * stream               ;
  CaTime        InputBufferAdcTime   ; /* The time when the first sample of the input buffer was captured at the ADC input */
  CaTime        CurrentTime          ; /* The time when the stream callback was invoked */
  CaTime        OutputBufferDacTime  ; /* The time when the first sample of the output buffer will output the DAC */
  CaStreamFlags underover            ;
  int           cbResult             ;
  volatile int  pending              ;
  volatile int  priming              ;
  volatile int  pinsStarted          ;
  unsigned long timeout              ;
  unsigned      captureHead          ;
  unsigned      captureTail          ;
  unsigned      renderHead           ;
  unsigned      renderTail           ;
  CaIOPacket    capturePackets [ 4 ] ;
  CaIOPacket    renderPackets  [ 4 ] ;
};

struct __CaWdmFilter                           {
  HANDLE             handle                    ;
  WdmKsDeviceInfo    devInfo                   ;
  DWORD              deviceNode                ;
  int                pinCount                  ;
  CaWdmPin        ** pins                      ;
  CaWdmFilter     *  topologyFilter            ;
  wchar_t            friendlyName [ MAX_PATH ] ;
  int                validPinCount             ;
  int                usageCount                ;
  KSMULTIPLE_ITEM *  connections               ;
  KSMULTIPLE_ITEM *  nodes                     ;
  int                filterRefCount            ;
}                                              ;

struct __CaWdmPin                                    {
  HANDLE                      handle                 ;
  CaWdmMuxedInput          ** inputs                 ;
  unsigned                    inputCount             ;
  wchar_t                     friendlyName[MAX_PATH] ;
  CaWdmFilter               * parentFilter           ;
  CaWdmKsSubType              pinKsSubType           ;
  unsigned long               pinId                  ;
  unsigned long               endpointPinId          ;
  KSPIN_CONNECT             * pinConnect             ;
  unsigned long               pinConnectSize         ;
  KSDATAFORMAT_WAVEFORMATEX * ksDataFormatWfx        ;
  KSPIN_COMMUNICATION         communication          ;
  KSDATARANGE               * dataRanges             ;
  KSMULTIPLE_ITEM           * dataRangesItem         ;
  KSPIN_DATAFLOW              dataFlow               ;
  KSPIN_CINSTANCES            instances              ;
  unsigned long               frameSize              ;
  int                         maxChannels            ;
  unsigned long               formats                ;
  int                         defaultSampleRate      ;
  ULONG                     * positionRegister       ;
  ULONG                       hwLatency              ;
  FunctionMemoryBarrier       fnMemBarrier           ;
  FunctionGetPinAudioPosition fnAudioPosition        ;
  FunctionPinHandler          fnEventHandler         ;
  FunctionPinHandler          fnSubmitHandler        ;
}                                                    ;

/* Used for transferring device infos during scanning / rescanning */
typedef struct __WdmScanDeviceInfosResults {
  DeviceInfo ** deviceInfos                ;
  CaDeviceIndex defaultInputDevice         ;
  CaDeviceIndex defaultOutputDevice        ;
} WdmScanDeviceInfosResults                ;

///////////////////////////////////////////////////////////////////////////////

class WdmKsStream : public Stream
{
  public:

    CpuLoad                   cpuLoadMeasurer                       ;
    BufferProcessor           bufferProcessor                       ;
    CaWdmKsSpecificStreamInfo hostApiStreamInfo                     ;
    AllocationGroup         * allocGroup                            ;
    CaWdmIOInfo               capture                               ;
    CaWdmIOInfo               render                                ;
    int                       streamStarted                         ;
    int                       streamActive                          ;
    int                       streamStop                            ;
    int                       streamAbort                           ;
    int                       oldProcessPriority                    ;
    HANDLE                    streamThread                          ;
    HANDLE                    eventAbort                            ;
    HANDLE                    eventStreamStart [ StreamStart_kCnt ] ;
    CaError                   threadResult                          ;
    CaStreamFlags             streamFlags                           ;
    RingBuffer                ringBuffer                            ;
    char                    * ringBufferData                        ;
    int                       userInputChannels                     ;
    int                       deviceInputChannels                   ;
    int                       userOutputChannels                    ;
    int                       deviceOutputChannels                  ;
    bool                      doResample                            ;
    void                    * Resampler                             ;
    double                    NearestSampleRate                     ;

    explicit         WdmKsStream    (void) ;
    virtual         ~WdmKsStream    (void) ;

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

    void             ResetEvents    (void) ;
    void             CloseEvents    (void) ;

  protected:

  private:

};

class WdmKsHostApi : public HostApi
{
  public:

    AllocationGroup * allocations             ;
    int               deviceCount             ;
    bool              CanDoResample           ;

    explicit  WdmKsHostApi       (void) ;
    virtual  ~WdmKsHostApi       (void) ;

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

    CaError ScanDeviceInfos      (CaHostApiIndex hostApiIndex     ,
                                  void        ** scanResults      ,
                                  int         *  newDeviceCount ) ;
    CaError CommitDeviceInfos    (CaHostApiIndex index            ,
                                  void        *  scanResults      ,
                                  int            deviceCount    ) ;
    CaError DisposeDeviceInfos   (void        *  scanResults      ,
                                  int            deviceCount    ) ;
    CaError ValidateSpecificStreamParameters                                (
                                  const StreamParameters * streamParameters ,
                                  const CaWdmKsInfo      * streamInfo     ) ;

  protected:

    double Nearest               (CaWdmPin              * pin           ,
                                  double                  sampleRate    ,
                                  CaSampleFormat          format        ,
                                  unsigned                channels      ,
                                  CaWaveFormatChannelMask channelMask ) ;

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError WdmKsInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAWDMKS_HPP
