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

LinearConduit:: LinearConduit (void)
              : Conduit       (    )
              , buffer        (NULL)
              , Size          (0   )
{
}

LinearConduit:: LinearConduit (int size)
              : Conduit       (        )
{
  Size   = size                         ;
  if ( size > 0 )                       {
    buffer = new unsigned char [ size ] ;
  }                                     ;
}

LinearConduit::~LinearConduit (void)
{
  if ( NULL != buffer ) {
    delete [] buffer    ;
    buffer = NULL       ;
    Size   = 0          ;
  }
}

int LinearConduit::obtain(void)
{
  return Complete ;
}

int LinearConduit::put(void)
{
  return LinearPut ( ) ;
}

void LinearConduit::finish(ConduitDirection direction ,
                           FinishCondition  condition )
{
}

int LinearConduit::setBufferSize(int size)
{
  if ( NULL != buffer )                 {
    delete [] buffer                    ;
    buffer = NULL                       ;
    Size   = 0                          ;
  }                                     ;
  Size = size                           ;
  if ( size > 0 )                       {
    buffer = new unsigned char [ size ] ;
    memset ( buffer , 0 , size )        ;
  }                                     ;
  return Size                           ;
}

int LinearConduit::size(void) const
{
  return Size ;
}

unsigned char * LinearConduit::window (void) const
{
  return buffer ;
}

int LinearConduit::LinearPut(void)
{
  if ( Input . isNull ( ) ) return Abort           ;
  long long bs = Input.Total()                     ;
  if ( bs <= 0    ) return Continue                ;
  if ( bs >  Size ) bs = Size                      ;
  int dp = Size - bs                               ;
  if ( dp > 0 )                                    {
    ::memcpy ( buffer      , buffer + bs    , dp ) ;
  }                                                ;
  ::memcpy   ( buffer + dp , Input . Buffer , bs ) ;
  return Continue                                  ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
