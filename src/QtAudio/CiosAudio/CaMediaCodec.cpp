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

MediaCodec:: MediaCodec    (void)
           : MediaFilename (NULL)
           , channels      (0   )
           , samplerate    (0   )
           , intervalTime  (100 )
           , waiting       (true)
           , AudioLength   (0   )
{
}

MediaCodec::~MediaCodec(void)
{
  if ( NULL != MediaFilename ) {
    delete [] MediaFilename    ;
    MediaFilename = NULL       ;
  }                            ;
}

char * MediaCodec::Filename(void)
{
  return MediaFilename ;
}

char * MediaCodec::setFilename(const char * filename)
{
  if ( NULL != MediaFilename ) delete [] MediaFilename ;
  MediaFilename = NULL                                 ;
  if (strlen(filename)>0)                              {
    MediaFilename = strdup(filename)                   ;
  }                                                    ;
  return MediaFilename                                 ;
}

bool MediaCodec::Wait(void)
{
  return waiting ;
}

bool MediaCodec::setWaiting(bool wait)
{
  waiting = wait ;
  return waiting ;
}

int MediaCodec::Interval(void)
{
  return intervalTime ;
}

int MediaCodec::setInterval(int interval)
{
  intervalTime = interval ;
  return intervalTime     ;
}

int MediaCodec::BufferTimeMs(void)
{
  return 5000 ;
}

int MediaCodec::PeriodSize(void)
{
  long long s = samplerate   ;
  s *= Interval ( )          ;
  s /= 1000                  ;
  return s                   ;
}

int MediaCodec::setChannels(int Channels)
{
  channels = Channels ;
  return channels     ;
}

int MediaCodec::Channels(void)
{
  return channels ;
}

int MediaCodec::setSampleRate (int SampleRate)
{
  samplerate = SampleRate ;
  return samplerate       ;
}

int MediaCodec::SampleRate(void)
{
  return samplerate ;
}

CaSampleFormat MediaCodec::setSampleFormat (CaSampleFormat Format)
{
  format = Format ;
  return format   ;
}

CaSampleFormat MediaCodec::SampleFormat(void)
{
  return format ;
}

int MediaCodec::BytesPerSample(void)
{
  return channels * Core::SampleSize(format) ;
}

bool MediaCodec::hasPreparation(void)
{
  return false ;
}

bool MediaCodec::Prepare(void * mediaPacket)
{
  return false ;
}

long long MediaCodec::Length(void)
{
  return AudioLength ;
}

long long MediaCodec::setLength(long long length)
{
  AudioLength = length ;
  return AudioLength   ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
