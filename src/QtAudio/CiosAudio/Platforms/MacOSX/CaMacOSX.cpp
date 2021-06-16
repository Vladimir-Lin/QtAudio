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

#include "CaMacOSX.hpp"

#ifdef ENABLE_HOST_COREAUDIO
#include "CaCoreAudio.hpp"
#endif

#ifdef ENABLE_HOST_OPENAL
#include "CaOpenAL.hpp"
#endif

#ifdef ENABLE_HOST_PULSEAUDIO
#include "CaPulseAudio.hpp"
#endif

#ifdef ENABLE_HOST_VLC
#include "CaVLC.hpp"
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

HostApiInitializer * caMacOSXHostApiInitializers [ ] = {
  #ifdef ENABLE_HOST_COREAUDIO
  CoreAudioInitialize                                  ,
  #endif
  #ifdef ENABLE_HOST_OPENAL
  OpenALInitialize                                     ,
  #endif
  #ifdef ENABLE_HOST_PULSEAUDIO
  PulseAudioInitialize                                 ,
  #endif
  #ifdef ENABLE_HOST_FFMPEG
  FFmpegInitialize                                     ,
  #endif
  #ifdef ENABLE_HOST_VLC
  VlcInitialize                                        ,
  #endif
  #ifdef SkeletonAPI
  SkeletonInitialize                                   ,
  #endif
  NULL                                               } ;

HostApiInitializer ** caHostApiInitializers = caMacOSXHostApiInitializers ;

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
