//#@ Platform : Solaris

#include "CiosAudioPrivate.hpp"

#ifdef DONT_USE_NAMESPACE
#else
namespace CiosAudio
{
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
  void * result                            ;
  result = (void *)::malloc ( size )       ;
  if ( NULL != result ) ++ numAllocations_ ;
  return result                            ;
}

void Allocator::free(void * block)
{
  if ( NULL == block ) return ;
  ::free ( block )            ;
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
