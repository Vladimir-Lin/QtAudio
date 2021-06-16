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

#include "CaFFmpeg.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

///////////////////////////////////////////////////////////////////////////////

#if defined(WIN32) || defined(_WIN32)

static unsigned FFmpegProc(void * arg)
{ // this code is stable
  CaRETURN ( ( CaIsNull ( arg ) ) , NoError )   ;
  FFmpegStream * stream = (FFmpegStream *)arg   ;
  CaError        result = stream -> Processor() ;
  return result                                 ;
}

#else

static void * FFmpegProc(void * arg)
{ // this code is stable
  CaRETURN ( ( CaIsNull ( arg ) ) , NULL )      ;
  FFmpegStream * stream = (FFmpegStream *)arg   ;
  CaError        result = stream -> Processor() ;
  return NULL                                   ;
}

#endif

///////////////////////////////////////////////////////////////////////////////

typedef struct MediaCodecSpecificStreamInfoHeader {
  unsigned long   size                            ;
  CaHostApiTypeId hostApiType                     ;
  unsigned long   version                         ;
  MediaCodec    * codec                           ;
} MediaCodecSpecificStreamInfoHeader              ;

///////////////////////////////////////////////////////////////////////////////

CaError FFmpegInitialize(HostApi ** hostApi,CaHostApiIndex hostApiIndex)
{ // this code is stable
  FFmpegHostApi * ffmpegHostApi = NULL                                       ;
  ////////////////////////////////////////////////////////////////////////////
  ffmpegHostApi = new FFmpegHostApi ( )                                      ;
  CaRETURN ( CaIsNull ( ffmpegHostApi ) , InsufficientMemory )               ;
  gPrintf ( ( "Initialize FFmpeg Host API\n" ) )                             ;
  gPrintf ( ( "\tavcodec    %s\n" , LIBAVCODEC_IDENT    ) )                  ;
  gPrintf ( ( "\tavformat   %s\n" , LIBAVFORMAT_IDENT   ) )                  ;
  gPrintf ( ( "\tavutil     %s\n" , LIBAVUTIL_IDENT     ) )                  ;
  gPrintf ( ( "\tswresample %s\n" , LIBSWRESAMPLE_IDENT ) )                  ;
  gPrintf ( ( "\tswscale    %s\n" , LIBSWSCALE_IDENT    ) )                  ;
  ////////////////////////////////////////////////////////////////////////////
  CaError result = NoError                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  *hostApi                                 = (HostApi *)ffmpegHostApi        ;
  (*hostApi) -> info . structVersion       = 1                               ;
  (*hostApi) -> info . type                = FFMPEG                          ;
  (*hostApi) -> info . index               = hostApiIndex                    ;
  (*hostApi) -> info . name                = "FFmpeg Host API"               ;
  (*hostApi) -> info . deviceCount         = 0                               ;
  (*hostApi) -> info . defaultInputDevice  = CaNoDevice                      ;
  (*hostApi) -> info . defaultOutputDevice = CaNoDevice                      ;
  gPrintf ( ( "Assign FFmpeg Host API\n" ) )                                 ;
  ////////////////////////////////////////////////////////////////////////////
  DeviceInfo * oudev = new DeviceInfo ( )                                    ;
  DeviceInfo * indev = new DeviceInfo ( )                                    ;
  ffmpegHostApi -> info . deviceCount += 2                                   ;
  ffmpegHostApi -> deviceInfos         = new DeviceInfo * [ 2 ]              ;
  ffmpegHostApi -> deviceInfos[0]      = oudev                               ;
  ffmpegHostApi -> deviceInfos[1]      = indev                               ;
  ////////////////////////////////////////////////////////////////////////////
  indev -> structVersion            = 1                                      ;
  indev -> name                     = "FFmpeg decoder"                       ;
  indev -> hostApi                  = hostApiIndex                           ;
  indev -> hostType                 = FFMPEG                                 ;
  indev -> maxInputChannels         = 2                                      ;
  indev -> maxOutputChannels        = 0                                      ;
  indev -> defaultLowInputLatency   = 0                                      ;
  indev -> defaultLowOutputLatency  = 0                                      ;
  indev -> defaultHighInputLatency  = 0                                      ;
  indev -> defaultHighOutputLatency = 0                                      ;
  indev -> defaultSampleRate        = 44100                                  ;
  ////////////////////////////////////////////////////////////////////////////
  oudev -> structVersion            = 1                                      ;
  oudev -> name                     = "FFmpeg encoder"                       ;
  oudev -> hostApi                  = hostApiIndex                           ;
  oudev -> hostType                 = FFMPEG                                 ;
  oudev -> maxInputChannels         = 0                                      ;
  oudev -> maxOutputChannels        = 2                                      ;
  oudev -> defaultLowInputLatency   = 0                                      ;
  oudev -> defaultLowOutputLatency  = 0                                      ;
  oudev -> defaultHighInputLatency  = 0                                      ;
  oudev -> defaultHighOutputLatency = 0                                      ;
  oudev -> defaultSampleRate        = 44100                                  ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

///////////////////////////////////////////////////////////////////////////////

FFmpegStream:: FFmpegStream (void)
             : Stream       (    )
{
  Codec           = NULL  ;
  isDecode        = false ;
  isEncode        = false ;
  ThreadId        = 0     ;
  isStopped       = 1     ;
  isActive        = 0     ;
  stopProcessing  = 0     ;
  abortProcessing = 0     ;
  /////////////////////////
  FormatCtx       =  NULL ;
  Dictionary      =  NULL ;
  AudioCtx        =  NULL ;
  AudioCodec      =  NULL ;
  AudioFrame      =  NULL ;
  AudioStream     =  NULL ;
  ConvertCtx      =  NULL ;
  AudioIndex      =    -1 ;
  Error           =     0 ;
  isAudio         = false ;
  Resample        =  NULL ;
  audioConvert    =  NULL ;
  inputConvert    =  NULL ;
  converted       =  NULL ;
  AudioRing       =  NULL ;
  AudioFeed       =  NULL ;
  RingStart       = 0     ;
  RingEnd         = 0     ;
  RingSize        = 0     ;
  RingUnit        = 0     ;
  RingInterval    = 0     ;
  RingFrames      = 0     ;
  OutputSize      = 0     ;
  inputBPF        = 0     ;
  resampleMUL     = 0     ;
  resampleDIV     = 0     ;
}

FFmpegStream::~FFmpegStream(void)
{ // this code is stable
}

CaError FFmpegStream::Start(void)
{ // this code is stable
  CaRETURN ( CaIsNull ( conduit                      ) , NullCallback )      ;
  CaRETURN ( CaNOT    ( CaOR ( isDecode , isEncode ) ) , InvalidFlag  )      ;
  CaError result = NoError                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  isStopped       = 0                                                        ;
  isActive        = 1                                                        ;
  stopProcessing  = 0                                                        ;
  abortProcessing = 0                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  bool threadStarted = false                                                 ;
  for ( int i = 0 ; CaAND ( !threadStarted , CaIsLess ( i , 10 ) ) ; i++ )   {
    #if defined(WIN32) || defined(_WIN32)
    DWORD        dwCreationFlags = 0                                         ;
    unsigned int dwThreadID                                                  ;
    ThreadId = (unsigned) ::_beginthreadex                                   (
      NULL                                                                   ,
      0                                                                      ,
      (unsigned int(__stdcall *)(void*))FFmpegProc                           ,
      (LPVOID)this                                                           ,
      dwCreationFlags                                                        ,
      &dwThreadID                                                          ) ;
    if ( CaNotEqual ( ThreadId   , 0 ) ) threadStarted = true                ;
    #else
    int th_retcode                                                           ;
    th_retcode = ::pthread_create                                            (
                   &ThreadId                                                 ,
                   NULL                                                      ,
                   FFmpegProc                                                ,
                   (void *)this                                            ) ;
    if ( CaIsEqual  ( th_retcode , 0 ) ) threadStarted = true                ;
    #endif
    if ( CaNOT ( threadStarted ) )                                           {
      dPrintf ( ( "Create thread failed\n" ) )                               ;
      Timer :: Sleep(100)                                                    ;
    }                                                                        ;
  }                                                                          ;
  if ( CaIsEqual ( ThreadId , 0 ) ) result = UnanticipatedHostError          ;
  return result                                                              ;
}

CaError FFmpegStream::Stop(void)
{ // this code is stable
  stopProcessing = 1                               ;
  while ( CaAND ( CaIsEqual  ( isActive  , 1 )     ,
                  CaNotEqual ( isStopped , 1 ) ) ) {
    Timer::Sleep(10)                               ;
  }                                                ;
  return NoError                                   ;
}

CaError FFmpegStream::Close(void)
{ // this code is stable
  return Stop ( ) ;
}

CaError FFmpegStream::Abort(void)
{ // this code is stable
  return Stop ( ) ;
}

CaError FFmpegStream::IsStopped(void)
{ // this code is stable
  return isStopped ;
}

CaError FFmpegStream::IsActive(void)
{ // this code is stable
  return isActive ;
}

CaTime FFmpegStream::GetTime(void)
{ // this code is stable
  if ( CaOR ( isDecode , isEncode ) ) {
    CaTime IBA = (CaTime)OutputSize   ;
    IBA *= 1000.0                     ;
    IBA /= RingUnit                   ;
    return IBA                        ;
  }                                   ;
  return Timer :: Time ( )            ;
}

double FFmpegStream::GetCpuLoad(void)
{ // this code is stable
  return cpuLoadMeasurer . Value ( ) ;
}

CaInt32 FFmpegStream::ReadAvailable(void)
{ // this code is stable
  return 0 ;
}

CaInt32 FFmpegStream::WriteAvailable(void)
{ // this code is stable
  return 0 ;
}

CaError FFmpegStream::Read(void * buffer,unsigned long frames)
{ // this code is stable
  return InternalError ;
}

CaError FFmpegStream::Write(const void * buffer,unsigned long frames)
{ // this code is stable
  return InternalError ;
}

bool FFmpegStream::hasVolume(void)
{ // this code is stable
  return false ;
}

CaVolume FFmpegStream::MinVolume(void)
{ // this code is stable
  return 0 ;
}

CaVolume FFmpegStream::MaxVolume(void)
{ // this code is stable
  return 10000.0 ;
}

CaVolume FFmpegStream::Volume(int atChannel)
{ // this code is stable
  return 10000.0 ;
}

CaVolume FFmpegStream::setVolume(CaVolume volume,int atChannel)
{ // this code is stable
  return 10000.0  ;
}

bool FFmpegStream::isRealTime(void)
{
  CaRETURN ( CaIsNull(Codec)                                 , false ) ;
  CaRETURN ( CaNOT ( CaIsGreater ( Codec->Interval() , 0 ) ) , false ) ;
  return true                                                          ;
}

bool FFmpegStream::isValid(StreamParameters * inputParameters)
{
  #define DPRINTF(z) dPrintf ( ( z , Codec->Filename() ) )
  if ( !OpenFFMPEG() )                                                       {
    DPRINTF ( "FFmpeg open %s failure\n"                                   ) ;
    return false                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ! FindStream ( ) )                                                    {
    CloseFFMPEG ( )                                                          ;
    DPRINTF (  "FFmpeg can not find stream in %s\n"                        ) ;
    return false                                                             ;
  }                                                                          ;
  if ( ! FindAudio  ( ) )                                                    {
    CloseFFMPEG ( )                                                          ;
    DPRINTF ( "FFmpeg can not find audio stream in %s\n"                   ) ;
    return false                                                             ;
  }                                                                          ;
  if ( ! OpenAudio  ( ) )                                                    {
    CloseFFMPEG ( )                                                          ;
    DPRINTF ( "FFmpeg can not open audio stream in %s\n"                   ) ;
    return false                                                             ;
  }                                                                          ;
  #undef  DPRINTF
  ////////////////////////////////////////////////////////////////////////////
  inputParameters -> channelCount = AudioCtx->channels                       ;
  switch ( AudioCtx -> sample_fmt )                                          {
    case AV_SAMPLE_FMT_U8                                                    :
    case AV_SAMPLE_FMT_U8P                                                   :
      inputParameters -> sampleFormat = cafUint8                             ;
    break                                                                    ;
    case AV_SAMPLE_FMT_S16                                                   :
    case AV_SAMPLE_FMT_S16P                                                  :
      inputParameters -> sampleFormat = cafInt16                             ;
    break                                                                    ;
    case AV_SAMPLE_FMT_S32                                                   :
    case AV_SAMPLE_FMT_S32P                                                  :
      inputParameters -> sampleFormat = cafInt32                             ;
    break                                                                    ;
    case AV_SAMPLE_FMT_FLT                                                   :
    case AV_SAMPLE_FMT_FLTP                                                  :
      inputParameters -> sampleFormat = cafFloat32                           ;
    break                                                                    ;
    case AV_SAMPLE_FMT_DBL                                                   :
    case AV_SAMPLE_FMT_DBLP                                                  :
      inputParameters -> sampleFormat = cafFloat64                           ;
    break                                                                    ;
    default                                                                  :
      inputParameters -> sampleFormat = cafNothing                           ;
    break                                                                    ;
  }                                                                          ;
  Codec -> setChannels     ( AudioCtx        -> channels     )               ;
  Codec -> setSampleRate   ( AudioCtx        -> sample_rate  )               ;
  Codec -> setSampleFormat ( inputParameters -> sampleFormat )               ;
  Codec -> setLength       ( FormatCtx       -> duration     )               ;
  ////////////////////////////////////////////////////////////////////////////
  CloseFFMPEG()                                                              ;
  return true                                                                ;
}

bool FFmpegStream::OpenFFMPEG(void)
{
  AudioIndex   = -1                   ;
  isAudio      = false                ;
  Error        = 0                    ;
  /////////////////////////////////////
  int ret                             ;
  if ( CaNotNull ( Dictionary ) )     {
    ret = ::avformat_open_input       (
          &FormatCtx                  ,
          Codec->Filename()           ,
          NULL                        ,
          &Dictionary               ) ;
  } else                              {
    ret = ::avformat_open_input       (
          &FormatCtx                  ,
          Codec->Filename()           ,
          NULL                        ,
          NULL                      ) ;
  }                                   ;
  /////////////////////////////////////
  if ( CaIsLess ( ret , 0 ) )         {
    Error = ret                       ;
    return false                      ;
  }                                   ;
  if ( CaIsNull ( FormatCtx ) )       {
    Error = AVERROR_DECODER_NOT_FOUND ;
    return false                      ;
  }                                   ;
  /////////////////////////////////////
  return true                         ;
}

bool FFmpegStream::CloseFFMPEG(void)
{
  if ( CaNotNull ( AudioCtx     ) ) ::avcodec_close        (  AudioCtx     ) ;
  if ( CaNotNull ( FormatCtx    ) ) ::avformat_close_input ( &FormatCtx    ) ;
  if ( CaNotNull ( AudioFrame   ) ) ::av_free              ( AudioFrame    ) ;
  if ( CaNotNull ( ConvertCtx   ) ) ::sws_freeContext      (  ConvertCtx   ) ;
  if ( CaNotNull ( Resample     ) ) ::swr_free             ( &Resample     ) ;
  if ( CaNotNull ( audioConvert ) ) ::av_freep             ( &audioConvert ) ;
  if ( CaNotNull ( inputConvert ) ) ::av_freep             ( &inputConvert ) ;
  if ( CaNotNull ( converted    ) ) ::av_freep             ( &converted    ) ;
  ////////////////////////////////////////////////////////////////////////////
  AudioFrame    = NULL                                                       ;
  AudioCtx      = NULL                                                       ;
  FormatCtx     = NULL                                                       ;
  Dictionary    = NULL                                                       ;
  AudioCodec    = NULL                                                       ;
  AudioStream   = NULL                                                       ;
  ConvertCtx    = NULL                                                       ;
  Resample      = NULL                                                       ;
  audioConvert  = NULL                                                       ;
  inputConvert  = NULL                                                       ;
  converted     = NULL                                                       ;
  ////////////////////////////////////////////////////////////////////////////
  return true                                                                ;
}

bool FFmpegStream::FindStream(void)
{
  int ret                                                 ;
  if ( CaIsNull ( FormatCtx ) )                           {
    Error = AVERROR_DECODER_NOT_FOUND                     ;
    return false                                          ;
  }                                                       ;
  /////////////////////////////////////////////////////////
  ret = ::avformat_find_stream_info(FormatCtx,NULL)       ;
  if ( CaIsLess ( ret , 0 ) )                             {
    Error = ret                                           ;
    ::avformat_close_input(&FormatCtx)                    ;
    FormatCtx = NULL                                      ;
    return false                                          ;
  }                                                       ;
  /////////////////////////////////////////////////////////
  for (int i=0;i<(int)FormatCtx->nb_streams;i++)          {
    int t = (int)FormatCtx->streams[i]->codec->codec_type ;
    switch ( t )                                          {
      case AVMEDIA_TYPE_VIDEO                             :
      break                                               ;
      case AVMEDIA_TYPE_AUDIO                             :
        AudioIndex = i                                    ;
      break                                               ;
      case AVMEDIA_TYPE_SUBTITLE                          :
      break                                               ;
      case AVMEDIA_TYPE_DATA                              :
      break                                               ;
      case AVMEDIA_TYPE_ATTACHMENT                        :
      break                                               ;
      case AVMEDIA_TYPE_NB                                :
      break                                               ;
      case AVMEDIA_TYPE_UNKNOWN                           :
      default                                             :
      break                                               ;
    }                                                     ;
  }                                                       ;
  /////////////////////////////////////////////////////////
  return true                                             ;
}

bool FFmpegStream::FindAudio(void)
{
  CaRETURN ( CaIsLess ( AudioIndex , 0 ) , false )         ;
  AudioStream = FormatCtx->streams[AudioIndex]             ;
  AudioCtx    = FormatCtx->streams[AudioIndex]->codec      ;
  AudioCodec  = ::avcodec_find_decoder(AudioCtx->codec_id) ;
  return CaNotNull ( AudioCodec )                          ;
}

bool FFmpegStream::OpenAudio(void)
{
  CaRETURN ( CaIsLess ( AudioIndex , 0 ) , false )                           ;
  CaRETURN ( CaIsNull ( AudioCtx       ) , false )                           ;
  CaRETURN ( CaIsNull ( AudioCodec     ) , false )                           ;
  int ret                                                                    ;
  if ( CaNotNull ( Dictionary ) )                                            {
    ret = ::avcodec_open2 ( AudioCtx , AudioCodec , &Dictionary )            ;
  } else                                                                     {
    ret = ::avcodec_open2 ( AudioCtx , AudioCodec , NULL        )            ;
  }                                                                          ;
  Error   = ret                                                              ;
  isAudio = ( ret >= 0 )                                                     ;
  if ( isAudio )                                                             {
    bool planar    = false                                                   ;
    int  av_planar = -1                                                      ;
    AudioFrame = ::av_frame_alloc()                                          ;
    ::av_frame_unref ( AudioFrame )                                          ;
    av_planar = ::av_sample_fmt_is_planar(AudioCtx->sample_fmt)              ;
    planar    = CaIsEqual ( av_planar , 1 )                                  ;
    if ( CaAND ( planar , CaIsGreater ( AudioCtx->channels , 1 ) ) )         {
      enum AVSampleFormat T = AudioCtx -> sample_fmt                         ;
      switch ( AudioCtx -> sample_fmt )                                      {
        case AV_SAMPLE_FMT_U8P  : T = AV_SAMPLE_FMT_U8  ; break              ;
        case AV_SAMPLE_FMT_S16P : T = AV_SAMPLE_FMT_S16 ; break              ;
        case AV_SAMPLE_FMT_S32P : T = AV_SAMPLE_FMT_S32 ; break              ;
        case AV_SAMPLE_FMT_FLTP : T = AV_SAMPLE_FMT_FLT ; break              ;
        case AV_SAMPLE_FMT_DBLP : T = AV_SAMPLE_FMT_DBL ; break              ;
        default                 :                         break              ;
      }                                                                      ;
      if ( CaNotEqual ( T , AudioCtx -> sample_fmt ) )                       {
        dPrintf ( ( "FFmpeg is now using resampling for planar layout\n" ) ) ;
        Resample = ::swr_alloc  (                                          ) ;
        #define AVSETOPT(ITEM,VALUE) ::av_opt_set_int(Resample,ITEM,VALUE,0)
        AVSETOPT ( "in_channel_layout"  , AudioCtx->channel_layout )         ;
        AVSETOPT ( "out_channel_layout" , AV_CH_LAYOUT_STEREO      )         ;
        AVSETOPT ( "in_sample_rate"     , AudioCtx->sample_rate    )         ;
        AVSETOPT ( "out_sample_rate"    , AudioCtx->sample_rate    )         ;
        #undef  AVSETOPT
        #define AVSETOPT(ITEM,VALUE) ::av_opt_set_sample_fmt(Resample,ITEM,VALUE,0)
        AVSETOPT ( "in_sample_fmt"      , AudioCtx->sample_fmt     )         ;
        AVSETOPT ( "out_sample_fmt"     , T                        )         ;
        #undef  AVSETOPT
        ::swr_init              ( Resample                                 ) ;
        ::av_samples_alloc      ( (uint8_t**)&audioConvert                   ,
                                  NULL                                       ,
                                  AudioCtx->channels                         ,
                                  AudioCtx->sample_rate                      ,
                                  T                                          ,
                                  0                                        ) ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  return isAudio                                                             ;
}

int FFmpegStream::DecodeAudio(AVPacket & Packet,unsigned char * data)
{
  unsigned char * dp       = NULL                                  ;
  int             audiolen = 0                                     ;
  int             len      = 0                                     ;
  int             dataSize = 0                                     ;
  int             gotFrame = 0                                     ;
  //////////////////////////////////////////////////////////////////
  ::av_frame_unref(AudioFrame)                                     ;
  while ( CaIsGreater ( Packet.size , 0 ) )                        {
    gotFrame = 0                                                   ;
    len      = ::avcodec_decode_audio4                             (
                 AudioCtx                                          ,
                 AudioFrame                                        ,
                 &gotFrame                                         ,
                 &Packet                                         ) ;
    CaRETURN ( CaIsLess ( len , 0 ) , audiolen )                   ;
    if ( CaIsGreater ( gotFrame , 0 ) )                            {
      dp      = data + audiolen                                    ;
      dataSize = ::av_samples_get_buffer_size                      (
                   NULL                                            ,
                   AudioCtx   -> channels                          ,
                   AudioFrame -> nb_samples                        ,
                   AudioCtx   -> sample_fmt                        ,
                   1                                             ) ;
      if ( CaIsGreater ( dataSize , 0 ) )                          {
        unsigned char * src = (unsigned char *)AudioFrame->data[0] ;
        if ( NULL != Resample )                                    {
          ::swr_convert ( Resample                                 ,
                          &audioConvert                            ,
                          AudioFrame -> nb_samples                 ,
        (const uint8_t **)AudioFrame -> data                       ,
                          AudioFrame -> nb_samples               ) ;
          src = (unsigned char *)audioConvert                      ;
        }                                                          ;
        ::memcpy ( dp , src , dataSize )                           ;
        audiolen += dataSize                                       ;
        return audiolen                                            ;
      }                                                            ;
    }                                                              ;
    Packet.size -= len                                             ;
    Packet.data += len                                             ;
    Packet.pts   = AV_NOPTS_VALUE                                  ;
    Packet.dts   = AV_NOPTS_VALUE                                  ;
  }                                                                ;
  return audiolen                                                  ;
}

int FFmpegStream::ReadPacket(AVPacket & Packet)
{
  CaRETURN ( CaIsNull ( FormatCtx ) , AVERROR_INVALIDDATA ) ;
  return ::av_read_frame ( FormatCtx , &Packet )            ;
}

bool FFmpegStream::isAudioChannel(AVPacket & Packet)
{
  CaRETURN ( CaIsLess ( AudioIndex , 0 ) , false )      ;
  return CaIsEqual ( Packet.stream_index , AudioIndex ) ;
}

long long FFmpegStream::AudioBufferSize(int milliseconds)
{
  CaRETURN ( CaIsNull ( AudioCtx ) , 0 )        ;
  int channels = AudioCtx -> channels           ;
  int rate     = AudioCtx -> sample_rate        ;
  int bytes    = 0                              ;
  switch ( AudioCtx -> sample_fmt )             {
    case AV_SAMPLE_FMT_U8   : bytes = 1 ; break ;
    case AV_SAMPLE_FMT_S16  : bytes = 2 ; break ;
    case AV_SAMPLE_FMT_S32  : bytes = 4 ; break ;
    case AV_SAMPLE_FMT_FLT  : bytes = 4 ; break ;
    case AV_SAMPLE_FMT_DBL  : bytes = 8 ; break ;
    case AV_SAMPLE_FMT_U8P  : bytes = 1 ; break ;
    case AV_SAMPLE_FMT_S16P : bytes = 2 ; break ;
    case AV_SAMPLE_FMT_S32P : bytes = 4 ; break ;
    case AV_SAMPLE_FMT_FLTP : bytes = 4 ; break ;
    case AV_SAMPLE_FMT_DBLP : bytes = 8 ; break ;
    case AV_SAMPLE_FMT_NONE                     :
    case AV_SAMPLE_FMT_NB                       :
    return 0                                    ;
  }                                             ;
  ///////////////////////////////////////////////
  long long s = bytes                           ;
  s *= channels                                 ;
  s *= rate                                     ;
  s *= milliseconds                             ;
  s /= 1000                                     ;
  return s                                      ;
}

CaSampleFormat FFmpegStream::AudioFormat(void)
{
  CaRETURN ( CaIsNull ( AudioCtx ) , cafNothing ) ;
  switch ( AudioCtx -> sample_fmt )               {
    case AV_SAMPLE_FMT_U8   : return cafUint8     ;
    case AV_SAMPLE_FMT_S16  : return cafInt16     ;
    case AV_SAMPLE_FMT_S32  : return cafInt32     ;
    case AV_SAMPLE_FMT_FLT  : return cafFloat32   ;
    case AV_SAMPLE_FMT_DBL  : return cafFloat64   ;
    case AV_SAMPLE_FMT_U8P  : return cafUint8     ;
    case AV_SAMPLE_FMT_S16P : return cafInt16     ;
    case AV_SAMPLE_FMT_S32P : return cafInt32     ;
    case AV_SAMPLE_FMT_FLTP : return cafFloat64   ;
    case AV_SAMPLE_FMT_DBLP : return cafFloat32   ;
    default                 :               break ;
  }                                               ;
  return cafNothing                               ;
}

int FFmpegStream::BytesPerFrame(void)
{
  CaRETURN ( CaIsNull ( AudioCtx ) , 0 )        ;
  int channels = AudioCtx -> channels           ;
  int bytes    = 0                              ;
  switch ( AudioCtx -> sample_fmt )             {
    case AV_SAMPLE_FMT_U8   : bytes = 1 ; break ;
    case AV_SAMPLE_FMT_S16  : bytes = 2 ; break ;
    case AV_SAMPLE_FMT_S32  : bytes = 4 ; break ;
    case AV_SAMPLE_FMT_FLT  : bytes = 4 ; break ;
    case AV_SAMPLE_FMT_DBL  : bytes = 8 ; break ;
    case AV_SAMPLE_FMT_U8P  : bytes = 1 ; break ;
    case AV_SAMPLE_FMT_S16P : bytes = 2 ; break ;
    case AV_SAMPLE_FMT_S32P : bytes = 4 ; break ;
    case AV_SAMPLE_FMT_FLTP : bytes = 4 ; break ;
    case AV_SAMPLE_FMT_DBLP : bytes = 8 ; break ;
    case AV_SAMPLE_FMT_NONE                     :
    case AV_SAMPLE_FMT_NB                       :
    return 0                                    ;
  }                                             ;
  return ( bytes * channels )                   ;
}

void FFmpegStream::Feeding(void)
{
  if ( CaIsEqual ( RingStart , RingEnd ) ) return                            ;
  CaTime CT    = Timer::Time()                                               ;
  CaTime DT    = CT - RingTime                                               ;
  CaTime CI    = Codec->Interval() - 10.0                                    ;
  DT *= 1000.0                                                               ;
  if ( CaAND ( Codec->Wait() , CaIsGreater ( CI , DT ) ) ) return            ;
  ////////////////////////////////////////////////////////////////////////////
  int    TOTAL = 0                                                           ;
  if ( CaIsGreater ( RingEnd , RingStart ) )                                 {
    TOTAL  = RingEnd  - RingStart                                            ;
  } else                                                                     {
    TOTAL  = RingSize - RingStart                                            ;
  }                                                                          ;
  if ( CaIsGreater ( TOTAL , RingInterval ) ) TOTAL = RingInterval           ;
  ////////////////////////////////////////////////////////////////////////////
  unsigned char * p  = AudioFeed                                             ;
  unsigned char * s  = AudioRing                                             ;
  int             RS = RingStart                                             ;
  int             RE = RingEnd                                               ;
  s += RingStart                                                             ;
  ::memcpy ( p , s , TOTAL )                                                 ;
  RingStart += TOTAL                                                         ;
  if ( RingStart >= RingSize ) RingStart -= RingSize                         ;
  OutputSize += TOTAL                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  int BPS            = TOTAL / BytesPerFrame ( )                             ;
  int callbackResult                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  CaTime IBA = (CaTime)OutputSize                                            ;
  IBA *= 1000.0                                                              ;
  IBA /= RingUnit                                                            ;
  conduit -> Input . Buffer     = AudioFeed                                  ;
  conduit -> Input . FrameCount = BPS                                        ;
  conduit -> Input . AdcDac     = IBA                                        ;
  conduit -> LockConduit   ( )                                               ;
  callbackResult                = conduit -> put ( )                         ;
  conduit -> UnlockConduit ( )                                               ;
  conduit -> Input . Buffer     = NULL                                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( Conduit::Continue == callbackResult )                                 {
  } else
  if ( Conduit::Abort    == callbackResult )                                 {
    abortProcessing = 1                                                      ;
  } else
  if ( Conduit::Postpone == callbackResult )                                 {
    RingStart = RS                                                           ;
    RingEnd   = RE                                                           ;
  } else                                                                     {
    stopProcessing  = 1                                                      ;
  }                                                                          ;
  RingTime = Timer::Time()                                                   ;
}

bool FFmpegStream::isBufferFull(void)
{
  CaRETURN ( CaIsEqual ( RingStart , RingEnd ) , false ) ;
  int TOTAL = 0                                          ;
  if ( CaIsGreater ( RingEnd , RingStart ) )             {
    TOTAL  = RingEnd - RingStart                         ;
  } else                                                 {
    TOTAL  = RingSize - RingStart                        ;
    TOTAL += RingEnd                                     ;
  }                                                      ;
  TOTAL += RingUnit                                      ;
  return ( TOTAL >= RingSize )                           ;
}

int FFmpegStream::AddBuffer(unsigned char * data,int length)
{
  int             NextEnd = RingEnd + length ;
  unsigned char * p       = AudioRing        ;
  unsigned char * s       = data             ;
  p      += RingEnd                          ;
  if ( CaIsGreater ( NextEnd , RingSize ) )  {
    NextEnd = RingSize - RingEnd             ;
    ::memcpy ( p , s , NextEnd )             ;
    s      += NextEnd                        ;
    NextEnd = length - NextEnd               ;
    p       = AudioRing                      ;
    ::memcpy ( p , s , NextEnd )             ;
    RingEnd  = NextEnd                       ;
  } else                                     {
    ::memcpy ( p , s , length )              ;
    RingEnd += length                        ;
  }                                          ;
  if ( RingEnd >= RingSize )                 {
    RingEnd -= RingSize                      ;
  }                                          ;
  return RingEnd                             ;
}

CaError FFmpegStream::Processor(void)
{
  CaRETURN ( isDecode , Decoding ( ) ) ;
  CaRETURN ( isEncode , Encoding ( ) ) ;
  return NoError                       ;
}

CaError FFmpegStream::Decoding(void)
{
  dPrintf ( ( "FFmpeg decoder started\n" ) )                                 ;
  CaRETURN ( CaIsNull ( conduit ) , NoError )                                ;
  if ( !OpenFFMPEG() )                                                       {
    dPrintf ( ( "FFmpeg open %s failure\n" , Codec->Filename() ) )           ;
    return false                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ! FindStream ( ) )                                                    {
    CloseFFMPEG ( )                                                          ;
    dPrintf ( ( "FFmpeg can not find stream in %s\n" , Codec->Filename() ) ) ;
    return false                                                             ;
  }                                                                          ;
  if ( ! FindAudio  ( ) )                                                    {
    CloseFFMPEG ( )                                                          ;
    dPrintf(("FFmpeg can not find audio stream in %s\n",Codec->Filename()))  ;
    return false                                                             ;
  }                                                                          ;
  if ( ! OpenAudio  ( ) )                                                    {
    CloseFFMPEG ( )                                                          ;
    dPrintf(("FFmpeg can not open audio stream in %s\n",Codec->Filename()))  ;
    return false                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  AVPacket        Packet                                                     ;
  bool            done       = false                                         ;
  long long       bufferSize = AudioBufferSize(1000)                         ;
  unsigned char * dat        = new unsigned char [ bufferSize ]              ;
  int             BPF        = BytesPerFrame ( )                             ;
  int             Ret                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  RingStart       = 0                                                        ;
  RingEnd         = 0                                                        ;
  RingUnit        = bufferSize                                               ;
  RingSize        = AudioBufferSize ( Codec -> BufferTimeMs ( ) )            ;
  RingInterval    = AudioBufferSize ( Codec -> Interval     ( ) )            ;
  RingFrames      = RingInterval / BPF                                       ;
  OutputSize      = 0                                                        ;
  AudioRing       = new unsigned char [ RingSize     ]                       ;
  AudioFeed       = new unsigned char [ RingInterval ]                       ;
  RingTime        = Timer::Time()                                            ;
  ::memset         ( dat       , 0 , bufferSize   )                          ;
  ::memset         ( AudioRing , 0 , RingSize     )                          ;
  ::memset         ( AudioFeed , 0 , RingInterval )                          ;
  ////////////////////////////////////////////////////////////////////////////
  conduit -> Input . BytesPerSample = BPF                                    ;
  ::av_init_packet ( &Packet                      )                          ;
  ////////////////////////////////////////////////////////////////////////////
  while ( ! done )                                                           {
    Feeding ( )                                                              ;
    if ( CaIsEqual ( isStopped       , 1 ) ) done = true                     ;
    if ( CaIsEqual ( stopProcessing  , 1 ) ) done = true                     ;
    if ( CaIsEqual ( abortProcessing , 1 ) ) done = true                     ;
    if ( CaIsEqual ( isActive        , 0 ) ) done = true                     ;
    if ( done                              ) continue                        ;
    if ( isBufferFull ( )     )                                              {
      Timer :: Sleep ( 10 )                                                  ;
      continue                                                               ;
    }                                                                        ;
    Ret = ReadPacket(Packet)                                                 ;
    switch (Ret)                                                             {
      case AVERROR_BUFFER_TOO_SMALL                                          :
      case AVERROR_EXTERNAL                                                  :
      case AVERROR_BUG2                                                      :
      case AVERROR_BSF_NOT_FOUND                                             :
      case AVERROR_BUG                                                       :
      case AVERROR_DECODER_NOT_FOUND                                         :
      case AVERROR_DEMUXER_NOT_FOUND                                         :
      case AVERROR_ENCODER_NOT_FOUND                                         :
      case AVERROR_EXIT                                                      :
      case AVERROR_FILTER_NOT_FOUND                                          :
      case AVERROR_INVALIDDATA                                               :
      case AVERROR_MUXER_NOT_FOUND                                           :
      case AVERROR_OPTION_NOT_FOUND                                          :
      case AVERROR_PATCHWELCOME                                              :
      case AVERROR_PROTOCOL_NOT_FOUND                                        :
      case AVERROR_STREAM_NOT_FOUND                                          :
      case AVERROR_UNKNOWN                                                   :
      case AVERROR_EXPERIMENTAL                                              :
        dPrintf(("%s stopped abnormally, %d\n",Codec->Filename(),Ret))       ;
        done = true                                                          ;
      break                                                                  ;
      case AVERROR_EOF                                                       :
        done = true                                                          ;
      break                                                                  ;
      default                                                                :
        if ( CaIsLess ( Ret , 0 ) ) done = true ;                         else
        if ( isAudioChannel ( Packet ) )                                     {
          int len                                                            ;
          len = DecodeAudio ( Packet , dat )                                 ;
          if ( CaIsGreater ( len , 0 ) ) AddBuffer ( dat , len )             ;
        }                                                                    ;
      break                                                                  ;
    }                                                                        ;
    ::av_free_packet ( &Packet )                                             ;
  }                                                                          ;
  done = false                                                               ;
  if ( CaIsEqual ( isStopped       , 1       ) ) done = true                 ;
  if ( CaIsEqual ( stopProcessing  , 1       ) ) done = true                 ;
  if ( CaIsEqual ( abortProcessing , 1       ) ) done = true                 ;
  if ( CaIsEqual ( isActive        , 0       ) ) done = true                 ;
  if ( CaIsEqual ( RingStart       , RingEnd ) ) done = true                 ;
  ////////////////////////////////////////////////////////////////////////////
  while ( ! done )                                                           {
    Feeding ( )                                                              ;
    if ( CaIsEqual ( isStopped       , 1       ) ) done = true               ;
    if ( CaIsEqual ( stopProcessing  , 1       ) ) done = true               ;
    if ( CaIsEqual ( abortProcessing , 1       ) ) done = true               ;
    if ( CaIsEqual ( isActive        , 0       ) ) done = true               ;
    if ( CaIsEqual ( RingStart       , RingEnd ) ) done = true               ;
    Timer :: Sleep ( 5 )                                                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  delete [] dat                                                              ;
  delete [] AudioRing                                                        ;
  delete [] AudioFeed                                                        ;
  dat       = NULL                                                           ;
  AudioRing = NULL                                                           ;
  AudioFeed = NULL                                                           ;
  ////////////////////////////////////////////////////////////////////////////
  CloseFFMPEG ( )                                                            ;
  if ( CaNotNull ( conduit ) )                                               {
    conduit -> finish ( Conduit::InputDirection , Conduit::Correct )         ;
  }                                                                          ;
  cpuLoadMeasurer . Reset ( )                                                ;
  StopThread  ( )                                                            ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

AVSampleFormat FFmpegStream::toSampleFormat(CaSampleFormat format)
{
  AVSampleFormat fmt = AV_SAMPLE_FMT_NONE             ;
  switch ( format )                                   {
    case cafUint8   : fmt = AV_SAMPLE_FMT_U8  ; break ;
    case cafInt16   : fmt = AV_SAMPLE_FMT_S16 ; break ;
    case cafInt32   : fmt = AV_SAMPLE_FMT_S32 ; break ;
    case cafFloat32 : fmt = AV_SAMPLE_FMT_FLT ; break ;
    case cafFloat64 : fmt = AV_SAMPLE_FMT_DBL ; break ;
    default                                           :
    break                                             ;
  }                                                   ;
  return fmt                                          ;
}

bool FFmpegStream::canEncode(StreamParameters * outputParameters)
{
  CaRETURN ( CaIsNull(outputParameters)                            , false ) ;
  CaRETURN ( CaIsNull(outputParameters->streamInfo) , false ) ;
  char * filename = Codec->Filename()                                        ;
  CaRETURN ( CaIsNull ( filename )                                 , false ) ;
  dPrintf ( ( "FFmpegStream::canEncode -> av_guess_format\n" ) )             ;
  AVOutputFormat * of                                                        ;
  of = ::av_guess_format ( NULL , filename , NULL )                          ;
  CaRETURN ( CaIsNull ( of ) , false )                                       ;
  AVCodecID codecId = of->audio_codec                                        ;
  AVCodec * codec = NULL                                                     ;
  dPrintf(("FFmpegStream::canEncode -> avcodec_find_encoder %d\n",codecId )) ;
  codec = ::avcodec_find_encoder(codecId)                                    ;
  if ( CaIsNull ( codec ) )                                                  {
    dPrintf ( ( "FFmpegStream::canEncode -> NULL == codec\n" ) )             ;
    return false                                                             ;
  }                                                                          ;
  dPrintf ( ( "FFmpegStream::canEncode -> Encoder found\n" ) )               ;
  ////////////////////////////////////////////////////////////////////////////
  #ifndef REMOVE_DEBUG_MESSAGE
  if ( CaAND ( CaNotNull ( codec ) , CaNotNull(debugger) ) )                 {
    const int            * samplerates = codec->supported_samplerates        ; // terminated by  0
    const AVSampleFormat * fmts        = codec->sample_fmts                  ; // terminated by -1
    const uint64_t       * channels    = codec->channel_layouts              ; // terminated by  0
    if ( CaNotNull ( samplerates ) )                                         {
      while ( CaNotEqual ( *samplerates , 0 ) )                              {
        dPrintf ( ( "Supported sample rate : %d\n", *samplerates ) )         ;
        samplerates++                                                        ;
      }                                                                      ;
    }                                                                        ;
    if ( CaNotNull ( channels ) )                                            {
      while ( CaNotEqual ( *channels , 0 ) )                                 {
        if ( CaIsEqual ( *channels , AV_CH_LAYOUT_MONO   ) )                 {
          dPrintf ( ( "Supported channel layout : Mono\n"                ) ) ;
        } else
        if ( CaIsEqual ( *channels , AV_CH_LAYOUT_STEREO ) )                 {
          dPrintf ( ( "Supported channel layout : Stereo\n"              ) ) ;
        } else {
          dPrintf ( ( "Supported channel layout : %llx\n", *channels ) )     ;
        }                                                                    ;
        channels++                                                           ;
      }                                                                      ;
    }                                                                        ;
    if ( CaNotNull ( fmts        ) )                                         {
      while ( CaNotEqual ( *fmts , -1 ) )                                    {
        switch ( *fmts )                                                     {
          case AV_SAMPLE_FMT_U8                                              :
          case AV_SAMPLE_FMT_U8P                                             :
            dPrintf ( ( "Supported sample format : 8 bits integer\n"  ) )    ;
          break                                                              ;
          case AV_SAMPLE_FMT_S16                                             :
          case AV_SAMPLE_FMT_S16P                                            :
            dPrintf ( ( "Supported sample format : 16 bits integer\n" ) )    ;
          break                                                              ;
          case AV_SAMPLE_FMT_S32                                             :
          case AV_SAMPLE_FMT_S32P                                            :
            dPrintf ( ( "Supported sample format : 32 bits integer\n" ) )    ;
          break                                                              ;
          case AV_SAMPLE_FMT_FLT                                             :
          case AV_SAMPLE_FMT_FLTP                                            :
            dPrintf ( ( "Supported sample format : 32 bits float\n"   ) )    ;
          break                                                              ;
          case AV_SAMPLE_FMT_DBL                                             :
          case AV_SAMPLE_FMT_DBLP                                            :
            dPrintf ( ( "Supported sample format : 64 bits float\n"   ) )    ;
          break                                                              ;
          default                                                            :
            dPrintf ( ( "Supported sample format ID : %d \n" , *fmts  ) )    ;
          break                                                              ;
        }                                                                    ;
        fmts++                                                               ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  #endif
  ////////////////////////////////////////////////////////////////////////////
  AVCodecContext * ctx                                                       ;
  ctx  = ::avcodec_alloc_context3(codec)                                     ;
  ctx -> sample_rate = Codec -> SampleRate ( )                               ;
  ctx -> channels    = Codec -> Channels   ( )                               ;
  ctx -> sample_fmt  = toSampleFormat ( Codec -> SampleFormat ( ) )          ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "FFmpegStream::canEncode -> avcodec_open2\n" ) )               ;
  if ( ! TryOpen ( ctx , codec ) ) return false                              ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ctx->frame_size <= 0 )                                                {
    dPrintf ( ( "FFmpegStream::canEncode -> ctx->frame_size<=0\n" ) )        ;
    if ( CaNotNull ( ctx ) ) ::avcodec_close ( ctx )                         ;
    return false                                                             ;
  }                                                                          ;
  dPrintf ( ( "FFmpegStream::canEncode -> avcodec_close\n" ) )               ;
  if ( CaNotNull ( ctx ) ) ::avcodec_close ( ctx )                           ;
  return true                                                                ;
}

int FFmpegStream ::   Encode (
      FILE          * fp     ,
      int             length ,
      unsigned char * data   ,
      bool            flush  ,
      int             BPF    )
{
  CaRETURN ( CaIsLess ( length , 0 ) , 0 )            ;
  CaRETURN ( CaIsNull ( AudioCodec ) , 0 )            ;
  CaRETURN ( CaIsNull ( AudioCtx   ) , 0 )            ;
  int  compressed = 0                                 ;
  int  fs         = AudioCtx -> frame_size            ;
  int  fb         = BPF * fs                          ;
  int  rest       = length                            ;
  int  index      = 0                                 ;
  int  mpr        = 0                                 ;
  bool written    = true                              ;
  if ( CaIsGreater ( fb , length ) ) written = false  ;
  if ( flush                       ) written = true   ;
  while ( written )                                   {
    if ( rest <= 0 ) written = false ;             else
    if ( CaIsGreater ( fb , rest ) )                  {
      if ( flush )                                    {
        memset ( AudioRing , 0            , fb  )     ;
        memcpy ( AudioRing , &data[index] , rest)     ;
        rest       = 0                                ;
        index      = length                           ;
        compressed = length                           ;
      } else                                          {
        written = false                               ;
      }                                               ;
    } else                                            {
      memcpy ( AudioRing , &data[index] , fb )        ;
      rest       -= fb                                ;
      index      += fb                                ;
      compressed += fb                                ;
    }                                                 ;
    if ( written )                                    {
#pragma message("avcodec_encode_audio needs to change to avcodec_encode_audio2")
#ifdef XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
      mpr = ::avcodec_encode_audio                    (
              AudioCtx                                ,
              (uint8_t*)audioConvert                  ,
              fs                                      ,
              (short*)AudioRing                     ) ;
#endif
      if ( CaAND ( CaIsGreater ( mpr , 0 )            ,
                   CaNotNull   ( fp      )        ) ) {
        ::fwrite ( audioConvert , 1 , mpr , fp )      ;
      }                                               ;
    }                                                 ;
  }                                                   ;
  return compressed                                   ;
}

int FFmpegStream::Convert(int frames,int toFrames)
{
  if ( NULL == Resample     ) return 0 ;
  if ( NULL == converted    ) return 0 ;
  if ( NULL == inputConvert ) return 0 ;
  //////////////////////////////////////
  return ::swr_convert                 (
    Resample                           ,
    &converted                         ,
    toFrames                           ,
    (const uint8_t **)&inputConvert    ,
    frames                           ) ;
}

bool FFmpegStream::TryOpen(AVCodecContext * ctx,AVCodec * codec)
{
  int rt = ::avcodec_open2(ctx,codec,NULL)                                   ;
  if ( rt >= 0 ) return true                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  bool           correct  = false                                            ;
  bool           drop     = false                                            ;
  CaSampleFormat format   = Codec -> SampleFormat ( )                        ;
  CaSampleFormat FORMAT   = format                                           ;
  int            channels = Codec -> Channels     ( )                        ;
  int            CHANNELS = channels                                         ;
  double         samples  = Codec -> SampleRate   ( )                        ;
  double         SAMPLE   = samples                                          ;
  int            SID      = -1                                               ;
  ////////////////////////////////////////////////////////////////////////////
  for ( int i = 0 ; CaAND ( SID < 0 , i < 15 ) ; i++ )                       {
    if ( CaIsEqual ( (int)AllSamplingRates[i] , (int) SAMPLE ) ) SID = i     ;
  }                                                                          ;
  if ( CaIsLess ( SID , 0 ) ) return false                                   ;
  ////////////////////////////////////////////////////////////////////////////
  while ( CaAND ( ! correct , ! drop ) )                                     {
    if ( CHANNELS > 1 )                                                      {
      ctx -> sample_rate = (int)SAMPLE                                       ;
      ctx -> channels    = 1                                                 ;
      ctx -> sample_fmt  = toSampleFormat ( FORMAT )                         ;
      rt = ::avcodec_open2(ctx,codec,NULL)                                   ;
      if ( rt >= 0 )                                                         {
        format   = FORMAT                                                    ;
        channels = 1                                                         ;
        samples  = SAMPLE                                                    ;
        correct  = true                                                      ;
        continue                                                             ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    drop = false                                                             ;
    while ( ! drop )                                                         {
      FORMAT = (CaSampleFormat)(((int)FORMAT)>>1)                            ;
      if ( FORMAT == cafNothing ) drop = true ; else                         {
        if ( AV_SAMPLE_FMT_NONE != toSampleFormat ( FORMAT ) ) drop = true   ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    drop = false                                                             ;
    if ( CaIsEqual ( FORMAT , cafNothing ) )                                 {
      SID++                                                                  ;
      FORMAT   = format                                                      ;
      SAMPLE   = AllSamplingRates [ SID ]                                    ;
      CHANNELS = channels                                                    ;
      if ( SAMPLE <= 0 ) drop = true                                         ;
      if ( ! drop )                                                          {
        ctx -> sample_rate = (int)SAMPLE                                     ;
        ctx -> channels    = CHANNELS                                        ;
        ctx -> sample_fmt  = toSampleFormat ( FORMAT )                       ;
        rt = ::avcodec_open2(ctx,codec,NULL)                                 ;
        if ( rt >= 0 )                                                       {
          format   = FORMAT                                                  ;
          channels = CHANNELS                                                ;
          samples  = SAMPLE                                                  ;
          correct  = true                                                    ;
          continue                                                           ;
        }                                                                    ;
      }                                                                      ;
    } else                                                                   {
      ctx -> sample_rate = (int)SAMPLE                                       ;
      ctx -> channels    = CHANNELS                                          ;
      ctx -> sample_fmt  = toSampleFormat ( FORMAT )                         ;
      rt = ::avcodec_open2(ctx,codec,NULL)                                   ;
      if ( rt >= 0 )                                                         {
        format   = FORMAT                                                    ;
        channels = CHANNELS                                                  ;
        samples  = SAMPLE                                                    ;
        correct  = true                                                      ;
        continue                                                             ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return correct                                                             ;
}

bool FFmpegStream::RealOpen(AVCodecContext * ctx,AVCodec * codec)
{
  int rt = ::avcodec_open2(ctx,codec,NULL)                                   ;
  if ( rt >= 0 ) return true                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  bool           correct  = false                                            ;
  bool           drop     = false                                            ;
  CaSampleFormat format   = Codec -> SampleFormat ( )                        ;
  CaSampleFormat FORMAT   = format                                           ;
  int            channels = Codec -> Channels     ( )                        ;
  int            CHANNELS = channels                                         ;
  double         samples  = Codec -> SampleRate   ( )                        ;
  double         SAMPLE   = samples                                          ;
  int            SID      = -1                                               ;
  ////////////////////////////////////////////////////////////////////////////
  for ( int i = 0 ; CaAND ( SID < 0 , i < 15 ) ; i++ )                       {
    if ( CaIsEqual ( (int)AllSamplingRates[i] , (int) SAMPLE ) ) SID = i     ;
  }                                                                          ;
  if ( CaIsLess ( SID , 0 ) ) return false                                   ;
  ////////////////////////////////////////////////////////////////////////////
  while ( CaAND ( ! correct , ! drop ) )                                     {
    if ( CHANNELS > 1 )                                                      {
      ctx -> sample_rate = (int)SAMPLE                                       ;
      ctx -> channels    = 1                                                 ;
      ctx -> sample_fmt  = toSampleFormat ( FORMAT )                         ;
      rt = ::avcodec_open2(ctx,codec,NULL)                                   ;
      if ( rt >= 0 )                                                         {
        format   = FORMAT                                                    ;
        channels = 1                                                         ;
        samples  = SAMPLE                                                    ;
        correct  = true                                                      ;
        continue                                                             ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    drop = false                                                             ;
    while ( ! drop )                                                         {
      FORMAT = (CaSampleFormat)(((int)FORMAT)>>1)                            ;
      if ( FORMAT == cafNothing ) drop = true ; else                         {
        if ( AV_SAMPLE_FMT_NONE != toSampleFormat ( FORMAT ) ) drop = true   ;
      }                                                                      ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    drop = false                                                             ;
    if ( CaIsEqual ( FORMAT , cafNothing ) )                                 {
      SID++                                                                  ;
      FORMAT   = format                                                      ;
      SAMPLE   = AllSamplingRates [ SID ]                                    ;
      CHANNELS = channels                                                    ;
      if ( SAMPLE <= 0 ) drop = true                                         ;
      if ( ! drop )                                                          {
        ctx -> sample_rate = (int)SAMPLE                                     ;
        ctx -> channels    = CHANNELS                                        ;
        ctx -> sample_fmt  = toSampleFormat ( FORMAT )                       ;
        rt = ::avcodec_open2(ctx,codec,NULL)                                 ;
        if ( rt >= 0 )                                                       {
          format   = FORMAT                                                  ;
          channels = CHANNELS                                                ;
          samples  = SAMPLE                                                  ;
          correct  = true                                                    ;
          continue                                                           ;
        }                                                                    ;
      }                                                                      ;
    } else                                                                   {
      ctx -> sample_rate = (int)SAMPLE                                       ;
      ctx -> channels    = CHANNELS                                          ;
      ctx -> sample_fmt  = toSampleFormat ( FORMAT )                         ;
      rt = ::avcodec_open2(ctx,codec,NULL)                                   ;
      if ( rt >= 0 )                                                         {
        format   = FORMAT                                                    ;
        channels = CHANNELS                                                  ;
        samples  = SAMPLE                                                    ;
        correct  = true                                                      ;
        continue                                                             ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( correct )                                                             {
    dPrintf ( ( "Old sample rate is %d , channels %d , format %d\n"          ,
                (int)Codec -> SampleRate   ( )                               ,
                (int)Codec -> Channels     ( )                               ,
                (int)Codec -> SampleFormat ( )                           ) ) ;
    dPrintf ( ( "New sample rate is %d , channels %d , format %d\n"          ,
                (int)samples,(int)channels,(int)format                   ) ) ;
    //////////////////////////////////////////////////////////////////////////
    int            inputLayout                                               ;
    int            outputLayout                                              ;
    int            inputSample                                               ;
    int            outputSample                                              ;
    AVSampleFormat inputFmt                                                  ;
    AVSampleFormat outputFmt                                                 ;
    inputLayout  = CaResampler::Channel(Codec->Channels())                   ;
    outputLayout = CaResampler::Channel(channels         )                   ;
    inputSample  = (int)Codec->SampleRate()                                  ;
    outputSample = (int)samples                                              ;
    inputFmt     = (AVSampleFormat)CaResampler::Format(Codec->SampleFormat());
    outputFmt    = (AVSampleFormat)CaResampler::Format(format               );
    //////////////////////////////////////////////////////////////////////////
    Resample = ::swr_alloc  (                                              ) ;
    ::av_opt_set_int        (Resample,"in_channel_layout" ,inputLayout ,0  ) ;
    ::av_opt_set_int        (Resample,"out_channel_layout",outputLayout,0  ) ;
    ::av_opt_set_int        (Resample,"in_sample_rate"    ,inputSample ,0  ) ;
    ::av_opt_set_int        (Resample,"out_sample_rate"   ,outputSample,0  ) ;
    ::av_opt_set_sample_fmt (Resample,"in_sample_fmt"     ,inputFmt    ,0  ) ;
    ::av_opt_set_sample_fmt (Resample,"out_sample_fmt"    ,outputFmt   ,0  ) ;
    ::swr_init              (Resample                                      ) ;
    ::av_samples_alloc      ( (uint8_t**)&inputConvert                       ,
                              NULL                                           ,
                              Codec->Channels()                              ,
                              inputSample * 5                                ,
                              inputFmt                                       ,
                              0                                            ) ;
    ::av_samples_alloc      ( (uint8_t**)&converted                          ,
                              NULL                                           ,
                              channels                                       ,
                              outputSample * 5                               ,
                              outputFmt                                      ,
                              0                                            ) ;
    resampleMUL = outputSample                                               ;
    resampleDIV = inputSample                                                ;
    inputBPF    = channels                                                   ;
    inputBPF   *= Core::SampleSize(format)                                   ;
    return ( NULL != Resample )                                              ;
  }                                                                          ;
  return correct                                                             ;
}

CaError FFmpegStream::Encoding(void)
{
  CaRETURN ( CaIsNull ( conduit ) , NoError )                                ;
  dPrintf ( ( "FFmpeg encoder started\n" ) )                                 ;
  bool initialized = false                                                   ;
  if ( Codec -> hasPreparation ( ) )                                         {
    FFmpegEncodePacket ffmpeg                                                ;
    ffmpeg . size        = sizeof(FFmpegEncodePacket)                        ;
    ffmpeg . hostApiType = FFMPEG                                            ;
    ffmpeg . version     = 1511                                              ;
    ffmpeg . AudioCtx    = AudioCtx                                          ;
    ffmpeg . AudioCodec  = AudioCodec                                        ;
    ffmpeg . AudioStream = AudioStream                                       ;
    ffmpeg . ConvertCtx  = ConvertCtx                                        ;
    ffmpeg . Resample    = Resample                                          ;
    if (Codec->Prepare(&ffmpeg))                                             {
      AudioCtx    = ffmpeg . AudioCtx                                        ;
      AudioCodec  = ffmpeg . AudioCodec                                      ;
      AudioStream = ffmpeg . AudioStream                                     ;
      ConvertCtx  = ffmpeg . ConvertCtx                                      ;
      Resample    = ffmpeg . Resample                                        ;
      initialized = true                                                     ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  char * filename = Codec->Filename()                                        ;
  CaRETURN ( CaIsNull ( filename ) , NoError  )                              ;
  if ( ! initialized )                                                       {
    AVOutputFormat * of                                                      ;
    of = ::av_guess_format ( NULL , filename , NULL )                        ;
    CaRETURN ( CaIsNull ( of ) , NoError )                                   ;
    AVCodecID codecId = of->audio_codec                                      ;
    AVCodec * codec = NULL                                                   ;
    codec = ::avcodec_find_encoder(codecId)                                  ;
    if ( CaIsNull ( codec ) )                                                {
      StopThread (    )                                                      ;
      return NoError                                                         ;
    }                                                                        ;
    AudioCodec = codec                                                       ;
    AudioCtx   = ::avcodec_alloc_context3 ( AudioCodec )                     ;
    AudioCtx  -> sample_rate = Codec -> SampleRate ( )                       ;
    AudioCtx  -> channels    = Codec -> Channels   ( )                       ;
    AudioCtx  -> sample_fmt  = toSampleFormat ( Codec -> SampleFormat ( ) )  ;
    if ( ! RealOpen ( AudioCtx , AudioCodec ) )                              {
      StopThread ( )                                                         ;
      return NoError                                                         ;
    }                                                                        ;
    if ( AudioCtx->frame_size <= 0 )                                         {
      ::avcodec_close ( AudioCtx )                                           ;
      StopThread      (          )                                           ;
      return NoError                                                         ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  bool            done       = false                                         ;
  int             BPF        = Codec->BytesPerSample()                       ;
  long long       bufferSize = BPF * Codec -> SampleRate ( )                 ;
  int             callbackResult                                             ;
  long long       RS                                                         ;
  long long       RE                                                         ;
  CaTime          DT         = 0                                             ;
  CaTime          LT         = 0                                             ;
  CaTime          XT         = 0                                             ;
  CaTime          IBA        = 0                                             ;
  unsigned char * dat                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  RingStart       = 0                                                        ;
  RingEnd         = 0                                                        ;
  RingUnit        = bufferSize                                               ;
  RingSize        = ( bufferSize * Codec -> BufferTimeMs ( ) ) / 1000        ;
  RingInterval    = ( bufferSize * Codec -> Interval     ( ) ) / 1000        ;
  RingFrames      = RingInterval / BPF                                       ;
  audioConvert    = new unsigned char [ RingSize         ]                   ;
  AudioRing       = new unsigned char [ RingSize         ]                   ;
  dat             = new unsigned char [ RingSize         ]                   ;
  AudioFeed       = new unsigned char [ RingInterval * 5 ]                   ;
  RingTime        = Timer::Time()                                            ;
  OutputSize      = 0                                                        ;
  ::memset         ( audioConvert , 0 , RingSize     )                       ;
  ::memset         ( AudioRing    , 0 , RingSize     )                       ;
  ::memset         ( AudioFeed    , 0 , RingInterval )                       ;
  ////////////////////////////////////////////////////////////////////////////
  FILE * fp = NULL                                                           ;
  ////////////////////////////////////////////////////////////////////////////
  fp = fopen(Codec->Filename(),"wb")                                         ;
  ////////////////////////////////////////////////////////////////////////////
  conduit -> Output . setSample                                              (
    Codec -> SampleFormat ( )                                                ,
    Codec -> Channels     ( )                                              ) ;
  LT  = Timer::Time()                                                        ;
  LT *= 1000.0                                                               ;
  while ( ! done )                                                           {
    if ( CaIsEqual ( isStopped       , 1 ) ) done = true                     ;
    if ( CaIsEqual ( stopProcessing  , 1 ) ) done = true                     ;
    if ( CaIsEqual ( abortProcessing , 1 ) ) done = true                     ;
    if ( CaIsEqual ( isActive        , 0 ) ) done = true                     ;
    if ( done                              ) break                           ;
    DT  = Timer::Time()                                                      ;
    DT *= 1000.0                                                             ;
    DT -= LT                                                                 ;
    XT += DT                                                                 ;
    //////////////////////////////////////////////////////////////////////////
    if ( DT < Codec -> Interval ( ) )                                        {
      Timer :: Sleep ( 3 )                                                   ;
      continue                                                               ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    IBA = (CaTime)OutputSize                                                 ;
    IBA *= 1000.0                                                            ;
    IBA /= RingUnit                                                          ;
    //////////////////////////////////////////////////////////////////////////
    LT  = Timer::Time()                                                      ;
    LT *= 1000.0                                                             ;
    conduit -> Output . Buffer     = AudioFeed                               ;
    conduit -> Output . FrameCount = RingFrames                              ;
    conduit -> Output . AdcDac     = IBA                                     ;
    conduit -> LockConduit   ( )                                             ;
    callbackResult                 = conduit -> obtain ( )                   ;
    conduit -> UnlockConduit ( )                                             ;
    conduit -> Output . Buffer     = NULL                                    ;
    RS = 1                                                                   ;
    RE = RingEnd                                                             ;
    //////////////////////////////////////////////////////////////////////////
    if ( Conduit::Continue == callbackResult )                               {
    } else
    if ( Conduit::Abort    == callbackResult )                               {
      abortProcessing = 1                                                    ;
    } else
    if ( Conduit::Postpone == callbackResult )                               {
      RingEnd = RE                                                           ;
      RS      = 0                                                            ;
    } else                                                                   {
      stopProcessing  = 1                                                    ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
    if ( CaIsGreater ( RS , 0 ) )                                            {
      int processed = 0                                                      ;
      cpuLoadMeasurer . Begin ( )                                            ;
      memcpy ( dat + RingEnd , AudioFeed , RingInterval )                    ;
      RingEnd    += RingInterval                                             ;
      OutputSize += RingInterval                                             ;
      if ( CaNotNull ( Resample ) )                                          {
        long long FromFrames = RingEnd / BPF                                 ;
        int ToFrames   = FromFrames                                          ;
        ToFrames *= resampleMUL                                              ;
        ToFrames /= resampleDIV                                              ;
        ::memcpy ( inputConvert , dat , RingEnd )                            ;
        RingStart  = Convert ( FromFrames , ToFrames ) * inputBPF            ;
        RingStart = ToFrames * inputBPF                                      ;
        if ( RingStart > 0 )                                                 {
          RingStart = Encode ( fp                                            ,
                               RingStart                                     ,
                               converted                                     ,
                               false                                         ,
                               inputBPF                                    ) ;
          if ( RingStart > 0 )                                               {
            RingStart /= inputBPF                                            ;
            RingStart *= resampleDIV                                         ;
            RingStart /= resampleMUL                                         ;
            RingStart *= BPF                                                 ;
          } else RingStart = 0                                               ;
        } else RingStart = 0                                                 ;
        if ( RingStart > RingEnd ) RingStart = RingEnd                       ;
      } else                                                                 {
        RingStart = Encode ( fp                                              ,
                             RingEnd                                         ,
                             dat                                             ,
                             false                                           ,
                             BPF                                           ) ;
      }                                                                      ;
      processed = RingEnd - RingStart                                        ;
      processed = processed / BPF                                            ;
      if ( RingStart > 0 )                                                   {
        if ( RingEnd > RingStart )                                           {
          ::memcpy ( dat , dat + RingStart , RingEnd - RingStart )           ;
          RingEnd   = RingEnd - RingStart                                    ;
          RingStart = 0                                                      ;
        } else                                                               {
          RingStart = 0                                                      ;
          RingEnd   = 0                                                      ;
        }                                                                    ;
      }                                                                      ;
      cpuLoadMeasurer . End ( processed )                                    ;
    }                                                                        ;
    //////////////////////////////////////////////////////////////////////////
  }                                                                          ;
  if ( CaIsGreater ( RingEnd , 0 ) )                                         {
    if ( CaNotNull ( Resample ) )                                            {
      int FromFrames = RingEnd / BPF                                         ;
      int ToFrames   = FromFrames                                            ;
      ToFrames *= resampleMUL                                                ;
      ToFrames /= resampleDIV                                                ;
      ::memcpy ( inputConvert , dat , RingEnd )                              ;
      RingStart  = Convert ( FromFrames , ToFrames ) * inputBPF              ;
      if ( RingStart > 0 )                                                   {
        Encode ( fp                                                          ,
                 RingStart                                                   ,
                 converted                                                   ,
                 false                                                       ,
                 inputBPF                                                  ) ;
      }                                                                      ;
    } else                                                                   {
      Encode ( fp , RingEnd , dat , false , BPF )                            ;
    }                                                                        ;
    RingEnd = 0                                                              ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( fp ) ) fclose ( fp )                                      ;
  ::avcodec_close ( AudioCtx )                                               ;
  ////////////////////////////////////////////////////////////////////////////
  delete [] dat                                                              ;
  delete [] audioConvert                                                     ;
  delete [] AudioRing                                                        ;
  delete [] AudioFeed                                                        ;
  dat            = NULL                                                      ;
  audioConvert   = NULL                                                      ;
  AudioRing      = NULL                                                      ;
  AudioFeed      = NULL                                                      ;
  if ( CaNotNull ( inputConvert ) ) ::av_freep ( &inputConvert )             ;
  if ( CaNotNull ( converted    ) ) ::av_freep ( &converted    )             ;
  if ( CaNotNull ( Resample     ) ) ::swr_free ( &Resample     )             ;
  inputConvert   = NULL                                                      ;
  converted      = NULL                                                      ;
  Resample       = NULL                                                      ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaNotNull ( conduit ) )                                               {
    conduit -> finish ( Conduit::OutputDirection , Conduit::Correct )        ;
  }                                                                          ;
  cpuLoadMeasurer . Reset ( )                                                ;
  StopThread ( )                                                             ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

void FFmpegStream::StopThread(void)
{ // this code is stable
  isStopped       = 1   ;
  isActive        = 0   ;
  stopProcessing  = 0   ;
  abortProcessing = 0   ;
}

///////////////////////////////////////////////////////////////////////////////

FFmpegHostApi:: FFmpegHostApi (void)
              : HostApi       (    )
{ // this code is stable
}

FFmpegHostApi::~FFmpegHostApi(void)
{ // this code is stable
}

HostApi::Encoding FFmpegHostApi::encoding(void) const
{ // this code is stable
  return UTF8 ;
}

bool FFmpegHostApi::hasDuplex(void) const
{ // this code is stable
  return false ;
}

CaError FFmpegHostApi::Correct(const  StreamParameters * parameters)
{ // this code is stable
  CaRETURN ( CaIsNull ( parameters ) , UnanticipatedHostError )              ;
  CaRETURN ( CaIsNull ( parameters->streamInfo )              ,
             IncompatibleStreamInfo                         ) ;
  MediaCodecSpecificStreamInfoHeader * mc                                    ;
  MediaCodec                         * codec                                 ;
  mc = (MediaCodecSpecificStreamInfoHeader *)parameters->streamInfo ;
  codec = mc->codec                                                          ;
  CaRETURN ( CaIsNull ( codec ) , BadStreamPtr )                             ;
  CaRETURN ( CaNotEqual ( mc->size , sizeof(MediaCodecSpecificStreamInfoHeader) ) ,
             IncompatibleStreamInfo                         ) ;
  CaRETURN ( CaNotEqual ( mc->hostApiType , FFMPEG )                         ,
             IncompatibleStreamInfo                         ) ;
  return NoError                                                             ;
}

CaError FFmpegHostApi ::            Open              (
          Stream                 ** s                 ,
          const StreamParameters *  inputParameters   ,
          const StreamParameters *  outputParameters  ,
          double                    sampleRate        ,
          CaUint32                  framesPerCallback ,
          CaStreamFlags             streamFlags       ,
          Conduit                *  conduit           )
{ // this code is stable
  bool isInput  = CaNotNull ( inputParameters  )                             ;
  bool isOutput = CaNotNull ( outputParameters )                             ;
  CaRETURN ( ( CaAND ( ! isInput , ! isOutput ) ) , NotInitialized )         ;
  CaRETURN ( ( CaAND (   isInput ,   isOutput ) ) , InvalidFlag    )         ;
  ////////////////////////////////////////////////////////////////////////////
  CaError      result = NoError                                              ;
  MediaCodec * codec = NULL                                                  ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "FFmpegHostApi::Open -> NULL != inputParameters\n" ) )         ;
  if ( isInput )                                                             {
    result = Correct ( inputParameters )                                     ;
    CaRETURN ( ( CaIsWrong ( result ) ) , result )                           ;
    MediaCodecSpecificStreamInfoHeader * mc                                  ;
    mc = (MediaCodecSpecificStreamInfoHeader *)inputParameters->streamInfo ;
    codec = mc->codec                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "FFmpegHostApi::Open -> NULL != outputParameters\n" ) )        ;
  if ( isOutput )                                                            {
    result = Correct ( outputParameters )                                    ;
    CaRETURN ( ( CaIsWrong ( result ) ) , result )                           ;
    MediaCodecSpecificStreamInfoHeader * mc                                  ;
    mc = (MediaCodecSpecificStreamInfoHeader *)outputParameters->streamInfo ;
    codec = mc->codec                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( CaIsNull ( codec ) ) return BadBufferPtr                              ;
  ////////////////////////////////////////////////////////////////////////////
  FFmpegStream * stream = NULL                                               ;
  stream             = new FFmpegStream ( )                                  ;
  stream -> debugger = debugger                                              ;
  stream -> Codec    = codec                                                 ;
  stream -> conduit  = conduit                                               ;
  dPrintf ( ( "FFmpegHostApi::Open -> FFmpegStream created\n" ) )            ;
  ////////////////////////////////////////////////////////////////////////////
  if ( isInput )                                                             {
    dPrintf ( ( "FFmpegHostApi::Open -> stream->isValid\n" ) )               ;
    stream->isDecode = true                                                  ;
    if ( ! stream -> isValid ( (StreamParameters *) inputParameters ) )      {
      return UnanticipatedHostError                                          ;
    }                                                                        ;
    dPrintf ( ( "Channels : %d , Format : %d\n"                              ,
                inputParameters->channelCount                                ,
                inputParameters->sampleFormat                            ) ) ;
  }                                                                          ;
  dPrintf ( ( "FFmpegHostApi::Open -> stream->isDecode\n" ) )                ;
  ////////////////////////////////////////////////////////////////////////////
  if ( isOutput )                                                            {
    dPrintf ( ( "FFmpegHostApi::Open -> stream->canEncode\n" ) )             ;
    stream->isEncode = true                                                  ;
    if ( ! stream -> canEncode ( (StreamParameters *) outputParameters ) )   {
      return UnanticipatedHostError                                          ;
    }                                                                        ;
    dPrintf ( ( "Channels : %d , Format : %d\n"                              ,
                outputParameters->channelCount                               ,
                outputParameters->sampleFormat                           ) ) ;
  }                                                                          ;
  dPrintf ( ( "FFmpegHostApi::Open -> stream->isEncode\n" ) )                ;
  ////////////////////////////////////////////////////////////////////////////
  dPrintf ( ( "FFmpegHostApi::Open -> stream opened\n" ) )                   ;
  *s = (Stream *)stream                                                      ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

void FFmpegHostApi::Terminate(void)
{ // this code is stable
  if ( CaIsNull ( deviceInfos ) ) return ;
  for (int i=0;i<2;i++)                  {
    delete deviceInfos[i]                ;
  }                                      ;
  delete [] deviceInfos                  ;
  deviceInfos = NULL                     ;
}

CaError FFmpegHostApi::isSupported                   (
          const  StreamParameters * inputParameters  ,
          const  StreamParameters * outputParameters ,
          double                    sampleRate       )
{ // this code is stable
  bool isInput  = CaNotNull ( inputParameters  )                             ;
  bool isOutput = CaNotNull ( outputParameters )                             ;
  CaRETURN ( ( CaAND ( ! isInput , ! isOutput ) ) , NotInitialized )         ;
  CaRETURN ( ( CaAND (   isInput ,   isOutput ) ) , InvalidFlag    )         ;
  ////////////////////////////////////////////////////////////////////////////
  CaError result = NoError                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  if ( isInput )                                                             {
    result = Correct ( inputParameters  )                                    ;
    CaRETURN ( ( CaIsWrong ( result ) ) , result )                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( isOutput )                                                            {
    result = Correct ( outputParameters )                                    ;
    CaRETURN ( ( CaIsWrong ( result ) ) , result )                           ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
