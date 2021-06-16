#include "CiosAudio.hpp"

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

ThreadData:: ThreadData  ( void  )
              : Id          ( 0     )
              , Type        ( 0     )
              , Priority    ( 0     )
              , Status      ( 0     )
              , Running     ( Idle  )
              , StackSize   ( 0     )
              , Reservation ( false )
              , Arguments   ( NULL  )
              , Extra       ( NULL  )
{
}

ThreadData::~ThreadData (void)
{
}

void ThreadData::Start(void)
{
  Running = Active ;
}

void ThreadData::Stop(void)
{
  Running = Deactive ;
}

void ThreadData::Join(void)
{
  if ( Deactive == Running ) return ;
  pthread_join ( Thread , NULL )    ;
  Running = Recycle                 ;
}

int ThreadData::setPriority(int priority)
{
  int         policy                           ;
  sched_param param                            ;
  pthread_getschedparam(Thread,&policy,&param) ;
  Priority             = priority              ;
  param.sched_priority = priority              ;
  pthread_setschedparam(Thread, policy,&param) ;
  return Priority                              ;
}

pthread_t ThreadData::ThreadId(void)
{
  return pthread_self() ;
}

bool ThreadData::isSelf(void)
{
  return ( ThreadId() == Thread ) ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
