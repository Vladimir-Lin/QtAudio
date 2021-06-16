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

#define CA_DITHER_BITS_ (15)
#define DITHER_SHIFT_  ((sizeof(CaInt32)*8 - CA_DITHER_BITS_) + 1)

/* Multiply by CA_FLOAT_DITHER_SCALE_ to get a float between -2.0 and +1.99999 */
#define CA_FLOAT_DITHER_SCALE_  (1.0f / ((1<<CA_DITHER_BITS_)-1))

static const float const_float_dither_scale_ = CA_FLOAT_DITHER_SCALE_ ;

Dither:: Dither(void)
{
  previous =        0 ;
  randSeed1 =   22222 ;
  randSeed2 = 5555555 ;
}

Dither::~Dither(void)
{
}

CaInt32 Dither::Dither16 (void)
{
  CaInt32 current                                   ;
  CaInt32 highPass                                  ;
  randSeed1 = ( randSeed1 * 196314165 ) + 907633515 ;
  randSeed2 = ( randSeed2 * 196314165 ) + 907633515 ;
  current = (((CaInt32)randSeed1)>>DITHER_SHIFT_)   +
            (((CaInt32)randSeed2)>>DITHER_SHIFT_)   ;
  highPass = current - previous                     ;
  previous = current                                ;
  return highPass                                   ;
}

CaFloat32 Dither::DitherFloat (void)
{
  CaInt32 current                                      ;
  CaInt32 highPass                                     ;
  randSeed1 = (randSeed1 * 196314165) + 907633515      ;
  randSeed2 = (randSeed2 * 196314165) + 907633515      ;
  current = (((CaInt32)randSeed1)>>DITHER_SHIFT_ )     +
            (((CaInt32)randSeed2)>>DITHER_SHIFT_ )     ;
  highPass = current - previous                        ;
  previous = current                                   ;
  return ((float)highPass) * const_float_dither_scale_ ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
