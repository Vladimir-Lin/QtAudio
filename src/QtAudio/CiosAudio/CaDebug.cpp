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

#if _MSC_VER
  #define VSNPRINTF    _vsnprintf
  #define LOG_BUF_SIZE 2048
#else
  #define VSNPRINTF    vsnprintf
#endif

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

typedef struct CaHostErrorInfo {
  CaHostApiTypeId hostApiType  ; /**< the host API which returned the error code */
  long            errorCode    ; /**< the error code returned */
  const char    * errorText    ; /**< a textual description of the error if available, otherwise a zero-length string */
}              CaHostErrorInfo ;

#define CA_LAST_HOST_ERROR_TEXT_LENGTH_  1024

static char lastHostErrorText_[ CA_LAST_HOST_ERROR_TEXT_LENGTH_ + 1 ] = {0};

static CaHostErrorInfo lastHostErrorInfo_ = {
                       (CaHostApiTypeId)-1  ,
                       0                    ,
                       lastHostErrorText_ } ;

void SetLastHostErrorInfo ( CaHostApiTypeId hostApiType ,
                            long            errorCode   ,
                            const char    * errorText   )
{
  lastHostErrorInfo_ . hostApiType = hostApiType ;
  lastHostErrorInfo_ . errorCode   = errorCode   ;
  if ( NULL == errorText ) return                ;
  ::strncpy( lastHostErrorText_                  ,
             errorText                           ,
             CA_LAST_HOST_ERROR_TEXT_LENGTH_   ) ;
}

Debugger:: Debugger (void)
{
}

Debugger::~Debugger (void)
{
}

const char * Debugger::lastError (void)
{
  return lastHostErrorText_ ;
}

void Debugger::printf(const char * format,...)
{
  // Optional logging into Output console of Visual Studio
  #if defined(_MSC_VER) && defined(ENABLE_MSVC_DEBUG_OUTPUT)
  char buf [ LOG_BUF_SIZE ]                              ;
  va_list ap                                             ;
  va_start           ( ap  , format                    ) ;
  VSNPRINTF          ( buf , sizeof(buf) , format , ap ) ;
  buf[sizeof(buf)-1] = 0                                 ;
  OutputDebugStringA ( buf                             ) ;
  va_end             ( ap                              ) ;
  #else
  va_list ap                                             ;
  va_start ( ap     , format      )                      ;
  vfprintf ( stderr , format , ap )                      ;
  va_end   ( ap                   )                      ;
  fflush   ( stderr               )                      ;
  #endif
}

const char * Debugger::Error(CaError errorCode)
{
  const char * result = NULL                                                      ;
  switch ( errorCode )                                                            {
    case NoError                                                                  :
      result = "Success"                                                          ;
    break                                                                         ;
    case NotInitialized                                                           :
      result = "CIOS Audio Core not initialized"                                  ;
    break                                                                         ;
    case UnanticipatedHostError                                                   :
      result = "Unanticipated host error"                                         ;
    break                                                                         ;
    case InvalidChannelCount                                                      :
      result = "Invalid number of channels"                                       ;
    break                                                                         ;
    case InvalidSampleRate                                                        :
      result = "Invalid sample rate"                                              ;
    break                                                                         ;
    case InvalidDevice                                                            :
      result = "Invalid device"                                                   ;
    break                                                                         ;
    case InvalidFlag                                                              :
      result = "Invalid flag"                                                     ;
    break                                                                         ;
    case SampleFormatNotSupported                                                 :
      result = "Sample format not supported"                                      ;
    break                                                                         ;
    case BadIODeviceCombination                                                   :
      result = "Illegal combination of I/O devices"                               ;
    break                                                                         ;
    case InsufficientMemory                                                       :
      result = "Insufficient memory"                                              ;
    break                                                                         ;
    case BufferTooBig                                                             :
      result = "Buffer too big"                                                   ;
    break                                                                         ;
    case BufferTooSmall                                                           :
      result = "Buffer too small"                                                 ;
    break                                                                         ;
    case NullCallback                                                             :
      result = "No callback routine specified"                                    ;
    break                                                                         ;
    case BadStreamPtr                                                             :
      result = "Invalid stream pointer"                                           ;
    break                                                                         ;
    case TimedOut                                                                 :
      result = "Wait timed out"                                                   ;
    break                                                                         ;
    case InternalError                                                            :
      result = "Internal CIOS Audio error"                                        ;
    break                                                                         ;
    case DeviceUnavailable                                                        :
      result = "Device unavailable"                                               ;
    break                                                                         ;
    case IncompatibleStreamInfo                                    :
      result = "Incompatible host API specific stream info"                       ;
    break                                                                         ;
    case StreamIsStopped                                                          :
      result = "Stream is stopped"                                                ;
    break                                                                         ;
    case StreamIsNotStopped                                                       :
      result = "Stream is not stopped"                                            ;
    break                                                                         ;
    case InputOverflowed                                                          :
      result = "Input overflowed"                                                 ;
    break                                                                         ;
    case OutputUnderflowed                                                        :
      result = "Output underflowed"                                               ;
    break                                                                         ;
    case HostApiNotFound                                                          :
      result = "Host API not found"                                               ;
    break                                                                         ;
    case InvalidHostApi                                                           :
      result = "Invalid host API"                                                 ;
    break                                                                         ;
    case CanNotReadFromACallbackStream                                            :
      result = "Can't read from a callback stream"                                ;
    break                                                                         ;
    case CanNotWriteToACallbackStream                                             :
      result = "Can't write to a callback stream"                                 ;
    break                                                                         ;
    case CanNotReadFromAnOutputOnlyStream                                         :
      result = "Can't read from an output only stream"                            ;
    break                                                                         ;
    case CanNotWriteToAnInputOnlyStream                                           :
      result = "Can't write to an input only stream"                              ;
    break                                                                         ;
    case IncompatibleStreamHostApi                                                :
      result = "Incompatible stream host API"                                     ;
    break                                                                         ;
    case BadBufferPtr                                                             :
      result = "Bad buffer pointer"                                               ;
    break                                                                         ;
    default                                                                       :
      if( errorCode > 0 ) result = "Invalid error code (value greater than zero)" ;
                     else result = "Invalid error code"                           ;
    break                                                                         ;
  }                                                                               ;
  return result                                                                   ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
