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

Mutex::Mutex(void)
{
  setMutexType(MutexDefault);
}

Mutex::~Mutex(void)
{
  releaseMutex();
}

void Mutex::setMutexType(MutexType MT)
{
  pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER ;
  memcpy(&mutex,&mu,sizeof(pthread_mutex_t))     ;
}

void Mutex::releaseMutex(void)
{
}

int Mutex::lock(void)
{
  return pthread_mutex_lock(&mutex);
}

int Mutex::unlock(void)
{
  return pthread_mutex_unlock(&mutex);
}

int Mutex::locked(void)
{
  return 1;
}

int Mutex::tryLock(void)
{
  return pthread_mutex_trylock(&mutex);
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
