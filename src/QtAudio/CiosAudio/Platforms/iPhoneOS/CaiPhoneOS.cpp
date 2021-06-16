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

#include "CaiPhoneOS.hpp"

#ifdef ENABLE_HOST_COREAUDIO
#include "CaCoreAudio.hpp"
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

#ifdef ENABLE_HOST_COREAUDIO
#include "CaCoreAudio.hpp"
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

HostApiInitializer * caiPhoneOSHostApiInitializers [ ] = {
  #ifdef ENABLE_HOST_COREAUDIO
  CoreAudioInitialize                                    ,
  #endif
  #ifdef ENABLE_HOST_FFMPEG
  FFmpegInitialize                                       ,
  #endif
  #ifdef SkeletonAPI
  SkeletonInitialize                                     ,
  #endif
  NULL                                                 } ;

HostApiInitializer ** caHostApiInitializers = caiPhoneOSHostApiInitializers ;


#ifdef DONT_USE_NAMESPACE
#else
}
#endif
