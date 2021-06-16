/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2015/02/13                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#ifndef CIOSAUDIO_HPP
#define CIOSAUDIO_HPP

// if you are going to remove all debug message inside this library
// un-comment the following define or add a define in cmake

// #define REMOVE_DEBUG_MESSAGE
// #define ENABLE_MSVC_DEBUG_OUTPUT 1

// #define CAUTILITIES 1
// #define FFMPEGLIB 1

// define your private namespace here
// #define CAC_NAMESPACE CiosAudio

// To avoid using namespace, define the following
// #define DONT_USE_NAMESPACE

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*****************************************************************************/
/*                             Endianness detection                          */
/*****************************************************************************/

#if defined(__APPLE__)
  #if defined(__LITTLE_ENDIAN__)
    #if !defined( CA_LITTLE_ENDIAN )
      #define CA_LITTLE_ENDIAN
    #endif
    #if defined( CA_BIG_ENDIAN )
      #undef CA_BIG_ENDIAN
    #endif
  #else
    #if !defined( CA_BIG_ENDIAN )
      #define CA_BIG_ENDIAN
    #endif
    #if defined( CA_LITTLE_ENDIAN )
      #undef CA_LITTLE_ENDIAN
    #endif
  #endif
#else
  #if defined(CA_LITTLE_ENDIAN) || defined(CA_BIG_ENDIAN)
    #if defined(CA_LITTLE_ENDIAN) && defined(CA_BIG_ENDIAN)
    #error both CA_LITTLE_ENDIAN and CA_BIG_ENDIAN have been defined - only one endianness at a time please
    #endif
  #else
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(LITTLE_ENDIAN) || defined(__i386) || defined(_M_IX86) || defined(__x86_64__)
      #define CA_LITTLE_ENDIAN
    #else
      #define CA_BIG_ENDIAN
    #endif
  #endif
  #if !defined(CA_LITTLE_ENDIAN) && !defined(CA_BIG_ENDIAN)
    #error CiosAudio was unable to automatically determine the endianness of the target platform
  #endif
#endif

#ifndef QT_STATIC
#    if defined(QT_BUILD_QTAUDIO_LIB)
#      if defined(Q_OS_WIN)
#        define AUDIO_EXPORT Q_DECL_EXPORT
#      else
#        define AUDIO_EXPORT
#      endif
#    else
#      if defined(Q_OS_WIN)
#        define AUDIO_EXPORT Q_DECL_IMPORT
#      else
#        define AUDIO_EXPORT
#      endif
#    endif
#else
#    define AUDIO_EXPORT
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <sys/types.h>
#include <wchar.h>

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#include <process.h>
#endif

#if defined(FFMPEGLIB)

#include <libavcodec/avcodec.h>
#include <libavcodec/version.h>
#include <libavformat/avformat.h>
#include <libavformat/version.h>
#include <libavutil/avutil.h>
#include <libavutil/samplefmt.h>
#include <libavutil/cpu.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/version.h>
#include <libswresample/swresample.h>
#include <libswresample/version.h>
#include <libswscale/swscale.h>
#include <libswscale/version.h>

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include <string>
#include <sstream>
#include <list>
#include <vector>
#include <stack>
#include <map>
#include <cstring>
#include <queue>
#include <iostream>
#include <iomanip>
#include <typeinfo>
#include <exception>
#include <new>
#include <utility>
#include <iterator>
#include <cstdlib>
#include <functional>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <iterator>
#include <set>

#if defined(DISPLAY_COMPILE_ARCHITECTURE)
#if defined(WIN32)
#if defined(_WIN64)
#pragma message("Compile in Windows x64 Architecture")
#else
#pragma message("Compile in Windows x86 Architecture")
#endif
#endif
#endif

#if !defined(CAC_NAMESPACE)
#define CAC_NAMESPACE CiosAudio
#endif

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

/*****************************************************************************/
/*                                Value Types                                */
/*****************************************************************************/

#ifndef CIOS_SHORT
#define CIOS_SHORT    2
#endif

#ifndef CIOS_INT
#define CIOS_INT      4
#endif

#ifndef CIOS_LONG
#define CIOS_LONG     4
#endif

#ifndef CIOS_LONGLONG
#define CIOS_LONGLONG 8
#endif

// determine 8 bits integer data type
typedef   signed char CaInt8        ;
typedef unsigned char CaUint8       ;

// determine 16 bits integer data type
#if   ( CIOS_SHORT == 2 )
typedef   signed short CaInt16      ;
typedef unsigned short CaUint16     ;
#elif ( CIOS_INT   == 2 )
typedef   signed int   CaInt16      ;
typedef unsigned int   CaUint16     ;
#else
#error CIOS Audio Core was unable to determine which type to use for 16 bits integer on the target platform
#endif

// determine 32 bits integer data type
#if   ( CIOS_SHORT == 4 )
typedef   signed short CaInt32      ;
typedef unsigned short CaUint32     ;
#elif ( CIOS_INT   == 4 )
typedef   signed int   CaInt32      ;
typedef unsigned int   CaUint32     ;
#elif ( CIOS_LONG  == 4 )
typedef   signed long  CaInt32      ;
typedef unsigned long  CaUint32     ;
#else
#error CIOS Audio Core was unable to determine which type to use for 32 bits integer on the target platform
#endif

// determine 64 bits integer data type
typedef   signed long long CaInt64  ;
typedef unsigned long long CaUint64 ;

// determine 32 bits floating point data type
typedef float         CaFloat32     ;

// determine 64 bits floating point data type
typedef double        CaFloat64     ;

// define CiosAudio Strings
typedef       char *  CaString      ;
typedef const char *  CaConstString ;

// define CiosAudio Time
typedef double        CaTime        ;

// define CiosAudio Audio Volume
typedef double        CaVolume      ;

/*****************************************************************************/
/*                       Typedefs, Enumerations and Defines                  */
/*****************************************************************************/

typedef int CaDeviceIndex                        ;
typedef int CaHostApiIndex                       ;
typedef int CaError                              ;

typedef enum CaErrorCode                         {
  NoError                               =  0     ,
  NotInitialized                        = -10000 ,
  UnanticipatedHostError                = -10001 ,
  InvalidChannelCount                   = -10002 ,
  InvalidSampleRate                     = -10003 ,
  InvalidDevice                         = -10004 ,
  InvalidFlag                           = -10005 ,
  SampleFormatNotSupported              = -10006 ,
  BadIODeviceCombination                = -10007 ,
  InsufficientMemory                    = -10008 ,
  BufferTooBig                          = -10009 ,
  BufferTooSmall                        = -10010 ,
  NullCallback                          = -10011 ,
  BadStreamPtr                          = -10012 ,
  TimedOut                              = -10013 ,
  InternalError                         = -10014 ,
  DeviceUnavailable                     = -10015 ,
  IncompatibleStreamInfo                = -10016 ,
  StreamIsStopped                       = -10017 ,
  StreamIsNotStopped                    = -10018 ,
  InputOverflowed                       = -10019 ,
  OutputUnderflowed                     = -10020 ,
  HostApiNotFound                       = -10021 ,
  InvalidHostApi                        = -10022 ,
  CanNotReadFromACallbackStream         = -10023 ,
  CanNotWriteToACallbackStream          = -10024 ,
  CanNotReadFromAnOutputOnlyStream      = -10025 ,
  CanNotWriteToAnInputOnlyStream        = -10026 ,
  IncompatibleStreamHostApi             = -10027 ,
  BadBufferPtr                          = -10028 }
  CaErrorCode                                    ;

typedef enum CaHostApiTypeId {
  Development     =  0       , /* use while developing support for a new host API */
  DirectSound     =  1       ,
  MME             =  2       ,
  WDMKS           =  3       ,
  WASAPI          =  4       ,
  ASIO            =  5       ,
  XAUDIO2         =  6       ,
  OSS             =  7       ,
  ALSA            =  8       ,
  JACK            =  9       ,
  FFADO           = 10       ,
  PulseAudio      = 11       ,
  ASIHPI          = 12       ,
  CoreAudio       = 13       ,
  Android         = 14       ,
  OpenAL          = 15       ,
  FFMPEG          = 16       ,
  VLC             = 17       ,
  MEDIA           = 18       , // Media Encoder/Decoder for developer
  Skeleton        = 19       ,
  HostApiEndId    = 20       }
  CaHostApiTypeId            ;

typedef enum CaSampleFormat      {
  cafNothing        = 0x00000000 ,
  cafInt8           = 0x00000001 ,
  cafUint8          = 0x00000002 ,
  cafInt16          = 0x00000004 ,
  cafInt24          = 0x00000008 ,
  cafInt32          = 0x00000010 ,
  cafFloat32        = 0x00000020 ,
  cafFloat64        = 0x00000040 ,
  cafCustomFormat   = 0x00010000 ,
  cafNonInterleaved = 0x80000000 }
  CaSampleFormat                 ;

typedef unsigned long CaStreamFlags ;

typedef enum CaStreamAllFlags                           {
  csfNoFlag                                = 0x00000000 ,
  csfClipOff                               = 0x00000001 , // Disable default clipping of out of range samples.
  csfDitherOff                             = 0x00000002 , // Disable default dithering.
  csfNeverDropInput                        = 0x00000004 ,
  csfPrimeOutputBuffersUsingStreamCallback = 0x00000008 ,
  csfPlatformSpecificFlags                 = 0xFFFF0000 }
  CaStreamAllFlags                                      ;

typedef enum CaHostBufferSizeMode                  {
  cabFixedHostBufferSize                       = 0 ,
  cabBoundedHostBufferSize                     = 1 ,
  cabUnknownHostBufferSize                     = 2 ,
  cabVariableHostBufferSizePartialUsageAllowed = 3 }
  CaHostBufferSizeMode                             ;

#define CaNoDevice                              ((CaDeviceIndex)-1)
#define CaUseHostApiSpecificDeviceSpecification ((CaDeviceIndex)-2)

/*****************************************************************************/
/*                           Classes declaration                             */
/*****************************************************************************/

class AUDIO_EXPORT Allocator        ;
class AUDIO_EXPORT Timer            ;
class AUDIO_EXPORT CpuLoad          ;
class AUDIO_EXPORT AllocationLink   ;
class AUDIO_EXPORT AllocationGroup  ;
class AUDIO_EXPORT Debugger         ;
class AUDIO_EXPORT Dither           ;
class AUDIO_EXPORT RingBuffer       ;
class AUDIO_EXPORT LoopBuffer       ;
class AUDIO_EXPORT StreamIO         ;
class AUDIO_EXPORT Conduit          ;
class AUDIO_EXPORT LinearConduit    ;
class AUDIO_EXPORT ConduitFunction  ;
class AUDIO_EXPORT BufferProcessor  ;
class AUDIO_EXPORT Stream           ;
class AUDIO_EXPORT HostApiInfo      ;
class AUDIO_EXPORT DeviceInfo       ;
class AUDIO_EXPORT MediaCodec       ;
class AUDIO_EXPORT StreamParameters ;
class AUDIO_EXPORT HostApi          ;
class AUDIO_EXPORT Core             ;

class AUDIO_EXPORT Allocator
{
  public:

    explicit      Allocator (void) ;
    virtual      ~Allocator (void) ;

    static void * allocate  (long size) ;
    static void   free      (void * block) ;
    static int    blocks    (void) ;

};

class AUDIO_EXPORT Timer
{
  public:

    explicit      Timer      (void) ;
    virtual      ~Timer      (void) ;

    static void   Initialize (void) ;
    static CaTime Time       (void) ;
    static void   Sleep      (int msec) ;

    void          Anchor     (CaTime latency) ;
    void          Anchor     (CaTime latency,double divided) ;
    bool          isArrival  (void) ;
    CaTime        dT         (double multiply) ;

  protected:

    CaTime TimeAt     ;
    CaTime TimeAnchor ;

};

class AUDIO_EXPORT CpuLoad
{
  public:

    CaTime samplingPeriod ;

    explicit       CpuLoad    (void) ;
    virtual       ~CpuLoad    (void) ;

    virtual void   Initialize (double sampleRate) ;
    virtual void   Reset      (void) ;

    virtual void   Begin      (void) ;
    virtual void   End        (unsigned long framesProcessed) ;

    virtual CaTime Value      (void) ;

  protected:

    CaTime measurementStartTime ;
    CaTime averageLoad          ;

};

class AUDIO_EXPORT AllocationLink
{
  public:

    AllocationLink * next   ;
    void           * buffer ;

    explicit AllocationLink (void) ;
             AllocationLink (AllocationLink * next,void * buffer) ;
             AllocationLink (const AllocationLink & link) ;
    virtual ~AllocationLink (void) ;

};

class AUDIO_EXPORT AllocationGroup
{
  public:

    explicit AllocationGroup          (void) ;
    virtual ~AllocationGroup          (void) ;

    virtual void * alloc              (int size) ;
    virtual void   free               (void * memory) ;
    virtual void   release            (void) ; // Free all

  protected:

    long             linkCount   ;
    AllocationLink * linkBlocks  ;
    AllocationLink * spareLinks  ;
    AllocationLink * allocations ;

    virtual AllocationLink * allocate (int              count     = 16     ,
                                       AllocationLink * nextBlock = NULL   ,
                                       AllocationLink * nextSpare = NULL ) ;

};

class AUDIO_EXPORT Debugger
{
  public:

    explicit             Debugger  (void) ;
    virtual             ~Debugger  (void) ;

    virtual void         printf    (const char * format,...) ;
    virtual const char * Error     (CaError errorCode) ;

    virtual const char * lastError (void) ;

};

AUDIO_EXPORT extern Debugger * globalDebugger ;

class AUDIO_EXPORT Dither
{
  public:

    CaUint32 previous  ;
    CaUint32 randSeed1 ;
    CaUint32 randSeed2 ;

    explicit Dither (void) ;
    virtual ~Dither (void) ;

    CaInt32   Dither16    (void) ;
    CaFloat32 DitherFloat (void) ;

};

class AUDIO_EXPORT RingBuffer
{
  public:

    CaInt32          bufferSize       ;
    volatile CaInt32 writeIndex       ;
    volatile CaInt32 readIndex        ;
    CaInt32          bigMask          ;
    CaInt32          smallMask        ;
    CaInt32          elementSizeBytes ;
    char           * buffer           ;

    explicit RingBuffer               (void) ;
             RingBuffer               (CaInt32 elementSizeBytes ,
                                       CaInt32 elementCount     ,
                                       void  * dataPtr        ) ;
    virtual ~RingBuffer               (void) ;

    virtual CaInt32 Initialize        (CaInt32 elementSizeBytes ,
                                       CaInt32 elementCount     ,
                                       void  * dataPtr        ) ;
    virtual CaInt32 ReadAvailable     (void) ;
    virtual CaInt32 WriteAvailable    (void) ;
    virtual void    Flush             (void) ;
    virtual CaInt32 WriteRegions      (CaInt32    elementCount ,
                                       void    ** dataPtr1     ,
                                       CaInt32  * sizePtr1     ,
                                       void    ** dataPtr2     ,
                                       CaInt32  * sizePtr2   ) ;
    virtual CaInt32 AdvanceWriteIndex (CaInt32 elementCount) ;
    virtual CaInt32 ReadRegions       (CaInt32    elementCount ,
                                       void    ** dataPtr1     ,
                                       CaInt32  * sizePtr1     ,
                                       void    ** dataPtr2     ,
                                       CaInt32  * sizePtr2   ) ;
    virtual CaInt32 AdvanceReadIndex  (CaInt32 elementCount) ;

    virtual CaInt32 Write             (const void * data,CaInt32 elementCount) ;
    virtual CaInt32 Read              (void * data,CaInt32 elementCount) ;

};

class AUDIO_EXPORT LoopBuffer
{
  public:

    explicit LoopBuffer    (void) ;
             LoopBuffer    (int size,int margin) ;
    virtual ~LoopBuffer    (void) ;

    int      size          (void) const ;
    int      margin        (void) const ;
    int      start         (void) const ;
    int      tail          (void) const ;

    int      setBufferSize (int size) ;
    int      setMargin     (int margin) ;

    void     reset         (void) ;
    int      available     (void) ;
    bool     isEmpty       (void) ;
    bool     isFull        (void) ;

    int      put           (void * data,int length) ;
    int      get           (void * data,int length) ;

  protected:

    unsigned char * buffer ;
    int             Size   ;
    int             Margin ;
    int             Start  ;
    int             Tail   ;

};

typedef void   Converter        ( void       * destinationBuffer     ,
                                  signed   int destinationStride     ,
                                  void       * sourceBuffer          ,
                                  signed   int sourceStride          ,
                                  unsigned int count                 ,
                                  Dither     * ditherGenerator     ) ;
typedef void   ZeroCopier       ( void       * destinationBuffer     ,
                                  signed   int destinationStride     ,
                                  unsigned int count               ) ;
AUDIO_EXPORT CaSampleFormat ClosestFormat                            (
                                  unsigned long  availableFormats    ,
                                  CaSampleFormat format            ) ;
AUDIO_EXPORT Converter    * SelectConverter                          (
                                  CaSampleFormat sourceFormat        ,
                                  CaSampleFormat destinationFormat   ,
                                  CaStreamFlags  flags             ) ;
AUDIO_EXPORT ZeroCopier   * SelectZeroCopier ( CaSampleFormat destinationFormat ) ;
typedef int (*CacInputFunction)  ( const void  * InputBuffer           ,
                                   unsigned long FrameCount            ,
                                   unsigned long StatusFlags           ,
                                   CaTime        CurrentTime           ,
                                   CaTime        InputBufferAdcTime  ) ;
typedef int (*CacOutputFunction) ( void        * OutputBuffer          ,
                                   unsigned long FrameCount            ,
                                   unsigned long StatusFlags           ,
                                   CaTime        CurrentTime           ,
                                   CaTime        OutputBufferDacTime ) ;

class AUDIO_EXPORT StreamIO
{
  public:

    typedef enum FlowSituation     {
      Stagnated = 0                ,
      Started   = 1                ,
      Stalled   = 2                ,
      Completed = 3                ,
      Ruptured  = 4                }
    FlowSituation                  ;

    typedef enum CallbackFlags     {
      InputUnderflow  = 0x00000001 ,
      InputOverflow   = 0x00000002 ,
      OutputUnderflow = 0x00000004 ,
      OutputOverflow  = 0x00000008 ,
      PrimingOutput   = 0x00000010 }
      CallbackFlags                ;

    void        * Buffer         ;
    int           BytesPerSample ;
    unsigned long FrameCount     ;
    unsigned long MaxSize        ;
    unsigned long StatusFlags    ;
    CaTime        CurrentTime    ;
    CaTime        AdcDac         ;
    FlowSituation Situation      ;
    int           Result         ;

    explicit  StreamIO  (void) ;
    virtual  ~StreamIO  (void) ;

    void      Reset     (void) ;
    long long Total     (void) ;
    bool      isNull    (void) ;
    int       setSample (CaSampleFormat format,int channels) ;

};

class AUDIO_EXPORT Conduit
{
  public:

    typedef enum CallBackResult   {
      Continue = 0                ,
      Complete = 1                ,
      Abort    = 2                ,
      Postpone = 3                }
      CallBackResult              ;

    typedef enum ConduitDirection {
      NoDirection     = 0         ,
      InputDirection  = 1         ,
      OutputDirection = 2         }
      ConduitDirection            ;

    typedef enum FinishCondition  {
      Correct      = 0            ,
      Abortion     = 1            ,
      Interruption = 2            ,
      Accident     = 3            }
      FinishCondition             ;

    StreamIO          Input          ;
    StreamIO          Output         ;
    CacInputFunction  inputFunction  ;
    CacOutputFunction outputFunction ;

    explicit     Conduit          (void) ;
    virtual     ~Conduit          (void) ;

    virtual int  obtain           (void) = 0 ;
    virtual int  put              (void) = 0 ;
    virtual void finish           (ConduitDirection direction = NoDirection   ,
                                   FinishCondition  condition = Correct ) = 0 ;

    virtual void LockConduit      (void) ;
    virtual void UnlockConduit    (void) ;

  protected:

    virtual int  ObtainByFunction (void) ;
    virtual int  PutByFunction    (void) ;

};

class AUDIO_EXPORT LinearConduit : public Conduit
{
  public:

    explicit     LinearConduit     (void) ;
                 LinearConduit     (int size) ;
    virtual     ~LinearConduit     (void) ;

    virtual int  obtain            (void) ;
    virtual int  put               (void) ;
    virtual void finish            (ConduitDirection direction = NoDirection ,
                                    FinishCondition  condition = Correct   ) ;

    virtual int  setBufferSize     (int size) ;
    virtual int  size              (void) const ;
    virtual unsigned char * window (void) const ;

  protected:

    unsigned char * buffer ;
    int             Size   ;

    virtual int  LinearPut    (void) ;

};

class AUDIO_EXPORT ConduitFunction : public Conduit
{
  public:

    explicit    ConduitFunction (void) ;
                ConduitFunction (CacInputFunction inputFunction) ;
                ConduitFunction (CacOutputFunction outputFunction) ;
    virtual    ~ConduitFunction (void) ;

    virtual int  obtain  (void) ;
    virtual int  put     (void) ;
    virtual void finish  (ConduitDirection direction = NoDirection ,
                          FinishCondition  condition = Correct   ) ;

};

typedef struct CaChannelDescriptor {
  void       * data                ;
  unsigned int stride              ;
}              CaChannelDescriptor ;

class AUDIO_EXPORT BufferProcessor
{
  public:

    CaUint32              framesPerUserBuffer                 ;
    CaUint32              framesPerHostBuffer                 ;
    CaHostBufferSizeMode  hostBufferSizeMode                  ;
    int                   useNonAdaptingProcess               ;
    int                   userOutputSampleFormatIsEqualToHost ;
    int                   userInputSampleFormatIsEqualToHost  ;
    CaUint32              framesPerTempBuffer                 ;

    CaUint32              inputChannelCount                   ;
    CaUint32              bytesPerHostInputSample             ;
    CaUint32              bytesPerUserInputSample             ;
    int                   userInputIsInterleaved              ;
    Converter           * inputConverter                      ;
    ZeroCopier          * inputZeroer                         ;

    CaUint32              outputChannelCount                  ;
    CaUint32              bytesPerHostOutputSample            ;
    CaUint32              bytesPerUserOutputSample            ;
    int                   userOutputIsInterleaved             ;
    Converter           * outputConverter                     ;
    ZeroCopier          * outputZeroer                        ;

    CaUint32              initialFramesInTempInputBuffer      ;
    CaUint32              initialFramesInTempOutputBuffer     ;

    void                * tempInputBuffer                     ;
    void               ** tempInputBufferPtrs                 ;
    CaUint32              framesInTempInputBuffer             ;

    void                * tempOutputBuffer                    ;
    void               ** tempOutputBufferPtrs                ;
    CaUint32              framesInTempOutputBuffer            ;

    int                   hostInputIsInterleaved              ;
    CaUint32              hostInputFrameCount  [ 2 ]          ;
    CaChannelDescriptor * hostInputChannels    [ 2 ]          ;
    int                   hostOutputIsInterleaved             ;
    CaUint32              hostOutputFrameCount [ 2 ]          ;
    CaChannelDescriptor * hostOutputChannels   [ 2 ]          ;

    double                samplePeriod                        ;
    Dither                dither                              ;
    Conduit             * conduit                             ;

    explicit BufferProcessor             (void) ;
    virtual ~BufferProcessor             (void) ;

    virtual CaError Initialize                                    (
                      int                  inputChannelCount      ,
                      CaSampleFormat       userInputSampleFormat  ,
                      CaSampleFormat       hostInputSampleFormat  ,
                      int                  outputChannelCount     ,
                      CaSampleFormat       userOutputSampleFormat ,
                      CaSampleFormat       hostOutputSampleFormat ,
                      double               sampleRate             ,
                      CaStreamFlags        streamFlags            ,
                      CaUint32             framesPerUserBuffer    ,
                      CaUint32             framesPerHostBuffer    ,
                      CaHostBufferSizeMode hostBufferSizeMode     ,
                      Conduit            * conduit              ) ;
    virtual void     Terminate           (void) ;
    virtual void     Reset               (void) ;

    virtual void     Begin               (Conduit * conduit) ;
    virtual CaUint32 End                 (int * callbackResult = NULL) ;

    virtual CaUint32 InputLatencyFrames  (void) ;
    virtual CaUint32 OutputLatencyFrames (void) ;
    virtual bool     isOutputEmpty       (void) ;

    virtual void     setNoInput          (void) ;
    virtual void     setInputFrameCount  (int      index          ,
                                          CaUint32 frameCount   ) ;
    virtual void     setInputChannel     (int      index          ,
                                          CaUint32 channel        ,
                                          void   * data           ,
                                          CaUint32 stride       ) ;
    virtual void     setInterleavedInputChannels                  (
                                          int      index          ,
                                          CaUint32 firstChannel   ,
                                          void   * data           ,
                                          CaUint32 channelCount ) ;
    virtual void     setNonInterleavedInputChannels               (
                                          int      index          ,
                                          CaUint32 channel        ,
                                          void   * data         ) ;
    virtual void     setNoOutput         (void) ;
    virtual void     setOutputFrameCount (int      index          ,
                                          CaUint32 frameCount   ) ;
    virtual void     setOutputChannel    (int      index          ,
                                          CaUint32 channel        ,
                                          void   * data           ,
                                          CaUint32 stride       ) ;
    virtual void     setInterleavedOutputChannels                 (
                                          int      index          ,
                                          CaUint32 firstChannel   ,
                                          void   * data           ,
                                          CaUint32 channelCount ) ;
    virtual void     setNonInterleavedOutputChannel               (
                                          int      index          ,
                                          CaUint32 channel        ,
                                          void   * data         ) ;

    virtual CaUint32 CopyInput           (void       ** buffer       ,
                                          CaUint32      frameCount ) ;
    virtual CaUint32 CopyOutput          (const void ** buffer       ,
                                          CaUint32      frameCount ) ;
    virtual CaUint32 ZeroOutput          (CaUint32      frameCount ) ;

  protected:

    inline void Delete (void * memory)
    {
      if ( NULL != memory ) Allocator :: free ( memory ) ;
    }

    /* greatest common divisor - PGCD in French */
    inline CaUint32 GCD( CaUint32 a, CaUint32 b )
    {
      return ( 0 == b ) ? a : GCD ( b , ( a % b ) ) ;
    }

    /* least common multiple - PPCM in French */
    inline CaUint32 LCM( CaUint32 a , CaUint32 b )
    {
      return ( a * b ) / GCD ( a , b ) ;
    }

    CaUint32 FrameShift ( CaUint32 M , CaUint32 N ) ;

  private:

    void             TempToHost          (void) ;
    CaUint32         NonAdapting         (int                 * streamCallbackResult ,
                                          CaChannelDescriptor * hic                  ,
                                          CaChannelDescriptor * hoc                  ,
                                          CaUint32              framesToProcess    ) ;
    CaUint32         AdaptingInputOnly   (int                 * streamCallbackResult ,
                                          CaChannelDescriptor * hostInputChannels    ,
                                          CaUint32              framesToProcess    ) ;
    CaUint32         AdaptingOutputOnly  (int                 * streamCallbackResult ,
                                          CaChannelDescriptor * hostOutputChannels   ,
                                          CaUint32              framesToProcess    ) ;
    CaUint32         Adapting            (int * streamCallbackResult        ,
                                          int   processPartialUserBuffers ) ;

};

class AUDIO_EXPORT Stream
{
  public:

    enum                                        {
      FramesPerBufferUnspecified = 0            ,
      STREAM_MAGIC               = 0x18273645 } ;

    CaUint32   magic         ;
    int        structVersion ;
    CaTime     inputLatency  ;
    CaTime     outputLatency ;
    double     sampleRate    ;
    Stream   * next          ;
    Conduit  * conduit       ;
    Debugger * debugger      ;

    explicit         Stream         (void) ;
    virtual         ~Stream         (void) ;

    void             Terminate      (void) ;
    bool             isValid        (void) const ;
    virtual bool     isRealTime     (void) ;
    virtual CaError  Error          (void) ;
    virtual char *   lastError      (void) ;

    virtual CaError  Start          (void) = 0 ;
    virtual CaError  Stop           (void) = 0 ;
    virtual CaError  Close          (void) = 0 ;
    virtual CaError  Abort          (void) = 0 ;
    virtual CaError  IsStopped      (void) = 0 ;
    virtual CaError  IsActive       (void) = 0 ;
    virtual CaTime   GetTime        (void) = 0 ;
    virtual double   GetCpuLoad     (void) = 0 ;
    virtual CaInt32  ReadAvailable  (void) = 0 ;
    virtual CaInt32  WriteAvailable (void) = 0 ;
    virtual CaError  Read           (void       * buffer,unsigned long frames) = 0 ;
    virtual CaError  Write          (const void * buffer,unsigned long frames) = 0 ;
    virtual bool     hasVolume       (void) = 0 ;
    virtual CaVolume MinVolume      (void) = 0 ;
    virtual CaVolume MaxVolume      (void) = 0 ;
    virtual CaVolume Volume         (int atChannel = -1) = 0 ;
    virtual CaVolume setVolume      (CaVolume volume,int atChannel = -1) = 0 ;

};

class AUDIO_EXPORT HostApiInfo
{
  public:

    int             structVersion       ;
    CaHostApiTypeId type                ;
    CaHostApiIndex  index               ;
    const char    * name                ;
    int             deviceCount         ;
    CaDeviceIndex   defaultInputDevice  ;
    CaDeviceIndex   defaultOutputDevice ;

    explicit HostApiInfo (void) ;
    virtual ~HostApiInfo (void) ;

};

class AUDIO_EXPORT DeviceInfo
{
  public:

    int             structVersion            ;
    const char    * name                     ;
    CaHostApiIndex  hostApi                  ;
    CaHostApiTypeId hostType                 ;
    int             maxInputChannels         ;
    int             maxOutputChannels        ;
    CaTime          defaultLowInputLatency   ;
    CaTime          defaultLowOutputLatency  ;
    CaTime          defaultHighInputLatency  ;
    CaTime          defaultHighOutputLatency ;
    double          defaultSampleRate        ;

    explicit DeviceInfo (void) ;
    virtual ~DeviceInfo (void) ;

} ;

typedef struct    StreamInfoHeader {
  unsigned long   size             ;
  CaHostApiTypeId hostApiType      ;
  unsigned long   version          ;
}                 StreamInfoHeader ;

class AUDIO_EXPORT MediaCodec
{
  public:

    explicit MediaCodec                    (void) ;
    virtual ~MediaCodec                    (void) ;

    virtual char *         Filename        (void) ;
    virtual char *         setFilename     (const char * filename) ;
    virtual int            Interval        (void) ;
    virtual int            setInterval     (int interval) ;
    virtual int            BufferTimeMs    (void) ;
    virtual int            PeriodSize      (void) ;
    virtual int            setChannels     (int channels) ;
    virtual int            Channels        (void) ;
    virtual int            setSampleRate   (int samplerate) ;
    virtual int            SampleRate      (void) ;
    virtual CaSampleFormat setSampleFormat (CaSampleFormat format) ;
    virtual CaSampleFormat SampleFormat    (void) ;
    virtual int            BytesPerSample  (void) ;
    virtual bool           Wait            (void) ;
    virtual bool           setWaiting      (bool wait) ;
    virtual bool           hasPreparation  (void) ;
    virtual bool           Prepare         (void * mediaPacket) ;
    virtual long long      Length          (void) ;
    virtual long long      setLength       (long long length) ;

  protected:

    char         * MediaFilename ;
    int            channels      ;
    int            samplerate    ;
    CaSampleFormat format        ;
    int            intervalTime  ;
    bool           waiting       ;
    long long      AudioLength   ;

};

class AUDIO_EXPORT StreamParameters
{
  public:

    CaDeviceIndex  device           ;
    int            channelCount     ;
    CaSampleFormat sampleFormat     ;
    CaTime         suggestedLatency ;
    void         * streamInfo       ;

    explicit StreamParameters (void) ;
             StreamParameters (int deviceId) ;
             StreamParameters (int            deviceId       ,
                               int            channelCount   ,
                               CaSampleFormat sampleFormat ) ;
    virtual ~StreamParameters (void) ;

    MediaCodec * CreateCodec  (void) ;
    void         setCodec     (CaHostApiTypeId host,MediaCodec * codec) ;

} ;

class AUDIO_EXPORT HostApi
{
  public:

    typedef enum {
      NATIVE = 0 ,
      UTF8   = 1 ,
      UTF16  = 2 }
      Encoding   ;

    unsigned long   baseDeviceIndex ;
    HostApiInfo     info            ;
    DeviceInfo   ** deviceInfos     ;

    explicit         HostApi     (void) ;
    virtual         ~HostApi     (void) ;

    virtual CaError  Open        (Stream                ** stream            ,
                                  const StreamParameters * inputParameters   ,
                                  const StreamParameters * outputParameters  ,
                                  double                   sampleRate        ,
                                  CaUint32                 framesPerCallback ,
                                  CaStreamFlags            streamFlags       ,
                                  Conduit                * streamCallback    ) = 0 ;
    virtual void     Terminate   (void) = 0 ;
    virtual CaError  isSupported (const  StreamParameters * inputParameters  ,
                                  const  StreamParameters * outputParameters ,
                                  double sampleRate                          ) = 0 ;
    virtual Encoding encoding    (void) const = 0 ;
    virtual bool     hasDuplex   (void) const = 0 ;

    void             setDebugger (Debugger * debug) ;

    virtual CaError  ToHostDeviceIndex               (
                       CaDeviceIndex * hostApiDevice ,
                       CaDeviceIndex   device      ) ;

  protected:

    Debugger * debugger ;

};

typedef CaError HostApiInitializer(HostApi **,CaHostApiIndex) ;

AUDIO_EXPORT extern HostApiInitializer ** caHostApiInitializers ;

AUDIO_EXPORT extern void ReplaceHostApiInitializer ( HostApiInitializer ** replacement ) ;

/*****************************************************************************/
/*                              CIOS Audio Core                              */
/*****************************************************************************/

class AUDIO_EXPORT Core
{
  public:

    explicit               Core                  (void) ;
    virtual               ~Core                  (void) ;

    static Core *          instance              (void) ;
    static bool            hasEncoder            (void) ;
    static bool            hasDecoder            (void) ;
    static bool            Play                  (const char * filename,bool threading = true) ;
    static bool            Play                  (const char * filename,int outputDevice,bool threading = true) ;

    int                    Version               (void) const ;
    const char *           VersionString         (void) ;

    virtual CaError        Initialize            (void) ;
    virtual CaError        Terminate             (void) ;

    static  int            SampleSize            (int caSampleFormat) ;
    static  bool           isValid               (int caSampleFormat) ;

    static  bool           IsLittleEndian        (void) ;
    static  bool           IsBigEndian           (void) ;

    void                   setDebugger           (Debugger * debug) ;

    virtual CaDeviceIndex  DefaultInputDevice    (void) ;
    virtual CaDeviceIndex  DefaultOutputDevice   (void) ;
    virtual CaDeviceIndex  DefaultDecoderDevice  (void) ;
    virtual CaDeviceIndex  DefaultEncoderDevice  (void) ;

    virtual int            HostAPIs              (void) ;
    virtual CaHostApiIndex HostApiCount          (void) ;
    virtual CaDeviceIndex  DeviceCount           (void) ;
    virtual CaHostApiIndex DefaultHostApi        (void) ;
    virtual CaHostApiIndex setDefaultHostApi     (CaHostApiIndex index) ;
    virtual int            FindHostApi           (CaDeviceIndex   device                    ,
                                                  int           * hostSpecificDeviceIndex ) ;
    static  CaHostApiIndex TypeIdToIndex         (CaHostApiTypeId type                    ) ;
    static  CaError        GetHostApi            (HostApi      ** hostApi                   ,
                                                  CaHostApiTypeId type                    ) ;
    static  CaDeviceIndex  ToDeviceIndex         (CaHostApiIndex  hostApi                   ,
                                                  int             hostApiDeviceIndex      ) ;
    virtual HostApiInfo *  GetHostApiInfo        (CaHostApiIndex  hostApi                 ) ;
    virtual DeviceInfo  *  GetDeviceInfo         (CaDeviceIndex   device                  ) ;
    virtual CaError        setConduit            (Stream        * stream                    ,
                                                  Conduit       * conduit                 ) ;

    virtual CaError        Start                 (Stream        * stream                  ) ;
    virtual CaError        Stop                  (Stream        * stream                  ) ;
    virtual CaError        Close                 (Stream        * stream                  ) ;
    virtual CaError        Abort                 (Stream        * stream                  ) ;
    virtual CaError        IsStopped             (Stream        * stream                  ) ;
    virtual CaError        IsActive              (Stream        * stream                  ) ;
    virtual CaTime         GetTime               (Stream        * stream                  ) ;
    virtual double         GetCpuLoad            (Stream        * stream                  ) ;
    virtual CaInt32        ReadAvailable         (Stream        * stream                  ) ;
    virtual CaInt32        WriteAvailable        (Stream        * stream                  ) ;
    virtual CaError        Read                  (Stream        * stream                    ,
                                                  void          * buffer                    ,
                                                  unsigned long   frames                  ) ;
    virtual CaError        Write                 (Stream        * stream                    ,
                                                  const void    * buffer                    ,
                                                  unsigned long   frames                  ) ;
    virtual bool           hasVolume             (Stream        * stream) ;
    virtual CaVolume       MinVolume             (Stream        * stream) ;
    virtual CaVolume       MaxVolume             (Stream        * stream) ;
    virtual CaVolume       Volume                (Stream        * stream           ,
                                                  int             atChannel = -1 ) ;
    virtual CaVolume       setVolume             (Stream        * stream           ,
                                                  CaVolume        volume           ,
                                                  int             atChannel = -1 ) ;

    virtual CaError        Open                  (Stream                ** stream             ,
                                                  const StreamParameters * inputParameters    ,
                                                  const StreamParameters * outputParameters   ,
                                                  double                   sampleRate         ,
                                                  CaUint32                 framesPerBuffer    ,
                                                  CaStreamFlags            streamFlags        ,
                                                  Conduit                * conduit          ) ;
    virtual CaError        Default               (Stream                ** stream             ,
                                                  int                      inputChannelCount  ,
                                                  int                      outputChannelCount ,
                                                  CaSampleFormat           sampleFormat       ,
                                                  double                   sampleRate         ,
                                                  CaUint32                 framesPerBuffer    ,
                                                  Conduit                * conduit          ) ;
    virtual CaError        isSupported           (const StreamParameters * inputParameters    ,
                                                  const StreamParameters * outputParameters   ,
                                                  double                   sampleRate       ) ;

    virtual bool           Play                  (Conduit      * conduit              ,
                                                  CaDeviceIndex  deviceId             ,
                                                  int            channelCount         ,
                                                  CaSampleFormat sampleFormat         ,
                                                  int            sampleRate           ,
                                                  int            frameCount           ,
                                                  CaStreamFlags  streamFlags   =  0   ,
                                                  int            sleepInterval = 50 ) ;
    virtual bool           Record                (Conduit      * conduit              ,
                                                  CaDeviceIndex  deviceId             ,
                                                  int            channelCount         ,
                                                  CaSampleFormat sampleFormat         ,
                                                  int            sampleRate           ,
                                                  int            frameCount           ,
                                                  CaStreamFlags  streamFlags   =  0   ,
                                                  int            sleepInterval = 50 ) ;

  protected:

    int                defaultHostApiIndex ;
    Stream          *  firstOpenStream     ;

    virtual CaError        InitializeHostApis    (void) ;
    virtual void           TerminateHostApis     (void) ;

    static  bool           isInitialized         (void) ;

    virtual void           Add                   (Stream * stream) ;
    virtual void           Remove                (Stream * stream) ;
    virtual void           CloseStreams          (void) ;

    virtual CaError        ValidateStreamPointer (Stream * stream) ;
    virtual CaError        ValidateStream        (const StreamParameters * inputParameters       ,
                                                  const StreamParameters * outputParameters      ,
                                                  double                   sampleRate            ,
                                                  CaUint32                 framesPerBuffer       ,
                                                  CaStreamFlags            streamFlags           ,
                                                  Conduit                * conduit               ,
                                                  HostApi               ** hostApi               ,
                                                  CaDeviceIndex          * hostApiInputDevice    ,
                                                  CaDeviceIndex          * hostApiOutputDevice ) ;

  private:

    Timer LocalTimer ;

};

AUDIO_EXPORT void SetLastHostErrorInfo          (
                    CaHostApiTypeId hostApiType ,
                    long            errorCode   ,
                    const char    * errorText ) ;

#if defined(ENABLE_HOST_OPENAL)

typedef struct OpenALStreamInfo {
  unsigned long   size          ;
  CaHostApiTypeId hostApiType   ;
  unsigned long   version       ;
  bool            isCapture     ;
  bool            isPlayback    ;
  bool            useExternal   ;
  int             format        ; // ALenum AL_FORMAT_...
  char *          name          ;
  void *          device        ; // ALCdevice  * device
  void *          context       ; // ALCcontext * context
  int             source        ; // audio source id
  int             buffer        ; // audio buffer id
}              OpenALStreamInfo ;

#endif

#if defined(FFMPEGLIB)

AUDIO_EXPORT extern AVCodecID AllFFmpegCodecIDs [] ;

typedef struct FFmpegEncodePacket  {
  unsigned long        size        ;
  CaHostApiTypeId      hostApiType ;
  unsigned long        version     ;
  AVCodecContext     * AudioCtx    ;
  AVCodec            * AudioCodec  ;
  AVStream           * AudioStream ;
  struct SwsContext  * ConvertCtx  ;
  SwrContext         * Resample    ;
}              FFmpegEncodePacket  ;

class AUDIO_EXPORT CaResampler
{
  public:

    SwrContext    * Resample         ;
    unsigned char * inputBuffer      ;
    unsigned char * outputBuffer     ;
    double          InputSampleRate  ;
    double          OutputSampleRate ;
    bool            autoDeletion     ;

    explicit CaResampler (void) ;
    virtual ~CaResampler (void) ;

    bool     Setup       (double         inputSampleRate  ,
                          int            inputChannels    ,
                          CaSampleFormat inputFormat      ,
                          double         outputSampleRate ,
                          int            outputChannels   ,
                          CaSampleFormat outputFormat   ) ;
    int      Convert     (int frames) ;
    int      ToFrames    (int frames) ;

    static int Format    (CaSampleFormat format) ;
    static int Channel   (int channels) ;

} ;

#endif

#if defined(CAUTILITIES)

class AUDIO_EXPORT Mutex         ;
class AUDIO_EXPORT ThreadData    ;
class AUDIO_EXPORT Thread        ;
class AUDIO_EXPORT BridgeConduit ;

#if defined(WIN32) || defined(_WIN32)
typedef unsigned int (*ThreadFunction)(void * arg) ;
#else
typedef void *       (*ThreadFunction)(void * arg) ;
#endif

class AUDIO_EXPORT Mutex
{
  public:

    enum MutexType    {
      MutexDefault    ,
      MutexNormal     ,
      MutexErrorCheck ,
      MutexRecursive
    }                 ;

    explicit  Mutex        (void);
    virtual ~ Mutex        (void);

    void      setMutexType (MutexType MT);

    int       lock         (void);
    int       unlock       (void);
    int       locked       (void);
    int       tryLock      (void);

  protected:

    void      releaseMutex (void);

  private:

    #if defined(WIN32) || defined(_WIN32)
    CRITICAL_SECTION mutex  ;
    int              Locked ;
    #else
    pthread_mutex_t  mutex;
    #endif

};

class AUDIO_EXPORT ThreadData
{
  public:

    enum ThreadState {
      Idle     = 0   ,
      Active   = 1   ,
      Deactive = 2   ,
      Recycle  = 3 } ;

    int     Id          ;
    int     Type        ;
    int     Priority    ;
    int     Status      ;
    int     Running     ;
    int     StackSize   ;
    bool    Reservation ; // STACK_SIZE_PARAM_IS_A_RESERVATION ;
    char *  Arguments   ;
    void *  Extra       ;

    #if defined(WIN32) || defined(_WIN32)
    HANDLE         Thread              ;
    unsigned int   dwThreadID          ;
    #else
    pthread_t      Thread              ;
    #endif
    ThreadFunction Function            ;

    explicit  ThreadData  (void) ;
    virtual ~ ThreadData  (void) ;

    void      Start       (void) ;
    void      Stop        (void) ;
    void      Join        (void) ;

    int       setPriority (int priority) ;

    #if defined(WIN32) || defined(_WIN32)
    HANDLE    ThreadId    (void) ;
    #else
    pthread_t ThreadId    (void) ;
    #endif

    bool      isSelf      (void) ;

};

class AUDIO_EXPORT Thread
{
  public:

    int                     running     ;
    int                     StackSize   ;
    bool                    Reservation ; // STACK_SIZE_PARAM_IS_A_RESERVATION ;
    ThreadData              Data        ;
    std::list<ThreadData *> AllThreads  ;

    explicit  Thread                  (int stackSize,bool reservation) ;
    explicit  Thread                  (void) ;
    virtual ~ Thread                  (void) ;

    int                   setPriority (int priority) ;

    virtual void          start       (void) ;
    virtual ThreadData  * start       (int Type) ;
    virtual ThreadData  * start       (int Type,char * arguments) ;
    virtual ThreadData  * start       (int Type,ThreadFunction ExternalThread) ;
    virtual void          quit        (void) ;
    virtual void          suspend     (void) ;
    virtual void          resume      (void) ;
    virtual void          terminate   (void) ;
    virtual void          cleanup     (void) ;
    virtual bool          proceed     (void) ;
    virtual bool          finalize    (int interval = 20) ;

    virtual int           exit        (int exitcode) ;

    void                  actualRun   (void) ;
    void                  actualRun   (int Type,ThreadData * data) ;
    void                  actualJoin  (void) ;

    #if defined(WIN32) || defined(_WIN32)
    HANDLE                Id          (void) ;
    #else
    pthread_t             Id          (void) ;
    #endif

  protected:

    virtual void         ThreadEvent  (void) ;
    virtual void         run          (void) ;
    virtual void         run          (int Type,ThreadData * Data) ;

    virtual bool         detection    (void) ;
    virtual bool         recycle      (void) ;

    virtual void         DeleteThread (ThreadData * Data) ;

  private:

    #if defined(WIN32) || defined(_WIN32)
    static unsigned int  SingleThread (LPVOID arg) ;
    static unsigned int  MultiThread  (LPVOID arg) ;
    #else
    static void *        SingleThread (void * arg) ;
    static void *        MultiThread  (void * arg) ;
    #endif

};

class AUDIO_EXPORT BridgeConduit : public Conduit
{
  public:

    LoopBuffer Buffer ;

    explicit     BridgeConduit (void) ;
    virtual     ~BridgeConduit (void) ;

    virtual void setBufferSize (int size,int margin) ;

    virtual int  obtain        (void) ;
    virtual int  put           (void) ;
    virtual void finish        (ConduitDirection direction = NoDirection ,
                                FinishCondition  condition = Correct   ) ;

    virtual void LockConduit   (void) ;
    virtual void UnlockConduit (void) ;

  protected:

    Mutex mutex ;

    int          BridgeObtain  (void) ;
    int          BridgePut     (void) ;

};

AUDIO_EXPORT bool Redirect                         (
                  int        milliseconds = -1     ,
                  int        samplerate   = 44100  ,
                  int        channels     = 1      ,
                  int        inputDevice  = -1     ,
                  int        outputDevice = -1     ,
                  int        latency      = 10     ,
                  Debugger * debugger     = NULL ) ;

#if defined(FFMPEGLIB)

AUDIO_EXPORT bool MediaPlay                        (
                  char     * filename              ,
                  int        deviceId     = -1     ,
                  int        decodeId     = -1     ,
                  Debugger * debugger     = NULL ) ;
AUDIO_EXPORT bool MediaRecord                      (
                  char     * filename              ,
                  int        milliseconds = 60000  ,
                  int        samplerate   = 44100  ,
                  int        channels     = 1      ,
                  int        inputDevice  = -1     ,
                  int        encodeDevice = -1     ,
                  int        latency      = 500    ,
                  Debugger * debugger     = NULL ) ;

#endif

#endif

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

#endif // CIOSAUDIO_HPP
