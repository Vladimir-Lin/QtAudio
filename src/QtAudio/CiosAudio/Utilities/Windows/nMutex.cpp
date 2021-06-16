/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
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

Mutex::Mutex(void)
{
  setMutexType ( MutexDefault ) ;
  Locked = 0                    ;
}

Mutex::~Mutex(void)
{
  releaseMutex();
}

void Mutex::setMutexType(MutexType MT)
{
  ::InitializeCriticalSection(&mutex) ;
}

void Mutex::releaseMutex(void)
{
  ::DeleteCriticalSection(&mutex) ;
}

int Mutex::lock(void)
{
  ::EnterCriticalSection(&mutex) ;
  Locked++                       ;
  return 1                       ;
}

int Mutex::unlock(void)
{
  ::LeaveCriticalSection(&mutex) ;
    if (Locked>0) Locked--       ;
  return 1                       ;
}

int Mutex::locked(void)
{
  return Locked;
}

int Mutex::tryLock(void)
{
  if ( Locked > 0 ) return 0 ;
  return lock ()             ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
