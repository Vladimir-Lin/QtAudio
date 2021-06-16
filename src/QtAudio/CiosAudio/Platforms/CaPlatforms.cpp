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

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

double DefaultSampleRateSearchOrders [ 15 ] = {
          44100.0                             ,
          48000.0                             ,
          32000.0                             ,
          24000.0                             ,
          22050.0                             ,
          88200.0                             ,
          96000.0                             ,
         192000.0                             ,
          16000.0                             ,
          12000.0                             ,
          11025.0                             ,
           9600.0                             ,
           8000.0                             ,
              0.0                           } ;

int AllSamplingRates[15] = {
  192000                   ,
  176400                   ,
   96000                   ,
   88200                   ,
   48000                   ,
   44100                   ,
   32000                   ,
   24000                   ,
   22050                   ,
   16000                   ,
   12000                   ,
   11025                   ,
    9600                   ,
    8000                   ,
       0                 } ;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
