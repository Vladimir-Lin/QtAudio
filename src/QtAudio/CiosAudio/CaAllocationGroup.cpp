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

#define CA_INITIAL_LINK_COUNT 16

AllocationGroup:: AllocationGroup (void)
                : linkCount       (0   )
                , linkBlocks      (NULL)
                , spareLinks      (NULL)
                , allocations     (NULL)
{
  AllocationLink * links = allocate ( CA_INITIAL_LINK_COUNT ) ;
  if ( NULL == links ) return                                 ;
  linkCount   = CA_INITIAL_LINK_COUNT                         ;
  linkBlocks  = &links [ 0 ]                                  ;
  spareLinks  = &links [ 1 ]                                  ;
  allocations = NULL                                          ;
}

AllocationGroup::~AllocationGroup (void)
{
  AllocationLink * current = linkBlocks          ;
  AllocationLink * next                          ;
  ////////////////////////////////////////////////
  while ( NULL != current )                      {
    next    = current -> next                    ;
    Allocator :: free ( current -> buffer )      ;
    current = next                               ;
  }                                              ;
  ////////////////////////////////////////////////
  if ( NULL != linkBlocks ) delete [] linkBlocks ;
  ////////////////////////////////////////////////
  linkBlocks  = NULL                             ;
  spareLinks  = NULL                             ;
  allocations = NULL                             ;
}

AllocationLink * AllocationGroup::allocate    (
                   int              count     ,
                   AllocationLink * nextBlock ,
                   AllocationLink * nextSpare )
{
  AllocationLink * result                     ;
  /////////////////////////////////////////////
  result = new AllocationLink [ count ]       ;
  if ( NULL == result ) return NULL           ;
  /////////////////////////////////////////////
  /* the block link                          */
  result   [ 0 ] . buffer = NULL              ;
  result   [ 0 ] . next   = nextBlock         ;
  /* the spare links                         */
  for (int i = 1 ; i < count ; ++i )          {
    result [ i ] . buffer = NULL              ;
    result [ i ] . next   = &result [ i + 1 ] ;
  }                                           ;
  /////////////////////////////////////////////
  result [ count - 1 ] . next = nextSpare     ;
  return result                               ;
}

void * AllocationGroup::alloc(int size)
{
  AllocationLink * link                                      ;
  AllocationLink * links                                     ;
  void           * result = 0                                ;
  /* allocate more links if necessary                       */
  if ( NULL == spareLinks )                                  {
    /* double the link count on each block allocation       */
    links = allocate ( linkCount , linkBlocks , spareLinks ) ;
    if ( NULL != links )                                     {
      linkCount += linkCount                                 ;
      linkBlocks = &links [ 0 ]                              ;
      spareLinks = &links [ 1 ]                              ;
    }                                                        ;
  }                                                          ;
  ////////////////////////////////////////////////////////////
  if ( NULL != spareLinks )                                  {
    result = Allocator :: allocate ( size )                  ;
    if ( NULL != result )                                    {
      link           = spareLinks                            ;
      spareLinks     = link->next                            ;
      link -> buffer = result                                ;
      link -> next   = allocations                           ;
      allocations    = link                                  ;
    }                                                        ;
  }                                                          ;
  ////////////////////////////////////////////////////////////
  return result                                              ;
}

void AllocationGroup::free(void * memory)
{
  if ( NULL == memory ) return             ;
  //////////////////////////////////////////
  AllocationLink * current  = allocations  ;
  AllocationLink * previous = 0            ;
  //////////////////////////////////////////
  /* find the right link and remove it    */
  while ( NULL != current )                {
    if ( current->buffer == memory )       {
      if ( NULL != previous )              {
        previous -> next = current -> next ;
      } else                               {
        allocations      = current -> next ;
      }                                    ;
      current -> buffer = 0                ;
      current -> next   = spareLinks       ;
      spareLinks        = current          ;
      break                                ;
    }                                      ;
    previous = current                     ;
    current  = current -> next             ;
  }                                        ;
  //////////////////////////////////////////
  Allocator :: free ( memory )             ;
}

void AllocationGroup::release(void)
{
  AllocationLink * current  = allocations      ;
  AllocationLink * previous = 0                ;
  /* free all buffers in the allocations list */
  while ( NULL != current )                    {
    Allocator :: free ( current -> buffer )    ;
    current   -> buffer = NULL                 ;
    previous   = current                       ;
    current    = current -> next               ;
  }                                            ;
  /* link the former allocations list onto
   *  the front of the spareLinks list        */
  if ( NULL != previous )                      {
    previous -> next = spareLinks              ;
    spareLinks       = allocations             ;
    allocations      = NULL                    ;
  }                                            ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
