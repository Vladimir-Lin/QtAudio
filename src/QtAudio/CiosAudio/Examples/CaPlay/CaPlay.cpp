/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 *                           Example code : CaPlay                           *
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

#include "CiosAudio.hpp"

void ListInputDevices  (bool debug = false) ;
void ListOutputDevices (bool debug = false) ;
void ListDecoders      (bool debug = false) ;
void ListEncoders      (bool debug = false) ;
void ListHostAPIs      (bool debug = false) ;
int  Diversion         (int argc,char ** argv) ;
int  Recorder          (int argc,char ** argv) ;

int main(int argc,char ** argv)
{
  if ( argc < 2 )                                                            {
    CiosAudio::Core core                                                     ;
    printf("%s Player\n",core.VersionString()                           )    ;
    printf("CaPlay --list-input   : List input  devices\n"              )    ;
    printf("CaPlay --list-output  : List output devices\n"              )    ;
    printf("CaPlay --list-hostapi : List Host APIs\n"                   )    ;
    printf("CaPlay --list-decoder : List FFmpeg decoders\n"             )    ;
    printf("CaPlay --list-encoder : List FFmpeg encoders\n"             )    ;
    printf("\n"                                                         )    ;
    printf("Play media files\n"                                         )    ;
    printf("CaPlay [options] audio-media.mp3(*.wav,...) [options] media files ...\n") ;
    printf("options:\n"                                                 )    ;
    printf("--output deviceId  : assign Device ID\n"                    )    ;
    printf("--decode decoderId : assign Media Decoder ID\n"             )    ;
    printf("--debug            : debug the process\n"                   )    ;
    printf("\n"                                                         )    ;
    printf("Redirect device I/O (Such as MIC to Speaker)\n"             )    ;
    printf("CaPlay --redirect [options] \n"                             )    ;
    printf("options:\n"                                                 )    ;
    printf("--time milliseconds         : redirect milliseconds, default forever\n" ) ;
    printf("--samplerate 8000(48000...) : assign sample rate\n"         )    ;
    printf("--channels 1(2...)          : assign channels\n"            )    ;
    printf("--output Output Device Id   : assign Output Device ID\n"    )    ;
    printf("--input Input Device Id     : assign Input Device ID\n"     )    ;
    printf("--latency milliseconds(10)  : assign Output Latency\n"      )    ;
    printf("--debug                     : debug the process\n"          )    ;
    printf("\n"                                                         )    ;
    printf("Record audio input into encoded media file\n"               )    ;
    printf("CaPlay --record [options] file1 [options] file2 ...\n"      )    ;
    printf("options:\n"                                                 )    ;
    printf("--time milliseconds         : redirect milliseconds, default forever\n" ) ;
    printf("--samplerate 8000(48000...) : assign sample rate\n"         )    ;
    printf("--channels 1(2...)          : assign channels\n"            )    ;
    printf("--output Encode Device Id   : assign Encoder Device ID\n"   )    ;
    printf("--input Input Device Id     : assign Input Device ID\n"     )    ;
    printf("--latency milliseconds(100) : assign Output Latency\n"      )    ;
    printf("--debug                     : debug the process\n"          )    ;
    printf("\n"                                                         )    ;
    return 0                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == strcmp(argv[1],"--list-input") )                                 {
    bool debug = false                                                       ;
    for (int i=2;i<argc;i++) if (0==strcmp(argv[i],"--debug")) debug = true  ;
    ListInputDevices  ( debug )                                              ;
    return 1                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == strcmp(argv[1],"--list-output") )                                {
    bool debug = false                                                       ;
    for (int i=2;i<argc;i++) if (0==strcmp(argv[i],"--debug")) debug = true  ;
    ListOutputDevices ( debug )                                              ;
    return 1                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == strcmp(argv[1],"--list-decoder") )                               {
    bool debug = false                                                       ;
    for (int i=2;i<argc;i++) if (0==strcmp(argv[i],"--debug")) debug = true  ;
    ListDecoders      ( debug )                                              ;
    return 1                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == strcmp(argv[1],"--list-encoder") )                               {
    bool debug = false                                                       ;
    for (int i=2;i<argc;i++) if (0==strcmp(argv[i],"--debug")) debug = true  ;
    ListEncoders      ( debug )                                              ;
    return 1                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == strcmp(argv[1],"--list-hostapi") )                               {
    bool debug = false                                                       ;
    for (int i=2;i<argc;i++) if (0==strcmp(argv[i],"--debug")) debug = true  ;
    ListHostAPIs      ( debug )                                              ;
    return 1                                                                 ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == strcmp(argv[1],"--redirect"    ) )                               {
    return Diversion ( argc , argv )                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( 0 == strcmp(argv[1],"--record"    ) )                                 {
    return Recorder  ( argc , argv )                                         ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  int                   at       =  1                                        ;
  int                   deviceId = -1                                        ;
  int                   decodeId = -1                                        ;
  bool                  debug    = false                                     ;
  CiosAudio::Debugger * debugger = NULL                                      ;
  while ( at < argc )                                                        {
    if ( 0 == strcmp(argv[at],"--output") )                                  {
      at++                                                                   ;
      deviceId = atoi(argv[at])                                              ;
      at++                                                                   ;
    } else
    if ( 0 == strcmp(argv[at],"--decode") )                                  {
      at++                                                                   ;
      decodeId = atoi(argv[at])                                              ;
      at++                                                                   ;
    } else
    if ( 0 == strcmp(argv[at],"--debug" ) )                                  {
      at++                                                                   ;
      debug = true                                                           ;
    } else                                                                   {
      if ( debug && ( NULL == debugger ) )                                   {
        debugger = new CiosAudio::Debugger ( )                               ;
      }                                                                      ;
      CiosAudio::MediaPlay ( argv [ at ] , deviceId , decodeId , debugger )  ;
      at++                                                                   ;
    }                                                                        ;
  }                                                                          ;
  if ( NULL != debugger ) delete debugger                                    ;
  ////////////////////////////////////////////////////////////////////////////
  return 1                                                                   ;
}

using namespace CiosAudio ;

void ListOutputDevices(bool debug)
{
  Core core                                            ;
  if ( debug ) core . setDebugger ( new Debugger ( ) ) ;
  core . Initialize ( )                                ;
  for (int i=0;i<core.DeviceCount();i++)               {
    DeviceInfo * dev                                   ;
    dev = core.GetDeviceInfo((CaDeviceIndex)i)         ;
    if ( NULL != dev && dev->maxOutputChannels>0)      {
      printf("%4d : %s\n",i,dev->name)                 ;
    }                                                  ;
  }                                                    ;
  core . Terminate  ( )                                ;
}

void ListInputDevices(bool debug)
{
  Core core                                            ;
  if ( debug ) core . setDebugger ( new Debugger ( ) ) ;
  core . Initialize ( )                                ;
  for (int i=0;i<core.DeviceCount();i++)               {
    DeviceInfo * dev                                   ;
    dev = core.GetDeviceInfo((CaDeviceIndex)i)         ;
    if ( NULL != dev && dev->maxInputChannels>0)       {
      printf("%4d : %s\n",i,dev->name)                 ;
    }                                                  ;
  }                                                    ;
  core . Terminate  ( )                                ;
}

void ListDecoders(bool debug)
{
  #if defined(FFMPEGLIB)
  Core core                                                             ;
  int             i     = 0                                             ;
  AVCodecID       id                                                    ;
  AVCodec       * codec = NULL                                          ;
  while ( AV_CODEC_ID_NONE != AllFFmpegCodecIDs[i] )                    {
    id = AllFFmpegCodecIDs[i]                                           ;
    codec = ::avcodec_find_encoder(id)                                  ;
    if ( NULL != codec )                                                {
      if ( AVMEDIA_TYPE_AUDIO == ( codec->type & AVMEDIA_TYPE_AUDIO ) ) {
        printf("[%4X] %s\n",id,codec->long_name)                        ;
      }                                                                 ;
    }                                                                   ;
    i++                                                                 ;
  }                                                                     ;
  #else
  printf("FFmpeg is not supported.\n")                                  ;
  #endif
}

void ListEncoders(bool debug)
{
  #if defined(FFMPEGLIB)
  Core core                                                             ;
  int             i     = 0                                             ;
  AVCodecID       id                                                    ;
  AVCodec       * codec = NULL                                          ;
  while ( AV_CODEC_ID_NONE != AllFFmpegCodecIDs[i] )                    {
    id = AllFFmpegCodecIDs[i]                                           ;
    codec = ::avcodec_find_decoder(id)                                  ;
    if ( NULL != codec )                                                {
      if ( AVMEDIA_TYPE_AUDIO == ( codec->type & AVMEDIA_TYPE_AUDIO ) ) {
        printf("[%4X] %s\n",id,codec->long_name)                        ;
      }                                                                 ;
    }                                                                   ;
    i++                                                                 ;
  }                                                                     ;
  #else
  printf("FFmpeg is not supported.\n")                                  ;
  #endif
}

void ListHostAPIs(bool debug)
{
  CiosAudio::Core core                                          ;
  if ( debug ) core . setDebugger ( new Debugger ( ) )          ;
  core . Initialize ( )                                         ;
  for (int i=0;i<core.HostApiCount();i++)                       {
    CiosAudio::HostApiInfo * hai = core.GetHostApiInfo(i)       ;
    if ( NULL != hai )                                          {
      printf ( "%4d [%4d] : %s\n" , i , hai->type , hai->name ) ;
    }                                                           ;
  }                                                             ;
  core . Terminate  ( )                                         ;
}

int Diversion(int argc,char ** argv)
{
  int        ms         = -1                                                 ;
  int        id         = -1                                                 ;
  int        od         = -1                                                 ;
  int        i          = 2                                                  ;
  int        samplerate = 8000                                               ;
  int        channels   = 1                                                  ;
  int        latency    = 10                                                 ;
  Debugger * debugger   = NULL                                               ;
  ////////////////////////////////////////////////////////////////////////////
  while ( i < argc )                                                         {
    if ( 0 == strcmp(argv[i],"--time") )                                     {
      i++                                                                    ;
      ms         = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--samplerate") )                               {
      i++                                                                    ;
      samplerate = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--channels") )                                 {
      i++                                                                    ;
      channels   = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--latency") )                                  {
      i++                                                                    ;
      latency    = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--output") )                                   {
      i++                                                                    ;
      od         = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--input") )                                    {
      i++                                                                    ;
      id         = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--debug" ) )                                   {
      if ( NULL == debugger )                                                {
        debugger = new Debugger ( )                                          ;
      }                                                                      ;
      i++                                                                    ;
    }                                                                        ;
    i++                                                                      ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if (Redirect(ms,samplerate,channels,id,od,latency,debugger))               {
    if ( NULL != debugger ) delete debugger                                  ;
    return 1                                                                 ;
  }                                                                          ;
  if ( NULL != debugger ) delete debugger                                    ;
  ////////////////////////////////////////////////////////////////////////////
  return 0                                                                   ;
}

int Recorder(int argc,char ** argv)
{
  int        ms         = 60000                                              ;
  int        id         = -1                                                 ;
  int        od         = -1                                                 ;
  int        i          = 2                                                  ;
  int        samplerate = 8000                                               ;
  int        channels   = 1                                                  ;
  int        latency    = 500                                                ;
  Debugger * debugger   = NULL                                               ;
  ////////////////////////////////////////////////////////////////////////////
  while ( i < argc )                                                         {
    if ( 0 == strcmp(argv[i],"--time") )                                     {
      i++                                                                    ;
      ms         = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--samplerate") )                               {
      i++                                                                    ;
      samplerate = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--channels") )                                 {
      i++                                                                    ;
      channels   = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--latency") )                                  {
      i++                                                                    ;
      latency    = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--output") )                                   {
      i++                                                                    ;
      od         = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--input") )                                    {
      i++                                                                    ;
      id         = atoi(argv[i])                                             ;
      i++                                                                    ;
    } else
    if ( 0 == strcmp(argv[i],"--debug" ) )                                   {
      if ( NULL == debugger )                                                {
        debugger = new Debugger ( )                                          ;
      }                                                                      ;
      i++                                                                    ;
    } else                                                                   {
      char * filename = argv [ i ]                                           ;
      i++                                                                    ;
      MediaRecord(filename,ms,samplerate,channels,id,od,latency,debugger)    ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != debugger ) delete debugger                                    ;
  ////////////////////////////////////////////////////////////////////////////
  return 1                                                                   ;
}
