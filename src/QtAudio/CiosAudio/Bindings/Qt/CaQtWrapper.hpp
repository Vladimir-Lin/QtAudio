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

#ifndef CAQTWRAPPER_HPP
#define CAQTWRAPPER_HPP

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include "CiosAudio.hpp"

#ifdef DONT_USE_NAMESPACE
#else
namespace CAC_NAMESPACE {
#endif

class CaQtDebugger ;

QString GetDeviceName(Core & core,DeviceInfo & device) ;

class CaQtDebugger : public QTextBrowser
                   , public Debugger
{
  Q_OBJECT
  public:

    explicit CaQtDebugger          (QWidget * parent = NULL) ;
    virtual ~CaQtDebugger          (void) ;

    virtual void         printf    (const char * format,...) ;
    virtual const char * Error     (CaError errorCode) ;

  protected:

    QStringList lines ;
    QMutex      mutex ;
    int         total ;
    QTimer      timer ;

  private:

  public slots:

    virtual void start             (int interval) ;
    virtual void stop              (void) ;
    virtual void Report            (void) ;

  protected slots:

  private slots:

  signals:

};



#ifdef DONT_USE_NAMESPACE
#else
}
#endif

#endif // CAQTWRAPPER_HPP
