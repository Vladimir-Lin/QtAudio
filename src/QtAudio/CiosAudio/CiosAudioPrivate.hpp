/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2015/02/13                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#ifndef CIOSAUDIOPRIVATE_HPP
#define CIOSAUDIOPRIVATE_HPP

#if defined(QT_CORE_LIB) && defined(CIOSLIB)
#include <Multimedia>
#else
#include "CiosAudio.hpp"
#endif

#ifndef max
#define max(a, b)  (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b)  (((a) < (b)) ? (a) : (b))
#endif

/*****************************************************************************/
/*                               Memory Barrier                              */
/*                                                                           */
/* define the following Memory Barrier functions :                           */
/*                                                                           */
/* 1. CaFullMemoryBarrier                                                    */
/* 2. CaReadMemoryBarrier                                                    */
/* 3. CaWriteMemoryBarrier()                                                 */
/*                                                                           */
/*****************************************************************************/

#if defined(__APPLE__)
#   include <libkern/OSAtomic.h>
    /* Here are the memory barrier functions. Mac OS X only provides
       full memory barriers, so the three types of barriers are the same,
       however, these barriers are superior to compiler-based ones. */
#   define CaFullMemoryBarrier()  OSMemoryBarrier()
#   define CaReadMemoryBarrier()  OSMemoryBarrier()
#   define CaWriteMemoryBarrier() OSMemoryBarrier()
#elif defined(__GNUC__)
    /* GCC >= 4.1 has built-in intrinsics. We'll use those */
#   if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#      define CaFullMemoryBarrier()  __sync_synchronize()
#      define CaReadMemoryBarrier()  __sync_synchronize()
#      define CaWriteMemoryBarrier() __sync_synchronize()
    /* as a fallback, GCC understands volatile asm and "memory" to mean it
     * should not reorder memory read/writes */
    /* Note that it is not clear that any compiler actually defines __PPC__,
     * it can probably removed safely. */
#   elif defined( __ppc__ ) || defined( __powerpc__) || defined( __PPC__ )
#      define CaFullMemoryBarrier()  asm volatile("sync":::"memory")
#      define CaReadMemoryBarrier()  asm volatile("sync":::"memory")
#      define CaWriteMemoryBarrier() asm volatile("sync":::"memory")
#   elif defined( __i386__ ) || defined( __i486__ ) || defined( __i586__ ) || \
         defined( __i686__ ) || defined( __x86_64__ )
#      define CaFullMemoryBarrier()  asm volatile("mfence":::"memory")
#      define CaReadMemoryBarrier()  asm volatile("lfence":::"memory")
#      define CaWriteMemoryBarrier() asm volatile("sfence":::"memory")
#   else
#      ifdef ALLOW_SMP_DANGERS
#         warning Memory barriers not defined on this system or system unknown
#         warning For SMP safety, you should fix this.
#         define CaFullMemoryBarrier()
#         define CaReadMemoryBarrier()
#         define CaWriteMemoryBarrier()
#      else
#         error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#      endif
#   endif
#elif (_MSC_VER >= 1400) && !defined(_WIN32_WCE)
#   include <intrin.h>
#   pragma intrinsic(_ReadWriteBarrier)
#   pragma intrinsic(_ReadBarrier)
#   pragma intrinsic(_WriteBarrier)
/* note that MSVC intrinsics _ReadWriteBarrier(), _ReadBarrier(), _WriteBarrier() are just compiler barriers *not* memory barriers */
#   define CaFullMemoryBarrier()  _ReadWriteBarrier()
#   define CaReadMemoryBarrier()  _ReadBarrier()
#   define CaWriteMemoryBarrier() _WriteBarrier()
#elif defined(_WIN32_WCE)
#   define CaFullMemoryBarrier()
#   define CaReadMemoryBarrier()
#   define CaWriteMemoryBarrier()
#elif defined(_MSC_VER) || defined(__BORLANDC__)
#   define CaFullMemoryBarrier()  _asm { lock add    [esp], 0 }
#   define CaReadMemoryBarrier()  _asm { lock add    [esp], 0 }
#   define CaWriteMemoryBarrier() _asm { lock add    [esp], 0 }
#else
#   ifdef ALLOW_SMP_DANGERS
#      warning Memory barriers not defined on this system or system unknown
#      warning For SMP safety, you should fix this.
#      define CaFullMemoryBarrier()
#      define CaReadMemoryBarrier()
#      define CaWriteMemoryBarrier()
#   else
#      error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#   endif
#endif

#define CaDebug(x) if ( NULL != debugger ) debugger->printf x
#define CaLogApi(x) if ( NULL != debugger ) debugger->printf x
#define CaLogApiEnter(functionName) if ( NULL != debugger ) debugger->printf( functionName " called.\n" )
#define CaLogApiEnterParams(functionName) if ( NULL != debugger ) debugger->printf( functionName " called:\n" )
#define CaLogApiExit(functionName) if ( NULL != debugger ) debugger->printf( functionName " returned.\n" )
#define CaLogApiExitError( functionName, result ) \
  if ( NULL != debugger )                       { \
    debugger -> printf ( functionName " returned:\n" ); \
    debugger -> printf ("\tError: %d ( %s )\n", result, debugger -> Error ( result )  ) ; \
  }
#define CaLogApiExitT( functionName, resultFormatString, result ) \
  if ( NULL != debugger )                                       { \
    debugger -> printf ( functionName " returned:\n"          ) ; \
    debugger -> printf ( "\t" resultFormatString "\n", result ) ; \
  }
#define CaLogApiExitErrorOrTResult( functionName, positiveResultFormatString, result ) \
  if ( NULL != debugger )                                       { \
    debugger -> printf ( functionName " returned:\n" )          ; \
    if ( result > 0 )                                             \
      debugger -> printf ( "\t" positiveResultFormatString "\n", result ); \
    else \
      debugger -> printf ( "\tError: %d ( %s )\n", result, debugger -> Error ( result ) ) ; \
  }

#ifndef REMOVE_DEBUG_MESSAGE
#define gPrintf(x) if ( NULL != globalDebugger ) globalDebugger -> printf x
#define dPrintf(x) if ( NULL != debugger       ) debugger       -> printf x
#else
#define gPrintf(x)
#define dPrintf(x)
#endif

#define CaTrackHere gPrintf ( (  "At %s , Line %d\n" , __FUNCTION__ , __LINE__ ) )

#define CaIsNull(x)      ( NULL    == (x)  )
#define CaNotNull(x)     ( NULL    != (x)  )
#define CaIsEqual(a,b)   ( (b) == (a)      )
#define CaNotEqual(a,b)  ( (b) != (a)      )
#define CaAND(a,b)       ( (a) && (b)      )
#define CaOR(a,b)        ( (a) || (b)      )
#define CaNOT(a)         ( ! (a)           )
#define CaIsLess(a,b)    ( (a)  < (b)      )
#define CaIsGreater(a,b) ( (a)  > (b)      )
#define CaIsCorrect(x)   ( NoError == (x)  )
#define CaIsWrong(x)     ( NoError != (x)  )
#define CaAnswer(a,b,c)  ( (a) ? (b) : (c) )
#define CaRETURN(a,b)    if ( a ) return (b)

#define CaEraseAllocation            \
  if ( CaNotNull ( allocations ) ) { \
    allocations -> release ( )     ; \
    delete allocations             ; \
    allocations = NULL             ; \
  }

#define CaExprCorrect(expr,rt) \
    if ( ( expr ) ) {          \
      result = rt   ;          \
      goto error    ;          \
    }

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

extern double DefaultSampleRateSearchOrders [ ] ;
extern int    AllSamplingRates              [ ] ;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

typedef struct CaDeviceConf  {
  int            channels    ;
  CaSampleFormat format      ;
  double         rate        ;
  int            support     ;
}              CaDeviceConf  ;

extern double NearestSampleRate      (
                double   sampleRate  ,
                int      Total       ,
                double * Supported ) ;

#if defined(CAUTILITIES)

#if defined(FFMPEGLIB)

class OffshorePlayer : public Thread
{
  public:

    char * filename ;
    int    deviceId ;
    int    decodeId ;

    explicit OffshorePlayer (void) ;
    virtual ~OffshorePlayer (void) ;

  protected:

    virtual void run        (void) ;

  private:

};

#endif

#endif

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

#endif // CIOSAUDIOPRIVATE_HPP
