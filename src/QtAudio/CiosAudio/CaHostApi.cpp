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

HostApi:: HostApi(void)
{
  baseDeviceIndex    = -1             ;
  info.structVersion = -1             ;
  deviceInfos        = NULL           ;
  debugger           = globalDebugger ;
}

HostApi::~HostApi(void)
{
}

void HostApi::setDebugger (Debugger * debug)
{
  debugger = debug ;
}

CaError HostApi::ToHostDeviceIndex      (
          CaDeviceIndex * hostApiDevice ,
          CaDeviceIndex   device        )
{
  CaError       result                       ;
  CaDeviceIndex x = device - baseDeviceIndex ;
  ////////////////////////////////////////////
  if ( ( x <  0                )            ||
       ( x >= info.deviceCount )           ) {
    result = InvalidDevice                   ;
  } else                                     {
    *hostApiDevice = x                       ;
    result         = NoError                 ;
  }                                          ;
  ////////////////////////////////////////////
  return result                              ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
