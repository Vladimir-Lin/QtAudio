/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/22                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CaQtWrapper.hpp"

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

QString GetDeviceName(Core & core,DeviceInfo & device)
{
  HostApi         * hostApi = NULL                            ;
  HostApi::Encoding e                                         ;
  QString           s                                         ;
  hostApi = NULL                                              ;
  core . GetHostApi ( &hostApi ,device.hostType)              ;
  if ( NULL != hostApi )                                      {
    e = hostApi->encoding()                                   ;
    if ( HostApi::NATIVE == e )                               {
      QTextCodec * codec = QTextCodec::codecForLocale()       ;
      s = codec->toUnicode(device.name)                       ;
    } else
    if ( HostApi::UTF8   == e )                               {
      QTextCodec * codec = QTextCodec::codecForName("UTF-8" ) ;
      s = codec->toUnicode(device.name)                       ;
    } else
    if ( HostApi::UTF16  == e )                               {
      QTextCodec * codec = QTextCodec::codecForName("UTF-16") ;
      s = codec->toUnicode(device.name)                       ;
    } else s = QString(device.name)                           ;
  } else s = QString(device.name)                             ;
  return s                                                    ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
