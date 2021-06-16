/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/19                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CaWindows.hpp"

#ifdef ENABLE_HOST_DIRECTSOUND
#include "CaDirectSound.hpp"
#endif

#ifdef ENABLE_HOST_WMME
#include "CaWmme.hpp"
#endif

#ifdef ENABLE_HOST_WASAPI
#include "CaWaSAPI.hpp"
#endif

#ifdef ENABLE_HOST_WDMKS
#include "CaWdmks.hpp"
#endif

#ifdef ENABLE_HOST_OPENAL
#include "CaOpenAL.hpp"
#endif

#ifdef ENABLE_HOST_PULSEAUDIO
#include "CaPulseAudio.hpp"
#endif

#ifdef ENABLE_HOST_VLC
#include "CaVLC.hpp"
#endif

#ifdef ENABLE_HOST_FFMPEG
#include "CaFFmpeg.hpp"
#endif

#ifdef SkeletonAPI
#include "CaSkeleton.hpp"
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <mmreg.h>

#if (defined(WIN32) && (defined(_MSC_VER) && (_MSC_VER >= 1400))) && !defined(_WIN32_WCE)
#pragma comment( lib, "ole32.lib" )
#endif

#if !defined(WAVE_FORMAT_EXTENSIBLE)
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003 // MinGW32 does not define this
#endif
#ifndef _WAVEFORMATEXTENSIBLE_
#define _WAVEFORMATEXTENSIBLE_        // MinGW32 does not define this
#endif
#ifndef _INC_MMREG
#define _INC_MMREG                    // for STATIC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
#endif

#if defined(__GNUC__)

#include <winioctl.h>                 // MinGW32 does not define this automatically
#include "MinGW/ks.h"
#include "MinGW/ksmedia.h"

#else

#include <ks.h>
#include <ksmedia.h>

#endif

bool IsEarlierThanVista(void)
{
  OSVERSIONINFO osvi                                ;
  osvi . dwOSVersionInfoSize = sizeof(osvi)         ;
  if (GetVersionEx(&osvi) && osvi.dwMajorVersion<6) {
    return true                                     ;
  }                                                 ;
  return false                                      ;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

//////////////////////////////////////////////////////////////////////////////

HostApiInitializer * caWindowsHostApiInitializers [ ] = {
  #ifdef ENABLE_HOST_DIRECTSOUND
  DirectSoundInitialize                                 ,
  #endif
  #ifdef ENABLE_HOST_WMME
  WmmeInitialize                                        ,
  #endif
  #ifdef ENABLE_HOST_OPENAL
  OpenALInitialize                                      ,
  #endif
  #ifdef ENABLE_HOST_WASAPI
  WaSapiInitialize                                      ,
  #endif
  #ifdef ENABLE_HOST_WDMKS
  WdmKsInitialize                                       ,
  #endif
  #ifdef ENABLE_HOST_PULSEAUDIO
  PulseAudioInitialize                                  ,
  #endif
  #ifdef ENABLE_HOST_FFMPEG
  FFmpegInitialize                                      ,
  #endif
  #ifdef ENABLE_HOST_VLC
  VlcInitialize                                         ,
  #endif
  #ifdef SkeletonAPI
  SkeletonInitialize                                    ,
  #endif
  NULL                                                } ;

HostApiInitializer ** caHostApiInitializers = caWindowsHostApiInitializers ;

//////////////////////////////////////////////////////////////////////////////

/* use some special bit patterns here to try to guard against uninitialized memory errors */
#define CA_COM_INITIALIZED       (0xb38f)
#define CA_COM_NOT_INITIALIZED   (0xf1cd)

CaError CaComInitialize                                       (
          CaHostApiTypeId             hostApiType             ,
          CaComInitializationResult * comInitializationResult )
{
  HRESULT hr                                                                  ;
  char  * lpMsgBuf                                                            ;
  /////////////////////////////////////////////////////////////////////////////
  comInitializationResult -> state = CA_COM_NOT_INITIALIZED                   ;
  hr = ::CoInitializeEx ( NULL , COINIT_APARTMENTTHREADED )                   ;
  if ( FAILED(hr) && ( hr != RPC_E_CHANGED_MODE ) )                           {
    if ( E_OUTOFMEMORY == hr ) return InsufficientMemory                      ;
    FormatMessage                                                             (
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM             ,
      NULL                                                                    ,
      hr                                                                      ,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)                               ,
      (LPTSTR) &lpMsgBuf                                                      ,
      0                                                                       ,
      NULL                                                                  ) ;
    gPrintf ( ( lpMsgBuf ) )                                                  ;
    SetLastHostErrorInfo ( hostApiType, hr, lpMsgBuf )                        ;
    ::LocalFree ( lpMsgBuf )                                                  ;
    return UnanticipatedHostError                                             ;
  }                                                                           ;
  /////////////////////////////////////////////////////////////////////////////
  if ( RPC_E_CHANGED_MODE != hr )                                             {
    comInitializationResult -> state                = CA_COM_INITIALIZED      ;
    comInitializationResult -> initializingThreadId = ::GetCurrentThreadId()  ;
  }                                                                           ;
  /////////////////////////////////////////////////////////////////////////////
  return NoError                                                              ;
}

void CaComUninitialize                                     (
       CaHostApiTypeId             hostApiType             ,
       CaComInitializationResult * comInitializationResult )
{
  if ( ( CA_COM_NOT_INITIALIZED != comInitializationResult->state )      &&
       ( CA_COM_INITIALIZED     != comInitializationResult->state )     ) {
  }                                                                       ;
  if ( CA_COM_INITIALIZED != comInitializationResult->state ) return      ;
  /////////////////////////////////////////////////////////////////////////
  DWORD currentThreadId = ::GetCurrentThreadId()                          ;
  if ( comInitializationResult->initializingThreadId != currentThreadId ) {
  } else                                                                  {
    ::CoUninitialize ( )                                                  ;
    comInitializationResult -> state = CA_COM_NOT_INITIALIZED             ;
  }                                                                       ;
}

//////////////////////////////////////////////////////////////////////////////

#if defined(_WIN64) || defined(_WIN32_WCE)

void CaInitializeX86PlainConverters( void )
{
}

#else

typedef struct                            {

  Converter * Float32_To_Int32            ;
  Converter * Float32_To_Int32_Dither     ;
  Converter * Float32_To_Int32_Clip       ;
  Converter * Float32_To_Int32_DitherClip ;

  Converter * Float32_To_Int24            ;
  Converter * Float32_To_Int24_Dither     ;
  Converter * Float32_To_Int24_Clip       ;
  Converter * Float32_To_Int24_DitherClip ;

  Converter * Float32_To_Int16            ;
  Converter * Float32_To_Int16_Dither     ;
  Converter * Float32_To_Int16_Clip       ;
  Converter * Float32_To_Int16_DitherClip ;

  Converter * Float32_To_Int8             ;
  Converter * Float32_To_Int8_Dither      ;
  Converter * Float32_To_Int8_Clip        ;
  Converter * Float32_To_Int8_DitherClip  ;

  Converter * Float32_To_UInt8            ;
  Converter * Float32_To_UInt8_Dither     ;
  Converter * Float32_To_UInt8_Clip       ;
  Converter * Float32_To_UInt8_DitherClip ;

  Converter * Int32_To_Float32            ;
  Converter * Int32_To_Int24              ;
  Converter * Int32_To_Int24_Dither       ;
  Converter * Int32_To_Int16              ;
  Converter * Int32_To_Int16_Dither       ;
  Converter * Int32_To_Int8               ;
  Converter * Int32_To_Int8_Dither        ;
  Converter * Int32_To_UInt8              ;
  Converter * Int32_To_UInt8_Dither       ;

  Converter * Int24_To_Float32            ;
  Converter * Int24_To_Int32              ;
  Converter * Int24_To_Int16              ;
  Converter * Int24_To_Int16_Dither       ;
  Converter * Int24_To_Int8               ;
  Converter * Int24_To_Int8_Dither        ;
  Converter * Int24_To_UInt8              ;
  Converter * Int24_To_UInt8_Dither       ;

  Converter * Int16_To_Float32            ;
  Converter * Int16_To_Int32              ;
  Converter * Int16_To_Int24              ;
  Converter * Int16_To_Int8               ;
  Converter * Int16_To_Int8_Dither        ;
  Converter * Int16_To_UInt8              ;
  Converter * Int16_To_UInt8_Dither       ;

  Converter * Int8_To_Float32             ;
  Converter * Int8_To_Int32               ;
  Converter * Int8_To_Int24               ;
  Converter * Int8_To_Int16               ;
  Converter * Int8_To_UInt8               ;

  Converter * UInt8_To_Float32            ;
  Converter * UInt8_To_Int32              ;
  Converter * UInt8_To_Int24              ;
  Converter * UInt8_To_Int16              ;
  Converter * UInt8_To_Int8               ;

  Converter * Copy_8_To_8                 ; /* copy without any conversion */
  Converter * Copy_16_To_16               ; /* copy without any conversion */
  Converter * Copy_24_To_24               ; /* copy without any conversion */
  Converter * Copy_32_To_32               ; /* copy without any conversion */

} ConverterTable                          ;

extern ConverterTable caConverters ;

/*round to nearest, 64 bit precision, all exceptions masked*/
static const short  fpuControlWord_      = 0x033F     ;
static const double int32Scaler_         = 0x7FFFFFFF ;
static const double ditheredInt32Scaler_ = 0x7FFFFFFE ;
static const double int24Scaler_         = 0x7FFFFF   ;
static const double ditheredInt24Scaler_ = 0x7FFFFE   ;
static const double int16Scaler_         = 0x7FFF     ;
static const double ditheredInt16Scaler_ = 0x7FFE     ;

#define PA_DITHER_BITS_   (15)
/* Multiply by PA_FLOAT_DITHER_SCALE_ to get a float between -2.0 and +1.99999 */
#define PA_FLOAT_DITHER_SCALE_  (1.0F / ((1<<PA_DITHER_BITS_)-1))
static const float const_float_dither_scale_ = PA_FLOAT_DITHER_SCALE_;
#define PA_DITHER_SHIFT_  ((32 - PA_DITHER_BITS_) + 1)

/* -------------------------------------------------------------------------- */

static void Float32_To_Int32(
    void *destinationBuffer, signed int destinationStride,
    void *sourceBuffer, signed int sourceStride,
    unsigned int count, Dither *ditherGenerator )
{
/*
    float *src = (float*)sourceBuffer;
    signed long *dest =  (signed long*)destinationBuffer;
    (void)ditherGenerator; // unused parameter

    while( count-- )
    {
        // REVIEW
        double scaled = *src * 0x7FFFFFFF;
        *dest = (signed long) scaled;

        src += sourceStride;
        dest += destinationStride;
    }
*/

    short savedFpuControlWord;

    (void) ditherGenerator; /* unused parameter */


    __asm{
        // esi -> source ptr
        // eax -> source byte stride
        // edi -> destination ptr
        // ebx -> destination byte stride
        // ecx -> source end ptr
        // edx -> temp

        mov     esi, sourceBuffer

        mov     edx, 4                  // sizeof float32 and int32
        mov     eax, sourceStride
        imul    eax, edx

        mov     ecx, count
        imul    ecx, eax
        add     ecx, esi

        mov     edi, destinationBuffer

        mov     ebx, destinationStride
        imul    ebx, edx

        fwait
        fstcw   savedFpuControlWord
        fldcw   fpuControlWord_

        fld     int32Scaler_             // stack:  (int)0x7FFFFFFF

    Float32_To_Int32_loop:

        // load unscaled value into st(0)
        fld     dword ptr [esi]         // stack:  value, (int)0x7FFFFFFF
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        fmul    st(0), st(1)            // st(0) *= st(1), stack:  value*0x7FFFFFFF, (int)0x7FFFFFFF
        /*
            note: we could store to a temporary qword here which would cause
            wraparound distortion instead of int indefinite 0x10. that would
            be more work, and given that not enabling clipping is only advisable
            when you know that your signal isn't going to clip it isn't worth it.
        */
        fistp   dword ptr [edi]         // pop st(0) into dest, stack:  (int)0x7FFFFFFF

        add     edi, ebx                // increment destination ptr
        //lea     edi, [edi+ebx]

        cmp     esi, ecx                // has src ptr reached end?
        jne     Float32_To_Int32_loop

        ffree   st(0)
        fincstp

        fwait
        fnclex
        fldcw   savedFpuControlWord
    }
}

/* -------------------------------------------------------------------------- */

static void Float32_To_Int32_Clip(
    void *destinationBuffer, signed int destinationStride,
    void *sourceBuffer, signed int sourceStride,
    unsigned int count, Dither *ditherGenerator )
{
/*
    float *src = (float*)sourceBuffer;
    signed long *dest =  (signed long*)destinationBuffer;
    (void) ditherGenerator; // unused parameter

    while( count-- )
    {
        // REVIEW
        double scaled = *src * 0x7FFFFFFF;
        PA_CLIP_( scaled, -2147483648., 2147483647.  );
        *dest = (signed long) scaled;

        src += sourceStride;
        dest += destinationStride;
    }
*/

    short savedFpuControlWord;

    (void) ditherGenerator; /* unused parameter */

    __asm{
        // esi -> source ptr
        // eax -> source byte stride
        // edi -> destination ptr
        // ebx -> destination byte stride
        // ecx -> source end ptr
        // edx -> temp

        mov     esi, sourceBuffer

        mov     edx, 4                  // sizeof float32 and int32
        mov     eax, sourceStride
        imul    eax, edx

        mov     ecx, count
        imul    ecx, eax
        add     ecx, esi

        mov     edi, destinationBuffer

        mov     ebx, destinationStride
        imul    ebx, edx

        fwait
        fstcw   savedFpuControlWord
        fldcw   fpuControlWord_

        fld     int32Scaler_             // stack:  (int)0x7FFFFFFF

    Float32_To_Int32_Clip_loop:

        mov     edx, dword ptr [esi]    // load floating point value into integer register

        and     edx, 0x7FFFFFFF         // mask off sign
        cmp     edx, 0x3F800000         // greater than 1.0 or less than -1.0

        jg      Float32_To_Int32_Clip_clamp

        // load unscaled value into st(0)
        fld     dword ptr [esi]         // stack:  value, (int)0x7FFFFFFF
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        fmul    st(0), st(1)            // st(0) *= st(1), stack:  value*0x7FFFFFFF, (int)0x7FFFFFFF
        fistp   dword ptr [edi]         // pop st(0) into dest, stack:  (int)0x7FFFFFFF
        jmp     Float32_To_Int32_Clip_stored

    Float32_To_Int32_Clip_clamp:
        mov     edx, dword ptr [esi]    // load floating point value into integer register
        shr     edx, 31                 // move sign bit into bit 0
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        add     edx, 0x7FFFFFFF         // convert to maximum range integers
        mov     dword ptr [edi], edx

    Float32_To_Int32_Clip_stored:

        //add     edi, ebx                // increment destination ptr
        lea     edi, [edi+ebx]

        cmp     esi, ecx                // has src ptr reached end?
        jne     Float32_To_Int32_Clip_loop

        ffree   st(0)
        fincstp

        fwait
        fnclex
        fldcw   savedFpuControlWord
    }
}

/* -------------------------------------------------------------------------- */

static void Float32_To_Int32_DitherClip(
    void *destinationBuffer, signed int destinationStride,
    void *sourceBuffer, signed int sourceStride,
    unsigned int count, Dither *ditherGenerator )
{
    /*
    float *src = (float*)sourceBuffer;
    signed long *dest =  (signed long*)destinationBuffer;

    while( count-- )
    {
        // REVIEW
        double dither  = PaUtil_GenerateFloatTriangularDither( ditherGenerator );
        // use smaller scaler to prevent overflow when we add the dither
        double dithered = ((double)*src * (2147483646.0)) + dither;
        PA_CLIP_( dithered, -2147483648., 2147483647.  );
        *dest = (signed long) dithered;


        src += sourceStride;
        dest += destinationStride;
    }
    */

    short savedFpuControlWord;

    // spill storage:
    signed long sourceByteStride;
    signed long highpassedDither;

    // dither state:
    unsigned long ditherPrevious = ditherGenerator->previous;
    unsigned long ditherRandSeed1 = ditherGenerator->randSeed1;
    unsigned long ditherRandSeed2 = ditherGenerator->randSeed2;

    __asm{
        // esi -> source ptr
        // eax -> source byte stride
        // edi -> destination ptr
        // ebx -> destination byte stride
        // ecx -> source end ptr
        // edx -> temp

        mov     esi, sourceBuffer

        mov     edx, 4                  // sizeof float32 and int32
        mov     eax, sourceStride
        imul    eax, edx

        mov     ecx, count
        imul    ecx, eax
        add     ecx, esi

        mov     edi, destinationBuffer

        mov     ebx, destinationStride
        imul    ebx, edx

        fwait
        fstcw   savedFpuControlWord
        fldcw   fpuControlWord_

        fld     ditheredInt32Scaler_    // stack:  int scaler

    Float32_To_Int32_DitherClip_loop:

        mov     edx, dword ptr [esi]    // load floating point value into integer register

        and     edx, 0x7FFFFFFF         // mask off sign
        cmp     edx, 0x3F800000         // greater than 1.0 or less than -1.0

        jg      Float32_To_Int32_DitherClip_clamp

        // load unscaled value into st(0)
        fld     dword ptr [esi]         // stack:  value, int scaler
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        fmul    st(0), st(1)            // st(0) *= st(1), stack:  value*(int scaler), int scaler

        /*
        // call PaUtil_GenerateFloatTriangularDither with C calling convention
        mov     sourceByteStride, eax   // save eax
        mov     sourceEnd, ecx          // save ecx
        push    ditherGenerator         // pass ditherGenerator parameter on stack
        call    PaUtil_GenerateFloatTriangularDither  // stack:  dither, value*(int scaler), int scaler
        pop     edx                     // clear parameter off stack
        mov     ecx, sourceEnd          // restore ecx
        mov     eax, sourceByteStride   // restore eax
        */

    // generate dither
        mov     sourceByteStride, eax   // save eax
        mov     edx, 196314165
        mov     eax, ditherRandSeed1
        mul     edx                     // eax:edx = eax * 196314165
        //add     eax, 907633515
        lea     eax, [eax+907633515]
        mov     ditherRandSeed1, eax
        mov     edx, 196314165
        mov     eax, ditherRandSeed2
        mul     edx                     // eax:edx = eax * 196314165
        //add     eax, 907633515
        lea     eax, [eax+907633515]
        mov     edx, ditherRandSeed1
        shr     edx, PA_DITHER_SHIFT_
        mov     ditherRandSeed2, eax
        shr     eax, PA_DITHER_SHIFT_
        //add     eax, edx                // eax -> current
        lea     eax, [eax+edx]
        mov     edx, ditherPrevious
        neg     edx
        lea     edx, [eax+edx]          // highpass = current - previous
        mov     highpassedDither, edx
        mov     ditherPrevious, eax     // previous = current
        mov     eax, sourceByteStride   // restore eax
        fild    highpassedDither
        fmul    const_float_dither_scale_
    // end generate dither, dither signal in st(0)

        faddp   st(1), st(0)            // stack: dither + value*(int scaler), int scaler
        fistp   dword ptr [edi]         // pop st(0) into dest, stack:  int scaler
        jmp     Float32_To_Int32_DitherClip_stored

    Float32_To_Int32_DitherClip_clamp:
        mov     edx, dword ptr [esi]    // load floating point value into integer register
        shr     edx, 31                 // move sign bit into bit 0
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        add     edx, 0x7FFFFFFF         // convert to maximum range integers
        mov     dword ptr [edi], edx

    Float32_To_Int32_DitherClip_stored:

        //add     edi, ebx              // increment destination ptr
        lea     edi, [edi+ebx]

        cmp     esi, ecx                // has src ptr reached end?
        jne     Float32_To_Int32_DitherClip_loop

        ffree   st(0)
        fincstp

        fwait
        fnclex
        fldcw   savedFpuControlWord
    }

    ditherGenerator->previous = ditherPrevious;
    ditherGenerator->randSeed1 = ditherRandSeed1;
    ditherGenerator->randSeed2 = ditherRandSeed2;
}

/* -------------------------------------------------------------------------- */

static void Float32_To_Int24(
    void *destinationBuffer, signed int destinationStride,
    void *sourceBuffer, signed int sourceStride,
    unsigned int count, Dither *ditherGenerator )
{
/*
    float *src = (float*)sourceBuffer;
    unsigned char *dest = (unsigned char*)destinationBuffer;
    signed long temp;

    (void) ditherGenerator; // unused parameter

    while( count-- )
    {
        // convert to 32 bit and drop the low 8 bits
        double scaled = *src * 0x7FFFFFFF;
        temp = (signed long) scaled;

        dest[0] = (unsigned char)(temp >> 8);
        dest[1] = (unsigned char)(temp >> 16);
        dest[2] = (unsigned char)(temp >> 24);

        src += sourceStride;
        dest += destinationStride * 3;
    }
*/

    short savedFpuControlWord;

    signed long tempInt32;

    (void) ditherGenerator; /* unused parameter */

    __asm{
        // esi -> source ptr
        // eax -> source byte stride
        // edi -> destination ptr
        // ebx -> destination byte stride
        // ecx -> source end ptr
        // edx -> temp

        mov     esi, sourceBuffer

        mov     edx, 4                  // sizeof float32
        mov     eax, sourceStride
        imul    eax, edx

        mov     ecx, count
        imul    ecx, eax
        add     ecx, esi

        mov     edi, destinationBuffer

        mov     edx, 3                  // sizeof int24
        mov     ebx, destinationStride
        imul    ebx, edx

        fwait
        fstcw   savedFpuControlWord
        fldcw   fpuControlWord_

        fld     int24Scaler_             // stack:  (int)0x7FFFFF

    Float32_To_Int24_loop:

        // load unscaled value into st(0)
        fld     dword ptr [esi]         // stack:  value, (int)0x7FFFFF
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        fmul    st(0), st(1)            // st(0) *= st(1), stack:  value*0x7FFFFF, (int)0x7FFFFF
        fistp   tempInt32               // pop st(0) into tempInt32, stack:  (int)0x7FFFFF
        mov     edx, tempInt32

        mov     byte ptr [edi], DL
        shr     edx, 8
        //mov     byte ptr [edi+1], DL
        //mov     byte ptr [edi+2], DH
        mov     word ptr [edi+1], DX

        //add     edi, ebx                // increment destination ptr
        lea     edi, [edi+ebx]

        cmp     esi, ecx                // has src ptr reached end?
        jne     Float32_To_Int24_loop

        ffree   st(0)
        fincstp

        fwait
        fnclex
        fldcw   savedFpuControlWord
    }
}

/* -------------------------------------------------------------------------- */

static void Float32_To_Int24_Clip(
    void *destinationBuffer, signed int destinationStride,
    void *sourceBuffer, signed int sourceStride,
    unsigned int count, Dither *ditherGenerator )
{
/*
    float *src = (float*)sourceBuffer;
    unsigned char *dest = (unsigned char*)destinationBuffer;
    signed long temp;

    (void) ditherGenerator; // unused parameter

    while( count-- )
    {
        // convert to 32 bit and drop the low 8 bits
        double scaled = *src * 0x7FFFFFFF;
        PA_CLIP_( scaled, -2147483648., 2147483647.  );
        temp = (signed long) scaled;

        dest[0] = (unsigned char)(temp >> 8);
        dest[1] = (unsigned char)(temp >> 16);
        dest[2] = (unsigned char)(temp >> 24);

        src += sourceStride;
        dest += destinationStride * 3;
    }
*/

    short savedFpuControlWord;

    signed long tempInt32;

    (void) ditherGenerator; /* unused parameter */

    __asm{
        // esi -> source ptr
        // eax -> source byte stride
        // edi -> destination ptr
        // ebx -> destination byte stride
        // ecx -> source end ptr
        // edx -> temp

        mov     esi, sourceBuffer

        mov     edx, 4                  // sizeof float32
        mov     eax, sourceStride
        imul    eax, edx

        mov     ecx, count
        imul    ecx, eax
        add     ecx, esi

        mov     edi, destinationBuffer

        mov     edx, 3                  // sizeof int24
        mov     ebx, destinationStride
        imul    ebx, edx

        fwait
        fstcw   savedFpuControlWord
        fldcw   fpuControlWord_

        fld     int24Scaler_             // stack:  (int)0x7FFFFF

    Float32_To_Int24_Clip_loop:

        mov     edx, dword ptr [esi]    // load floating point value into integer register

        and     edx, 0x7FFFFFFF         // mask off sign
        cmp     edx, 0x3F800000         // greater than 1.0 or less than -1.0

        jg      Float32_To_Int24_Clip_clamp

        // load unscaled value into st(0)
        fld     dword ptr [esi]         // stack:  value, (int)0x7FFFFF
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        fmul    st(0), st(1)            // st(0) *= st(1), stack:  value*0x7FFFFF, (int)0x7FFFFF
        fistp   tempInt32               // pop st(0) into tempInt32, stack:  (int)0x7FFFFF
        mov     edx, tempInt32
        jmp     Float32_To_Int24_Clip_store

    Float32_To_Int24_Clip_clamp:
        mov     edx, dword ptr [esi]    // load floating point value into integer register
        shr     edx, 31                 // move sign bit into bit 0
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        add     edx, 0x7FFFFF           // convert to maximum range integers

    Float32_To_Int24_Clip_store:

        mov     byte ptr [edi], DL
        shr     edx, 8
        //mov     byte ptr [edi+1], DL
        //mov     byte ptr [edi+2], DH
        mov     word ptr [edi+1], DX

        //add     edi, ebx                // increment destination ptr
        lea     edi, [edi+ebx]

        cmp     esi, ecx                // has src ptr reached end?
        jne     Float32_To_Int24_Clip_loop

        ffree   st(0)
        fincstp

        fwait
        fnclex
        fldcw   savedFpuControlWord
    }
}

/* -------------------------------------------------------------------------- */

static void Float32_To_Int24_DitherClip(
    void *destinationBuffer, signed int destinationStride,
    void *sourceBuffer, signed int sourceStride,
    unsigned int count, Dither *ditherGenerator )
{
/*
    float *src = (float*)sourceBuffer;
    unsigned char *dest = (unsigned char*)destinationBuffer;
    signed long temp;

    while( count-- )
    {
        // convert to 32 bit and drop the low 8 bits

        // FIXME: the dither amplitude here appears to be too small by 8 bits
        double dither  = PaUtil_GenerateFloatTriangularDither( ditherGenerator );
        // use smaller scaler to prevent overflow when we add the dither
        double dithered = ((double)*src * (2147483646.0)) + dither;
        PA_CLIP_( dithered, -2147483648., 2147483647.  );

        temp = (signed long) dithered;

        dest[0] = (unsigned char)(temp >> 8);
        dest[1] = (unsigned char)(temp >> 16);
        dest[2] = (unsigned char)(temp >> 24);

        src += sourceStride;
        dest += destinationStride * 3;
    }
*/

    short savedFpuControlWord;

    // spill storage:
    signed long sourceByteStride;
    signed long highpassedDither;

    // dither state:
    unsigned long ditherPrevious = ditherGenerator->previous;
    unsigned long ditherRandSeed1 = ditherGenerator->randSeed1;
    unsigned long ditherRandSeed2 = ditherGenerator->randSeed2;

    signed long tempInt32;

    __asm{
        // esi -> source ptr
        // eax -> source byte stride
        // edi -> destination ptr
        // ebx -> destination byte stride
        // ecx -> source end ptr
        // edx -> temp

        mov     esi, sourceBuffer

        mov     edx, 4                  // sizeof float32
        mov     eax, sourceStride
        imul    eax, edx

        mov     ecx, count
        imul    ecx, eax
        add     ecx, esi

        mov     edi, destinationBuffer

        mov     edx, 3                  // sizeof int24
        mov     ebx, destinationStride
        imul    ebx, edx

        fwait
        fstcw   savedFpuControlWord
        fldcw   fpuControlWord_

        fld     ditheredInt24Scaler_    // stack:  int scaler

    Float32_To_Int24_DitherClip_loop:

        mov     edx, dword ptr [esi]    // load floating point value into integer register

        and     edx, 0x7FFFFFFF         // mask off sign
        cmp     edx, 0x3F800000         // greater than 1.0 or less than -1.0

        jg      Float32_To_Int24_DitherClip_clamp

        // load unscaled value into st(0)
        fld     dword ptr [esi]         // stack:  value, int scaler
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        fmul    st(0), st(1)            // st(0) *= st(1), stack:  value*(int scaler), int scaler

    /*
        // call PaUtil_GenerateFloatTriangularDither with C calling convention
        mov     sourceByteStride, eax   // save eax
        mov     sourceEnd, ecx          // save ecx
        push    ditherGenerator         // pass ditherGenerator parameter on stack
        call    PaUtil_GenerateFloatTriangularDither  // stack:  dither, value*(int scaler), int scaler
        pop     edx                     // clear parameter off stack
        mov     ecx, sourceEnd          // restore ecx
        mov     eax, sourceByteStride   // restore eax
    */

    // generate dither
        mov     sourceByteStride, eax   // save eax
        mov     edx, 196314165
        mov     eax, ditherRandSeed1
        mul     edx                     // eax:edx = eax * 196314165
        //add     eax, 907633515
        lea     eax, [eax+907633515]
        mov     ditherRandSeed1, eax
        mov     edx, 196314165
        mov     eax, ditherRandSeed2
        mul     edx                     // eax:edx = eax * 196314165
        //add     eax, 907633515
        lea     eax, [eax+907633515]
        mov     edx, ditherRandSeed1
        shr     edx, PA_DITHER_SHIFT_
        mov     ditherRandSeed2, eax
        shr     eax, PA_DITHER_SHIFT_
        //add     eax, edx                // eax -> current
        lea     eax, [eax+edx]
        mov     edx, ditherPrevious
        neg     edx
        lea     edx, [eax+edx]          // highpass = current - previous
        mov     highpassedDither, edx
        mov     ditherPrevious, eax     // previous = current
        mov     eax, sourceByteStride   // restore eax
        fild    highpassedDither
        fmul    const_float_dither_scale_
    // end generate dither, dither signal in st(0)

        faddp   st(1), st(0)            // stack: dither * value*(int scaler), int scaler
        fistp   tempInt32               // pop st(0) into tempInt32, stack:  int scaler
        mov     edx, tempInt32
        jmp     Float32_To_Int24_DitherClip_store

    Float32_To_Int24_DitherClip_clamp:
        mov     edx, dword ptr [esi]    // load floating point value into integer register
        shr     edx, 31                 // move sign bit into bit 0
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        add     edx, 0x7FFFFF           // convert to maximum range integers

    Float32_To_Int24_DitherClip_store:

        mov     byte ptr [edi], DL
        shr     edx, 8
        //mov     byte ptr [edi+1], DL
        //mov     byte ptr [edi+2], DH
        mov     word ptr [edi+1], DX

        //add     edi, ebx                // increment destination ptr
        lea     edi, [edi+ebx]

        cmp     esi, ecx                // has src ptr reached end?
        jne     Float32_To_Int24_DitherClip_loop

        ffree   st(0)
        fincstp

        fwait
        fnclex
        fldcw   savedFpuControlWord
    }

    ditherGenerator->previous = ditherPrevious;
    ditherGenerator->randSeed1 = ditherRandSeed1;
    ditherGenerator->randSeed2 = ditherRandSeed2;
}

/* -------------------------------------------------------------------------- */

static void Float32_To_Int16(
    void *destinationBuffer, signed int destinationStride,
    void *sourceBuffer, signed int sourceStride,
    unsigned int count, Dither *ditherGenerator )
{
/*
    float *src = (float*)sourceBuffer;
    signed short *dest =  (signed short*)destinationBuffer;
    (void)ditherGenerator; // unused parameter

    while( count-- )
    {

        short samp = (short) (*src * (32767.0f));
        *dest = samp;

        src += sourceStride;
        dest += destinationStride;
    }
*/

    short savedFpuControlWord;

    (void) ditherGenerator; /* unused parameter */

    __asm{
        // esi -> source ptr
        // eax -> source byte stride
        // edi -> destination ptr
        // ebx -> destination byte stride
        // ecx -> source end ptr
        // edx -> temp

        mov     esi, sourceBuffer

        mov     edx, 4                  // sizeof float32
        mov     eax, sourceStride
        imul    eax, edx                // source byte stride

        mov     ecx, count
        imul    ecx, eax
        add     ecx, esi                // source end ptr = count * source byte stride + source ptr

        mov     edi, destinationBuffer

        mov     edx, 2                  // sizeof int16
        mov     ebx, destinationStride
        imul    ebx, edx                // destination byte stride

        fwait
        fstcw   savedFpuControlWord
        fldcw   fpuControlWord_

        fld     int16Scaler_            // stack:  (int)0x7FFF

    Float32_To_Int16_loop:

        // load unscaled value into st(0)
        fld     dword ptr [esi]         // stack:  value, (int)0x7FFF
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        fmul    st(0), st(1)            // st(0) *= st(1), stack:  value*0x7FFF, (int)0x7FFF
        fistp   word ptr [edi]          // store scaled int into dest, stack:  (int)0x7FFF

        add     edi, ebx                // increment destination ptr
        //lea     edi, [edi+ebx]

        cmp     esi, ecx                // has src ptr reached end?
        jne     Float32_To_Int16_loop

        ffree   st(0)
        fincstp

        fwait
        fnclex
        fldcw   savedFpuControlWord
    }
}

/* -------------------------------------------------------------------------- */

static void Float32_To_Int16_Clip(
    void *destinationBuffer, signed int destinationStride,
    void *sourceBuffer, signed int sourceStride,
    unsigned int count, Dither *ditherGenerator )
{
/*
    float *src = (float*)sourceBuffer;
    signed short *dest =  (signed short*)destinationBuffer;
    (void)ditherGenerator; // unused parameter

    while( count-- )
    {
        long samp = (signed long) (*src * (32767.0f));
        PA_CLIP_( samp, -0x8000, 0x7FFF );
        *dest = (signed short) samp;

        src += sourceStride;
        dest += destinationStride;
    }
*/

    short savedFpuControlWord;

    (void) ditherGenerator; /* unused parameter */

    __asm{
        // esi -> source ptr
        // eax -> source byte stride
        // edi -> destination ptr
        // ebx -> destination byte stride
        // ecx -> source end ptr
        // edx -> temp

        mov     esi, sourceBuffer

        mov     edx, 4                  // sizeof float32
        mov     eax, sourceStride
        imul    eax, edx                // source byte stride

        mov     ecx, count
        imul    ecx, eax
        add     ecx, esi                // source end ptr = count * source byte stride + source ptr

        mov     edi, destinationBuffer

        mov     edx, 2                  // sizeof int16
        mov     ebx, destinationStride
        imul    ebx, edx                // destination byte stride

        fwait
        fstcw   savedFpuControlWord
        fldcw   fpuControlWord_

        fld     int16Scaler_            // stack:  (int)0x7FFF

    Float32_To_Int16_Clip_loop:

        mov     edx, dword ptr [esi]    // load floating point value into integer register

        and     edx, 0x7FFFFFFF         // mask off sign
        cmp     edx, 0x3F800000         // greater than 1.0 or less than -1.0

        jg      Float32_To_Int16_Clip_clamp

        // load unscaled value into st(0)
        fld     dword ptr [esi]         // stack:  value, (int)0x7FFF
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        fmul    st(0), st(1)            // st(0) *= st(1), stack:  value*0x7FFF, (int)0x7FFF
        fistp   word ptr [edi]          // store scaled int into dest, stack:  (int)0x7FFF
        jmp     Float32_To_Int16_Clip_stored

    Float32_To_Int16_Clip_clamp:
        mov     edx, dword ptr [esi]    // load floating point value into integer register
        shr     edx, 31                 // move sign bit into bit 0
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        add     dx, 0x7FFF              // convert to maximum range integers
        mov     word ptr [edi], dx      // store clamped into into dest

    Float32_To_Int16_Clip_stored:

        add     edi, ebx                // increment destination ptr
        //lea     edi, [edi+ebx]

        cmp     esi, ecx                // has src ptr reached end?
        jne     Float32_To_Int16_Clip_loop

        ffree   st(0)
        fincstp

        fwait
        fnclex
        fldcw   savedFpuControlWord
    }
}

/* -------------------------------------------------------------------------- */

static void Float32_To_Int16_DitherClip(
    void *destinationBuffer, signed int destinationStride,
    void *sourceBuffer, signed int sourceStride,
    unsigned int count, Dither *ditherGenerator )
{
/*
    float *src = (float*)sourceBuffer;
    signed short *dest =  (signed short*)destinationBuffer;
    (void)ditherGenerator; // unused parameter

    while( count-- )
    {

        float dither  = PaUtil_GenerateFloatTriangularDither( ditherGenerator );
        // use smaller scaler to prevent overflow when we add the dither
        float dithered = (*src * (32766.0f)) + dither;
        signed long samp = (signed long) dithered;
        PA_CLIP_( samp, -0x8000, 0x7FFF );
        *dest = (signed short) samp;

        src += sourceStride;
        dest += destinationStride;
    }
*/

    short savedFpuControlWord;

    // spill storage:
    signed long sourceByteStride;
    signed long highpassedDither;

    // dither state:
    unsigned long ditherPrevious = ditherGenerator->previous;
    unsigned long ditherRandSeed1 = ditherGenerator->randSeed1;
    unsigned long ditherRandSeed2 = ditherGenerator->randSeed2;

    __asm{
        // esi -> source ptr
        // eax -> source byte stride
        // edi -> destination ptr
        // ebx -> destination byte stride
        // ecx -> source end ptr
        // edx -> temp

        mov     esi, sourceBuffer

        mov     edx, 4                  // sizeof float32
        mov     eax, sourceStride
        imul    eax, edx                // source byte stride

        mov     ecx, count
        imul    ecx, eax
        add     ecx, esi                // source end ptr = count * source byte stride + source ptr

        mov     edi, destinationBuffer

        mov     edx, 2                  // sizeof int16
        mov     ebx, destinationStride
        imul    ebx, edx                // destination byte stride

        fwait
        fstcw   savedFpuControlWord
        fldcw   fpuControlWord_

        fld     ditheredInt16Scaler_    // stack:  int scaler

    Float32_To_Int16_DitherClip_loop:

        mov     edx, dword ptr [esi]    // load floating point value into integer register

        and     edx, 0x7FFFFFFF         // mask off sign
        cmp     edx, 0x3F800000         // greater than 1.0 or less than -1.0

        jg      Float32_To_Int16_DitherClip_clamp

        // load unscaled value into st(0)
        fld     dword ptr [esi]         // stack:  value, int scaler
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        fmul    st(0), st(1)            // st(0) *= st(1), stack:  value*(int scaler), int scaler

        /*
        // call PaUtil_GenerateFloatTriangularDither with C calling convention
        mov     sourceByteStride, eax   // save eax
        mov     sourceEnd, ecx          // save ecx
        push    ditherGenerator         // pass ditherGenerator parameter on stack
        call    PaUtil_GenerateFloatTriangularDither  // stack:  dither, value*(int scaler), int scaler
        pop     edx                     // clear parameter off stack
        mov     ecx, sourceEnd          // restore ecx
        mov     eax, sourceByteStride   // restore eax
        */

    // generate dither
        mov     sourceByteStride, eax   // save eax
        mov     edx, 196314165
        mov     eax, ditherRandSeed1
        mul     edx                     // eax:edx = eax * 196314165
        //add     eax, 907633515
        lea     eax, [eax+907633515]
        mov     ditherRandSeed1, eax
        mov     edx, 196314165
        mov     eax, ditherRandSeed2
        mul     edx                     // eax:edx = eax * 196314165
        //add     eax, 907633515
        lea     eax, [eax+907633515]
        mov     edx, ditherRandSeed1
        shr     edx, PA_DITHER_SHIFT_
        mov     ditherRandSeed2, eax
        shr     eax, PA_DITHER_SHIFT_
        //add     eax, edx                // eax -> current
        lea     eax, [eax+edx]            // current = randSeed1>>x + randSeed2>>x
        mov     edx, ditherPrevious
        neg     edx
        lea     edx, [eax+edx]          // highpass = current - previous
        mov     highpassedDither, edx
        mov     ditherPrevious, eax     // previous = current
        mov     eax, sourceByteStride   // restore eax
        fild    highpassedDither
        fmul    const_float_dither_scale_
    // end generate dither, dither signal in st(0)

        faddp   st(1), st(0)            // stack: dither * value*(int scaler), int scaler
        fistp   word ptr [edi]          // store scaled int into dest, stack:  int scaler
        jmp     Float32_To_Int16_DitherClip_stored

    Float32_To_Int16_DitherClip_clamp:
        mov     edx, dword ptr [esi]    // load floating point value into integer register
        shr     edx, 31                 // move sign bit into bit 0
        add     esi, eax                // increment source ptr
        //lea     esi, [esi+eax]
        add     dx, 0x7FFF              // convert to maximum range integers
        mov     word ptr [edi], dx      // store clamped into into dest

    Float32_To_Int16_DitherClip_stored:

        add     edi, ebx                // increment destination ptr
        //lea     edi, [edi+ebx]

        cmp     esi, ecx                // has src ptr reached end?
        jne     Float32_To_Int16_DitherClip_loop

        ffree   st(0)
        fincstp

        fwait
        fnclex
        fldcw   savedFpuControlWord
    }

    ditherGenerator->previous = ditherPrevious;
    ditherGenerator->randSeed1 = ditherRandSeed1;
    ditherGenerator->randSeed2 = ditherRandSeed2;
}

/* -------------------------------------------------------------------------- */

void PaUtil_InitializeX86PlainConverters( void )
{
    caConverters.Float32_To_Int32 = Float32_To_Int32;
    caConverters.Float32_To_Int32_Clip = Float32_To_Int32_Clip;
    caConverters.Float32_To_Int32_DitherClip = Float32_To_Int32_DitherClip;

    caConverters.Float32_To_Int24 = Float32_To_Int24;
    caConverters.Float32_To_Int24_Clip = Float32_To_Int24_Clip;
    caConverters.Float32_To_Int24_DitherClip = Float32_To_Int24_DitherClip;

    caConverters.Float32_To_Int16 = Float32_To_Int16;
    caConverters.Float32_To_Int16_Clip = Float32_To_Int16_Clip;
    caConverters.Float32_To_Int16_DitherClip = Float32_To_Int16_DitherClip;
}

#endif

//////////////////////////////////////////////////////////////////////////////

static GUID ksDataFormatSubtypeGuidBase = {
  (USHORT)(WAVE_FORMAT_PCM)               ,
  0x0000                                  ,
  0x0010                                  ,
  0x0080                                  ,
  0x0000                                  ,
  0x0000                                  ,
  0x00AA                                  ,
  0x0000                                  ,
  0x0038                                  ,
  0x009B                                  ,
  0x0071                                } ;

int CaSampleFormatToLinearWaveFormatTag(CaSampleFormat sampleFormat)
{
  if ( cafFloat32 == sampleFormat ) return CA_WAVE_FORMAT_IEEE_FLOAT ;
  return CA_WAVE_FORMAT_PCM                                          ;
}

void CaInitializeWaveFormatEx       (
       CaWaveFormat * waveFormat    ,
       int            numChannels   ,
       CaSampleFormat sampleFormat  ,
       int            waveFormatTag ,
       double         sampleRate    )
{
  WAVEFORMATEX * waveFormatEx   = (WAVEFORMATEX *)waveFormat        ;
  int            bytesPerSample = Core::SampleSize ( sampleFormat ) ;
  unsigned long  bytesPerFrame  = numChannels * bytesPerSample      ;
  ///////////////////////////////////////////////////////////////////
  waveFormatEx -> wFormatTag       = waveFormatTag                  ;
  waveFormatEx -> nChannels        = (WORD)numChannels              ;
  waveFormatEx -> nSamplesPerSec   = (DWORD)sampleRate              ;
  waveFormatEx -> nAvgBytesPerSec  = waveFormatEx->nSamplesPerSec   ;
  waveFormatEx -> nAvgBytesPerSec *= bytesPerFrame                  ;
  waveFormatEx -> nBlockAlign      = (WORD)bytesPerFrame            ;
  waveFormatEx -> wBitsPerSample   = bytesPerSample * 8             ;
  waveFormatEx -> cbSize           = 0                              ;
}

void CaInitializeWaveFormatExtensible        (
       CaWaveFormat          * waveFormat    ,
       int                     numChannels   ,
       CaSampleFormat          sampleFormat  ,
       int                     waveFormatTag ,
       double                  sampleRate    ,
       CaWaveFormatChannelMask channelMask   )
{
  WAVEFORMATEX * waveFormatEx   = (WAVEFORMATEX *)waveFormat                 ;
  int            bytesPerSample = Core::SampleSize ( sampleFormat )          ;
  unsigned long  bytesPerFrame  = numChannels * bytesPerSample               ;
  GUID           guid                                                        ;
  ////////////////////////////////////////////////////////////////////////////
  waveFormatEx -> wFormatTag       = WAVE_FORMAT_EXTENSIBLE                  ;
  waveFormatEx -> nChannels        = (WORD)numChannels                       ;
  waveFormatEx -> nSamplesPerSec   = (DWORD)sampleRate                       ;
  waveFormatEx -> nAvgBytesPerSec  = waveFormatEx->nSamplesPerSec            ;
  waveFormatEx -> nAvgBytesPerSec *= bytesPerFrame                           ;
  waveFormatEx -> nBlockAlign      = (WORD)bytesPerFrame                     ;
  waveFormatEx -> wBitsPerSample   = bytesPerSample * 8                      ;
  waveFormatEx -> cbSize           = 22                                      ;
  ////////////////////////////////////////////////////////////////////////////
  *((WORD *)&waveFormat->fields[CA_INDEXOF_WVALIDBITSPERSAMPLE]) = waveFormatEx->wBitsPerSample ;
  *((DWORD*)&waveFormat->fields[CA_INDEXOF_DWCHANNELMASK      ]) = channelMask                  ;
  ////////////////////////////////////////////////////////////////////////////
  guid         = ksDataFormatSubtypeGuidBase                                 ;
  guid . Data1 = (USHORT)waveFormatTag                                       ;
  *((GUID*)&waveFormat->fields[CA_INDEXOF_SUBFORMAT]) = guid                 ;
}

CaWaveFormatChannelMask CaDefaultChannelMask(int numChannels)
{
  switch ( numChannels )                                                     {
    case 1                                                                   :
    return CA_SPEAKER_MONO                                                   ;
    case 2                                                                   :
    return CA_SPEAKER_STEREO                                                 ;
    case 3                                                                   :
    return CA_SPEAKER_FRONT_LEFT                                             |
           CA_SPEAKER_FRONT_CENTER                                           |
           CA_SPEAKER_FRONT_RIGHT                                            ;
    case 4                                                                   :
    return CA_SPEAKER_QUAD                                                   ;
    case 5                                                                   :
    return CA_SPEAKER_QUAD | CA_SPEAKER_FRONT_CENTER                         ;
    case 6                                                                   :
      /* The meaning of the PAWIN_SPEAKER_5POINT1 flag has changed over time:
         http://msdn2.microsoft.com/en-us/library/aa474707.aspx
         We use CA_SPEAKER_5POINT1 (not CA_SPEAKER_5POINT1_SURROUND)
         because on some cards (eg Audigy) PAWIN_SPEAKER_5POINT1_SURROUND
         results in a virtual mixdown placing the rear output in the
         front _and_ rear speakers.                                         */
    return CA_SPEAKER_5POINT1                                                ;
    /* case 7:                                                              */
    case 8                                                                   :
       /* RoBi: PAWIN_SPEAKER_7POINT1_SURROUND fits normal surround sound setups better than PAWIN_SPEAKER_7POINT1, f.i. NVidia HDMI Audio
          output is silent on channels 5&6 with NVidia drivers, and channel 7&8 with Micrsoft HD Audio driver using PAWIN_SPEAKER_7POINT1.
          With PAWIN_SPEAKER_7POINT1_SURROUND both setups work OK.          */
    return CA_SPEAKER_7POINT1_SURROUND                                       ;
  }
  /* Apparently some Audigy drivers will output silence
     if the direct-out constant (0) is used. So this is not ideal.
     RoBi 2012-12-19: Also, NVidia driver seem to output garbage instead. Again not very ideal.
  */
  return CA_SPEAKER_DIRECTOUT                                                ;
  /* Note that Alec Rogers proposed the following as an alternate method to
     generate the default channel mask, however it doesn't seem to be an improvement
     over the above, since some drivers will matrix outputs mapping to non-present
     speakers accross multiple physical speakers.

     if ( nChannels == 1 ) {
       pwfFormat->dwChannelMask = SPEAKER_FRONT_CENTER;
     } else {
       pwfFormat->dwChannelMask = 0;
       for(i=0; i<nChannels; i++)
         pwfFormat->dwChannelMask = (pwfFormat->dwChannelMask << 1) | 0x1;
     }
  */
}

//////////////////////////////////////////////////////////////////////////////

#if !defined(PA_WDMKS_NO_KSGUID_LIB) && !defined(PAWIN_WDMKS_NO_KSGUID_LIB) && !defined(__GNUC__)
  #if (defined(WIN32) && (defined(_MSC_VER) && (_MSC_VER >= 1200))) /* MSC version 6 and above */
    #pragma comment( lib, "ksguid.lib" )
  #endif
  #define Ca_KSDATAFORMAT_TYPE_AUDIO            KSDATAFORMAT_TYPE_AUDIO
  #define Ca_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT    KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
  #define Ca_KSDATAFORMAT_SUBTYPE_PCM           KSDATAFORMAT_SUBTYPE_PCM
  #define Ca_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX  KSDATAFORMAT_SUBTYPE_WAVEFORMATEX
  #define Ca_KSMEDIUMSETID_Standard             KSMEDIUMSETID_Standard
  #define Ca_KSINTERFACESETID_Standard          KSINTERFACESETID_Standard
  #define Ca_KSPROPSETID_Pin                    KSPROPSETID_Pin
#else
  static const GUID Ca_KSDATAFORMAT_TYPE_AUDIO            = { STATIC_KSDATAFORMAT_TYPE_AUDIO           } ;
  static const GUID Ca_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT    = { STATIC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT   } ;
  static const GUID Ca_KSDATAFORMAT_SUBTYPE_PCM           = { STATIC_KSDATAFORMAT_SUBTYPE_PCM          } ;
  static const GUID Ca_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX  = { STATIC_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX } ;
  static const GUID Ca_KSMEDIUMSETID_Standard             = { STATIC_KSMEDIUMSETID_Standard            } ;
  static const GUID Ca_KSINTERFACESETID_Standard          = { STATIC_KSINTERFACESETID_Standard         } ;
  static const GUID Ca_KSPROPSETID_Pin                    = { STATIC_KSPROPSETID_Pin                   } ;
#endif

#define Ca_IS_VALID_WAVEFORMATEX_GUID(Guid) \
  (!memcmp(((PUSHORT)&Ca_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX) + 1, ((PUSHORT)(Guid)) + 1, sizeof(GUID) - sizeof(USHORT)))

static CaError WdmGetPinPropertySimple (
                 HANDLE   handle       ,
                 CaUint32 pinId        ,
                 CaUint32 property     ,
                 void   * value        ,
                 CaUint32 valueSize    )
{
  DWORD   bytesReturned                                                      ;
  KSP_PIN ksPProp                                                            ;
  ////////////////////////////////////////////////////////////////////////////
  ksPProp . Property . Set   = Ca_KSPROPSETID_Pin                            ;
  ksPProp . Property . Id    = property                                      ;
  ksPProp . Property . Flags = KSPROPERTY_TYPE_GET                           ;
  ksPProp . PinId            = pinId                                         ;
  ksPProp . Reserved         = 0                                             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( ::DeviceIoControl                                                   (
             handle                                                          ,
             IOCTL_KS_PROPERTY                                               ,
             &ksPProp                                                        ,
             sizeof(KSP_PIN)                                                 ,
             value                                                           ,
             valueSize                                                       ,
             &bytesReturned                                                  ,
             NULL ) == 0 )                                                  ||
       ( bytesReturned != valueSize )                                      ) {
    return UnanticipatedHostError                                            ;
  } else                                                                     {
    return NoError                                                           ;
  }                                                                          ;
}

static CaError WdmGetPinPropertyMulti              (
                 HANDLE             handle         ,
                 CaUint32           pinId          ,
                 CaUint32           property       ,
                 KSMULTIPLE_ITEM ** ksMultipleItem )
{
  CaUint32 multipleItemSize = 0                                              ;
  KSP_PIN  ksPProp                                                           ;
  DWORD    bytesReturned                                                     ;
  ////////////////////////////////////////////////////////////////////////////
  *ksMultipleItem = NULL                                                     ;
  ////////////////////////////////////////////////////////////////////////////
  ksPProp . Property . Set   = Ca_KSPROPSETID_Pin                            ;
  ksPProp . Property . Id    = property                                      ;
  ksPProp . Property . Flags = KSPROPERTY_TYPE_GET                           ;
  ksPProp . PinId            = pinId                                         ;
  ksPProp . Reserved         = 0                                             ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ::DeviceIoControl                                                     (
           handle                                                            ,
           IOCTL_KS_PROPERTY                                                 ,
           &ksPProp.Property                                                 ,
           sizeof(KSP_PIN)                                                   ,
           NULL                                                              ,
           0                                                                 ,
           (LPDWORD)&multipleItemSize                                        ,
           NULL ) == 0 && ( GetLastError() != ERROR_MORE_DATA ) )            {
    return UnanticipatedHostError                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  *ksMultipleItem = (KSMULTIPLE_ITEM *)Allocator::allocate(multipleItemSize) ;
  if ( !*ksMultipleItem ) return InsufficientMemory                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ::DeviceIoControl                                                     (
           handle                                                            ,
           IOCTL_KS_PROPERTY                                                 ,
           &ksPProp                                                          ,
           sizeof(KSP_PIN)                                                   ,
           (void*)*ksMultipleItem                                            ,
           multipleItemSize                                                  ,
           &bytesReturned                                                    ,
           NULL ) == 0 || ( bytesReturned != multipleItemSize )            ) {
    Allocator::free ( ksMultipleItem )                                       ;
    return UnanticipatedHostError                                            ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  return NoError                                                             ;
}

static int GetKSFilterPinCount(HANDLE deviceHandle)
{
  DWORD result                         ;
  if ( WdmGetPinPropertySimple         (
         deviceHandle                  ,
         0                             ,
         KSPROPERTY_PIN_CTYPES         ,
         &result                       ,
         sizeof(result) ) == NoError ) {
    return result                      ;
  }                                    ;
  return 0                             ;
}

static KSPIN_COMMUNICATION GetKSFilterPinPropertyCommunication (
                             HANDLE deviceHandle               ,
                             int    pinId                      )
{
  KSPIN_COMMUNICATION result           ;
  if ( WdmGetPinPropertySimple         (
         deviceHandle                  ,
         pinId                         ,
         KSPROPERTY_PIN_COMMUNICATION  ,
         &result                       ,
         sizeof(result) ) == NoError ) {
    return result                      ;
  }                                    ;
  return KSPIN_COMMUNICATION_NONE      ;
}

static KSPIN_DATAFLOW GetKSFilterPinPropertyDataflow (
                        HANDLE deviceHandle          ,
                        int    pinId                 )
{
  KSPIN_DATAFLOW result                ;
  if ( WdmGetPinPropertySimple         (
         deviceHandle                  ,
         pinId                         ,
         KSPROPERTY_PIN_DATAFLOW       ,
         &result                       ,
         sizeof(result) ) == NoError ) {
    return result                      ;
  }                                    ;
  return (KSPIN_DATAFLOW) 0            ;
}

static int KSFilterPinPropertyIdentifiersInclude (
             HANDLE       deviceHandle           ,
             int          pinId                  ,
             CaUint32     property               ,
             const GUID * identifierSet          ,
             CaUint32     identifierId           )
{
  KSMULTIPLE_ITEM * item   = NULL           ;
  KSIDENTIFIER    * identifier              ;
  int               i                       ;
  int               result = 0              ;
  ///////////////////////////////////////////
  if ( WdmGetPinPropertyMulti               (
         deviceHandle                       ,
         pinId                              ,
         property                           ,
         &item) != NoError )                {
    return 0                                ;
  }                                         ;
  ///////////////////////////////////////////
  identifier = (KSIDENTIFIER *)( item + 1 ) ;
  ///////////////////////////////////////////
  for ( i = 0; i < (int)item->Count ; i++ ) {
    if ( !::memcmp                          (
           (void*)&identifier[i].Set        ,
           (void*)identifierSet             ,
           sizeof( GUID )                ) &&
         (identifier[i].Id==identifierId))  {
      result = 1                            ;
      break                                 ;
    }                                       ;
  }                                         ;
  ///////////////////////////////////////////
  Allocator::free ( item )                  ;
  ///////////////////////////////////////////
  return result                             ;
}

/* return the maximum channel count supported by any pin on the device.
   if isInput is non-zero we query input pins, otherwise output pins.
*/

int CaWdmksQueryFilterMaximumChannelCount(void * wcharDevicePath,int isInput)
{
  HANDLE         deviceHandle                                                ;
  ULONG          i                                                           ;
  int            pinCount                                                    ;
  int            pinId                                                       ;
  int            result = 0                                                  ;
  KSPIN_DATAFLOW requiredDataflowDirection                                   ;
  ////////////////////////////////////////////////////////////////////////////
  requiredDataflowDirection=(isInput ? KSPIN_DATAFLOW_OUT:KSPIN_DATAFLOW_IN) ;
  if ( NULL == wcharDevicePath ) return 0                                    ;
  ////////////////////////////////////////////////////////////////////////////
  deviceHandle = ::CreateFileW                                               (
                    (LPCWSTR)wcharDevicePath                                 ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE                       ,
                    0                                                        ,
                    NULL                                                     ,
                    OPEN_EXISTING                                            ,
                    0                                                        ,
                    NULL                                                   ) ;
  if ( INVALID_HANDLE_VALUE == deviceHandle ) return 0                       ;
  pinCount = GetKSFilterPinCount ( deviceHandle )                            ;
  ////////////////////////////////////////////////////////////////////////////
  for ( pinId = 0 ; pinId < pinCount ; ++pinId )                             {
    KSPIN_COMMUNICATION communication = GetKSFilterPinPropertyCommunication ( deviceHandle , pinId ) ;
    KSPIN_DATAFLOW      dataflow      = GetKSFilterPinPropertyDataflow      ( deviceHandle , pinId ) ;
    if (   ( dataflow      == requiredDataflowDirection                   ) &&
         ( ( communication == KSPIN_COMMUNICATION_SINK                    ) ||
           ( communication == KSPIN_COMMUNICATION_BOTH                  ) ) &&
           ( KSFilterPinPropertyIdentifiersInclude                           (
               deviceHandle                                                  ,
               pinId                                                         ,
               KSPROPERTY_PIN_INTERFACES                                     ,
               &Ca_KSINTERFACESETID_Standard                                 ,
               KSINTERFACE_STANDARD_STREAMING                             ) ||
             KSFilterPinPropertyIdentifiersInclude                           (
               deviceHandle                                                  ,
               pinId                                                         ,
               KSPROPERTY_PIN_INTERFACES                                     ,
               &Ca_KSINTERFACESETID_Standard                                 ,
               KSINTERFACE_STANDARD_LOOPED_STREAMING                    ) ) &&
             KSFilterPinPropertyIdentifiersInclude                           (
               deviceHandle                                                  ,
               pinId                                                         ,
               KSPROPERTY_PIN_MEDIUMS                                        ,
               &Ca_KSMEDIUMSETID_Standard                                    ,
               KSMEDIUM_STANDARD_DEVIO                                   ) ) {
      KSMULTIPLE_ITEM * item = NULL                                          ;
      if ( WdmGetPinPropertyMulti                                            (
             deviceHandle                                                    ,
             pinId                                                           ,
             KSPROPERTY_PIN_DATARANGES                                       ,
             &item ) == NoError                                            ) {
        KSDATARANGE * dataRange = (KSDATARANGE *) ( item + 1 )               ;
        for ( i=0 ; i < item->Count ; ++i )                                  {
          if ( Ca_IS_VALID_WAVEFORMATEX_GUID(&dataRange->SubFormat)         ||
               ::memcmp( (void *)&dataRange->SubFormat                       ,
                         (void *)&Ca_KSDATAFORMAT_SUBTYPE_PCM                ,
                         sizeof(GUID) ) == 0                                ||
               ::memcmp( (void*)&dataRange->SubFormat                        ,
                         (void*)&Ca_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT          ,
                         sizeof(GUID) ) == 0                                ||
           ( ( ::memcmp( (void*)&dataRange->MajorFormat                      ,
                         (void*)&Ca_KSDATAFORMAT_TYPE_AUDIO                  ,
                         sizeof(GUID) ) == 0                              ) &&
             ( ::memcmp( (void*)&dataRange->SubFormat                        ,
                         (void*)&KSDATAFORMAT_SUBTYPE_WILDCARD               ,
                         sizeof(GUID) ) == 0                           ) ) ) {
            KSDATARANGE_AUDIO * dataRangeAudio                               ;
            dataRangeAudio = (KSDATARANGE_AUDIO *)dataRange                  ;
            /*
              printf( ">>> %d %d %d %d %S\n", isInput, dataflow, communication, dataRangeAudio->MaximumChannels, devicePath );

              if ( memcmp((void*)&dataRange->Specifier, (void*)&KSDATAFORMAT_SPECIFIER_WAVEFORMATEX, sizeof(GUID) ) == 0 )
                printf( "\tspecifier: KSDATAFORMAT_SPECIFIER_WAVEFORMATEX\n" );
              else
              if ( memcmp((void*)&dataRange->Specifier, (void*)&KSDATAFORMAT_SPECIFIER_DSOUND, sizeof(GUID) ) == 0 )
                printf( "\tspecifier: KSDATAFORMAT_SPECIFIER_DSOUND\n" );
              else
              if ( memcmp((void*)&dataRange->Specifier, (void*)&KSDATAFORMAT_SPECIFIER_WILDCARD, sizeof(GUID) ) == 0 )
                printf( "\tspecifier: KSDATAFORMAT_SPECIFIER_WILDCARD\n" );
              else
                printf( "\tspecifier: ?\n" );
             */
             /*
               We assume that very high values for MaximumChannels are not useful and indicate
               that the driver isn't prepared to tell us the real number of channels which it supports.
              */
            if ( ( dataRangeAudio->MaximumChannels  < 0xFFFFUL )            &&
                 (int)dataRangeAudio->MaximumChannels > result )             {
              result = (int)dataRangeAudio->MaximumChannels                  ;
            }                                                                ;
          }                                                                  ;
          dataRange = (KSDATARANGE *)(((char *)dataRange)+dataRange->FormatSize) ;
        }                                                                    ;
        Allocator::free ( item )                                             ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  ::CloseHandle ( deviceHandle )                                             ;
  ////////////////////////////////////////////////////////////////////////////
  return result                                                              ;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
