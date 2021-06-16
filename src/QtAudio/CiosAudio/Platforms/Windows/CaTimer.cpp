/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/19                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

//#@ Platform : Windows

#include "CiosAudioPrivate.hpp"

#include <windows.h>
#include <mmsystem.h>

#if (defined(WIN32) && (defined(_MSC_VER) && (_MSC_VER >= 1200))) && !defined(_WIN32_WCE) /* MSC version 6 and above */
#pragma comment( lib, "winmm.lib" )
#endif

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

static int    usePerformanceCounter_ = 0 ;
static double secondsPerTick_        = 0 ;

Timer:: Timer (void)
{
  Initialize ( ) ;
}

Timer::~Timer (void)
{
}

void Timer::Initialize(void)
{
  LARGE_INTEGER ticksPerSecond                                       ;
  ////////////////////////////////////////////////////////////////////
  if ( 0 != ::QueryPerformanceFrequency( &ticksPerSecond ) )         {
    usePerformanceCounter_ = 1                                       ;
    secondsPerTick_        = 1.0 / (double)ticksPerSecond . QuadPart ;
  } else                                                             {
    usePerformanceCounter_ = 0                                       ;
  }                                                                  ;
}

CaTime Timer::Time(void)
{
  LARGE_INTEGER time;
  if ( 1 == usePerformanceCounter_ )       {
    ::QueryPerformanceCounter ( &time )    ;
    return time.QuadPart * secondsPerTick_ ;
  } else                                   {
    return GetTickCount() * 0.001          ;
  }                                        ;
}

void Timer::Sleep(int msec)
{
  ::Sleep ( msec ) ;
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
