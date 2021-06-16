/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Lastest update : 2014/12/16                                               *
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

#if defined(CAUTILITIES)

BridgeConduit:: BridgeConduit ( void )
              : Conduit       (      )
{
}

BridgeConduit::~BridgeConduit(void)
{
}

void BridgeConduit::setBufferSize (int size,int margin)
{
  Buffer . setBufferSize ( size   ) ;
  Buffer . setMargin     ( margin ) ;
}

void BridgeConduit::LockConduit(void)
{
  mutex . lock   ( ) ;
}

void BridgeConduit::UnlockConduit(void)
{
  mutex . unlock ( ) ;
}

int BridgeConduit::obtain(void)
{
  return BridgeObtain ( ) ;
}

int BridgeConduit::put(void)
{
  return BridgePut ( ) ;
}

void BridgeConduit::finish(ConduitDirection direction,
                           FinishCondition  condition)
{
}

int BridgeConduit::BridgeObtain(void)
{
  if ( Input . Situation == StreamIO::Stagnated )  {
    return Continue                                ;
  }                                                ;
  //////////////////////////////////////////////////
  if ( Output . FrameCount <= 0 )                  {
    Output . Situation = StreamIO::Ruptured        ;
    return Complete                                ;
  }                                                ;
  //////////////////////////////////////////////////
  if ( Output . isNull ( ) )                       {
    Output . Situation = StreamIO::Ruptured        ;
    return Complete                                ;
  }                                                ;
  if ( ( Buffer . isEmpty ( )                   ) &&
       ( StreamIO::Started == Output.Situation) )  {
    Output . Situation = StreamIO::Completed       ;
    return Complete                                ;
  }                                                ;
  //////////////////////////////////////////////////
  int  bs = Output . Total ( )                     ;
  Buffer . get ( Output . Buffer , bs )            ;
  Output . Situation = StreamIO::Started           ;
  return Continue                                  ;
}

int BridgeConduit::BridgePut(void)
{
  if ( Input . FrameCount <= 0 )                 {
    Input . Situation  = StreamIO::Ruptured      ;
    return Abort                                 ;
  }                                              ;
  ////////////////////////////////////////////////
  if ( Input.isNull() )                          {
    Input . Situation  = StreamIO::Ruptured      ;
    return Abort                                 ;
  }                                              ;
  ////////////////////////////////////////////////
  if ( Buffer . isFull ( )  )                    {
    Input . Situation  = StreamIO::Stalled       ;
    return Postpone                              ;
  }                                              ;
  ////////////////////////////////////////////////
  int fc = Input . Total ( )                     ;
  Buffer . put ( Input . Buffer , fc )           ;
  Input  . Situation  = StreamIO::Started        ;
  return Continue                                ;
}

#endif

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
