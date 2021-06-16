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


#ifndef CAWINDOWS_HPP
#define CAWINDOWS_HPP

///////////////////////////////////////////////////////////////////////////////

#include "CiosAudioPrivate.hpp"

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <process.h>

bool IsEarlierThanVista(void) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif /* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

///////////////////////////////////////////////////////////////////////////////

typedef struct CaComInitializationResult {
  int state                              ;
  int initializingThreadId               ;
} CaComInitializationResult              ;

CaError CaComInitialize   ( CaHostApiTypeId             hostApiType               ,
                            CaComInitializationResult * comInitializationResult ) ;
void    CaComUninitialize ( CaHostApiTypeId             hostApiType,
                            CaComInitializationResult * comInitializationResult ) ;

///////////////////////////////////////////////////////////////////////////////

void CaInitializeX86PlainConverters ( void ) ;

///////////////////////////////////////////////////////////////////////////////

/*
    The following #defines for speaker channel masks are the same
    as those in ksmedia.h, except with CA_ prepended, KSAUDIO_ removed
    in some cases, and casts to CaWaveFormatChannelMask added.
*/

typedef unsigned long CaWaveFormatChannelMask ;

/* Speaker Positions: */
#define CA_SPEAKER_FRONT_LEFT            ((CaWaveFormatChannelMask)0x00001)
#define CA_SPEAKER_FRONT_RIGHT           ((CaWaveFormatChannelMask)0x00002)
#define CA_SPEAKER_FRONT_CENTER          ((CaWaveFormatChannelMask)0x00004)
#define CA_SPEAKER_LOW_FREQUENCY         ((CaWaveFormatChannelMask)0x00008)
#define CA_SPEAKER_BACK_LEFT             ((CaWaveFormatChannelMask)0x00010)
#define CA_SPEAKER_BACK_RIGHT            ((CaWaveFormatChannelMask)0x00020)
#define CA_SPEAKER_FRONT_LEFT_OF_CENTER  ((CaWaveFormatChannelMask)0x00040)
#define CA_SPEAKER_FRONT_RIGHT_OF_CENTER ((CaWaveFormatChannelMask)0x00080)
#define CA_SPEAKER_BACK_CENTER           ((CaWaveFormatChannelMask)0x00100)
#define CA_SPEAKER_SIDE_LEFT             ((CaWaveFormatChannelMask)0x00200)
#define CA_SPEAKER_SIDE_RIGHT            ((CaWaveFormatChannelMask)0x00400)
#define CA_SPEAKER_TOP_CENTER            ((CaWaveFormatChannelMask)0x00800)
#define CA_SPEAKER_TOP_FRONT_LEFT        ((CaWaveFormatChannelMask)0x01000)
#define CA_SPEAKER_TOP_FRONT_CENTER      ((CaWaveFormatChannelMask)0x02000)
#define CA_SPEAKER_TOP_FRONT_RIGHT       ((CaWaveFormatChannelMask)0x04000)
#define CA_SPEAKER_TOP_BACK_LEFT         ((CaWaveFormatChannelMask)0x08000)
#define CA_SPEAKER_TOP_BACK_CENTER       ((CaWaveFormatChannelMask)0x10000)
#define CA_SPEAKER_TOP_BACK_RIGHT        ((CaWaveFormatChannelMask)0x20000)

/* Bit mask locations reserved for future use */
#define CA_SPEAKER_RESERVED              ((CaWaveFormatChannelMask)0x7FFC0000)

/* Used to specify that any possible permutation of speaker configurations */
#define CA_SPEAKER_ALL                   ((CaWaveFormatChannelMask)0x80000000)

/* DirectSound Speaker Config */
#define CA_SPEAKER_DIRECTOUT             0
#define CA_SPEAKER_MONO                  ( CA_SPEAKER_FRONT_CENTER          )
#define CA_SPEAKER_STEREO                ( CA_SPEAKER_FRONT_LEFT            | \
                                           CA_SPEAKER_FRONT_RIGHT           )
#define CA_SPEAKER_QUAD                  ( CA_SPEAKER_FRONT_LEFT            | \
                                           CA_SPEAKER_FRONT_RIGHT           | \
                                           CA_SPEAKER_BACK_LEFT             | \
                                           CA_SPEAKER_BACK_RIGHT            )
#define CA_SPEAKER_SURROUND              ( CA_SPEAKER_FRONT_LEFT            | \
                                           CA_SPEAKER_FRONT_RIGHT           | \
                                           CA_SPEAKER_FRONT_CENTER          | \
                                           CA_SPEAKER_BACK_CENTER           )
#define CA_SPEAKER_5POINT1               ( CA_SPEAKER_FRONT_LEFT            | \
                                           CA_SPEAKER_FRONT_RIGHT           | \
                                           CA_SPEAKER_FRONT_CENTER          | \
                                           CA_SPEAKER_LOW_FREQUENCY         | \
                                           CA_SPEAKER_BACK_LEFT             | \
                                           CA_SPEAKER_BACK_RIGHT            )
#define CA_SPEAKER_7POINT1               ( CA_SPEAKER_FRONT_LEFT            | \
                                           CA_SPEAKER_FRONT_RIGHT           | \
                                           CA_SPEAKER_FRONT_CENTER          | \
                                           CA_SPEAKER_LOW_FREQUENCY         | \
                                           CA_SPEAKER_BACK_LEFT             | \
                                           CA_SPEAKER_BACK_RIGHT            | \
                                           CA_SPEAKER_FRONT_LEFT_OF_CENTER  | \
                                           CA_SPEAKER_FRONT_RIGHT_OF_CENTER )
#define CA_SPEAKER_5POINT1_SURROUND      ( CA_SPEAKER_FRONT_LEFT            | \
                                           CA_SPEAKER_FRONT_RIGHT           | \
                                           CA_SPEAKER_FRONT_CENTER          | \
                                           CA_SPEAKER_LOW_FREQUENCY         | \
                                           CA_SPEAKER_SIDE_LEFT             | \
                                           CA_SPEAKER_SIDE_RIGHT            )
#define CA_SPEAKER_7POINT1_SURROUND      ( CA_SPEAKER_FRONT_LEFT            | \
                                           CA_SPEAKER_FRONT_RIGHT           | \
                                           CA_SPEAKER_FRONT_CENTER          | \
                                           CA_SPEAKER_LOW_FREQUENCY         | \
                                           CA_SPEAKER_BACK_LEFT             | \
                                           CA_SPEAKER_BACK_RIGHT            | \
                                           CA_SPEAKER_SIDE_LEFT             | \
                                           CA_SPEAKER_SIDE_RIGHT            )
/*
 According to the Microsoft documentation:
 The following are obsolete 5.1 and 7.1 settings (they lack side speakers).  Note this means
 that the default 5.1 and 7.1 settings (KSAUDIO_SPEAKER_5POINT1 and KSAUDIO_SPEAKER_7POINT1 are
 similarly obsolete but are unchanged for compatibility reasons).
*/
#define CA_SPEAKER_5POINT1_BACK          CA_SPEAKER_5POINT1
#define CA_SPEAKER_7POINT1_WIDE          CA_SPEAKER_7POINT1

/* DVD Speaker Positions */
#define CA_SPEAKER_GROUND_FRONT_LEFT     CA_SPEAKER_FRONT_LEFT
#define CA_SPEAKER_GROUND_FRONT_CENTER   CA_SPEAKER_FRONT_CENTER
#define CA_SPEAKER_GROUND_FRONT_RIGHT    CA_SPEAKER_FRONT_RIGHT
#define CA_SPEAKER_GROUND_REAR_LEFT      CA_SPEAKER_BACK_LEFT
#define CA_SPEAKER_GROUND_REAR_RIGHT     CA_SPEAKER_BACK_RIGHT
#define CA_SPEAKER_TOP_MIDDLE            CA_SPEAKER_TOP_CENTER
#define CA_SPEAKER_SUPER_WOOFER          CA_SPEAKER_LOW_FREQUENCY

/*
    CaWaveFormat is defined here to provide compatibility with
    compilation environments which don't have headers defining
    WAVEFORMATEXTENSIBLE (e.g. older versions of MSVC, Borland C++ etc.

    The fields for WAVEFORMATEX and WAVEFORMATEXTENSIBLE are declared as an
    unsigned char array here to avoid clients who include this file having
    a dependency on windows.h and mmsystem.h, and also to to avoid having
    to write separate packing pragmas for each compiler.
*/

#define CA_SIZEOF_WAVEFORMATEX           18
#define CA_SIZEOF_WAVEFORMATEXTENSIBLE   ( CA_SIZEOF_WAVEFORMATEX + 22 )

typedef struct                                            {
  unsigned char fields [ CA_SIZEOF_WAVEFORMATEXTENSIBLE ] ;
  unsigned long extraLongForAlignment                     ;
  /* ensure that compiler aligns struct to DWORD         */
} CaWaveFormat                                            ;

/*
    WAVEFORMATEXTENSIBLE fields:

    union  {
        WORD  wValidBitsPerSample;
        WORD  wSamplesPerBlock;
        WORD  wReserved;
    } Samples;
    DWORD  dwChannelMask;
    GUID  SubFormat;
*/

#define CA_INDEXOF_WVALIDBITSPERSAMPLE   ( CA_SIZEOF_WAVEFORMATEX + 0 )
#define CA_INDEXOF_DWCHANNELMASK         ( CA_SIZEOF_WAVEFORMATEX + 2 )
#define CA_INDEXOF_SUBFORMAT             ( CA_SIZEOF_WAVEFORMATEX + 6 )

/*
    Valid values to pass for the waveFormatTag CA_InitializeWaveFormatEx and
    CA_InitializeWaveFormatExtensible functions below. These must match
    the standard Windows WAVE_FORMAT_* values.
*/
#define CA_WAVE_FORMAT_PCM               (      1 )
#define CA_WAVE_FORMAT_IEEE_FLOAT        (      3 )
#define CA_WAVE_FORMAT_DOLBY_AC3_SPDIF   ( 0x0092 )
#define CA_WAVE_FORMAT_WMA_SPDIF         ( 0x0164 )

/*
    returns CA_WAVE_FORMAT_PCM or CA_WAVE_FORMAT_IEEE_FLOAT
    depending on the sampleFormat parameter.
*/

int CaSampleFormatToLinearWaveFormatTag(CaSampleFormat sampleFormat) ;

/*
    Use the following two functions to initialize the waveformat structure.
*/

void CaInitializeWaveFormatEx       (
       CaWaveFormat * waveFormat    ,
       int            numChannels   ,
       CaSampleFormat sampleFormat  ,
       int            waveFormatTag ,
       double         sampleRate  ) ;


void CaInitializeWaveFormatExtensible        (
       CaWaveFormat          * waveFormat    ,
       int                     numChannels   ,
       CaSampleFormat          sampleFormat  ,
       int                     waveFormatTag ,
       double                  sampleRate    ,
       CaWaveFormatChannelMask channelMask ) ;


/* Map a channel count to a speaker channel mask */
CaWaveFormatChannelMask CaDefaultChannelMask(int numChannels) ;

///////////////////////////////////////////////////////////////////////////////

int CaWdmksQueryFilterMaximumChannelCount(void * wcharDevicePath,int isInput) ;

///////////////////////////////////////////////////////////////////////////////

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

///////////////////////////////////////////////////////////////////////////////

#endif // CAWINDOWS_HPP
