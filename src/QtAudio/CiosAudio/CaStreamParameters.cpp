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

#include "CiosAudioPrivate.hpp"

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

typedef struct MediaCodecSpecificStreamInfoHeader {
  unsigned long   size                            ;
  CaHostApiTypeId hostApiType                     ;
  unsigned long   version                         ;
  MediaCodec    * codec                           ;
} MediaCodecSpecificStreamInfoHeader              ;

StreamParameters:: StreamParameters (void)
{
  device           = -1         ;
  channelCount     = 0          ;
  sampleFormat     = cafNothing ;
  suggestedLatency = 0          ;
  streamInfo       = NULL       ;
}

StreamParameters:: StreamParameters (int deviceId)
{
  device           = deviceId ;
  channelCount     = 1        ;
  sampleFormat     = cafInt16 ;
  suggestedLatency = 0        ;
  streamInfo       = NULL     ;
}

StreamParameters::StreamParameters              (
                    int            deviceId     ,
                    int            ChannelCount ,
                    CaSampleFormat SampleFormat )
{
  device           = deviceId     ;
  channelCount     = ChannelCount ;
  sampleFormat     = SampleFormat ;
  suggestedLatency = 0            ;
  streamInfo       = NULL         ;
}

StreamParameters::~StreamParameters (void)
{
}

MediaCodec * StreamParameters::CreateCodec(void)
{
  MediaCodecSpecificStreamInfoHeader * mc                        ;
  mc = new MediaCodecSpecificStreamInfoHeader ( )                ;
  MediaCodec * codec = new MediaCodec ( )                        ;
  mc -> size        = sizeof(MediaCodecSpecificStreamInfoHeader) ;
  mc -> hostApiType = FFMPEG                                     ;
  mc -> version     = 1511                                       ;
  mc -> codec       = codec                                      ;
  streamInfo        = (void *)mc                                 ;
  return codec                                                   ;
}

void StreamParameters::setCodec(CaHostApiTypeId host,MediaCodec * codec)
{
  MediaCodecSpecificStreamInfoHeader * mc                        ;
  mc = new MediaCodecSpecificStreamInfoHeader ( )                ;
  mc -> size        = sizeof(MediaCodecSpecificStreamInfoHeader) ;
  mc -> hostApiType = host                                       ;
  mc -> version     = 1511                                       ;
  mc -> codec       = codec                                      ;
  streamInfo        = (void *)mc                                 ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
