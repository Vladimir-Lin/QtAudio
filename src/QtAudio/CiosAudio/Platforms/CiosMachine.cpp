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

double NearestSampleRate     (
         double   sampleRate ,
         int      Total      ,
         double * Supported  )
{
  double nearest = -1                                                        ;
  int    WaID    = 0                                                         ;
  ////////////////////////////////////////////////////////////////////////////
  while ( CaAND ( CaIsGreater ( WaID + 1 , Total )                           ,
          CaAND ( CaIsGreater ( Supported [ WaID ] , 0 )                     ,
                  CaIsLess    ( nearest            , 0 )               ) ) ) {
    if ( CaIsEqual ( (int)sampleRate , (int)Supported [ WaID ] )           ) {
      nearest = Supported [ WaID ]                                           ;
    } else
    if ( CaIsLess  ( Supported [ WaID + 1 ] , 0 ) )                          {
      nearest = Supported [ WaID ]                                           ;
    } else                                                                   {
      if ( CaIsGreater ( sampleRate , Supported [ WaID + 1 ] ) )             {
        nearest = Supported [ WaID ]                                         ;
      }                                                                      ;
    }                                                                        ;
    WaID++                                                                   ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return nearest                                                             ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
