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

CpuLoad:: CpuLoad(void)
{
  samplingPeriod       = 0 ;
  measurementStartTime = 0 ;
  averageLoad          = 0 ;
}

CpuLoad::~CpuLoad(void)
{
}

void CpuLoad::Initialize(double sampleRate)
{
  if ( sampleRate <= 0 ) return     ;
  samplingPeriod = 1.0 / sampleRate ;
  averageLoad    = 0.0              ;
}

void CpuLoad::Reset(void)
{
  averageLoad = 0.0 ;
}

void CpuLoad::Begin(void)
{
  measurementStartTime = Timer :: Time ( ) ;
}

void CpuLoad::End(unsigned long framesProcessed)
{
  if ( framesProcessed <= 0 ) return                               ;
  //////////////////////////////////////////////////////////////////
  double measurementEndTime                                        ;
  double secondsFor100Percent                                      ;
  double measuredLoad                                              ;
  //////////////////////////////////////////////////////////////////
  measurementEndTime   = Timer :: Time ( )                         ;
  secondsFor100Percent = framesProcessed * samplingPeriod          ;
  measuredLoad         = measurementEndTime - measurementStartTime ;
  measuredLoad        /= secondsFor100Percent                      ;
  //////////////////////////////////////////////////////////////////
  #define LOWPASS_COEFFICIENT_0 (0.9)
  #define LOWPASS_COEFFICIENT_1 (0.99999 - LOWPASS_COEFFICIENT_0)
  averageLoad = ( LOWPASS_COEFFICIENT_0 * averageLoad  )           +
                ( LOWPASS_COEFFICIENT_1 * measuredLoad )           ;
}

CaTime CpuLoad::Value(void)
{
  return averageLoad ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
