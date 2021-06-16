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

Thread:: Thread      (int stackSize,bool reservation)
       : running     (0                             )
       , StackSize   (stackSize                     )
       , Reservation (reservation                   )
{
}

Thread:: Thread      ( void             )
       : running     ( 0                )
       , StackSize   ( 16 * 1024 * 1024 )
       , Reservation ( false            )
{
}

Thread::~Thread(void)
{
}

int Thread::setPriority(int priority)
{
  return Data.setPriority(priority) ;
}

void Thread::start(void)
{
  if ( 1 == running ) return            ;
  ///////////////////////////////////////
  while (!recycle()) Timer::Sleep(10)   ;
  ///////////////////////////////////////
  Data.Function = Thread::SingleThread  ;
  int th_retcode                        ;
  do                                    {
    th_retcode = pthread_create         (
                   &Data.Thread         ,
                   NULL                 ,
                   Data.Function        ,
                   (void *)this      )  ;
    if (th_retcode!=0) Timer::Sleep(10) ;
  } while (th_retcode!=0)               ;
  Data.Status = th_retcode              ;
}

ThreadData * Thread::start(int Type)
{
  return start(Type,Thread::MultiThread) ;
}

ThreadData * Thread::start(int Type,char * arguments)
{
  cleanup ( )                                           ;
  ThreadData * data = new ThreadData    ( )             ;
  ThreadData * last = AllThreads . back ( )             ;
  ///////////////////////////////////////////////////////
  data->Running      = 0                                ;
  data->Type         = Type                             ;
  data->Function     = Thread::MultiThread              ;
  data->Arguments    = arguments                        ;
  data->Extra        = this                             ;
  data->StackSize    = StackSize                        ;
  data->Reservation  = Reservation                      ;
  ///////////////////////////////////////////////////////
  if ( NULL == last )                                   {
    data->Id = last->Id + 1                             ;
  } else data->Id = 1                                   ;
  ///////////////////////////////////////////////////////
  int th_retcode                                        ;
  do                                                    {
    th_retcode = pthread_create                         (
                   &data->Thread                        ,
                   NULL                                 ,
                   data->Function                       ,
                   (void *)data                       ) ;
    if (th_retcode!=0) Timer::Sleep(10)                 ;
  } while (th_retcode!=0)                               ;
  data->Status = th_retcode                             ;
  ///////////////////////////////////////////////////////
  AllThreads . push_back ( data )                       ;
  ///////////////////////////////////////////////////////
  return data                                           ;
}

ThreadData * Thread::start(int Type,ThreadFunction ExternalThread)
{
  cleanup ( )                                           ;
  ThreadData * data = new ThreadData ()                 ;
  ThreadData * last = AllThreads.back()                 ;
  ///////////////////////////////////////////////////////
  data->Running      = 0                                ;
  data->Type         = Type                             ;
  data->Function     = ExternalThread                   ;
  data->Extra        = this                             ;
  data->StackSize    = StackSize                        ;
  data->Reservation  = Reservation                      ;
  ///////////////////////////////////////////////////////
  if ( NULL != last )                                   {
    data->Id = last->Id + 1                             ;
  } else data->Id = 1                                   ;
  ///////////////////////////////////////////////////////
  int th_retcode                                        ;
  do                                                    {
    th_retcode = pthread_create                         (
                   &data->Thread                        ,
                   NULL                                 ,
                   data->Function                       ,
                   (void *)data                       ) ;
    if (th_retcode!=0) Timer::Sleep(10)                 ;
  } while (th_retcode!=0)                               ;
  data->Status = th_retcode                             ;
  ///////////////////////////////////////////////////////
  AllThreads . push_back ( data )                       ;
  ///////////////////////////////////////////////////////
  return data                                           ;
}

void * Thread::SingleThread(void * arg)
{
  Thread * nthread  = (Thread *)arg ;
  srand (time(NULL))                      ;
  srand (rand(    ))                      ;
  nthread -> running = 1                  ;
  nthread -> actualRun ( )                ;
  nthread -> running = 2                  ;
  pthread_exit(nthread)                   ;
  return NULL                             ;
}

void * Thread::MultiThread(void * arg)
{
  ThreadData * data    = (ThreadData *)arg         ;
  Thread     * nthread = (Thread     *)data->Extra ;
  srand (time(NULL))                                     ;
  srand (rand(    ))                                     ;
  nthread -> running    = 1                              ;
  nthread -> actualRun ( data->Type , data )             ;
  nthread -> running    = 2                              ;
  pthread_exit(nthread)                                  ;
  return NULL                                            ;
}

void Thread::quit(void)
{
}

void Thread::suspend(void)
{
}

void Thread::resume(void)
{
}

void Thread::terminate(void)
{
  pthread_join ( Data.Thread , NULL ) ;
}

bool Thread::proceed(void)
{
  return true ;
}

int Thread::exit(int exitcode)
{
  pthread_exit ( NULL ) ;
}

void Thread::actualRun(void)
{
  run();
}

void Thread::actualRun(int Type,ThreadData * data)
{
  run ( Type , data ) ;
}

void Thread::ThreadEvent(void)
{
}

void Thread::run(void)
{
}

void Thread::run(int Type,ThreadData * data)
{
}

void Thread::actualJoin(void)
{
  pthread_join(Data.Thread,NULL) ;
  running = 3                    ;
}

bool Thread::detection(void)
{
  if ( finalize ( 5 ) ) return false ;
  if ( 0 == running   ) return false ;
  return   ( 1 == running )          ;
}

bool Thread::recycle(void)
{
  if (running==0) return true  ;
  if (running==1) return false ;
  if (running==2) actualJoin() ;
  if (running==3) running = 0  ;
  return (running==0)          ;
}

void Thread::DeleteThread(ThreadData * data)
{
  for (std::list<ThreadData *>::iterator i = AllThreads.begin() ;
       i!=AllThreads.end()                                      ;
       i++                                                    ) {
    ThreadData * d = (*i)                                       ;
    if ( d == data )                                            {
      AllThreads . erase ( i )                                  ;
      delete data                                               ;
      return                                                    ;
    }                                                           ;
  }                                                             ;
}

void Thread::cleanup(void)
{
  std::list<ThreadData *> Dead                                  ;
  for (std::list<ThreadData *>::iterator i = AllThreads.begin() ;
       i!=AllThreads.end()                                      ;
       i++                                                    ) {
    if ( 2 == ((*i)->Running))                                  {
      Dead.push_back(*i)                                        ;
    }                                                           ;
  }                                                             ;
  ///////////////////////////////////////////////////////////////
  if (Dead.size()<=0) return                                    ;
  for (std::list<ThreadData *>::iterator i = Dead.begin()       ;
       i!=Dead.end()                                            ;
       i++                                                    ) {
    ThreadData * data = *i                                      ;
    pthread_join(data->Thread,NULL)                             ;
    DeleteThread ( data )                                       ;
    /////////////////////////////////////////////////////////////
  }                                                             ;
}

bool Thread::finalize(int interval)
{
  while (running!=0 || AllThreads.size()>0)     {
    switch (running)                            {
      case 0                                    :
      case 1                                    :
      break                                     ;
      case 2                                    :
        pthread_join(Data.Thread,NULL)          ;
        running = 3                             ;
      break                                     ;
      case 3                                    :
        running = 0                             ;
      break                                     ;
    }                                           ;
    if (AllThreads.size()>0)                    {
      ThreadData * data = AllThreads.front()    ;
      if ( NULL != data )                       {
        switch (data->Running)                  {
          case 0                                :
          case 1                                :
          break                                 ;
          case 2                                :
            pthread_join(data->Thread,NULL)     ;
            data->Running = 3                   ;
          break                                 ;
          case 3                                :
            data->Running = 0                   ;
            DeleteThread ( data )               ;
          break                                 ;
        }                                       ;
      }                                         ;
    }                                           ;
    Timer::Sleep ( interval )                   ;
    ThreadEvent  (          )                   ;
  }                                             ;
  return true                                   ;
}

pthread_t Thread::Id(void)
{
  return Data.Thread ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
