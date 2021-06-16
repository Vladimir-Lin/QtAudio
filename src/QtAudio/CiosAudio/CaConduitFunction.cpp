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

ConduitFunction:: ConduitFunction (void)
                : Conduit         (    )
{
}

ConduitFunction:: ConduitFunction (CacInputFunction input)
                : Conduit         (                      )
{
  inputFunction = input ;
}

ConduitFunction:: ConduitFunction (CacOutputFunction output)
                : Conduit         (                        )
{
  outputFunction = output ;
}

ConduitFunction::~ConduitFunction (void)
{
}

int ConduitFunction::obtain(void)
{
  return ObtainByFunction ( ) ;
}

int ConduitFunction::put(void)
{
  return PutByFunction    ( ) ;
}

void ConduitFunction::finish(ConduitDirection direction,
                             FinishCondition  condition)
{
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
