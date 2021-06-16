/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/22                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#ifndef CAWASAPI_HPP
#define CAWASAPI_HPP

#include "CaWindows.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <mmsystem.h>
#include <mmreg.h>  // must be before other Wasapi headers
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
  #include <Avrt.h>
  #include <Audioclient.h>
  #include <endpointvolume.h>
  #include <mmdeviceapi.h>
  #include <functiondiscoverykeys.h>
  #include <devicetopology.h> // Used to get IKsJackDescription interface
#endif

#ifndef __MWERKS__
#include <malloc.h>
#include <memory.h>
#endif

#ifndef NTDDI_VERSION

    #undef WINVER
    #undef _WIN32_WINNT
    #define WINVER       0x0600 // VISTA
    #define _WIN32_WINNT WINVER

    #ifndef _AVRT_ //<< fix MinGW dummy compile by defining missing type: AVRT_PRIORITY
        typedef enum _AVRT_PRIORITY
        {
            AVRT_PRIORITY_LOW = -1,
            AVRT_PRIORITY_NORMAL,
            AVRT_PRIORITY_HIGH,
            AVRT_PRIORITY_CRITICAL
        } AVRT_PRIORITY, *PAVRT_PRIORITY;
    #endif

    #include <basetyps.h> // << for IID/CLSID
    #include <rpcsal.h>
    #include <sal.h>

    #ifndef __LPCGUID_DEFINED__
        #define __LPCGUID_DEFINED__
        typedef const GUID *LPCGUID;
    #endif

    #ifndef PROPERTYKEY_DEFINED
        #define PROPERTYKEY_DEFINED
        typedef struct _tagpropertykey
        {
            GUID fmtid;
            DWORD pid;
        } 	PROPERTYKEY;
    #endif

    #ifdef __midl_proxy
        #define __MIDL_CONST
    #else
        #define __MIDL_CONST const
    #endif

    #ifdef WIN64
        #include <wtypes.h>
        typedef LONG NTSTATUS;
        #define FASTCALL
        #include <oleidl.h>
        #include <objidl.h>
     #else
        typedef struct _BYTE_BLOB
        {
            unsigned long clSize;
            unsigned char abData[ 1 ];
        } 	BYTE_BLOB;
        typedef /* [unique] */  __RPC_unique_pointer BYTE_BLOB *UP_BYTE_BLOB;
        typedef LONGLONG REFERENCE_TIME;
        #define NONAMELESSUNION
    #endif

    #ifndef WAVE_FORMAT_IEEE_FLOAT
        #define WAVE_FORMAT_IEEE_FLOAT 0x0003 // 32-bit floating-point
    #endif

    #ifndef __MINGW_EXTENSION
        #if defined(__GNUC__) || defined(__GNUG__)
            #define __MINGW_EXTENSION __extension__
        #else
            #define __MINGW_EXTENSION
        #endif
    #endif

    #include <sdkddkver.h>
    #include <propkeydef.h>
    #define COBJMACROS
    #include <audioclient.h>
    #include <mmdeviceapi.h>
    #include <endpointvolume.h>
    #include <functiondiscoverykeys.h>
    #include <devicetopology.h> // Used to get IKsJackDescription interface

#endif // NTDDI_VERSION

/* use CreateThread for CYGWIN/Windows Mobile, _beginthreadex for all others */
#if !defined(__CYGWIN__) && !defined(_WIN32_WCE)
  #define CA_THREAD_FUNC static unsigned WINAPI
  #define CA_THREAD_ID          unsigned
#else
  #define CA_THREAD_FUNC static DWORD WINAPI
  #define CA_THREAD_ID          DWORD
#endif

#define MAX_STR_LEN 512

///////////////////////////////////////////////////////////////////////////////

typedef enum CaWaSapiFlags             { /* Setup flags */
  CaWaSapiExclusive             = 0x01 , /* puts WASAPI into exclusive mode */
  CaWaSapiRedirectHostProcessor = 0x02 , /* allows to skip internal PA processing completely */
  CaWaSapiUseChannelMask        = 0x04 , /* assigns custom channel mask */
  CaWaSapiPolling               = 0x08 , /* selects non-Event driven method of data read/write
                                            Note: WASAPI Event driven core is capable of 2ms latency!!!, but Polling
                                            method can only provide 15-20ms latency. */
  CaWaSapiThread                = 0x10   /* forces custom thread priority setting.
                                            must be used if CaWasapiStreamInfo::threadPriority
                                            is set to custom value. */
} CaWaSapiFlags                        ;

typedef enum CaWaSapiDeviceRole       { /* Device role */
  eRoleRemoteNetworkDevice       =  0 ,
  eRoleSpeakers                  =  1 ,
  eRoleLineLevel                 =  2 ,
  eRoleHeadphones                =  3 ,
  eRoleMicrophone                =  4 ,
  eRoleHeadset                   =  5 ,
  eRoleHandset                   =  6 ,
  eRoleUnknownDigitalPassthrough =  7 ,
  eRoleSPDIF                     =  8 ,
  eRoleHDMI                      =  9 ,
  eRoleUnknownFormFactor         = 10
} CaWaSapiDeviceRole                  ;

typedef enum CaWaSapiJackConnectionType { /* Jack connection type */
  eJackConnTypeUnknown                  ,
  eJackConnType3Point5mm                ,
  eJackConnTypeQuarter                  ,
  eJackConnTypeAtapiInternal            ,
  eJackConnTypeRCA                      ,
  eJackConnTypeOptical                  ,
  eJackConnTypeOtherDigital             ,
  eJackConnTypeOtherAnalog              ,
  eJackConnTypeMultichannelAnalogDIN    ,
  eJackConnTypeXlrProfessional          ,
  eJackConnTypeRJ11Modem                ,
  eJackConnTypeCombination
} CaWaSapiJackConnectionType            ;

typedef enum CaWaSapiJackGeoLocation { /* Jack geometric location */
  eJackGeoLocUnk              = 0    ,
  eJackGeoLocRear             = 1    , /* matches EPcxGeoLocation::eGeoLocRear */
  eJackGeoLocFront            = 2    ,
  eJackGeoLocLeft             = 3    ,
  eJackGeoLocRight            = 4    ,
  eJackGeoLocTop              = 5    ,
  eJackGeoLocBottom           = 6    ,
  eJackGeoLocRearPanel        = 7    ,
  eJackGeoLocRiser            = 8    ,
  eJackGeoLocInsideMobileLid  = 9    ,
  eJackGeoLocDrivebay         = 10   ,
  eJackGeoLocHDMI             = 11   ,
  eJackGeoLocOutsideMobileLid = 12   ,
  eJackGeoLocATAPI            = 13   ,
  eJackGeoLocReserved5        = 14   ,
  eJackGeoLocReserved6        = 15
} CaWaSapiJackGeoLocation            ;

typedef enum CaWaSapiJackGenLocation { /* Jack general location */
  eJackGenLocPrimaryBox = 0          ,
  eJackGenLocInternal   = 1          ,
  eJackGenLocSeparate   = 2          ,
  eJackGenLocOther      = 3
} CaWaSapiJackGenLocation            ;

typedef enum CaWaSapiJackPortConnection  { /* Jack's type of port */
  eJackPortConnJack                  = 0 ,
  eJackPortConnIntegratedDevice      = 1 ,
  eJackPortConnBothIntegratedAndJack = 2 ,
  eJackPortConnUnknown               = 3
} CaWaSapiJackPortConnection             ;

typedef enum CaWaSapiThreadPriority { // Thread priority
  eThreadPriorityNone          = 0  ,
  eThreadPriorityAudio         = 1  , // Default for Shared mode.
  eThreadPriorityCapture       = 2  ,
  eThreadPriorityDistribution  = 3  ,
  eThreadPriorityGames         = 4  ,
  eThreadPriorityPlayback      = 5  ,
  eThreadPriorityProAudio      = 6  , // Default for Exclusive mode.
  eThreadPriorityWindowManager = 7
} CaWaSapiThreadPriority            ;

enum               {
  S_INPUT      = 0 ,
  S_OUTPUT     = 1 ,
  S_COUNT      = 2 ,
  S_FULLDUPLEX = 0
}                  ;

// Number of packets which compose single contignous buffer. With trial and error it was calculated
// that WASAPI Input sub-system uses 6 packets per whole buffer. Please provide more information
// or corrections if available.
enum { WASAPI_PACKETS_PER_INPUT_BUFFER = 6 };

// Mixer function
typedef void (*MixMonoToStereoF) (void *__to, void *__from, UINT32 count);

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

typedef struct CaWaSapiStreamInfo        { // Stream descriptor
  unsigned long           size           ; // sizeof(CaWaSapiStreamInfo)
  CaHostApiTypeId         hostApiType    ; // WASAPI
  unsigned long           version        ; // 1
  unsigned long           flags          ; // collection of PaWasapiFlags
  CaWaveFormatChannelMask channelMask    ;
  CaWaSapiThreadPriority  threadPriority ;
} CaWaSapiStreamInfo                     ;

///////////////////////////////////////////////////////////////////////////////

class WaSapiDeviceInfo : public DeviceInfo
{
  public:

    IMMDevice          * device                     ;
    WCHAR                szDeviceID [ MAX_STR_LEN ] ;
    DWORD                state                      ;
    EDataFlow            flow                       ;
    REFERENCE_TIME       DefaultDevicePeriod        ;
    REFERENCE_TIME       MinimumDevicePeriod        ;
    WAVEFORMATEXTENSIBLE DefaultFormat              ;
    EndpointFormFactor   formFactor                 ;

    explicit WaSapiDeviceInfo (void) ;
    virtual ~WaSapiDeviceInfo (void) ;

    virtual void Initialize   (void) ;

  protected:

  private:

} ;

class WaSapiHostApiInfo : public HostApiInfo
{
  public:

    explicit WaSapiHostApiInfo (void) ;
    virtual ~WaSapiHostApiInfo (void) ;

  protected:

  private:

};

typedef struct CaWaSapiAudioClientParams {
  WaSapiDeviceInfo * device_info         ;
  StreamParameters   stream_params       ;
  CaWaSapiStreamInfo wasapi_params       ;
  UINT32             frames_per_buffer   ;
  double             sample_rate         ;
  BOOL               blocking            ;
  BOOL               full_duplex         ;
  BOOL               wow64_workaround    ;
} CaWaSapiAudioClientParams              ;

typedef struct              CaWaSapiSubStream      {
  IAudioClient            * clientParent           ;
  IStream                 * clientStream           ;
  IAudioClient            * clientProc             ;
  WAVEFORMATEXTENSIBLE      wavex                  ;
  UINT32                    bufferSize             ;
  REFERENCE_TIME            deviceLatency          ;
  REFERENCE_TIME            period                 ;
  double                    latencySeconds         ;
  UINT32                    framesPerHostCallback  ;
  AUDCLNT_SHAREMODE         shareMode              ;
  UINT32                    streamFlags            ; // AUDCLNT_STREAMFLAGS_EVENTCALLBACK, ...
  UINT32                    flags                  ;
  CaWaSapiAudioClientParams params                 ; // parameters
  UINT32                    buffers                ; // number of buffers used (from host side)
  UINT32                    framesPerBuffer        ; // number of frames per 1 buffer
  BOOL                      userBufferAndHostMatch ;
  void                    * monoBuffer             ; // pointer to buffer
  UINT32                    monoBufferSize         ; // buffer size in bytes
  MixMonoToStereoF          monoMixer              ; // pointer to mixer function
  RingBuffer              * tailBuffer             ; // buffer with trailing sample for blocking mode operations (only for Input)
  void                    * tailBufferMemory       ; // tail buffer memory region
}                           CaWaSapiSubStream      ;

class WaSapiStream : public Stream
{
  public:

    CpuLoad                cpuLoadMeasurer     ;
    BufferProcessor        bufferProcessor     ;
    CaWaSapiSubStream      in                  ;
    IAudioCaptureClient  * captureClientParent ;
    IStream              * captureClientStream ;
    IAudioCaptureClient  * captureClient       ;
    IAudioEndpointVolume * inVol               ;
    CaWaSapiSubStream      out                 ;
    IAudioRenderClient   * renderClientParent  ;
    IStream              * renderClientStream  ;
    IAudioRenderClient   * renderClient        ;
    IAudioEndpointVolume * outVol              ;
    HANDLE                 event[S_COUNT]      ;
    CaHostBufferSizeMode   bufferMode          ;
    volatile BOOL          running             ;
    CA_THREAD_ID           dwThreadId          ;
    HANDLE                 hThread             ;
    HANDLE                 hCloseRequest       ;
    HANDLE                 hThreadStart        ;
    HANDLE                 hThreadExit         ;
    HANDLE                 hBlockingOpStreamRD ;
    HANDLE                 hBlockingOpStreamWR ;
    BOOL                   bBlocking           ;
    HANDLE                 hAvTask             ;
    CaWaSapiThreadPriority nThreadPriority     ;
    bool                   doResample          ;
    void                 * Resampler           ;
    double                 NearestSampleRate   ;

    explicit         WaSapiStream   (void) ;
    virtual         ~WaSapiStream   (void) ;

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

    CaError  GetFramesPerHostBuffer (unsigned int * nInput    ,
                                     unsigned int * nOutput ) ;
    CaUint32 GetOutputFrameCount    (void) ;

    HRESULT  ProcessInputBuffer     (void) ;
    HRESULT  ProcessOutputBuffer    (UINT32 frames) ;

    HRESULT  CreateAudioClient      (CaWaSapiSubStream * pSub       ,
                                     BOOL                output     ,
                                     CaError           * pa_error ) ;

    void     StreamOnStop           (void) ;
    void     Cleanup                (void) ;
    void     Finish                 (void) ;

    HRESULT PollGetInputFramesAvailable   (UINT32 * available) ;
    HRESULT PollGetOutputFramesAvailable  (UINT32 * available) ;

    HRESULT MarshalStreamComPointers      (void) ;
    HRESULT UnmarshalStreamComPointers    (void) ;
    void    ReleaseUnmarshaledComPointers (void) ;

    CaError ActivateAudioClientInput      (void) ;
    CaError ActivateAudioClientOutput     (void) ;

    bool    setResampling                 (double         inputSampleRate  ,
                                           double         outputSampleRate ,
                                           int            channels         ,
                                           CaSampleFormat format         ) ;

  protected:

    void    InitializeSubStream           (CaWaSapiSubStream * substream) ;

  private:

};

class WaSapiHostApi : public HostApi
{
  public:

    AllocationGroup         * allocations                     ;
    CaComInitializationResult comInitializationResult         ;
    IMMDeviceEnumerator     * enumerator                      ;
    UINT32                    deviceCount                     ;
    WCHAR                     defaultRenderer [ MAX_STR_LEN ] ;
    WCHAR                     defaultCapturer [ MAX_STR_LEN ] ;
    WaSapiDeviceInfo        * devInfo                         ;
    BOOL                      useWOW64Workaround              ;
    bool                      CanDoResample                   ;

    explicit  WaSapiHostApi      (void) ;
    virtual  ~WaSapiHostApi      (void) ;

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
                                  double                    sampleRate     ) ;
    virtual CaError  isValid     (const  StreamParameters * inputParameters  ,
                                  const  StreamParameters * outputParameters ,
                                  double                    sampleRate     ) ;
    virtual Encoding encoding    (void) const ;
    virtual bool     hasDuplex   (void) const ;

  protected:

    virtual double   Nearest     (const  StreamParameters * inputParameters  ,
                                  const  StreamParameters * outputParameters ,
                                  double                    sampleRate     ) ;

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError WaSapiInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAWASAPI_HPP
