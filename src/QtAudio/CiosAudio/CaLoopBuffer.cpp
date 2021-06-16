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

LoopBuffer:: LoopBuffer (void)
           : buffer     (NULL)
           , Size       (0   )
           , Margin     (0   )
           , Start      (0   )
           , Tail       (0   )
{
}


LoopBuffer:: LoopBuffer (int size,int margin     )
           : buffer     (new unsigned char [size])
           , Size       (size                    )
           , Margin     (margin                  )
           , Start      (0                       )
           , Tail       (0                       )
{
}

LoopBuffer::~LoopBuffer (void)
{
  if ( NULL != buffer ) {
    delete [] buffer    ;
    buffer = NULL       ;
  }                     ;
}

int LoopBuffer::size(void) const
{
  return Size ;
}

int LoopBuffer::margin(void) const
{
  return Margin ;
}

int LoopBuffer::start(void) const
{
  return Start ;
}

int LoopBuffer::tail(void) const
{
  return Tail ;
}

int LoopBuffer::setBufferSize (int size)
{
  Size = size                       ;
  if ( NULL != buffer )             {
    delete [] buffer                ;
  }                                 ;
  buffer = new unsigned char [size] ;
  memset ( buffer , 0 , size )      ;
  return Size                       ;
}

int LoopBuffer::setMargin(int margin)
{
  Margin = margin ;
  return Margin   ;
}

void LoopBuffer::reset(void)
{
  Start = 0 ;
  Tail  = 0 ;
}

int LoopBuffer::available(void)
{
  if ( Start == Tail  ) return 0            ;
  if ( Tail  >  Start ) return Tail - Start ;
  return ( Tail + Size - Start )            ;
}

bool LoopBuffer::isEmpty(void)
{
  return ( Start == Tail ) ;
}

bool LoopBuffer::isFull(void)
{
  if ( Start == Tail ) return false ;
  int total = 0                     ;
  if ( Tail > Start )               {
    total  = Tail - Start           ;
  } else                            {
    total  = Size - Start           ;
    total += Tail                   ;
  }                                 ;
  total += Margin                   ;
  return ( total > Size )           ;
}

int LoopBuffer::put(void * data,int length)
{
  int             NextEnd = Tail + length           ;
  unsigned char * p       = (unsigned char *)buffer ;
  unsigned char * s       = (unsigned char *)data   ;
  p += Tail                                         ;
  if ( NextEnd > Size )                             {
    NextEnd = Size - Tail                           ;
    ::memcpy ( p , s , NextEnd )                    ;
    s      += NextEnd                               ;
    NextEnd = length - NextEnd                      ;
    p       = (unsigned char *)buffer               ;
    ::memcpy ( p , s , NextEnd )                    ;
    Tail  = NextEnd                                 ;
  } else                                            {
    ::memcpy ( p , s , length )                     ;
    Tail += length                                  ;
  }                                                 ;
  if ( Tail >= Size ) Tail -= Size                  ;
  return Tail                                       ;
}

int LoopBuffer::get(void * data,int length)
{
  if ( Tail == Start ) return 0                   ;
  int             total = 0                       ;
  unsigned char * p     = (unsigned char *)buffer ;
  unsigned char * s     = (unsigned char *)data   ;
  /////////////////////////////////////////////////
  p += Start                                      ;
  /////////////////////////////////////////////////
  if ( Tail > Start )                             {
    total  = Tail  - Start                        ;
    if ( total > length ) total = length          ;
    ::memcpy ( s , p , total )                    ;
    Start += total                                ;
    if ( Start >= Size ) Start -= Size            ;
    return total                                  ;
  }                                               ;
  /////////////////////////////////////////////////
  if ( ( Start + length ) <= Size )               {
    ::memcpy ( s , p , length )                   ;
    Start += length                               ;
    if ( Start >= Size ) Start -= Size            ;
    return length                                 ;
  }                                               ;
  /////////////////////////////////////////////////
  total   = Size - Start                          ;
  length -= total                                 ;
  ::memcpy ( s , p , total )                      ;
  s      += total                                 ;
  Start   = 0                                     ;
  total  += get ( s , length )                    ;
  /////////////////////////////////////////////////
  return total                                    ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
