/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2015/02/11                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#ifndef CAANDROID_HPP
#define CAANDROID_HPP

///////////////////////////////////////////////////////////////////////////////

#include "CiosAudioPrivate.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

extern pthread_t caUnixMainThread ;

CaError CaUnixThreadingInitialize(void) ;
CaError CaCancelThreading(pthread_t * threading,int wait,CaError * exitResult) ;

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAANDROID_HPP
