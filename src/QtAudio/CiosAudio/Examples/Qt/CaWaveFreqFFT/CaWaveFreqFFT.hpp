#ifndef CAWAVEFREQFFT_HPP
#define CAWAVEFREQFFT_HPP

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include "CiosAudio.hpp"
#include "CaQtWrapper.hpp"

namespace Ui          {
  class CaWaveFreqFFT ;
  class CiosDevices   ;
}

class WaveConduit : public CiosAudio::Conduit
{
  public:

    CiosAudio::LoopBuffer Buffer ;

    explicit     WaveConduit       (void) ;
    virtual     ~WaveConduit       (void) ;

    virtual void setBufferSize     (int size,int margin) ;
    virtual void setTime           (int size) ;

    virtual int  obtain            (void) ;
    virtual int  put               (void) ;
    virtual void finish            (ConduitDirection direction = NoDirection ,
                                    FinishCondition  condition = Correct   ) ;

    virtual void LockConduit       (void) ;
    virtual void UnlockConduit     (void) ;

    virtual int  size              (void) const ;
    virtual unsigned char * window (void) const ;
    virtual long long Total        (void) const ;

  protected:

    CiosAudio::Mutex mutex  ;
    unsigned char  * buffer ;
    int              Size   ;
    long long        All    ;

    int          BridgeObtain      (void) ;
    int          BridgePut         (void) ;
    virtual int  LinearPut         (void) ;

  private:

};

class CaWaveDisplay : public QWidget
{
  Q_OBJECT
  public:

    QImage Image ;
    QColor Color ;

    explicit CaWaveDisplay   (QWidget * parent = NULL) ;
    virtual ~CaWaveDisplay   (void) ;

    virtual void lock        (void) ;
    virtual void unlock      (void) ;

  protected:

    QMutex Mutex ;
    QTimer Timer ;

    virtual void resizeEvent (QResizeEvent * event) ;
    virtual void paintEvent  (QPaintEvent  * event) ;

  private:

  public slots:

    virtual void start       (void) ;
    virtual void stop        (void) ;

  protected slots:

  private slots:

  signals:

};

class CaPlayFile : public QSplitter
                 , public CiosAudio::Thread
{
  Q_OBJECT
  public:

    QString                   Filename ;
    CiosAudio::CaQtDebugger * debugger ;
    int                       deviceId ;
    int                       decodeId ;

    explicit CaPlayFile (QWidget * parent = NULL) ;
    virtual ~CaPlayFile (void) ;

  protected:

    QSplitter             * display     ;
    QSplitter             * channels    ;
    QSplitter             * tools       ;
    QToolButton           * stopit      ;
    QSlider               * timebar     ;
    QSlider               * left        ;
    QSlider               * right       ;
    CaWaveDisplay         * w1          ;
    CaWaveDisplay         * w2          ;
    CiosAudio::Stream     * out         ;
    CiosAudio::MediaCodec * mc          ;
    int                     currentTime ;
    QTimer                  timer       ;
    WaveConduit             WC          ;
    unsigned char         * C1          ;
    unsigned char         * C2          ;

    virtual void run    (void) ;

  private:

    void Drawing        (CaWaveDisplay           * w          ,
                         unsigned char           * c          ,
                         int                       SampleRate ,
                         CiosAudio::CaSampleFormat format   ) ;

  public slots:

  protected slots:

    virtual void volumeChanged  (int vol) ;
    virtual void rightChanged   (int vol) ;
    virtual void setCurrentTime (void) ;

  private slots:

  signals:

    void final (void) ;

};

class CaWaveFreqFFT : public QMainWindow
                    , public CiosAudio::Thread
{
  Q_OBJECT
  public:

    explicit CaWaveFreqFFT   (QWidget * parent = 0) ;
    virtual ~CaWaveFreqFFT   (void) ;

  protected:

    Ui::CaWaveFreqFFT       * ui       ;
    Ui::CiosDevices         * devices  ;
    QWidget                 * dgui     ;
    QTabWidget              * TABs     ;
    CiosAudio::CaQtDebugger * debugger ;

    virtual void run         (int Type, CiosAudio::ThreadData * data) ;

  private:

    virtual void ListDetails (void) ;
    virtual void Play        (char * filename) ;

  public slots:

    virtual void startup     (void) ;

    virtual void Open        (void) ;
    virtual void Quit        (void) ;

    virtual void Speaker     (void) ;
    virtual void Record      (void) ;
    virtual void Wave        (void) ;
    virtual void Display     (void) ;

  protected slots:

  private slots:

  signals:

};

#endif // CAWAVEFREQFFT_HPP
