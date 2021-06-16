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

#ifndef CAFFMPEG_HPP
#define CAFFMPEG_HPP

#include "CiosAudioPrivate.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/samplefmt.h>
#include <libavutil/imgutils.h>
#include <libavutil/cpu.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#if defined(WIN32) || defined(_WIN32)
#include <libpostproc/postprocess.h>
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

class FFmpegStream : public Stream
{
  public:

    CpuLoad              cpuLoadMeasurer ;
    MediaCodec         * Codec           ;
    bool                 isDecode        ;
    bool                 isEncode        ;
    //////////////////////////////////////
    #if defined(WIN32) || defined(_WIN32)
    unsigned             ThreadId        ;
    #else
    pthread_t            ThreadId        ;
    #endif
    //////////////////////////////////////
    volatile int         isStopped       ;
    volatile int         isActive        ;
    volatile int         stopProcessing  ;
    volatile int         abortProcessing ;
    //////////////////////////////////////
    AVFormatContext    * FormatCtx       ;
    AVDictionary       * Dictionary      ;
    AVCodecContext     * AudioCtx        ;
    AVCodec            * AudioCodec      ;
    AVFrame            * AudioFrame      ;
    AVStream           * AudioStream     ;
    struct SwsContext  * ConvertCtx      ;
    int                  AudioIndex      ;
    int                  Error           ;
    bool                 isAudio         ;
    SwrContext         * Resample        ;
    unsigned char      * audioConvert    ;
    unsigned char      * inputConvert    ;
    unsigned char      * converted       ;
    //////////////////////////////////////
    unsigned char      * AudioRing       ;
    unsigned char      * AudioFeed       ;
    long long            RingStart       ;
    long long            RingEnd         ;
    long long            RingSize        ;
    long long            RingUnit        ;
    long long            RingInterval    ;
    long long            RingFrames      ;
    CaTime               RingTime        ;
    long long            OutputSize      ;
    //////////////////////////////////////
    int                  inputBPF        ;
    int                  resampleMUL     ;
    int                  resampleDIV     ;

    explicit         FFmpegStream   (void) ;
    virtual         ~FFmpegStream   (void) ;

    virtual bool     isRealTime     (void) ;

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

    virtual bool     isValid        (StreamParameters * inputParameters ) ;
    virtual bool     canEncode      (StreamParameters * outputParameters) ;

    virtual bool     OpenFFMPEG     (void) ;
    virtual bool     CloseFFMPEG    (void) ;
    virtual bool     FindStream     (void) ;
    virtual bool     FindAudio      (void) ;
    virtual bool     OpenAudio      (void) ;

    virtual CaError  Processor      (void) ;
    virtual CaError  Decoding       (void) ;
    virtual CaError  Encoding       (void) ;

  protected:

    int              DecodeAudio    (AVPacket & Packet,unsigned char * data) ;
    int              ReadPacket     (AVPacket & Packet) ;
    bool             isAudioChannel (AVPacket & Packet) ;
    long long        AudioBufferSize(int milliseconds) ;
    CaSampleFormat   AudioFormat    (void) ;
    int              BytesPerFrame  (void) ;
    bool             isBufferFull   (void) ;
    int              AddBuffer      (unsigned char * data,int length) ;
    void             Feeding        (void) ;

    int              Encode         (FILE          * fp              ,
                                     int             length          ,
                                     unsigned char * data            ,
                                     bool            flush           ,
                                     int             BytesPerFrame ) ;

    void             StopThread     (void) ;

    AVSampleFormat   toSampleFormat (CaSampleFormat format) ;
    bool             TryOpen        (AVCodecContext * ctx,AVCodec * codec) ;
    bool             RealOpen       (AVCodecContext * ctx,AVCodec * codec) ;
    int              Convert        (int frames,int toFrames) ;

  private:

};

class FFmpegHostApi : public HostApi
{
  public:

    explicit  FFmpegHostApi      (void) ;
    virtual  ~FFmpegHostApi      (void) ;

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

    virtual CaError  Correct     (const  StreamParameters * parameters) ;

  protected:

  private:

};

///////////////////////////////////////////////////////////////////////////////

CaError FFmpegInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAWMME_HPP
