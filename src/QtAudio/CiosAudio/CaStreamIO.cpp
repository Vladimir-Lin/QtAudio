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

StreamIO:: StreamIO (void)
{
  Reset ( ) ;
}

StreamIO::~StreamIO (void)
{
}

void StreamIO::Reset(void)
{
  Buffer         = NULL      ;
  BytesPerSample = 0         ;
  FrameCount     = 0         ;
  MaxSize        = 0         ;
  StatusFlags    = 0         ;
  CurrentTime    = 0         ;
  AdcDac         = 0         ;
  Situation      = Stagnated ;
  Result         = 0         ;
}

long long StreamIO::Total(void)
{
  return ( BytesPerSample * FrameCount ) ;
}

bool StreamIO::isNull(void)
{
  return ( NULL == Buffer ) ;
}

int StreamIO::setSample(CaSampleFormat format,int channels)
{
  BytesPerSample = Core::SampleSize(format) * channels ;
  return BytesPerSample                                ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
