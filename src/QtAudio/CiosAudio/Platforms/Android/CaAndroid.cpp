/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2015/02/11                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CaAndroid.hpp"

#ifdef ENABLE_HOST_ALSA
#include "CaALSA.hpp"
#endif

#ifdef ENABLE_HOST_ALSA
#include "CaOSS.hpp"
#endif

#ifdef ENABLE_HOST_FFMPEG
#include "CaFFmpeg.hpp"
#endif

#ifdef SkeletonAPI
#include "CaSkeleton.hpp"
#endif

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

HostApiInitializer * caLinuxHostApiInitializers [ ] = {
  #ifdef ENABLE_HOST_ALSA
  AlsaInitialize                                      ,
  #endif
  #ifdef ENABLE_HOST_OSS
  OssInitialize                                       ,
  #endif
  #ifdef ENABLE_HOST_FFMPEG
  FFmpegInitialize                                    ,
  #endif
  #ifdef SkeletonAPI
  SkeletonInitialize                                  ,
  #endif
  NULL                                              } ;

HostApiInitializer ** caHostApiInitializers = caLinuxHostApiInitializers ;

pthread_t caUnixMainThread = 0 ;

CaError CaUnixThreadingInitialize(void)
{
  caUnixMainThread = pthread_self() ;
  return NoError                    ;
}

CaError CaCancelThreading(pthread_t * threading,int wait,CaError * exitResult)
{
  CaError result = NoError                                                   ;
  void  * pret                                                               ;
  ////////////////////////////////////////////////////////////////////////////
  if ( NULL != exitResult ) *exitResult = NoError                            ;
//  if ( !wait ) ::pthread_cancel ( *threading )                               ;
  ::pthread_join( *threading , &pret )                                       ;
  ////////////////////////////////////////////////////////////////////////////
//  if ( ( NULL != pret ) && ( pret != PTHREAD_CANCELED ) )                    {
//    if ( NULL != exitResult ) *exitResult = *(CaError *) pret                ;
//    ::free ( pret )                                                          ;
//  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
