/*****************************************************************************
 *                                                                           *
 *                            CIOS Audio Core                                *
 *                                                                           *
 * Version        : 1.5.11                                                   *
 * Author         : Brian Lin <lin.foxman@gmail.com>                         *
 * Skpye          : wolfram_lin                                              *
 * Lastest update : 2014/12/22                                               *
 * Site           : http://ciosaudio.sourceforge.net                         *
 * License        : LGPLv3                                                   *
 *                                                                           *
 * Documents are separated, you will receive the full document when you      *
 * download the source code.  It is all-in-one, there is no comment in the   *
 * source code.                                                              *
 *                                                                           *
 *****************************************************************************/

#include "CaQtWrapper.hpp"

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

CaQtDebugger:: CaQtDebugger (QWidget * parent)
             : QTextBrowser (          parent)
{
  total = 0                  ;
  timer . setParent ( this ) ;
}

CaQtDebugger::~CaQtDebugger (void)
{
  stop ( ) ;
}

void CaQtDebugger::printf(const char * format,...)
{
  char buf [ 4096 ]                                      ;
  va_list ap                                             ;
  va_start           ( ap  , format                    ) ;
  vsnprintf          ( buf , sizeof(buf) , format , ap ) ;
  buf[sizeof(buf)-1] = 0                                 ;
  va_end             ( ap                              ) ;
  mutex . lock       (                                 ) ;
  lines << QString   ( buf                             ) ;
  mutex . unlock     (                                 ) ;
}

const char * CaQtDebugger::Error(CaError errorCode)
{
  QString result                                                             ;
  switch ( errorCode )                                                       {
    case NoError                                                             :
      result = tr("Success")                                                 ;
    break                                                                    ;
    case NotInitialized                                                      :
      result = tr("CIOS Audio Core not initialized")                         ;
    break                                                                    ;
    case UnanticipatedHostError                                              :
      result = tr("Unanticipated host error")                                ;
    break                                                                    ;
    case InvalidChannelCount                                                 :
      result = tr("Invalid number of channels")                              ;
    break                                                                    ;
    case InvalidSampleRate                                                   :
      result = tr("Invalid sample rate")                                     ;
    break                                                                    ;
    case InvalidDevice                                                       :
      result = tr("Invalid device")                                          ;
    break                                                                    ;
    case InvalidFlag                                                         :
      result = tr("Invalid flag")                                            ;
    break                                                                    ;
    case SampleFormatNotSupported                                            :
      result = tr("Sample format not supported")                             ;
    break                                                                    ;
    case BadIODeviceCombination                                              :
      result = tr("Illegal combination of I/O devices")                      ;
    break                                                                    ;
    case InsufficientMemory                                                  :
      result = tr("Insufficient memory")                                     ;
    break                                                                    ;
    case BufferTooBig                                                        :
      result = tr("Buffer too big")                                          ;
    break                                                                    ;
    case BufferTooSmall                                                      :
      result = tr("Buffer too small")                                        ;
    break                                                                    ;
    case NullCallback                                                        :
      result = tr("No callback routine specified")                           ;
    break                                                                    ;
    case BadStreamPtr                                                        :
      result = tr("Invalid stream pointer")                                  ;
    break                                                                    ;
    case TimedOut                                                            :
      result = tr("Wait timed out")                                          ;
    break                                                                    ;
    case InternalError                                                       :
      result = tr("Internal CIOS Audio error")                               ;
    break                                                                    ;
    case DeviceUnavailable                                                   :
      result = tr("Device unavailable")                                      ;
    break                                                                    ;
    case IncompatibleStreamInfo                                              :
      result = tr("Incompatible host API specific stream info")              ;
    break                                                                    ;
    case StreamIsStopped                                                     :
      result = tr("Stream is stopped")                                       ;
    break                                                                    ;
    case StreamIsNotStopped                                                  :
      result = tr("Stream is not stopped")                                   ;
    break                                                                    ;
    case InputOverflowed                                                     :
      result = tr("Input overflowed")                                        ;
    break                                                                    ;
    case OutputUnderflowed                                                   :
      result = tr("Output underflowed")                                      ;
    break                                                                    ;
    case HostApiNotFound                                                     :
      result = tr("Host API not found")                                      ;
    break                                                                    ;
    case InvalidHostApi                                                      :
      result = tr("Invalid host API")                                        ;
    break                                                                    ;
    case CanNotReadFromACallbackStream                                       :
      result = tr("Can't read from a callback stream")                       ;
    break                                                                    ;
    case CanNotWriteToACallbackStream                                        :
      result = tr("Can't write to a callback stream")                        ;
    break                                                                    ;
    case CanNotReadFromAnOutputOnlyStream                                    :
     result = tr("Can't read from an output only stream")                    ;
    break                                                                    ;
    case CanNotWriteToAnInputOnlyStream                                      :
      result = tr("Can't write to an input only stream")                     ;
    break                                                                    ;
    case IncompatibleStreamHostApi                                           :
      result = tr("Incompatible stream host API")                            ;
    break                                                                    ;
    case BadBufferPtr                                                        :
      result = tr("Bad buffer pointer")                                      ;
    break                                                                    ;
    default                                                                  :
      if ( errorCode > 0 )                                                   {
        result = tr("Invalid error code (value greater than zero)")          ;
      } else                                                                 {
        result = tr("Invalid error code")                                    ;
      }                                                                      ;
    break                                                                    ;
  }                                                                          ;
  return result.toUtf8().constData()                                         ;
}

void CaQtDebugger::Report(void)
{
  if ( total > 10000 )             {
    clear ( )                      ;
    total = 0                      ;
  }                                ;
  if ( lines.count() <= 0 ) return ;
  while ( lines . count ( ) > 0 )  {
    QString     s                  ;
    QStringList l                  ;
    mutex . lock   ( )             ;
    s = lines [ 0 ]                ;
    lines . takeAt (0)             ;
    mutex . unlock ( )             ;
    l = s . split ( '\n' )         ;
    foreach ( s , l )              {
      s = s.replace("\n","")       ;
      s = s.replace("\r","")       ;
      if ( s.length() > 0 )        {
        append ( s )               ;
        total ++                   ;
      }                            ;
      qApp -> processEvents ( )    ;
    }                              ;
  }                                ;
}

void CaQtDebugger::start(int interval)
{
  if ( timer . isActive ( ) ) return                                ;
  lines << tr("Debugger started")                                   ;
  disconnect ( &timer , SIGNAL(timeout()) , NULL , NULL           ) ;
  connect    ( &timer , SIGNAL(timeout()) , this , SLOT(Report()) ) ;
  timer . start ( interval )                                        ;
}

void CaQtDebugger::stop(void)
{
  if ( ! timer . isActive ( ) ) return ;
  timer . stop ( )                     ;
}

#ifdef DONT_USE_NAMESPACE
#else
}
#endif
