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

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

static int numAllocations_ = 0 ;

Allocator:: Allocator (void)
{
}

Allocator::~Allocator (void)
{
}

void * Allocator::allocate(long size)
{
  void * result                                ;
  result = (void *)::GlobalAlloc( GPTR, size ) ;
  if ( NULL != result ) ++ numAllocations_     ;
  return result                                ;
}

void Allocator::free(void * block)
{
  if ( NULL == block ) return ;
  ::GlobalFree ( block )      ;
  -- numAllocations_          ;
}

int Allocator::blocks(void)
{
  return numAllocations_ ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
