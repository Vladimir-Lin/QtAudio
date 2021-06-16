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
  Running = Active   ;
}

void ThreadData::Stop(void)
{
  Running = Deactive ;
}

void ThreadData::Join(void)
{
  if ( Deactive == Running ) return           ;
  ::WaitForSingleObject ( Thread , INFINITE ) ;
  Running = Recycle                           ;
}

int ThreadData::setPriority(int priority)
{
  Priority = priority                        ;
  ::SetThreadPriority  ( Thread , priority ) ;
  return priority                            ;
}

HANDLE ThreadData::ThreadId(void)
{
  return (HANDLE)::GetCurrentThreadId() ;
}

bool ThreadData::isSelf(void)
{
  return ( ThreadId() == Thread ) ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
