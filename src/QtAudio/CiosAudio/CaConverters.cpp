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

////////////////////////////////////////////////////////////////////////////////

#define CA_SELECT_FORMAT_( format, float32, int32, int24, int16, int8, uint8 ) \
  switch ( ((int)format) & ~cafNonInterleaved ) {                              \
    case cafFloat32:                                                           \
        float32                                                                \
    case cafInt32:                                                             \
        int32                                                                  \
    case cafInt24:                                                             \
        int24                                                                  \
    case cafInt16:                                                             \
        int16                                                                  \
    case cafInt8:                                                              \
        int8                                                                   \
    case cafUint8:                                                             \
        uint8                                                                  \
    default: return NULL ;                                                     \
  }

////////////////////////////////////////////////////////////////////////////////

#define CA_SELECT_CONVERTER_DITHER_CLIP_( flags, source, destination )         \
  if ( flags & csfClipOff ) { /* no clip */                                    \
    if ( flags & csfDitherOff ) { /* no dither */                              \
      return caConverters . source ## _To_ ## destination                    ; \
    } else { /* dither */                                                      \
      return caConverters . source ## _To_ ## destination ## _Dither         ; \
    }                                                                          \
  } else { /* clip */                                                          \
    if ( flags & csfDitherOff ) { /* no dither */                              \
      return caConverters. source ## _To_ ## destination ## _Clip            ; \
    } else { /* dither */                                                      \
      return caConverters. source ## _To_ ## destination ## _DitherClip      ; \
    }                                                                          \
  }

////////////////////////////////////////////////////////////////////////////////

#define CA_SELECT_CONVERTER_DITHER_( flags, source, destination )              \
  if ( flags & csfDitherOff ){ /* no dither */                                 \
    return caConverters . source ## _To_ ## destination                      ; \
  } else { /* dither */                                                        \
    return caConverters . source ## _To_ ## destination ## _Dither           ; \
  }

////////////////////////////////////////////////////////////////////////////////

#define CA_USE_CONVERTER_( source, destination )     \
  return caConverters. source ## _To_ ## destination ;

////////////////////////////////////////////////////////////////////////////////

#define CA_UNITY_CONVERSION_( wordlength )                       \
  return caConverters. Copy_ ## wordlength ## _To_ ## wordlength ;

////////////////////////////////////////////////////////////////////////////////

#define CA_CLIP_( val, min, max )                                        \
    { val = ((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val)); }

static const float  const_1_div_128_        = 1.0f /        128.0f ; /*  8 bit multiplier */
static const float  const_1_div_32768_      = 1.0f /      32768.0f ; /* 16 bit multiplier */
static const double const_1_div_2147483648_ = 1.0  / 2147483648.0  ; /* 32 bit multiplier */

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int32                 (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaFloat32 * src  = (CaFloat32 *) sourceBuffer      ;
  CaInt32   * dest = (CaInt32   *) destinationBuffer ;
  ////////////////////////////////////////////////////
  while ( count-- )                                  {
    double scaled = (*src) * 0x7FFFFFFF              ;
    *dest = (CaInt32) scaled                         ;
    src  += sourceStride                             ;
    dest += destinationStride                        ;
  }                                                  ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int32_Dither          (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32 * src  = (CaFloat32 *) sourceBuffer                       ;
  CaInt32   * dest = (CaInt32   *) destinationBuffer                  ;
  /////////////////////////////////////////////////////////////////////
  while ( count-- )                                                   {
    double di  = dither->DitherFloat()                                ;
    /* use smaller scaler to prevent overflow when we add the dither */
    double dithered = (((double)*src) * (2147483646.0)) + di          ;
    *dest = (CaInt32) dithered                                        ;
    src  += sourceStride                                              ;
    dest += destinationStride                                         ;
  }                                                                   ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int32_Clip            (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaFloat32 * src  = (CaFloat32 *) sourceBuffer      ;
  CaInt32   * dest = (CaInt32   *) destinationBuffer ;
  ////////////////////////////////////////////////////
  while ( count-- )                                  {
    double scaled = (*src) * 0x7FFFFFFF              ;
    CA_CLIP_( scaled, -2147483648.0, 2147483647.0  ) ;
    *dest = (CaInt32) scaled                         ;
    src  += sourceStride                             ;
    dest += destinationStride                        ;
  }                                                  ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int32_DitherClip      (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32 * src  = (CaFloat32 *) sourceBuffer                       ;
  CaInt32   * dest = (CaInt32   *) destinationBuffer                  ;
  /////////////////////////////////////////////////////////////////////
  while ( count-- )                                                   {
    double di = dither->DitherFloat()                                 ;
    /* use smaller scaler to prevent overflow when we add the dither */
    double dithered = (((double)*src) * (2147483646.0)) + di          ;
    CA_CLIP_( dithered, -2147483648.0, 2147483647.0  )                ;
    *dest = (CaInt32) dithered                                        ;
    src  += sourceStride                                              ;
    dest += destinationStride                                         ;
  }                                                                   ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int24                 (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaFloat32     * src  = (CaFloat32     *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  int             ds   = destinationStride * 3               ;
  CaInt32         temp                                       ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    /* convert to 32 bit and drop the low 8 bits            */
    double scaled = ((double)(*src)) * 2147483647.0          ;
    temp    = (CaInt32) scaled                               ;
    #if defined(CA_LITTLE_ENDIAN)
    dest[0] = (unsigned char)(temp >>  8)                    ;
    dest[1] = (unsigned char)(temp >> 16)                    ;
    dest[2] = (unsigned char)(temp >> 24)                    ;
    #elif defined(CA_BIG_ENDIAN)
    dest[0] = (unsigned char)(temp >> 24)                    ;
    dest[1] = (unsigned char)(temp >> 16)                    ;
    dest[2] = (unsigned char)(temp >>  8)                    ;
    #endif
    src    += sourceStride                                   ;
    dest   += ds                                             ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int24_Dither          (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32     * src  = (CaFloat32    *) sourceBuffer                ;
  unsigned char * dest = (unsigned char*) destinationBuffer           ;
  int             ds   = destinationStride * 3                        ;
  CaInt32         temp                                                ;
  /////////////////////////////////////////////////////////////////////
  while ( count-- )                                                   {
    /* convert to 32 bit and drop the low 8 bits                     */
    double di       = dither->DitherFloat()                           ;
    /* use smaller scaler to prevent overflow when we add the dither */
    double dithered = (((double)*src) * (2147483646.0)) + di          ;
    temp    = (CaInt32) dithered                                      ;
    #if defined(CA_LITTLE_ENDIAN)
    dest[0] = (unsigned char)(temp >>  8)                             ;
    dest[1] = (unsigned char)(temp >> 16)                             ;
    dest[2] = (unsigned char)(temp >> 24)                             ;
    #elif defined(CA_BIG_ENDIAN)
    dest[0] = (unsigned char)(temp >> 24)                             ;
    dest[1] = (unsigned char)(temp >> 16)                             ;
    dest[2] = (unsigned char)(temp >>  8)                             ;
    #endif
    src    += sourceStride                                            ;
    dest   += ds                                                      ;
  }                                                                   ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int24_Clip            (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaFloat32     * src  = (CaFloat32     *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  int             ds   = destinationStride * 3               ;
  CaInt32         temp                                       ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    /* convert to 32 bit and drop the low 8 bits            */
    double scaled = (*src) * 0x7FFFFFFF                      ;
    CA_CLIP_( scaled, -2147483648.0, 2147483647.0 )          ;
    temp    = (CaInt32) scaled                               ;
    #if defined(CA_LITTLE_ENDIAN)
    dest[0] = (unsigned char)(temp >>  8)                    ;
    dest[1] = (unsigned char)(temp >> 16)                    ;
    dest[2] = (unsigned char)(temp >> 24)                    ;
    #elif defined(CA_BIG_ENDIAN)
    dest[0] = (unsigned char)(temp >> 24)                    ;
    dest[1] = (unsigned char)(temp >> 16)                    ;
    dest[2] = (unsigned char)(temp >>  8)                    ;
    #endif
    src    += sourceStride                                   ;
    dest   += ds                                             ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int24_DitherClip      (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32     * src  = (CaFloat32     *) sourceBuffer              ;
  unsigned char * dest = (unsigned char *) destinationBuffer         ;
  int             ds   = destinationStride * 3                       ;
  CaInt32         temp                                               ;
  ////////////////////////////////////////////////////////////////////
  while ( count-- )                                                  {
    /* convert to 32 bit and drop the low 8 bits                    */
    double di       = dither->DitherFloat()                          ;
   /* use smaller scaler to prevent overflow when we add the dither */
    double dithered = (((double)*src) * (2147483646.0)) + di         ;
    CA_CLIP_( dithered, -2147483648.0, 2147483647.0 )                ;
    temp    = (CaInt32) dithered                                     ;
    #if defined(CA_LITTLE_ENDIAN)
    dest[0] = (unsigned char)(temp >>  8)                            ;
    dest[1] = (unsigned char)(temp >> 16)                            ;
    dest[2] = (unsigned char)(temp >> 24)                            ;
    #elif defined(CA_BIG_ENDIAN)
    dest[0] = (unsigned char)(temp >> 24)                            ;
    dest[1] = (unsigned char)(temp >> 16)                            ;
    dest[2] = (unsigned char)(temp >>  8)                            ;
    #endif
    src    += sourceStride                                           ;
    dest   += ds                                                     ;
  }                                                                  ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int16                 (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaFloat32 * src  = (CaFloat32 *) sourceBuffer      ;
  CaInt16   * dest = (CaInt16   *) destinationBuffer ;
  ////////////////////////////////////////////////////
  while ( count-- )                                  {
    short samp = (short) (*src * (32767.0f))         ;
    *dest = samp                                     ;
    src  += sourceStride                             ;
    dest += destinationStride                        ;
  }                                                  ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int16_Dither          (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32 * src  = (CaFloat32 *) sourceBuffer                       ;
  CaInt16   * dest = (CaInt16   *) destinationBuffer                  ;
  /////////////////////////////////////////////////////////////////////
  while ( count-- )                                                   {
    CaFloat32 di       = dither->DitherFloat()                        ;
    /* use smaller scaler to prevent overflow when we add the dither */
    CaFloat32 dithered = ((*src) * (32766.0f)) + di                   ;
    *dest = (CaInt16) dithered                                        ;
    src  += sourceStride                                              ;
    dest += destinationStride                                         ;
  }                                                                   ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int16_Clip       (
         void       * destinationBuffer ,
         signed   int destinationStride ,
         void       * sourceBuffer      ,
         signed   int sourceStride      ,
         unsigned int count             ,
         Dither     *                   )
{
  CaFloat32 * src  = (CaFloat32 *) sourceBuffer      ;
  CaInt16   * dest = (CaInt16   *) destinationBuffer ;
  ////////////////////////////////////////////////////
  while ( count-- )                                  {
    long samp = (CaInt32) ((*src) * (32767.0f))      ;
    CA_CLIP_( samp, -0x8000, 0x7FFF )                ;
    *dest = (CaInt16) samp                           ;
    src  += sourceStride                             ;
    dest += destinationStride                        ;
  }                                                  ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int16_DitherClip      (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32 * src  = (CaFloat32 *) sourceBuffer      ;
  CaInt16   * dest = (CaInt16   *) destinationBuffer ;
  ////////////////////////////////////////////////////
  while ( count-- )                                  {
    CaFloat32 di = dither->DitherFloat()             ;
    /* use smaller scaler to prevent overflow when we add the dither */
    CaFloat32 dithered = ((*src) * (32766.0f)) + di  ;
    CaInt32 samp = (CaInt32) dithered                ;
    CA_CLIP_( samp, -0x8000, 0x7FFF )                ;
    *dest = (CaInt16) samp                           ;
    src  += sourceStride                             ;
    dest += destinationStride                        ;
  }                                                  ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int8                  (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaFloat32   * src  = (CaFloat32   *) sourceBuffer      ;
  signed char * dest = (signed char *) destinationBuffer ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    signed char samp = (signed char) (*src * (127.0f))   ;
    *dest = samp                                         ;
    src  += sourceStride                                 ;
    dest += destinationStride                            ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int8_Dither           (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32   * src  = (CaFloat32   *) sourceBuffer      ;
  signed char * dest = (signed char *) destinationBuffer ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    CaFloat32 di       = dither->DitherFloat()           ;
    /* use smaller scaler to prevent overflow when we add the dither */
    CaFloat32 dithered = (*src * (126.0f)) + di          ;
    CaInt32   samp     = (CaInt32) dithered              ;
    *dest = (signed char) samp                           ;
    src  += sourceStride                                 ;
    dest += destinationStride                            ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int8_Clip             (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaFloat32   * src  = (CaFloat32   *) sourceBuffer      ;
  signed char * dest = (signed char *) destinationBuffer ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    CaInt32 samp = (CaInt32)((*src) * (127.0f))          ;
    CA_CLIP_( samp, -0x80, 0x7F )                        ;
    *dest = (signed char) samp                           ;
    src  += sourceStride                                 ;
    dest += destinationStride                            ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_Int8_DitherClip       (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32   * src  = (CaFloat32   *) sourceBuffer                   ;
  signed char * dest = (signed char *) destinationBuffer              ;
  /////////////////////////////////////////////////////////////////////
  while ( count-- )                                                   {
    CaFloat32 di       = dither->DitherFloat()                        ;
    /* use smaller scaler to prevent overflow when we add the dither */
    CaFloat32 dithered = ((*src) * (126.0f)) + di                     ;
    CaInt32   samp     = (CaInt32) dithered                           ;
    CA_CLIP_( samp, -0x80, 0x7F )                                     ;
    *dest = (signed char) samp                                        ;
    src  += sourceStride                                              ;
    dest += destinationStride                                         ;
  }                                                                   ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_UInt8                 (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaFloat32     * src  = (CaFloat32     *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    unsigned char samp = (unsigned char)(128 + ((unsigned char) (*src * (127.0f))));
    *dest = samp                                             ;
    src  += sourceStride                                     ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_UInt8_Dither          (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32     * src  = (CaFloat32     *) sourceBuffer               ;
  unsigned char * dest = (unsigned char *) destinationBuffer          ;
  /////////////////////////////////////////////////////////////////////
  while ( count-- )                                                   {
    CaFloat32 di       = dither->DitherFloat()                        ;
    /* use smaller scaler to prevent overflow when we add the dither */
    CaFloat32 dithered = (*src * (126.0f)) + di                       ;
    CaInt32   samp     = (CaInt32) dithered                           ;
    *dest = (unsigned char) (128 + samp)                              ;
    src  += sourceStride                                              ;
    dest += destinationStride                                         ;
  }                                                                   ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_UInt8_Clip            (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaFloat32     * src  = (CaFloat32     *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    CaInt32 samp = 128 + (CaInt32)(*src * (127.0f))          ;
    CA_CLIP_( samp, 0x0000, 0x00FF )                         ;
    *dest = (unsigned char) samp                             ;
    src  += sourceStride                                     ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Float32_To_UInt8_DitherClip      (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaFloat32     * src  = (CaFloat32     *) sourceBuffer               ;
  unsigned char * dest = (unsigned char *) destinationBuffer          ;
  /////////////////////////////////////////////////////////////////////
  while ( count-- )                                                   {
    CaFloat32 di       = dither->DitherFloat()                        ;
    /* use smaller scaler to prevent overflow when we add the dither */
    CaFloat32 dithered = (*src * (126.0f)) + di                       ;
    CaInt32   samp     = 128 + (CaInt32) dithered                     ;
    CA_CLIP_( samp, 0x0000, 0x00FF )                                  ;
    *dest = (unsigned char) samp                                      ;
    src  += sourceStride                                              ;
    dest += destinationStride                                         ;
  }                                                                   ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int32_To_Float32                 (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt32   * src  = (CaInt32   *) sourceBuffer                     ;
  CaFloat32 * dest = (CaFloat32 *) destinationBuffer                ;
  ///////////////////////////////////////////////////////////////////
  while ( count-- )                                                 {
    *dest = (CaFloat32)(((double)(*src)) * const_1_div_2147483648_) ;
    src  += sourceStride                                            ;
    dest += destinationStride                                       ;
  }                                                                 ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int32_To_Int24                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt32       * src  = (CaInt32       *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  int             ds   = destinationStride * 3               ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    /* REVIEW                                               */
    #if   defined(CA_LITTLE_ENDIAN)
    dest[0] = (unsigned char)(*src >>  8)                    ;
    dest[1] = (unsigned char)(*src >> 16)                    ;
    dest[2] = (unsigned char)(*src >> 24)                    ;
    #elif defined(CA_BIG_ENDIAN)
    dest[0] = (unsigned char)(*src >> 24)                    ;
    dest[1] = (unsigned char)(*src >> 16)                    ;
    dest[2] = (unsigned char)(*src >>  8)                    ;
    #endif
    src    += sourceStride                                   ;
    dest   += ds                                             ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int32_To_Int24_Dither (
              void       *        ,
              signed   int        ,
              void       *        ,
              signed   int        ,
              unsigned int        ,
              Dither     *        )
{
  /* IMPLEMENT ME */
}

////////////////////////////////////////////////////////////////////////////////

static void Int32_To_Int16                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt32 * src  = (CaInt32 *) sourceBuffer      ;
  CaInt16 * dest = (CaInt16 *) destinationBuffer ;
  ////////////////////////////////////////////////
  while ( count-- )                              {
    *dest = (CaInt16) ((*src) >> 16)             ;
    src  += sourceStride                         ;
    dest += destinationStride                    ;
  }                                              ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int32_To_Int16_Dither            (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaInt32 * src  = (CaInt32 *) sourceBuffer      ;
  CaInt16 * dest = (CaInt16 *) destinationBuffer ;
  CaInt32   di                                   ;
  ////////////////////////////////////////////////
  while ( count-- )                              {
    /* REVIEW                                   */
    di    = dither->Dither16()                   ;
    *dest = (CaInt16) ((((*src)>>1) + di) >> 15) ;
    src  += sourceStride                         ;
    dest += destinationStride                    ;
  }                                              ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int32_To_Int8                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt32     * src  = (CaInt32     *) sourceBuffer      ;
  signed char * dest = (signed char *) destinationBuffer ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    *dest = (signed char) ((*src) >> 24)                 ;
    src  += sourceStride                                 ;
    dest += destinationStride                            ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int32_To_Int8_Dither             (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  CaInt32     * src  = (CaInt32     *) sourceBuffer      ;
  signed char * dest = (signed char *) destinationBuffer ;
  CaInt32       di                                       ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    /* REVIEW                                           */
    di    = dither->Dither16()                           ;
    *dest = (signed char) ((((*src)>>1) + di) >> 23)     ;
    src  += sourceStride                                 ;
    dest += destinationStride                            ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int32_To_UInt8                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt32       * src  = (CaInt32       *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    (*dest) = (unsigned char)(((*src) >> 24) + 128)          ;
    src    += sourceStride                                   ;
    dest   += destinationStride                              ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int32_To_UInt8_Dither            (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt32       * src  = (CaInt32       *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    /* IMPLEMENT ME                                         */
    src  += sourceStride                                     ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int24_To_Float32                 (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  CaFloat32     * dest = (CaFloat32     *) destinationBuffer ;
  int             ss   = sourceStride * 3                    ;
  CaInt32         temp                                       ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    #if   defined(CA_LITTLE_ENDIAN)
    temp  =        (((CaInt32)src[0]) <<  8)                 ;
    temp  = temp | (((CaInt32)src[1]) << 16)                 ;
    temp  = temp | (((CaInt32)src[2]) << 24)                 ;
    #elif defined(CA_BIG_ENDIAN)
    temp  =        (((CaInt32)src[0]) << 24)                 ;
    temp  = temp | (((CaInt32)src[1]) << 16)                 ;
    temp  = temp | (((CaInt32)src[2]) <<  8)                 ;
    #endif
    *dest = (float) ((double)temp * const_1_div_2147483648_) ;
    src  += ss                                               ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int24_To_Int32                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  CaInt32       * dest = (CaInt32       *) destinationBuffer ;
  int             ss   = sourceStride * 3                    ;
  CaInt32         temp                                       ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    #if   defined(CA_LITTLE_ENDIAN)
    temp  =        (((CaInt32)src[0]) <<  8)                 ;
    temp  = temp | (((CaInt32)src[1]) << 16)                 ;
    temp  = temp | (((CaInt32)src[2]) << 24)                 ;
    #elif defined(CA_BIG_ENDIAN)
    temp  =        (((CaInt32)src[0]) << 24)                 ;
    temp  = temp | (((CaInt32)src[1]) << 16)                 ;
    temp  = temp | (((CaInt32)src[2]) <<  8)                 ;
    #endif
    *dest = temp                                             ;
    src  += ss                                               ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int24_To_Int16                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  CaInt16       * dest = (CaInt16       *) destinationBuffer ;
  int             ss   = sourceStride * 3                    ;
  CaInt16         temp                                       ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    #if defined(CA_LITTLE_ENDIAN)
    /* src[0] is discarded                                  */
    temp  = (((CaInt16)src[1]))                              ;
    temp  = temp | (CaInt16)(((CaInt16)src[2]) << 8)         ;
    #elif defined(CA_BIG_ENDIAN)
    /* src[2] is discarded                                  */
    temp  = (CaInt16)(((CaInt16)src[0]) << 8)                ;
    temp  = temp | (((CaInt16)src[1]))                       ;
    #endif
    *dest = temp                                             ;
    src  += ss                                               ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int24_To_Int16_Dither            (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  CaInt16       * dest = (CaInt16       *) destinationBuffer ;
  int             ss   = sourceStride * 3                    ;
  CaInt32         temp                                       ;
  CaInt32         di                                         ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    #if defined(CA_LITTLE_ENDIAN)
    temp =        (((CaInt32)src[0]) <<  8)                  ;
    temp = temp | (((CaInt32)src[1]) << 16)                  ;
    temp = temp | (((CaInt32)src[2]) << 24)                  ;
    #elif defined(CA_BIG_ENDIAN)
    temp =        (((CaInt32)src[0]) << 24)                  ;
    temp = temp | (((CaInt32)src[1]) << 16)                  ;
    temp = temp | (((CaInt32)src[2]) <<  8)                  ;
    #endif
    /* REVIEW                                               */
    di    = dither->Dither16()                               ;
    *dest = (CaInt16) (((temp >> 1) + di) >> 15)             ;
    src  += ss                                               ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int24_To_Int8                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  signed   char * dest = (signed   char *) destinationBuffer ;
  int             ss   = sourceStride * 3                    ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    #if defined(CA_LITTLE_ENDIAN)
    /* src[0] is discarded                                  */
    /* src[1] is discarded                                  */
    *dest = src [ 2 ]                                        ;
    #elif defined(CA_BIG_ENDIAN)
    /* src[2] is discarded                                  */
    /* src[1] is discarded                                  */
    *dest = src [ 0 ]                                        ;
    #endif
    src  += ss                                               ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int24_To_Int8_Dither             (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     * dither            )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  signed   char * dest = (signed   char *) destinationBuffer ;
  int             ss   = sourceStride * 3                    ;
  CaInt32         temp                                       ;
  CaInt32         di                                         ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    #if defined(CA_LITTLE_ENDIAN)
    temp  =        (((CaInt32)src[0]) <<  8)                 ;
    temp  = temp | (((CaInt32)src[1]) << 16)                 ;
    temp  = temp | (((CaInt32)src[2]) << 24)                 ;
    #elif defined(CA_BIG_ENDIAN)
    temp  =        (((CaInt32)src[0]) << 24)                 ;
    temp  = temp | (((CaInt32)src[1]) << 16)                 ;
    temp  = temp | (((CaInt32)src[2]) <<  8)                 ;
    #endif
    /* REVIEW */
    di    = dither->Dither16()                               ;
    *dest = (signed char) (((temp >> 1) + di) >> 23)         ;
    src  += ss                                               ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int24_To_UInt8                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  int             ss   = sourceStride * 3                    ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    #if defined(CA_LITTLE_ENDIAN)
    /* src[0] is discarded                                  */
    /* src[1] is discarded                                  */
    *dest = (unsigned char)(src[2] + 128)                    ;
    #elif defined(CA_BIG_ENDIAN)
    *dest = (unsigned char)(src[0] + 128)                    ;
        /* src[1] is discarded                              */
        /* src[2] is discarded                              */
    #endif
    src  += ss                                               ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int24_To_UInt8_Dither (
              void       *        ,
              signed   int        ,
              void       *        ,
              signed   int        ,
              unsigned int        ,
              Dither     *        )
{
  /* IMPLEMENT ME */
}

////////////////////////////////////////////////////////////////////////////////

static void Int16_To_Float32                 (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt16   * src  = (CaInt16   *) sourceBuffer      ;
  CaFloat32 * dest = (CaFloat32 *) destinationBuffer ;
  ////////////////////////////////////////////////////
  while ( count-- )                                  {
    CaFloat32 samp = (*src) * const_1_div_32768_     ;
    /* FIXME: i'm concerned about this being asymetrical with float->int16 -rb */
    *dest = samp                                     ;
    src  += sourceStride                             ;
    dest += destinationStride                        ;
  }                                                  ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int16_To_Int32                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt16 * src  = (CaInt16 *) sourceBuffer      ;
  CaInt32 * dest = (CaInt32 *) destinationBuffer ;
  ////////////////////////////////////////////////
  while ( count-- )                              {
    /* REVIEW: we should consider something like
       (*src << 16) | (*src & 0xFFFF)           */
    *dest = *src << 16                           ;
    src  += sourceStride                         ;
    dest += destinationStride                    ;
  }                                              ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int16_To_Int24                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt16       * src  = (CaInt16       *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  int             ds   = destinationStride * 3               ;
  CaInt16         temp                                       ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    temp    = *src                                           ;
    #if defined(CA_LITTLE_ENDIAN)
    dest[0] = 0                                              ;
    dest[1] = (unsigned char)(temp     )                     ;
    dest[2] = (unsigned char)(temp >> 8)                     ;
    #elif defined(CA_BIG_ENDIAN)
    dest[0] = (unsigned char)(temp >> 8)                     ;
    dest[1] = (unsigned char)(temp     )                     ;
    dest[2] = 0                                              ;
    #endif
    src    += sourceStride                                   ;
    dest   += ds                                             ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int16_To_Int8                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt16     * src  = (CaInt16     *) sourceBuffer      ;
  signed char * dest = (signed char *) destinationBuffer ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    (*dest) = (signed char)((*src) >> 8)                 ;
    src    += sourceStride                               ;
    dest   += destinationStride                          ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int16_To_Int8_Dither             (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt16     * src  = (CaInt16     *) sourceBuffer      ;
  signed char * dest = (signed char *) destinationBuffer ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    /* IMPLEMENT ME                                     */
    src  += sourceStride                                 ;
    dest += destinationStride                            ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int16_To_UInt8                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt16       * src  = (CaInt16       *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    (*dest) = (unsigned char)(((*src) >> 8) + 128)           ;
    src    += sourceStride                                   ;
    dest   += destinationStride                              ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int16_To_UInt8_Dither            (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaInt16       * src  = (CaInt16       *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    /* IMPLEMENT ME                                         */
    src  += sourceStride                                     ;
    dest += destinationStride                                ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int8_To_Float32                  (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  signed char * src  = (signed char *) sourceBuffer      ;
  CaFloat32   * dest = (CaFloat32   *) destinationBuffer ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    CaFloat32 samp = (*src) * const_1_div_128_           ;
    *dest = samp                                         ;
    src  += sourceStride                                 ;
    dest += destinationStride                            ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int8_To_Int32                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  signed char * src  = (signed char *) sourceBuffer      ;
  CaInt32     * dest = (CaInt32     *) destinationBuffer ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    (*dest) = (*src) << 24                               ;
    src    += sourceStride                               ;
    dest   += destinationStride                          ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int8_To_Int24                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  signed   char * src  = (signed   char *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  int             ds   = destinationStride * 3               ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    #if defined(CA_LITTLE_ENDIAN)
    dest[0] = 0                                              ;
    dest[1] = 0                                              ;
    dest[2] = (*src);
    #elif defined(CA_BIG_ENDIAN)
    dest[0] = (*src);
    dest[1] = 0                                              ;
    dest[2] = 0                                              ;
    #endif
    src    += sourceStride                                   ;
    dest   += ds                                             ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int8_To_Int16                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  signed char * src  = (signed char *) sourceBuffer      ;
  CaInt16     * dest = (CaInt16     *) destinationBuffer ;
  ////////////////////////////////////////////////////////
  while ( count-- )                                      {
    (*dest) = (CaInt16)((*src) << 8)                     ;
    src    += sourceStride                               ;
    dest   += destinationStride                          ;
  }                                                      ;
}

////////////////////////////////////////////////////////////////////////////////

static void Int8_To_UInt8                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  signed   char * src  = (signed   char *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    (*dest) = (unsigned char)(*src + 128)                    ;
    src    += sourceStride                                   ;
    dest   += destinationStride                              ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void UInt8_To_Float32                 (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer             ;
  CaFloat32     * dest = (CaFloat32     *) destinationBuffer        ;
  ///////////////////////////////////////////////////////////////////
  while ( count-- )                                                 {
    CaFloat32 samp = (((CaFloat32)(*src)) - 128) * const_1_div_128_ ;
    *dest = samp                                                    ;
    src  += sourceStride                                            ;
    dest += destinationStride                                       ;
  }                                                                 ;
}

////////////////////////////////////////////////////////////////////////////////

static void UInt8_To_Int32                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  CaInt32       * dest = (CaInt32       *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    (*dest) = (((CaInt32)(*src)) - 128) << 24                ;
    src    += sourceStride                                   ;
    dest   += destinationStride                              ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void UInt8_To_Int24                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  int             ds   = destinationStride * 3               ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    #if defined(CA_LITTLE_ENDIAN)
    dest[0] = 0                                              ;
    dest[1] = 0                                              ;
    dest[2] = (unsigned char)(*src - 128)                    ;
    #elif defined(CA_BIG_ENDIAN)
    dest[0] = (unsigned char)(*src - 128)                    ;
    dest[1] = 0                                              ;
    dest[2] = 0                                              ;
    #endif
    src    += sourceStride                                   ;
    dest   += ds                                             ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void UInt8_To_Int16                   (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  CaInt16       * dest = (CaInt16       *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
   (*dest) = (((CaInt16)(*src))- 128) << 8                   ;
   src    += sourceStride                                    ;
   dest   += destinationStride                               ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void UInt8_To_Int8                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  signed   char * dest = (signed   char *) destinationBuffer ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
   (*dest) = ((signed char)(*src)) - 128                     ;
   src    += sourceStride                                    ;
   dest   += destinationStride                               ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Copy_8_To_8                      (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char*) sourceBuffer      ;
  unsigned char * dest = (unsigned char*) destinationBuffer ;
  ///////////////////////////////////////////////////////////
  while ( count-- )                                         {
    *dest = *src                                            ;
    src  += sourceStride                                    ;
    dest += destinationStride                               ;
  }                                                         ;
}

////////////////////////////////////////////////////////////////////////////////

static void Copy_16_To_16          (
    void       * destinationBuffer ,
    signed   int destinationStride ,
    void       * sourceBuffer      ,
    signed   int sourceStride      ,
    unsigned int count             ,
    Dither     *                   )
{
  CaUint16 * src  = (CaUint16 *) sourceBuffer      ;
  CaUint16 * dest = (CaUint16 *) destinationBuffer ;
  //////////////////////////////////////////////////
  while ( count-- )                                {
    *dest = *src                                   ;
    src  += sourceStride                           ;
    dest += destinationStride                      ;
  }                                                ;
}

////////////////////////////////////////////////////////////////////////////////

static void Copy_24_To_24                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  unsigned char * src  = (unsigned char *) sourceBuffer      ;
  unsigned char * dest = (unsigned char *) destinationBuffer ;
  int             ss   = sourceStride      * 3               ;
  int             ds   = destinationStride * 3               ;
  ////////////////////////////////////////////////////////////
  while ( count-- )                                          {
    dest[0] = src[0]                                         ;
    dest[1] = src[1]                                         ;
    dest[2] = src[2]                                         ;
    src    += ss                                             ;
    dest   += ds                                             ;
  }                                                          ;
}

////////////////////////////////////////////////////////////////////////////////

static void Copy_32_To_32                    (
              void       * destinationBuffer ,
              signed   int destinationStride ,
              void       * sourceBuffer      ,
              signed   int sourceStride      ,
              unsigned int count             ,
              Dither     *                   )
{
  CaUint32 * dest = (CaUint32 *) destinationBuffer ;
  CaUint32 * src  = (CaUint32 *) sourceBuffer      ;
  //////////////////////////////////////////////////
  while ( count-- )                                {
    *dest = *src                                   ;
    src  += sourceStride                           ;
    dest += destinationStride                      ;
  }                                                ;
}

////////////////////////////////////////////////////////////////////////////////

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

ConverterTable caConverters =                                                    {
    Float32_To_Int32,              /* Converter * Float32_To_Int32            ; */
    Float32_To_Int32_Dither,       /* Converter * Float32_To_Int32_Dither     ; */
    Float32_To_Int32_Clip,         /* Converter * Float32_To_Int32_Clip       ; */
    Float32_To_Int32_DitherClip,   /* Converter * Float32_To_Int32_DitherClip ; */

    Float32_To_Int24,              /* Converter * Float32_To_Int24            ; */
    Float32_To_Int24_Dither,       /* Converter * Float32_To_Int24_Dither     ; */
    Float32_To_Int24_Clip,         /* Converter * Float32_To_Int24_Clip       ; */
    Float32_To_Int24_DitherClip,   /* Converter * Float32_To_Int24_DitherClip ; */

    Float32_To_Int16,              /* Converter * Float32_To_Int16            ; */
    Float32_To_Int16_Dither,       /* Converter * Float32_To_Int16_Dither     ; */
    Float32_To_Int16_Clip,         /* Converter * Float32_To_Int16_Clip       ; */
    Float32_To_Int16_DitherClip,   /* Converter * Float32_To_Int16_DitherClip ; */

    Float32_To_Int8,               /* Converter * Float32_To_Int8             ; */
    Float32_To_Int8_Dither,        /* Converter * Float32_To_Int8_Dither      ; */
    Float32_To_Int8_Clip,          /* Converter * Float32_To_Int8_Clip        ; */
    Float32_To_Int8_DitherClip,    /* Converter * Float32_To_Int8_DitherClip  ; */

    Float32_To_UInt8,              /* Converter * Float32_To_UInt8            ; */
    Float32_To_UInt8_Dither,       /* Converter * Float32_To_UInt8_Dither     ; */
    Float32_To_UInt8_Clip,         /* Converter * Float32_To_UInt8_Clip       ; */
    Float32_To_UInt8_DitherClip,   /* Converter * Float32_To_UInt8_DitherClip ; */

    Int32_To_Float32,              /* Converter * Int32_To_Float32            ; */
    Int32_To_Int24,                /* Converter * Int32_To_Int24              ; */
    Int32_To_Int24_Dither,         /* Converter * Int32_To_Int24_Dither       ; */
    Int32_To_Int16,                /* Converter * Int32_To_Int16              ; */
    Int32_To_Int16_Dither,         /* Converter * Int32_To_Int16_Dither       ; */
    Int32_To_Int8,                 /* Converter * Int32_To_Int8               ; */
    Int32_To_Int8_Dither,          /* Converter * Int32_To_Int8_Dither        ; */
    Int32_To_UInt8,                /* Converter * Int32_To_UInt8              ; */
    Int32_To_UInt8_Dither,         /* Converter * Int32_To_UInt8_Dither       ; */

    Int24_To_Float32,              /* Converter * Int24_To_Float32            ; */
    Int24_To_Int32,                /* Converter * Int24_To_Int32              ; */
    Int24_To_Int16,                /* Converter * Int24_To_Int16              ; */
    Int24_To_Int16_Dither,         /* Converter * Int24_To_Int16_Dither       ; */
    Int24_To_Int8,                 /* Converter * Int24_To_Int8               ; */
    Int24_To_Int8_Dither,          /* Converter * Int24_To_Int8_Dither        ; */
    Int24_To_UInt8,                /* Converter * Int24_To_UInt8              ; */
    Int24_To_UInt8_Dither,         /* Converter * Int24_To_UInt8_Dither       ; */

    Int16_To_Float32,              /* Converter * Int16_To_Float32            ; */
    Int16_To_Int32,                /* Converter * Int16_To_Int32              ; */
    Int16_To_Int24,                /* Converter * Int16_To_Int24              ; */
    Int16_To_Int8,                 /* Converter * Int16_To_Int8               ; */
    Int16_To_Int8_Dither,          /* Converter * Int16_To_Int8_Dither        ; */
    Int16_To_UInt8,                /* Converter * Int16_To_UInt8              ; */
    Int16_To_UInt8_Dither,         /* Converter * Int16_To_UInt8_Dither       ; */

    Int8_To_Float32,               /* Converter * Int8_To_Float32             ; */
    Int8_To_Int32,                 /* Converter * Int8_To_Int32               ; */
    Int8_To_Int24,                 /* Converter * Int8_To_Int24               ; */
    Int8_To_Int16,                 /* Converter * Int8_To_Int16               ; */
    Int8_To_UInt8,                 /* Converter * Int8_To_UInt8               ; */

    UInt8_To_Float32,              /* Converter * UInt8_To_Float32            ; */
    UInt8_To_Int32,                /* Converter * UInt8_To_Int32              ; */
    UInt8_To_Int24,                /* Converter * UInt8_To_Int24              ; */
    UInt8_To_Int16,                /* Converter * UInt8_To_Int16              ; */
    UInt8_To_Int8,                 /* Converter * UInt8_To_Int8               ; */

    Copy_8_To_8,                   /* Converter * Copy_8_To_8                 ; */
    Copy_16_To_16,                 /* Converter * Copy_16_To_16               ; */
    Copy_24_To_24,                 /* Converter * Copy_24_To_24               ; */
    Copy_32_To_32                  /* Converter * Copy_32_To_32               ; */
}                                                                                ;

CaSampleFormat ClosestFormat                     (
                 unsigned long  availableFormats ,
                 CaSampleFormat format           )
{
  CaSampleFormat result = cafNothing                                      ;
  unsigned long  fmt    = (unsigned long)format                           ;
  unsigned long  avf    = (unsigned long)availableFormats                 ;
  /////////////////////////////////////////////////////////////////////////
  fmt &= ~cafNonInterleaved                                               ;
  avf &= ~cafNonInterleaved                                               ;
  /////////////////////////////////////////////////////////////////////////
  if ( 0 != ( fmt & avf ) )                                               {
    result = format                                                       ;
    return result                                                         ;
  }                                                                       ;
  /////////////////////////////////////////////////////////////////////////
  /* NOTE: this code depends on the sample format constants being in
     descending order of quality - ie best quality is 0
     FIXME: should write an assert which checks that all of the
     known constants conform to that requirement.                        */
  if ( cafFloat32 != fmt )                                                {
    /* scan for better formats                                           */
    int r = fmt                                                           ;
    do r <<= 1                                                            ;
    while ( ( 0 == ( r & avf ) ) && ( cafCustomFormat != r ) )            ;
    if ( 0 == ( r & avf ) ) result = cafNothing                           ;
                       else result = (CaSampleFormat) r                   ;
  } else result = cafNothing                                              ;
  /////////////////////////////////////////////////////////////////////////
  if ( cafNothing == result )                                             {
    /* scan for worse formats                                            */
    int r = fmt                                                           ;
    do r >>= 1                                                            ;
    while ( ( 0 == ( r & avf ) ) && ( 0 != r ) )                          ;
    if ( 0 == ( r & avf ) ) result = cafNothing                           ;
                       else result = (CaSampleFormat) r                   ;
  }                                                                       ;
  /////////////////////////////////////////////////////////////////////////
  return result                                                           ;
}

Converter * SelectConverter                    (
              CaSampleFormat sourceFormat      ,
              CaSampleFormat destinationFormat ,
              CaStreamFlags  flags             )
{
  CA_SELECT_FORMAT_( sourceFormat                                                 ,
      /* cafFloat32:                                                             */
    CA_SELECT_FORMAT_ ( destinationFormat                                         ,
      /* cafFloat32: */ CA_UNITY_CONVERSION_            ( 32                    ) ,
      /* cafInt32:   */ CA_SELECT_CONVERTER_DITHER_CLIP_( flags, Float32, Int32 ) ,
      /* cafInt24:   */ CA_SELECT_CONVERTER_DITHER_CLIP_( flags, Float32, Int24 ) ,
      /* cafInt16:   */ CA_SELECT_CONVERTER_DITHER_CLIP_( flags, Float32, Int16 ) ,
      /* cafInt8 :   */ CA_SELECT_CONVERTER_DITHER_CLIP_( flags, Float32, Int8  ) ,
      /* cafUint8:   */ CA_SELECT_CONVERTER_DITHER_CLIP_( flags, Float32, UInt8 ) )
    , /* cafInt32:                                                               */
    CA_SELECT_FORMAT_( destinationFormat                                          ,
      /* cafFloat32: */ CA_USE_CONVERTER_               ( Int32, Float32        ) ,
      /* cafInt32:   */ CA_UNITY_CONVERSION_            ( 32                    ) ,
      /* cafInt24:   */ CA_SELECT_CONVERTER_DITHER_     ( flags, Int32, Int24   ) ,
      /* cafInt16:   */ CA_SELECT_CONVERTER_DITHER_     ( flags, Int32, Int16   ) ,
      /* cafInt8:    */ CA_SELECT_CONVERTER_DITHER_     ( flags, Int32, Int8    ) ,
      /* cafUint8:   */ CA_SELECT_CONVERTER_DITHER_     ( flags, Int32, UInt8   ) )
    , /* cafInt24:                                                               */
    CA_SELECT_FORMAT_( destinationFormat                                          ,
      /* cafFloat32: */ CA_USE_CONVERTER_               ( Int24, Float32        ) ,
      /* cafInt32:   */ CA_USE_CONVERTER_               ( Int24, Int32          ) ,
      /* cafInt24:   */ CA_UNITY_CONVERSION_            ( 24                    ) ,
      /* cafInt16:   */ CA_SELECT_CONVERTER_DITHER_     ( flags, Int24, Int16   ) ,
      /* cafInt8:    */ CA_SELECT_CONVERTER_DITHER_     ( flags, Int24, Int8    ) ,
      /* cafUint8:   */ CA_SELECT_CONVERTER_DITHER_     ( flags, Int24, UInt8   ) )
    , /* cafInt16:                                                               */
    CA_SELECT_FORMAT_( destinationFormat                                          ,
      /* cafFloat32: */ CA_USE_CONVERTER_               ( Int16, Float32        ) ,
      /* cafInt32:   */ CA_USE_CONVERTER_               ( Int16, Int32          ) ,
      /* cafInt24:   */ CA_USE_CONVERTER_               ( Int16, Int24          ) ,
      /* cafInt16:   */ CA_UNITY_CONVERSION_            ( 16                    ) ,
      /* cafInt8:    */ CA_SELECT_CONVERTER_DITHER_     ( flags, Int16, Int8    ) ,
      /* cafUint8:   */ CA_SELECT_CONVERTER_DITHER_     ( flags, Int16, UInt8   ) )
    , /* cafInt8:                                                                */
    CA_SELECT_FORMAT_( destinationFormat                                          ,
      /* cafFloat32: */ CA_USE_CONVERTER_               ( Int8, Float32         ) ,
      /* cafInt32:   */ CA_USE_CONVERTER_               ( Int8, Int32           ) ,
      /* cafInt24:   */ CA_USE_CONVERTER_               ( Int8, Int24           ) ,
      /* cafInt16:   */ CA_USE_CONVERTER_               ( Int8, Int16           ) ,
      /* cafInt8:    */ CA_UNITY_CONVERSION_            ( 8                     ) ,
      /* cafUint8:   */ CA_USE_CONVERTER_               ( Int8, UInt8           ) )
    , /* cafUint8:                                                               */
    CA_SELECT_FORMAT_( destinationFormat                                          ,
      /* cafFloat32: */ CA_USE_CONVERTER_               ( UInt8, Float32        ) ,
      /* cafInt32:   */ CA_USE_CONVERTER_               ( UInt8, Int32          ) ,
      /* cafInt24:   */ CA_USE_CONVERTER_               ( UInt8, Int24          ) ,
      /* cafInt16:   */ CA_USE_CONVERTER_               ( UInt8, Int16          ) ,
      /* cafInt8:    */ CA_USE_CONVERTER_               ( UInt8, Int8           ) ,
      /* cafUint8:   */ CA_UNITY_CONVERSION_            ( 8                     ) )
  )                                                                               ;
}

////////////////////////////////////////////////////////////////////////////////

static void ZeroU8 ( void       * destinationBuffer ,
                     signed   int destinationStride ,
                     unsigned int count             )
{
  unsigned char * dest = (unsigned char*)destinationBuffer ;
  while ( count-- )                                        {
    *dest = 128                                            ;
    dest += destinationStride                              ;
  }                                                        ;
}

////////////////////////////////////////////////////////////////////////////////

static void Zero8 ( void       * destinationBuffer ,
                    signed   int destinationStride ,
                    unsigned int count             )
{
  unsigned char * dest = (unsigned char*)destinationBuffer ;
  while ( count-- )                                        {
    *dest = 0                                              ;
    dest += destinationStride                              ;
  }                                                        ;
}

////////////////////////////////////////////////////////////////////////////////

static void Zero16 ( void       * destinationBuffer ,
                     signed   int destinationStride ,
                     unsigned int count             )
{
  CaUint16 * dest = (CaUint16 *)destinationBuffer ;
  while ( count-- )                               {
    *dest = 0                                     ;
    dest += destinationStride                     ;
  }                                               ;
}

////////////////////////////////////////////////////////////////////////////////

static void Zero24 ( void       * destinationBuffer ,
                     signed   int destinationStride ,
                     unsigned int count             )
{
  unsigned char * dest = (unsigned char*)destinationBuffer ;
  int             gap  = destinationStride * 3             ;
  while ( count-- )                                        {
    dest[0] = 0                                            ;
    dest[1] = 0                                            ;
    dest[2] = 0                                            ;
    dest   += gap                                          ;
  }                                                        ;
}

////////////////////////////////////////////////////////////////////////////////

static void Zero32 ( void       * destinationBuffer ,
                     signed   int destinationStride ,
                     unsigned int count             )
{
  CaUint32 * dest = (CaUint32 *)destinationBuffer ;
  while ( count-- )                               {
    *dest = 0                                     ;
    dest += destinationStride                     ;
  }                                               ;
}

////////////////////////////////////////////////////////////////////////////////

typedef struct        {
  ZeroCopier * ZeroU8 ; /* unsigned 8 bit, zero == 128 */
  ZeroCopier * Zero8  ;
  ZeroCopier * Zero16 ;
  ZeroCopier * Zero24 ;
  ZeroCopier * Zero32 ;
} ZeroCopierTable     ;

ZeroCopierTable caZeroers = {
  ZeroU8                    , /* ZeroCopier * ZeroU8; */
  Zero8                     , /* ZeroCopier * Zero8 ; */
  Zero16                    , /* ZeroCopier * Zero16; */
  Zero24                    , /* ZeroCopier * Zero24; */
  Zero32                    , /* ZeroCopier * Zero32; */
}                           ;

////////////////////////////////////////////////////////////////////////////////

ZeroCopier * SelectZeroCopier (CaSampleFormat destinationFormat)
{
  switch ( ((int)destinationFormat) & ~cafNonInterleaved ) {
    case cafFloat32                                        :
    return caZeroers . Zero32                              ;
    case cafInt32                                          :
    return caZeroers . Zero32                              ;
    case cafInt24                                          :
    return caZeroers . Zero24                              ;
    case cafInt16                                          :
    return caZeroers . Zero16                              ;
    case cafInt8                                           :
    return caZeroers . Zero8                               ;
    case cafUint8                                          :
    return caZeroers . ZeroU8                              ;
  }                                                        ;
  return NULL                                              ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif

