/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Lastest update : 2014/12/20                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CiosAudioPrivate.hpp"

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

#if defined(FFMPEGLIB)

AVCodecID AllFFmpegCodecIDs [] = {
  AV_CODEC_ID_MPEG1VIDEO,
  AV_CODEC_ID_MPEG2VIDEO,
  AV_CODEC_ID_MPEG2VIDEO_XVMC,
  AV_CODEC_ID_H261,
  AV_CODEC_ID_H263,
  AV_CODEC_ID_RV10,
  AV_CODEC_ID_RV20,
  AV_CODEC_ID_MJPEG,
  AV_CODEC_ID_MJPEGB,
  AV_CODEC_ID_LJPEG,
  AV_CODEC_ID_SP5X,
  AV_CODEC_ID_JPEGLS,
  AV_CODEC_ID_MPEG4,
  AV_CODEC_ID_RAWVIDEO,
  AV_CODEC_ID_MSMPEG4V1,
  AV_CODEC_ID_MSMPEG4V2,
  AV_CODEC_ID_MSMPEG4V3,
  AV_CODEC_ID_WMV1,
  AV_CODEC_ID_WMV2,
  AV_CODEC_ID_H263P,
  AV_CODEC_ID_H263I,
  AV_CODEC_ID_FLV1,
  AV_CODEC_ID_SVQ1,
  AV_CODEC_ID_SVQ3,
  AV_CODEC_ID_DVVIDEO,
  AV_CODEC_ID_HUFFYUV,
  AV_CODEC_ID_CYUV,
  AV_CODEC_ID_H264,
  AV_CODEC_ID_INDEO3,
  AV_CODEC_ID_VP3,
  AV_CODEC_ID_THEORA,
  AV_CODEC_ID_ASV1,
  AV_CODEC_ID_ASV2,
  AV_CODEC_ID_FFV1,
  AV_CODEC_ID_4XM,
  AV_CODEC_ID_VCR1,
  AV_CODEC_ID_CLJR,
  AV_CODEC_ID_MDEC,
  AV_CODEC_ID_ROQ,
  AV_CODEC_ID_INTERPLAY_VIDEO,
  AV_CODEC_ID_XAN_WC3,
  AV_CODEC_ID_XAN_WC4,
  AV_CODEC_ID_RPZA,
  AV_CODEC_ID_CINEPAK,
  AV_CODEC_ID_WS_VQA,
  AV_CODEC_ID_MSRLE,
  AV_CODEC_ID_MSVIDEO1,
  AV_CODEC_ID_IDCIN,
  AV_CODEC_ID_8BPS,
  AV_CODEC_ID_SMC,
  AV_CODEC_ID_FLIC,
  AV_CODEC_ID_TRUEMOTION1,
  AV_CODEC_ID_VMDVIDEO,
  AV_CODEC_ID_MSZH,
  AV_CODEC_ID_ZLIB,
  AV_CODEC_ID_QTRLE,
  AV_CODEC_ID_TSCC,
  AV_CODEC_ID_ULTI,
  AV_CODEC_ID_QDRAW,
  AV_CODEC_ID_VIXL,
  AV_CODEC_ID_QPEG,
  AV_CODEC_ID_PNG,
  AV_CODEC_ID_PPM,
  AV_CODEC_ID_PBM,
  AV_CODEC_ID_PGM,
  AV_CODEC_ID_PGMYUV,
  AV_CODEC_ID_PAM,
  AV_CODEC_ID_FFVHUFF,
  AV_CODEC_ID_RV30,
  AV_CODEC_ID_RV40,
  AV_CODEC_ID_VC1,
  AV_CODEC_ID_WMV3,
  AV_CODEC_ID_LOCO,
  AV_CODEC_ID_WNV1,
  AV_CODEC_ID_AASC,
  AV_CODEC_ID_INDEO2,
  AV_CODEC_ID_FRAPS,
  AV_CODEC_ID_TRUEMOTION2,
  AV_CODEC_ID_BMP,
  AV_CODEC_ID_CSCD,
  AV_CODEC_ID_MMVIDEO,
  AV_CODEC_ID_ZMBV,
  AV_CODEC_ID_AVS,
  AV_CODEC_ID_SMACKVIDEO,
  AV_CODEC_ID_NUV,
  AV_CODEC_ID_KMVC,
  AV_CODEC_ID_FLASHSV,
  AV_CODEC_ID_CAVS,
  AV_CODEC_ID_JPEG2000,
  AV_CODEC_ID_VMNC,
  AV_CODEC_ID_VP5,
  AV_CODEC_ID_VP6,
  AV_CODEC_ID_VP6F,
  AV_CODEC_ID_TARGA,
  AV_CODEC_ID_DSICINVIDEO,
  AV_CODEC_ID_TIERTEXSEQVIDEO,
  AV_CODEC_ID_TIFF,
  AV_CODEC_ID_GIF,
  AV_CODEC_ID_DXA,
  AV_CODEC_ID_DNXHD,
  AV_CODEC_ID_THP,
  AV_CODEC_ID_SGI,
  AV_CODEC_ID_C93,
  AV_CODEC_ID_BETHSOFTVID,
  AV_CODEC_ID_PTX,
  AV_CODEC_ID_TXD,
  AV_CODEC_ID_VP6A,
  AV_CODEC_ID_AMV,
  AV_CODEC_ID_VB,
  AV_CODEC_ID_PCX,
  AV_CODEC_ID_SUNRAST,
  AV_CODEC_ID_INDEO4,
  AV_CODEC_ID_INDEO5,
  AV_CODEC_ID_MIMIC,
  AV_CODEC_ID_RL2,
  AV_CODEC_ID_ESCAPE124,
  AV_CODEC_ID_DIRAC,
  AV_CODEC_ID_BFI,
  AV_CODEC_ID_CMV,
  AV_CODEC_ID_MOTIONPIXELS,
  AV_CODEC_ID_TGV,
  AV_CODEC_ID_TGQ,
  AV_CODEC_ID_TQI,
  AV_CODEC_ID_AURA,
  AV_CODEC_ID_AURA2,
  AV_CODEC_ID_V210X,
  AV_CODEC_ID_TMV,
  AV_CODEC_ID_V210,
  AV_CODEC_ID_DPX,
  AV_CODEC_ID_MAD,
  AV_CODEC_ID_FRWU,
  AV_CODEC_ID_FLASHSV2,
  AV_CODEC_ID_CDGRAPHICS,
  AV_CODEC_ID_R210,
  AV_CODEC_ID_ANM,
  AV_CODEC_ID_BINKVIDEO,
  AV_CODEC_ID_IFF_ILBM,
  AV_CODEC_ID_IFF_BYTERUN1,
  AV_CODEC_ID_KGV1,
  AV_CODEC_ID_YOP,
  AV_CODEC_ID_VP8,
  AV_CODEC_ID_PICTOR,
  AV_CODEC_ID_ANSI,
  AV_CODEC_ID_A64_MULTI,
  AV_CODEC_ID_A64_MULTI5,
  AV_CODEC_ID_R10K,
  AV_CODEC_ID_MXPEG,
  AV_CODEC_ID_LAGARITH,
  AV_CODEC_ID_PRORES,
  AV_CODEC_ID_JV,
  AV_CODEC_ID_DFA,
  AV_CODEC_ID_WMV3IMAGE,
  AV_CODEC_ID_VC1IMAGE,
  AV_CODEC_ID_UTVIDEO,
  AV_CODEC_ID_BMV_VIDEO,
  AV_CODEC_ID_VBLE,
  AV_CODEC_ID_DXTORY,
  AV_CODEC_ID_V410,
  AV_CODEC_ID_XWD,
  AV_CODEC_ID_CDXL,
  AV_CODEC_ID_XBM,
  AV_CODEC_ID_ZEROCODEC,
  AV_CODEC_ID_MSS1,
  AV_CODEC_ID_MSA1,
  AV_CODEC_ID_TSCC2,
  AV_CODEC_ID_MTS2,
  AV_CODEC_ID_CLLC,
  AV_CODEC_ID_MSS2,
  AV_CODEC_ID_VP9,
  AV_CODEC_ID_AIC,
  AV_CODEC_ID_HNM4_VIDEO,
  AV_CODEC_ID_BRENDER_PIX,
  AV_CODEC_ID_Y41P,
  AV_CODEC_ID_ESCAPE130,
  AV_CODEC_ID_EXR,
  AV_CODEC_ID_AVRP,
  AV_CODEC_ID_012V,
  AV_CODEC_ID_G2M,
  AV_CODEC_ID_AVUI,
  AV_CODEC_ID_AYUV,
  AV_CODEC_ID_TARGA_Y216,
  AV_CODEC_ID_V308,
  AV_CODEC_ID_V408,
  AV_CODEC_ID_YUV4,
  AV_CODEC_ID_SANM,
  AV_CODEC_ID_PAF_VIDEO,
  AV_CODEC_ID_AVRN,
  AV_CODEC_ID_CPIA,
  AV_CODEC_ID_XFACE,
  AV_CODEC_ID_SGIRLE,
  AV_CODEC_ID_MVC1,
  AV_CODEC_ID_MVC2,
  AV_CODEC_ID_SNOW,
  AV_CODEC_ID_WEBP,
  AV_CODEC_ID_SMVJPEG,
  AV_CODEC_ID_HEVC,
  AV_CODEC_ID_FIRST_AUDIO,
  AV_CODEC_ID_PCM_S16LE,
  AV_CODEC_ID_PCM_S16BE,
  AV_CODEC_ID_PCM_U16LE,
  AV_CODEC_ID_PCM_U16BE,
  AV_CODEC_ID_PCM_S8,
  AV_CODEC_ID_PCM_U8,
  AV_CODEC_ID_PCM_MULAW,
  AV_CODEC_ID_PCM_ALAW,
  AV_CODEC_ID_PCM_S32LE,
  AV_CODEC_ID_PCM_S32BE,
  AV_CODEC_ID_PCM_U32LE,
  AV_CODEC_ID_PCM_U32BE,
  AV_CODEC_ID_PCM_S24LE,
  AV_CODEC_ID_PCM_S24BE,
  AV_CODEC_ID_PCM_U24LE,
  AV_CODEC_ID_PCM_U24BE,
  AV_CODEC_ID_PCM_S24DAUD,
  AV_CODEC_ID_PCM_ZORK,
  AV_CODEC_ID_PCM_S16LE_PLANAR,
  AV_CODEC_ID_PCM_DVD,
  AV_CODEC_ID_PCM_F32BE,
  AV_CODEC_ID_PCM_F32LE,
  AV_CODEC_ID_PCM_F64BE,
  AV_CODEC_ID_PCM_F64LE,
  AV_CODEC_ID_PCM_BLURAY,
  AV_CODEC_ID_PCM_LXF,
  AV_CODEC_ID_S302M,
  AV_CODEC_ID_PCM_S8_PLANAR,
  AV_CODEC_ID_PCM_S24LE_PLANAR,
  AV_CODEC_ID_PCM_S32LE_PLANAR,
  AV_CODEC_ID_PCM_S16BE_PLANAR,
  AV_CODEC_ID_ADPCM_IMA_QT,
  AV_CODEC_ID_ADPCM_IMA_WAV,
  AV_CODEC_ID_ADPCM_IMA_DK3,
  AV_CODEC_ID_ADPCM_IMA_DK4,
  AV_CODEC_ID_ADPCM_IMA_WS,
  AV_CODEC_ID_ADPCM_IMA_SMJPEG,
  AV_CODEC_ID_ADPCM_MS,
  AV_CODEC_ID_ADPCM_4XM,
  AV_CODEC_ID_ADPCM_XA,
  AV_CODEC_ID_ADPCM_ADX,
  AV_CODEC_ID_ADPCM_EA,
  AV_CODEC_ID_ADPCM_G726,
  AV_CODEC_ID_ADPCM_CT,
  AV_CODEC_ID_ADPCM_SWF,
  AV_CODEC_ID_ADPCM_YAMAHA,
  AV_CODEC_ID_ADPCM_SBPRO_4,
  AV_CODEC_ID_ADPCM_SBPRO_3,
  AV_CODEC_ID_ADPCM_SBPRO_2,
  AV_CODEC_ID_ADPCM_THP,
  AV_CODEC_ID_ADPCM_IMA_AMV,
  AV_CODEC_ID_ADPCM_EA_R1,
  AV_CODEC_ID_ADPCM_EA_R3,
  AV_CODEC_ID_ADPCM_EA_R2,
  AV_CODEC_ID_ADPCM_IMA_EA_SEAD,
  AV_CODEC_ID_ADPCM_IMA_EA_EACS,
  AV_CODEC_ID_ADPCM_EA_XAS,
  AV_CODEC_ID_ADPCM_EA_MAXIS_XA,
  AV_CODEC_ID_ADPCM_IMA_ISS,
  AV_CODEC_ID_ADPCM_G722,
  AV_CODEC_ID_ADPCM_IMA_APC,
  AV_CODEC_ID_VIMA,
  AV_CODEC_ID_ADPCM_AFC,
  AV_CODEC_ID_ADPCM_IMA_OKI,
  AV_CODEC_ID_ADPCM_DTK,
  AV_CODEC_ID_ADPCM_IMA_RAD,
  AV_CODEC_ID_ADPCM_G726LE,
  AV_CODEC_ID_AMR_NB,
  AV_CODEC_ID_AMR_WB,
  AV_CODEC_ID_RA_144,
  AV_CODEC_ID_RA_288,
  AV_CODEC_ID_ROQ_DPCM,
  AV_CODEC_ID_INTERPLAY_DPCM,
  AV_CODEC_ID_XAN_DPCM,
  AV_CODEC_ID_SOL_DPCM,
  AV_CODEC_ID_MP2,
  AV_CODEC_ID_MP3,
  AV_CODEC_ID_AAC,
  AV_CODEC_ID_AC3,
  AV_CODEC_ID_DTS,
  AV_CODEC_ID_VORBIS,
  AV_CODEC_ID_DVAUDIO,
  AV_CODEC_ID_WMAV1,
  AV_CODEC_ID_WMAV2,
  AV_CODEC_ID_MACE3,
  AV_CODEC_ID_MACE6,
  AV_CODEC_ID_VMDAUDIO,
  AV_CODEC_ID_FLAC,
  AV_CODEC_ID_MP3ADU,
  AV_CODEC_ID_MP3ON4,
  AV_CODEC_ID_SHORTEN,
  AV_CODEC_ID_ALAC,
  AV_CODEC_ID_WESTWOOD_SND1,
  AV_CODEC_ID_GSM,
  AV_CODEC_ID_QDM2,
  AV_CODEC_ID_COOK,
  AV_CODEC_ID_TRUESPEECH,
  AV_CODEC_ID_TTA,
  AV_CODEC_ID_SMACKAUDIO,
  AV_CODEC_ID_QCELP,
  AV_CODEC_ID_WAVPACK,
  AV_CODEC_ID_DSICINAUDIO,
  AV_CODEC_ID_IMC,
  AV_CODEC_ID_MUSEPACK7,
  AV_CODEC_ID_MLP,
  AV_CODEC_ID_GSM_MS,
  AV_CODEC_ID_ATRAC3,
  AV_CODEC_ID_VOXWARE,
  AV_CODEC_ID_APE,
  AV_CODEC_ID_NELLYMOSER,
  AV_CODEC_ID_MUSEPACK8,
  AV_CODEC_ID_SPEEX,
  AV_CODEC_ID_WMAVOICE,
  AV_CODEC_ID_WMAPRO,
  AV_CODEC_ID_WMALOSSLESS,
  AV_CODEC_ID_ATRAC3P,
  AV_CODEC_ID_EAC3,
  AV_CODEC_ID_SIPR,
  AV_CODEC_ID_MP1,
  AV_CODEC_ID_TWINVQ,
  AV_CODEC_ID_TRUEHD,
  AV_CODEC_ID_MP4ALS,
  AV_CODEC_ID_ATRAC1,
  AV_CODEC_ID_BINKAUDIO_RDFT,
  AV_CODEC_ID_BINKAUDIO_DCT,
  AV_CODEC_ID_AAC_LATM,
  AV_CODEC_ID_QDMC,
  AV_CODEC_ID_CELT,
  AV_CODEC_ID_G723_1,
  AV_CODEC_ID_G729,
  AV_CODEC_ID_8SVX_EXP,
  AV_CODEC_ID_8SVX_FIB,
  AV_CODEC_ID_BMV_AUDIO,
  AV_CODEC_ID_RALF,
  AV_CODEC_ID_IAC,
  AV_CODEC_ID_ILBC,
  AV_CODEC_ID_COMFORT_NOISE,
  AV_CODEC_ID_METASOUND,
  AV_CODEC_ID_FFWAVESYNTH,
  AV_CODEC_ID_SONIC,
  AV_CODEC_ID_SONIC_LS,
  AV_CODEC_ID_PAF_AUDIO,
  AV_CODEC_ID_OPUS,
  AV_CODEC_ID_TAK,
  AV_CODEC_ID_EVRC,
  AV_CODEC_ID_SMV,
  AV_CODEC_ID_FIRST_SUBTITLE,
  AV_CODEC_ID_DVD_SUBTITLE,
  AV_CODEC_ID_DVB_SUBTITLE,
  AV_CODEC_ID_TEXT,
  AV_CODEC_ID_XSUB,
  AV_CODEC_ID_SSA,
  AV_CODEC_ID_MOV_TEXT,
  AV_CODEC_ID_HDMV_PGS_SUBTITLE,
  AV_CODEC_ID_DVB_TELETEXT,
  AV_CODEC_ID_SRT,
  AV_CODEC_ID_MICRODVD,
  AV_CODEC_ID_EIA_608,
  AV_CODEC_ID_JACOSUB,
  AV_CODEC_ID_SAMI,
  AV_CODEC_ID_REALTEXT,
  AV_CODEC_ID_SUBVIEWER1,
  AV_CODEC_ID_SUBVIEWER,
  AV_CODEC_ID_SUBRIP,
  AV_CODEC_ID_WEBVTT,
  AV_CODEC_ID_MPL2,
  AV_CODEC_ID_VPLAYER,
  AV_CODEC_ID_PJS,
  AV_CODEC_ID_ASS,
  AV_CODEC_ID_FIRST_UNKNOWN,
  AV_CODEC_ID_TTF,
  AV_CODEC_ID_BINTEXT,
  AV_CODEC_ID_XBIN,
  AV_CODEC_ID_IDF,
  AV_CODEC_ID_OTF,
  AV_CODEC_ID_SMPTE_KLV,
  AV_CODEC_ID_DVD_NAV,
  AV_CODEC_ID_PROBE,
  AV_CODEC_ID_MPEG2TS,
  AV_CODEC_ID_MPEG4SYSTEMS,
  AV_CODEC_ID_FFMETADATA,
  AV_CODEC_ID_NONE
};

//////////////////////////////////////////////////////////////////////////////

CaResampler:: CaResampler(void)
{
  Resample         = NULL  ;
  inputBuffer      = NULL  ;
  outputBuffer     = NULL  ;
  InputSampleRate  = 0     ;
  OutputSampleRate = 0     ;
  autoDeletion     = false ;
}

CaResampler::~CaResampler(void)
{
  if ( NULL != Resample )         {
    ::swr_free ( &Resample )      ;
  }                               ;
  Resample = NULL                 ;
  if ( autoDeletion )             {
    if ( NULL != inputBuffer  )   {
      ::av_freep (&inputBuffer  ) ;
      inputBuffer  = NULL         ;
    }                             ;
    if ( NULL != outputBuffer )   {
      ::av_freep (&outputBuffer ) ;
      outputBuffer = NULL         ;
    }                             ;
  }                               ;
}

int CaResampler::Format(CaSampleFormat format)
{
  switch ( format )          {
    case cafInt8             :
    return AV_SAMPLE_FMT_U8  ;
    case cafUint8            :
    return AV_SAMPLE_FMT_U8  ;
    case cafInt16            :
    return AV_SAMPLE_FMT_S16 ;
    case cafInt32            :
    return AV_SAMPLE_FMT_S32 ;
    case cafFloat32          :
    return AV_SAMPLE_FMT_FLT ;
    case cafFloat64          :
    return AV_SAMPLE_FMT_DBL ;
    default                  :
    break                    ;
  }                          ;
  return AV_SAMPLE_FMT_NONE  ;
}

int CaResampler::Channel(int channels)
{
  if ( channels <= 0 ) return  0                ;
  if ( 1 == channels ) return AV_CH_LAYOUT_MONO ;
  return AV_CH_LAYOUT_STEREO                    ;
}

bool CaResampler::Setup(double         inputSampleRate  ,
                        int            inputChannels    ,
                        CaSampleFormat inputFormat      ,
                        double         outputSampleRate ,
                        int            outputChannels   ,
                        CaSampleFormat outputFormat     )
{
  if ( 0 == inputChannels  ) return false                                    ;
  if ( 0 == outputChannels ) return false                                    ;
  int            inputLayout  = Channel                 ( inputChannels  )   ;
  int            outputLayout = Channel                 ( outputChannels )   ;
  int            inputSample  = (int)inputSampleRate                         ;
  int            outputSample = (int)outputSampleRate                        ;
  AVSampleFormat inputFmt     = (AVSampleFormat)Format  ( inputFormat    )   ;
  AVSampleFormat outputFmt    = (AVSampleFormat)Format  ( outputFormat   )   ;
  if ( AV_SAMPLE_FMT_NONE == inputFmt  ) return false                        ;
  if ( AV_SAMPLE_FMT_NONE == outputFmt ) return false                        ;
  Resample = ::swr_alloc  (                                                ) ;
  ::av_opt_set_int        (Resample,"in_channel_layout" ,inputLayout  , 0  ) ;
  ::av_opt_set_int        (Resample,"out_channel_layout",outputLayout , 0  ) ;
  ::av_opt_set_int        (Resample,"in_sample_rate"    ,inputSample  , 0  ) ;
  ::av_opt_set_int        (Resample,"out_sample_rate"   ,outputSample , 0  ) ;
  ::av_opt_set_sample_fmt (Resample,"in_sample_fmt"     ,inputFmt     , 0  ) ;
  ::av_opt_set_sample_fmt (Resample,"out_sample_fmt"    ,outputFmt    , 0  ) ;
  ::swr_init              (Resample                                        ) ;
  ::av_samples_alloc      ( (uint8_t**)&inputBuffer                          ,
                            NULL                                             ,
                            inputChannels                                    ,
                            inputSample * 5                                  ,
                            inputFmt                                         ,
                            0                                              ) ;
  ::av_samples_alloc      ( (uint8_t**)&outputBuffer                         ,
                            NULL                                             ,
                            outputChannels                                   ,
                            outputSample * 5                                 ,
                            outputFmt                                        ,
                            0                                              ) ;
  autoDeletion     = true                                                    ;
  InputSampleRate  = inputSampleRate                                         ;
  OutputSampleRate = outputSampleRate                                        ;
  return ( NULL != Resample )                                                ;
}

int CaResampler::Convert(int frames)
{
  if ( NULL == Resample     ) return 0 ;
  if ( NULL == inputBuffer  ) return 0 ;
  if ( NULL == outputBuffer ) return 0 ;
  //////////////////////////////////////
  return ::swr_convert                 (
    Resample                           ,
    &outputBuffer                      ,
    ToFrames(frames)                   ,
    (const uint8_t **)&inputBuffer     ,
    frames                           ) ;
}

int CaResampler::ToFrames(int frames)
{
  long long pp = frames       ;
  pp *= (int)OutputSampleRate ;
  pp /= (int)InputSampleRate  ;
  return pp                   ;
}

#endif

//////////////////////////////////////////////////////////////////////////////

#if defined(CAUTILITIES)

#if defined(FFMPEGLIB)

bool MediaPlay(char * filename,int deviceId,int decodeId,Debugger * debugger)
{
  Core c1                                                                    ;
  Core c2                                                                    ;
  c1 . setDebugger ( (Debugger *)debugger )                                  ;
  c2 . setDebugger ( (Debugger *)debugger )                                  ;
  c1 . Initialize  (                      )                                  ;
  c2 . Initialize  (                      )                                  ;
  ////////////////////////////////////////////////////////////////////////////
  if ( decodeId < 0 )                                                        {
    for (int i=0;(decodeId < 0) && i<c1.DeviceCount();i++)                   {
      DeviceInfo * dev                                                       ;
      dev = c1.GetDeviceInfo((CaDeviceIndex)i)                               ;
      if ( NULL != dev && dev->maxInputChannels>0)                           {
        CaHostApiTypeId hid = dev->hostType                                  ;
        if ( FFMPEG == hid ) decodeId = i                                    ;
        if ( VLC    == hid ) decodeId = i                                    ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( decodeId < 0 )                                                        {
    gPrintf ( ( "No media decoder supported\n" ) )                           ;
    c1 . Terminate ( )                                                       ;
    c2 . Terminate ( )                                                       ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Decoder is %d\n" , decodeId ) )                               ;
  ////////////////////////////////////////////////////////////////////////////
  if ( deviceId < 0 ) deviceId = c1 . DefaultOutputDevice ( )                ;
  gPrintf ( ( "Output device is %d\n" , deviceId ) )                         ;
  ////////////////////////////////////////////////////////////////////////////
  StreamParameters INSP ( decodeId )                                         ;
  StreamParameters ONSP                                                      ;
  Stream         * stream   = NULL                                           ;
  Stream         * out      = NULL                                           ;
  BridgeConduit    conduit                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  MediaCodec * mc = INSP . CreateCodec ( )                                   ;
  mc -> setFilename ( filename )                                             ;
  mc -> setInterval ( 100      )                                             ;
  mc -> setWaiting  ( true     )                                             ;
  gPrintf ( ( "MediaCodec created\n" ) )                                     ;
  ////////////////////////////////////////////////////////////////////////////
  CaError rt                                                                 ;
  rt = c1 . Open ( &stream                                                   ,
                   &INSP                                                     ,
                   NULL                                                      ,
                   44100  /* actually, this parameter does nothing */        ,
                   2205   /* actually, this parameter does nothing */        ,
                   0                                                         ,
                   (Conduit *)&conduit                                     ) ;
  if ( ( rt != NoError ) || ( NULL == stream ) )                             {
    gPrintf ( ( "Open media input %s failure\n" , filename ) )               ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Media decoder opened\n" ) )                                   ;
  ////////////////////////////////////////////////////////////////////////////
  int bpf = mc->SampleRate() * mc->BytesPerSample()                          ;
  conduit . setBufferSize ( bpf * 5 , bpf )                                  ;
  gPrintf ( ( "Conduit buffer created\n" ) )                                 ;
  ////////////////////////////////////////////////////////////////////////////
  ONSP . device           = deviceId                                         ;
  ONSP . channelCount     = mc -> Channels     ( )                           ;
  ONSP . sampleFormat     = mc -> SampleFormat ( )                           ;
  ONSP . suggestedLatency = c2.GetDeviceInfo(deviceId)->defaultLowOutputLatency ;
  ONSP . streamInfo       = NULL                                             ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c2. Open ( &out                                                       ,
                  NULL                                                       ,
                  &ONSP                                                      ,
                  mc->SampleRate()                                           ,
                  mc->PeriodSize()                                           ,
                  0                                                          ,
                  (Conduit *)&conduit                                      ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( rt != NoError ) || ( NULL == out ) )                                {
    gPrintf ( ( "Open audio output failure\n" ) )                            ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Audio output device opened\n" ) )                             ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c1 . Start ( stream )                                                 ;
  if ( ( rt != NoError ) )                                                   {
    gPrintf ( ( "Audio input can not start\n" ) )                            ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Decoder started\n" ) )                                        ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c2 . Start ( out    )                                                 ;
  if ( ( rt != NoError ) )                                                   {
    gPrintf ( ( "Audio output can not start\n" ) )                           ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Audio output started\n" ) )                                   ;
  ////////////////////////////////////////////////////////////////////////////
  while ( 1 == c1 . IsActive ( stream )                                     ||
          1 == c2 . IsActive ( out    )                                    ) {
    Timer::Sleep ( 250 )                                                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "Stream stopped\n" ) )                                         ;
  if ( 0 != c1 . IsStopped ( stream ) ) c1 . Stop ( stream )                 ;
  if ( 0 != c2 . IsStopped ( out    ) ) c2 . Stop ( out    )                 ;
  gPrintf ( ( "Stream closing\n" ) )                                         ;
  c1 . Close ( stream )                                                      ;
  c2 . Close ( out    )                                                      ;
  gPrintf ( ( "Stream closed\n" ) )                                          ;
  ////////////////////////////////////////////////////////////////////////////
  c1 . Terminate ( )                                                         ;
  c2 . Terminate ( )                                                         ;
  gPrintf ( ( "Core terminated\n" ) )                                        ;
  ////////////////////////////////////////////////////////////////////////////
  return true                                                                ;
}

#endif

//////////////////////////////////////////////////////////////////////////////

bool Redirect ( int        milliseconds ,
                int        samplerate   ,
                int        channels     ,
                int        inputDevice  ,
                int        outputDevice ,
                int        latency      ,
                Debugger * debugger     )
{
  Core c1                                                                    ;
  Core c2                                                                    ;
  c1 . setDebugger ( (Debugger *)debugger )                                  ;
  c2 . setDebugger ( (Debugger *)debugger )                                  ;
  c1 . Initialize  (                      )                                  ;
  c2 . Initialize  (                      )                                  ;
  ////////////////////////////////////////////////////////////////////////////
  if ( inputDevice  < 0 ) inputDevice  = c1 . DefaultInputDevice  ( )        ;
  if ( outputDevice < 0 ) outputDevice = c1 . DefaultOutputDevice ( )        ;
  ////////////////////////////////////////////////////////////////////////////
  StreamParameters INSP                                                      ;
  StreamParameters ONSP                                                      ;
  Stream         * stream   = NULL                                           ;
  Stream         * out      = NULL                                           ;
  BridgeConduit    conduit                                                   ;
  int              bpf      = 2 * channels * samplerate                      ;
  ////////////////////////////////////////////////////////////////////////////
  conduit . setBufferSize ( bpf * 5 , bpf )                                  ;
  conduit . Input  . BytesPerSample = 2 * channels                           ;
  conduit . Output . BytesPerSample = 2 * channels                           ;
  ////////////////////////////////////////////////////////////////////////////
  INSP . device                    = inputDevice                             ;
  INSP . channelCount              = channels                                ;
  INSP . sampleFormat              = cafInt16                                ;
  INSP . suggestedLatency          = c1.GetDeviceInfo(inputDevice )->defaultLowInputLatency  ;
  INSP . streamInfo = NULL                                    ;
  ////////////////////////////////////////////////////////////////////////////
  ONSP . device                    = outputDevice                            ;
  ONSP . channelCount              = channels                                ;
  ONSP . sampleFormat              = cafInt16                                ;
  ONSP . suggestedLatency          = c2.GetDeviceInfo(outputDevice)->defaultLowOutputLatency ;
  ONSP . streamInfo = NULL                                    ;
  ////////////////////////////////////////////////////////////////////////////
  CaError rt                                                                 ;
  rt = c1 . Open ( &stream                                                   ,
                   &INSP                                                     ,
                   NULL                                                      ,
                   samplerate                                                ,
                   samplerate / 10                                           ,
                   0                                                         ,
                   (Conduit *)&conduit                                     ) ;
  if ( ( rt != NoError ) || ( NULL == stream ) )                             {
    gPrintf ( ( "Open input device %d failure\n" , inputDevice ) )           ;
    return false                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c2. Open ( &out                                                       ,
                  NULL                                                       ,
                  &ONSP                                                      ,
                  samplerate                                                 ,
                  samplerate / 10                                            ,
                  0                                                          ,
                  (Conduit *)&conduit                                      ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( rt != NoError ) || ( NULL == out ) )                                {
    gPrintf ( ( "Open audio output failure\n" ) )                            ;
    return false                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c1 . Start ( stream )                                                 ;
  if ( ( rt != NoError ) )                                                   {
    gPrintf ( ( "Audio input can not start\n" ) )                            ;
    return false                                                             ;
  }                                                                          ;
  if ( latency > 0 ) Timer::Sleep(latency)                                   ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c2 . Start ( out    )                                                 ;
  if ( ( rt != NoError ) )                                                   {
    gPrintf ( ( "Audio output can not start\n" ) )                           ;
    return false                                                             ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  bool   keep = true                                                         ;
  CaTime ST   = Timer::Time()                                                ;
  CaTime LT   = 0                                                            ;
  CaTime DT   = 0                                                            ;
  ST *= 1000                                                                 ;
  while ( keep )                                                             {
    if ( milliseconds > 0 )                                                  {
      keep = false                                                           ;
      if ( 1 == c1 . IsActive ( stream ) ) keep = true                       ;
      if ( 1 == c2 . IsActive ( out    ) ) keep = true                       ;
      if ( keep )                                                            {
        LT  = Timer::Time()                                                  ;
        LT *= 1000                                                           ;
        DT += ( LT - ST )                                                    ;
        ST  = LT                                                             ;
        if ( DT > milliseconds ) keep = false                                ;
      }                                                                      ;
    }                                                                        ;
    Timer::Sleep ( 100 )                                                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 != c1 . IsStopped ( stream ) ) c1 . Stop ( stream )                 ;
  if ( 0 != c2 . IsStopped ( out    ) ) c2 . Stop ( out    )                 ;
  c1 . Close ( stream )                                                      ;
  c2 . Close ( out    )                                                      ;
  ////////////////////////////////////////////////////////////////////////////
  c1 . Terminate ( )                                                         ;
  c2 . Terminate ( )                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  return true                                                                ;
}

//////////////////////////////////////////////////////////////////////////////

#if defined(FFMPEGLIB)

bool MediaRecord (char     * filename     ,
                  int        milliseconds ,
                  int        samplerate   ,
                  int        channels     ,
                  int        inputDevice  ,
                  int        encodeDevice ,
                  int        latency      ,
                  Debugger * debugger     )
{
  Core c1                                                                    ;
  Core c2                                                                    ;
  c1 . setDebugger ( (Debugger *)debugger )                                  ;
  c2 . setDebugger ( (Debugger *)debugger )                                  ;
  c1 . Initialize  (                      )                                  ;
  c2 . Initialize  (                      )                                  ;
  ////////////////////////////////////////////////////////////////////////////
  if ( encodeDevice < 0 )                                                    {
    for (int i=0;(encodeDevice < 0) && i<c1.DeviceCount();i++)               {
      DeviceInfo * dev                                                       ;
      dev = c1.GetDeviceInfo((CaDeviceIndex)i)                               ;
      if ( NULL != dev && dev->maxOutputChannels>0)                          {
        CaHostApiTypeId hid = dev->hostType                                  ;
        if ( FFMPEG == hid ) encodeDevice = i                                ;
        if ( VLC    == hid ) encodeDevice = i                                ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( encodeDevice < 0 )                                                    {
    gPrintf ( ( "No media decoder supported\n" ) )                           ;
    c1 . Terminate ( )                                                       ;
    c2 . Terminate ( )                                                       ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Encoder is %d\n" , encodeDevice ) )                           ;
  ////////////////////////////////////////////////////////////////////////////
  if ( inputDevice < 0 ) inputDevice = c1 . DefaultInputDevice ( )           ;
  gPrintf ( ( "Input device is %d\n" , inputDevice ) )                       ;
  ////////////////////////////////////////////////////////////////////////////
  StreamParameters INSP                                                      ;
  StreamParameters ONSP ( encodeDevice )                                     ;
  Stream         * stream   = NULL                                           ;
  Stream         * out      = NULL                                           ;
  BridgeConduit    conduit                                                   ;
  ////////////////////////////////////////////////////////////////////////////
  MediaCodec * mc = ONSP . CreateCodec ( )                                   ;
  mc -> setFilename     ( filename   )                                       ;
  mc -> setInterval     ( 100        )                                       ;
  mc -> setWaiting      ( true       )                                       ;
  mc -> setChannels     ( channels   )                                       ;
  mc -> setSampleRate   ( samplerate )                                       ;
  mc -> setSampleFormat ( cafInt16   )                                       ;
  gPrintf ( ( "MediaCodec created\n" ) )                                     ;
  ////////////////////////////////////////////////////////////////////////////
  CaError rt                                                                 ;
  ////////////////////////////////////////////////////////////////////////////
  INSP . device                    = inputDevice                             ;
  INSP . channelCount              = channels                                ;
  INSP . sampleFormat              = cafInt16                                ;
  INSP . suggestedLatency          = c1.GetDeviceInfo(inputDevice)->defaultLowInputLatency ;
  INSP . streamInfo = NULL                                    ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c1. Open ( &stream                                                    ,
                  &INSP                                                      ,
                  NULL                                                       ,
                  samplerate                                                 ,
                  samplerate / 10                                            ,
                  0                                                          ,
                  (Conduit *)&conduit                                      ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( rt != NoError ) || ( NULL == stream ) )                             {
    gPrintf ( ( "Open audio input failure\n" ) )                             ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Audio input device opened\n" ) )                              ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c2 . Open ( &out                                                      ,
                   NULL                                                      ,
                   &ONSP                                                     ,
                   samplerate                                                ,
                   samplerate / 10                                           ,
                   0                                                         ,
                   (Conduit *)&conduit                                     ) ;
  if ( ( rt != NoError ) || ( NULL == out ) )                                {
    gPrintf ( ( "Open media encode %s failure\n" , filename ) )              ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Media encoder opened\n" ) )                                   ;
  ////////////////////////////////////////////////////////////////////////////
  int bpf = mc->SampleRate() * mc->BytesPerSample()                          ;
  conduit . setBufferSize ( bpf * 5 , bpf )                                  ;
  conduit . Input  . BytesPerSample = 2 * channels                           ;
  conduit . Output . BytesPerSample = 2 * channels                           ;
  gPrintf ( ( "Conduit buffer created\n" ) )                                 ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c1 . Start ( stream )                                                 ;
  if ( ( rt != NoError ) )                                                   {
    gPrintf ( ( "Audio input can not start\n" ) )                            ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Audio input started\n" ) )                                    ;
  if ( latency > 0 ) Timer::Sleep(latency)                                   ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c2 . Start ( out    )                                                 ;
  if ( ( rt != NoError ) )                                                   {
    gPrintf ( ( "Audio encoder can not start\n" ) )                          ;
    return false                                                             ;
  }                                                                          ;
  gPrintf ( ( "Media encoder started\n" ) )                                  ;
  ////////////////////////////////////////////////////////////////////////////
  bool   keep = true                                                         ;
  CaTime ST   = Timer::Time()                                                ;
  CaTime LT   = 0                                                            ;
  CaTime DT   = 0                                                            ;
  ST *= 1000                                                                 ;
  while ( keep )                                                             {
    if ( milliseconds > 0 )                                                  {
      keep = false                                                           ;
      if ( 1 == c1 . IsActive ( stream ) ) keep = true                       ;
      if ( 1 == c2 . IsActive ( out    ) ) keep = true                       ;
      if ( keep )                                                            {
        LT  = Timer::Time()                                                  ;
        LT *= 1000                                                           ;
        DT += ( LT - ST )                                                    ;
        ST  = LT                                                             ;
        if ( DT > milliseconds ) keep = false                                ;
      }                                                                      ;
    }                                                                        ;
    Timer::Sleep ( 100 )                                                     ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  gPrintf ( ( "Stream stopped\n" ) )                                         ;
  if ( 0 != c1 . IsStopped ( stream ) ) c1 . Stop ( stream )                 ;
  if ( 0 != c2 . IsStopped ( out    ) ) c2 . Stop ( out    )                 ;
  gPrintf ( ( "Stream closing\n"  ) )                                        ;
  c1 . Close ( stream )                                                      ;
  c2 . Close ( out    )                                                      ;
  gPrintf ( ( "Stream closed\n"   ) )                                        ;
  ////////////////////////////////////////////////////////////////////////////
  c1 . Terminate ( )                                                         ;
  c2 . Terminate ( )                                                         ;
  gPrintf ( ( "Core terminated\n" ) )                                        ;
  ////////////////////////////////////////////////////////////////////////////
  return true                                                                ;
}

//////////////////////////////////////////////////////////////////////////////

OffshorePlayer:: OffshorePlayer (void)
               : Thread         (    )
               , filename       (NULL)
               , deviceId       (-1  )
               , decodeId       (-1  )
{
}

OffshorePlayer::~OffshorePlayer (void)
{
}

void OffshorePlayer::run(void)
{
  if ( NULL != filename )                 {
    MediaPlay(filename,deviceId,decodeId) ;
    ::free ( filename )                   ;
    filename = NULL                       ;
  }                                       ;
  delete this                             ;
}

//////////////////////////////////////////////////////////////////////////////

#endif

#endif

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
