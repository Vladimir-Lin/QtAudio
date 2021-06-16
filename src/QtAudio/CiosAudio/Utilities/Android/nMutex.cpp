#include "CiosAudio.hpp"

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
