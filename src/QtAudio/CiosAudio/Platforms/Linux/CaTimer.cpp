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

//#@ Platform : Linux

#include "CiosAudioPrivate.hpp"

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h> /* For memset */
#include <math.h>
#include <errno.h>

#if defined(__APPLE__) && !defined(HAVE_MACH_ABSOLUTE_TIME)
#define HAVE_MACH_ABSOLUTE_TIME
#endif

#ifdef HAVE_MACH_ABSOLUTE_TIME

#include <mach/mach_time.h>

static double machSecondsConversionScaler_ = 0.0;

#endif

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

Timer:: Timer (void)
{
  Initialize ( ) ;
}

Timer::~Timer (void)
{
}

void Timer::Initialize(void)
{
  #ifdef HAVE_MACH_ABSOLUTE_TIME
  mach_timebase_info_data_t info                     ;
  kern_return_t err = ::mach_timebase_info( &info )  ;
  if ( 0 == err )                                    {
    machSecondsConversionScaler_ = 1e-9 * (double) info.numer / (double) info.denom;
  }                                                  ;
  #endif
}

CaTime Timer::Time(void)
{
  #ifdef HAVE_MACH_ABSOLUTE_TIME
    return ::mach_absolute_time() * machSecondsConversionScaler_ ;
  #elif defined(HAVE_CLOCK_GETTIME)
    struct timespec tp;
    ::clock_gettime(CLOCK_REALTIME, &tp);
    return (PaTime)(tp.tv_sec + tp.tv_nsec * 1e-9);
  #else
  struct timeval tv                             ;
  ::gettimeofday ( &tv, NULL )                  ;
  return (double) tv.tv_usec * 1e-6 + tv.tv_sec ;
  #endif
}

void Timer::Sleep(int msec)
{
  #ifdef HAVE_NANOSLEEP
  struct timespec req = {0}, rem = {0}             ;
  PaTime time = msec / 1.e3                        ;
  req.tv_sec = (time_t)time                        ;
  assert ( time - req.tv_sec < 1.0 )               ;
  req.tv_nsec = (long)((time - req.tv_sec) * 1.e9) ;
  ::nanosleep ( &req , &rem )                      ;
  /* XXX: Try sleeping the remaining time (contained in rem) if interrupted by a signal? */
  #else
  while ( msec > 999 )                             {
    ::usleep( 999000 )                             ;
    msec -= 999                                    ;
  }                                                ;
  ::usleep ( msec * 1000 )                         ;
  #endif
}

void Timer::Anchor(CaTime latency)
{
  TimeAt      = Time ( )          ;
  TimeAnchor  = TimeAt + latency  ;
}

void Timer::Anchor(CaTime latency,double divided)
{
  latency    /= divided           ;
  TimeAt      = Time ( )          ;
  TimeAnchor  = TimeAt + latency  ;
}

bool Timer::isArrival(void)
{
  CaTime T = Time()                 ;
  if ( T > TimeAnchor ) return true ;
  return ( TimeAt > T )             ;
}

CaTime Timer::dT(double multiply)
{
  return ( TimeAnchor - Time() ) * multiply ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
