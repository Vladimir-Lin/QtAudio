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

DeviceInfo:: DeviceInfo(void)
{
  structVersion            = 2           ;
  name                     = NULL        ;
  hostApi                  = -1          ;
  hostType                 = Development ;
  maxInputChannels         = 0           ;
  maxOutputChannels        = 0           ;
  defaultLowInputLatency   = 0           ;
  defaultLowOutputLatency  = 0           ;
  defaultHighInputLatency  = 0           ;
  defaultHighOutputLatency = 0           ;
  defaultSampleRate        = 0           ;
}

DeviceInfo::~DeviceInfo(void)
{
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
