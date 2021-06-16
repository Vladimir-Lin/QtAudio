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

Conduit:: Conduit (void)
{
  Input          . Reset ( ) ;
  Output         . Reset ( ) ;
  inputFunction  = NULL      ;
  outputFunction = NULL      ;
}

Conduit::~Conduit (void)
{
}

void Conduit::LockConduit(void)
{
}

void Conduit::UnlockConduit(void)
{
}

int Conduit::ObtainByFunction(void)
{
  if ( NULL == outputFunction ) return Abort   ;
  return outputFunction ( Output . Buffer      ,
                          Output . FrameCount  ,
                          Output . StatusFlags ,
                          Output . CurrentTime ,
                          Output . AdcDac    ) ;
}

int Conduit::PutByFunction(void)
{
  if ( NULL == inputFunction ) return Abort    ;
  return inputFunction  ( Input  . Buffer      ,
                          Input  . FrameCount  ,
                          Input  . StatusFlags ,
                          Input  . CurrentTime ,
                          Input  . AdcDac    ) ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
