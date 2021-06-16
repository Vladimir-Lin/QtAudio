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

static char * emptyErrorString = {0} ;

Stream:: Stream (void)
{
  magic         = STREAM_MAGIC ;
  structVersion = 1            ;
  inputLatency  = 0.0          ;
  outputLatency = 0.0          ;
  sampleRate    = 0.0          ;
  next          = NULL         ;
  conduit       = NULL         ;
  debugger      = NULL         ;
}

Stream::~Stream (void)
{
}

void Stream::Terminate(void)
{
  magic = 0   ;
  delete this ;
}

bool Stream::isValid(void) const
{
  return ( STREAM_MAGIC == magic ) ;
}

bool Stream::isRealTime(void)
{
  return true ;
}

CaError Stream::Error(void)
{
  return NoError ;
}

char * Stream::lastError(void)
{
  return emptyErrorString ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
