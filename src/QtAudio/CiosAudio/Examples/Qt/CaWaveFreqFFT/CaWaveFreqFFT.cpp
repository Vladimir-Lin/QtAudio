#include "CaWaveFreqFFT.hpp"
#include "ui_CaWaveFreqFFT.h"
#include "ui_CiosDevices.h"

#define QSLIDER_STYLESHEET \
"QSlider::groove:horizontal" \
"{" \
"border: 1px solid #bbb;" \
"background: white;" \
"width: 10px;" \
"border-radius: 4px;" \
"}" \
"" \
"QSlider::sub-page:vertical" \
"{" \
"background: QLinearGradient( x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #fff, stop: 0.4999 #eee, stop: 0.5 #ddd, stop: 1 #eee );" \
"border: 1px solid #777;" \
"width: 10px;" \
"border-radius: 4px;" \
"}" \
"" \
"QSlider::add-page:vertical {" \
"background: QLinearGradient( x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #78d, stop: 0.4999 #46a, stop: 0.5 #45a, stop: 1 #238 );" \
"" \
"border: 1px solid #777;" \
"width: 10px;" \
"border-radius: 4px;" \
"}" \
"" \
"QSlider::handle:vertical {" \
"background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #eee, stop:1 #ccc);" \
"border: 1px solid #777;" \
"height: 13px;" \
"margin-top: -2px;" \
"margin-bottom: -2px;" \
"border-radius: 4px;" \
"}" \
"" \
"QSlider::handle:vertical:hover {" \
"background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #fff, stop:1 #ddd);" \
"border: 1px solid #444;" \
"border-radius: 4px;" \
"}" \
"" \
"QSlider::sub-page:vertical:disabled {" \
"background: #bbb;" \
"border-color: #999;" \
"}" \
"" \
"QSlider::add-page:vertical:disabled {" \
"background: #eee;" \
"border-color: #999;" \
"}" \
"" \
"QSlider::handle:vertical:disabled {" \
"background: #eee;" \
"border: 1px solid #aaa;" \
"border-radius: 4px;" \
"}"

//////////////////////////////////////////////////////////////////////////////

int main(int argc,char * argv[])
{
  QApplication  a ( argc , argv ) ;
  CaWaveFreqFFT w                 ;
  w . startup     ( )             ;
  w . show        ( )             ;
  return a . exec ( )             ;
}

//////////////////////////////////////////////////////////////////////////////

WaveConduit:: WaveConduit        ( void )
            : CiosAudio::Conduit (      )
            , buffer             ( NULL )
            , Size               ( 0    )
            , All                ( 0    )
{
}

WaveConduit::~WaveConduit(void)
{
  if ( NULL != buffer ) {
    delete [] buffer    ;
    buffer = NULL       ;
    Size   = 0          ;
  }
}

void WaveConduit::setBufferSize (int size,int margin)
{
  Buffer . setBufferSize ( size   ) ;
  Buffer . setMargin     ( margin ) ;
}

void WaveConduit::setTime (int size)
{
  if ( NULL != buffer )                 {
    delete [] buffer                    ;
    buffer = NULL                       ;
    Size   = 0                          ;
  }                                     ;
  Size = size                           ;
  if ( size > 0 )                       {
    buffer = new unsigned char [ size ] ;
    memset ( buffer , 0 , size )        ;
  }                                     ;
}

void WaveConduit::LockConduit(void)
{
  mutex . lock   ( ) ;
}

void WaveConduit::UnlockConduit(void)
{
  mutex . unlock ( ) ;
}

int WaveConduit::obtain(void)
{
  int r = BridgeObtain ( ) ;
  All += Input.Total   ( ) ;
  LinearPut            ( ) ;
  return r                 ;
}

int WaveConduit::put(void)
{
  return BridgePut   ( ) ;
}

void WaveConduit::finish(ConduitDirection direction,
                           FinishCondition  condition)
{
}

int WaveConduit::BridgeObtain(void)
{
  if ( Input . Situation == CiosAudio::StreamIO::Stagnated )  {
    return Continue                                ;
  }                                                ;
  //////////////////////////////////////////////////
  if ( Output . FrameCount <= 0 )                  {
    Output . Situation = CiosAudio::StreamIO::Ruptured        ;
    return Complete                                ;
  }                                                ;
  //////////////////////////////////////////////////
  if ( Output . isNull ( ) )                       {
    Output . Situation = CiosAudio::StreamIO::Ruptured        ;
    return Complete                                ;
  }                                                ;
  if ( ( Buffer . isEmpty ( )                   ) &&
       ( CiosAudio::StreamIO::Started == Output.Situation) )  {
    Output . Situation = CiosAudio::StreamIO::Completed       ;
    return Complete                                ;
  }                                                ;
  //////////////////////////////////////////////////
  int  bs = Output . Total ( )                     ;
  Buffer . get ( Output . Buffer , bs )            ;
  Output . Situation = CiosAudio::StreamIO::Started           ;
  return Continue                                  ;
}

int WaveConduit::BridgePut(void)
{
  if ( Input . FrameCount <= 0 )                 {
    Input . Situation  = CiosAudio::StreamIO::Ruptured      ;
    return Abort                                 ;
  }                                              ;
  ////////////////////////////////////////////////
  if ( Input.isNull() )                          {
    Input . Situation  = CiosAudio::StreamIO::Ruptured      ;
    return Abort                                 ;
  }                                              ;
  ////////////////////////////////////////////////
  if ( Buffer . isFull ( )  )                    {
    Input . Situation  = CiosAudio::StreamIO::Stalled       ;
    return Postpone                              ;
  }                                              ;
  ////////////////////////////////////////////////
  int fc = Input . Total ( )                     ;
  Buffer . put ( Input . Buffer , fc )           ;
  Input  . Situation  = CiosAudio::StreamIO::Started        ;
  return Continue                                ;
}

long long WaveConduit::Total(void) const
{
  return All ;
}

int WaveConduit::size(void) const
{
  return Size ;
}

unsigned char * WaveConduit::window (void) const
{
  return buffer ;
}

int WaveConduit::LinearPut(void)
{
  if ( Output . isNull ( ) ) return Abort           ;
  long long bs = Output.Total()                     ;
  if ( bs <= 0    ) return Continue                 ;
  if ( bs >  Size ) bs = Size                       ;
  int dp = Size - bs                                ;
  if ( dp > 0 )                                     {
    ::memcpy ( buffer      , buffer + bs     , dp ) ;
  }                                                 ;
  ::memcpy   ( buffer + dp , Output . Buffer , bs ) ;
  return Continue                                   ;
}

//////////////////////////////////////////////////////////////////////////////

CaWaveDisplay:: CaWaveDisplay (QWidget * parent)
              : QWidget       (          parent)
{
  Image = QImage ( 256 , 256 , QImage::Format_ARGB32 ) ;
  Image . fill ( QColor ( 255 , 255 , 255 ) )          ;
  Timer . setParent ( this )                           ;
  Color = QColor ( 0 , 0 , 255 )                       ;
}

CaWaveDisplay::~CaWaveDisplay (void)
{
  stop ( ) ;
}

void CaWaveDisplay::lock(void)
{
  Mutex . lock ( ) ;
}

void CaWaveDisplay::unlock(void)
{
  Mutex . unlock ( ) ;
}

void CaWaveDisplay::resizeEvent(QResizeEvent * event)
{
  QSize s = size ( )                          ;
  lock   ( )                                  ;
  Image = QImage(s,QImage::Format_ARGB32)     ;
  Image . fill ( QColor ( 255 , 255 , 255 ) ) ;
  unlock ( )                                  ;
}

void CaWaveDisplay::paintEvent(QPaintEvent * event)
{
  QPainter p ( this )      ;
  lock   ( )               ;
  p . drawImage(0,0,Image) ;
  unlock ( )               ;
}

void CaWaveDisplay::start(void)
{
  if ( Timer . isActive ( ) ) return                                ;
  disconnect ( &Timer , SIGNAL(timeout()) , NULL , NULL           ) ;
  connect    ( &Timer , SIGNAL(timeout()) , this , SLOT(update()) ) ;
  Timer . start ( 100 )                                             ;
}

void CaWaveDisplay::stop(void)
{
  if ( ! Timer . isActive ( ) ) return ;
  Timer . stop ( )                     ;
}

//////////////////////////////////////////////////////////////////////////////

CaPlayFile:: CaPlayFile  ( QWidget      * parent )
           : QSplitter   ( Qt::Vertical , parent )
           , debugger    ( NULL                  )
           , out         ( NULL                  )
           , deviceId    ( -1                    )
           , decodeId    ( -1                    )
           , w1          ( NULL                  )
           , w2          ( NULL                  )
           , mc          ( NULL                  )
           , currentTime ( 0                     )
           , C1          ( NULL                  )
           , C2          ( NULL                  )
{
  setHandleWidth ( 1 )                                      ;
  display   = new QSplitter(Qt::Horizontal,this   )         ;
  display  -> setHandleWidth ( 1 )                          ;
  tools     = new QSplitter(Qt::Horizontal,this   )         ;
  stopit    = new QToolButton ( tools )                     ;
  timebar   = new QSlider     ( tools )                     ;
  left      = new QSlider     ( this  )                     ;
  right     = new QSlider     ( this  )                     ;
  timebar  -> setOrientation(Qt::Horizontal)                ;
  left     -> setOrientation(Qt::Vertical  )                ;
  right    -> setOrientation(Qt::Vertical  )                ;
  left     -> setStyleSheet(QSLIDER_STYLESHEET)             ;
  right    -> setStyleSheet(QSLIDER_STYLESHEET)             ;
  addWidget ( display )                                     ;
  addWidget ( tools   )                                     ;
  tools    -> addWidget         ( stopit   )                ;
  tools    -> addWidget         ( timebar  )                ;
  display  -> addWidget         ( left     )                ;
  channels  = new QSplitter(Qt::Vertical  ,display)         ;
  channels -> setHandleWidth    ( 1                       ) ;
  display  -> addWidget         ( channels                ) ;
  display  -> addWidget         ( right                   ) ;
  w1        = new CaWaveDisplay ( channels                ) ;
  w2        = new CaWaveDisplay ( channels                ) ;
  channels -> addWidget         ( w1                      ) ;
  channels -> addWidget         ( w2                      ) ;
  stopit   -> setMinimumSize    (  24 , 24                ) ;
  stopit   -> setMaximumSize    (  24 , 24                ) ;
  timebar  -> setMinimumHeight  (  24                     ) ;
  timebar  -> setMaximumHeight  (  24                     ) ;
  timebar  -> setTickInterval   (  10                     ) ;
  timebar  -> setTickPosition   ( QSlider::TicksAbove     ) ;
  left     -> setMinimumWidth   (  12                     ) ;
  left     -> setMaximumWidth   (  12                     ) ;
  left     -> setMinimum        (   0                     ) ;
  left     -> setMaximum        ( 100                     ) ;
  left     -> setTickInterval   (   5                     ) ;
  left     -> setValue          ( 100                     ) ;
  left     -> setTickPosition   ( QSlider::TicksBothSides ) ;
  right    -> setMinimumWidth   (  12                     ) ;
  right    -> setMaximumWidth   (  12                     ) ;
  right    -> setMinimum        (   0                     ) ;
  right    -> setMaximum        ( 100                     ) ;
  right    -> setTickInterval   (   5                     ) ;
  right    -> setValue          ( 100                     ) ;
  right    -> setTickPosition   ( QSlider::TicksBothSides ) ;
  connect ( left   , SIGNAL (valueChanged (int))            ,
            this   , SLOT   (volumeChanged(int))          ) ;
  connect ( right  , SIGNAL (valueChanged (int))            ,
            this   , SLOT   (rightChanged (int))          ) ;
  timer . setParent ( this )                                ;
  connect ( &timer , SIGNAL ( timeout        () )           ,
            this   , SLOT   ( setCurrentTime () )         ) ;
  timer . start             ( 100                         ) ;
  connect ( this , SIGNAL ( final       ( ) )               ,
            this , SLOT   ( deleteLater ( ) )             ) ;
  stopit -> setAutoRaise ( true )                           ;
  stopit -> setCheckable ( true )                           ;
  stopit -> setChecked   ( true )                           ;
  stopit -> setIcon      ( QIcon(":/images/stop.png") )     ;
  w1     -> Color = QColor ( 255 , 0 ,   0 )                ;
  w2     -> Color = QColor (   0 , 0 , 255 )                ;
  w1     -> start ( )                                       ;
  w2     -> start ( )                                       ;
}

CaPlayFile::~CaPlayFile (void)
{
  timer . stop ( )               ;
  if ( NULL != C1 ) delete [] C1 ;
  if ( NULL != C2 ) delete [] C2 ;
}

void CaPlayFile::volumeChanged (int vol)
{
  if ( NULL == debugger ) return                       ;
  debugger -> printf ( "Volume %d\n" , vol )           ;
  if ( NULL == out      ) return                       ;
  if ( ! out->hasVolume ( ) ) return                   ;
  int id = 0                                           ;
  if ( NULL == right ) id = -1                         ;
  out -> setVolume ( vol * 100 , id )                  ;
  if ( NULL == right )                                 {
    left ->setToolTip(tr("Master volume %1").arg(vol)) ;
  } else                                               {
    left ->setToolTip(tr("Left channel %1" ).arg(vol)) ;
  }                                                    ;
}

void CaPlayFile::rightChanged (int vol)
{
  if ( NULL == debugger ) return                      ;
  debugger -> printf ( "Volume %d\n" , vol )          ;
  if ( NULL == out      ) return                      ;
  if ( ! out->hasVolume ( ) ) return                  ;
  out -> setVolume ( vol * 100 , 1 )                  ;
  right ->setToolTip(tr("Right channel %1").arg(vol)) ;
}

void CaPlayFile::setCurrentTime(void)
{
  if ( NULL == timebar ) return           ;
  timebar -> setValue ( currentTime )     ;
  /////////////////////////////////////////
  QString tt                              ;
  tt = tr("%1 of %2 seconds").arg(currentTime).arg(timebar->maximum()) ;
  timebar -> setToolTip ( tt )            ;
  /////////////////////////////////////////
  if ( NULL == mc      ) return           ;
  CiosAudio::CaSampleFormat fmt           ;
  int samplerate                          ;
  int channels                            ;
  fmt        = mc -> SampleFormat ( )     ;
  samplerate = mc -> SampleRate   ( )     ;
  channels   = mc -> Channels     ( )     ;
  /////////////////////////////////////////
  unsigned char * B                       ;
  short         * S                       ;
  int           * I                       ;
  float         * F                       ;
  /////////////////////////////////////////
  B = (unsigned char *) WC . window ( )   ;
  S = (short         *) WC . window ( )   ;
  I = (int           *) WC . window ( )   ;
  F = (float         *) WC . window ( )   ;
  /////////////////////////////////////////
  if ( channels > 2 ) return              ;
  /////////////////////////////////////////
  unsigned char * C [ 2 ]                 ;
  C [ 0 ] = C1                            ;
  C [ 1 ] = C2                            ;
  for (int i=0;i<samplerate;i++)          {
    for (int j=0;j<channels;j++)          {
      switch ( fmt )                      {
        case CiosAudio::cafInt8           :
        case CiosAudio::cafUint8          :
          ((unsigned char *)C[j])[i] = *B ;
          B++                             ;
        break                             ;
        case CiosAudio::cafInt16          :
          ((short         *)C[j])[i] = *S ;
          S++                             ;
        break                             ;
        case CiosAudio::cafInt32          :
          ((int           *)C[j])[i] = *I ;
          I++                             ;
        break                             ;
        case CiosAudio::cafFloat32        :
          ((float         *)C[j])[i] = *F ;
          F++                             ;
        break                             ;
        default                           :
        break                             ;
      }                                   ;
    }                                     ;
  }                                       ;
  /////////////////////////////////////////
  CaWaveDisplay * W [ 2 ]                 ;
  W [ 0 ] = w1                            ;
  W [ 1 ] = w2                            ;
  for (int i=0;i<channels;i++)            {
    Drawing ( W[i],C[i],samplerate,fmt )  ;
  }                                       ;
}

void CaPlayFile::Drawing                    (
       CaWaveDisplay           * w          ,
       unsigned char           * c          ,
       int                       SampleRate ,
       CiosAudio::CaSampleFormat format     )
{
  QSize  s = w -> Image . size ( )           ;
  QImage I = QImage(s,QImage::Format_ARGB32) ;
  ////////////////////////////////////////////
  QPainter p                                 ;
  int H = s . height ( ) / 2                 ;
  I . fill ( QColor ( 255 , 255 , 255 ) )    ;
  p . begin  ( &I         )                  ;
  p . setPen ( w -> Color )                  ;
  for (int i=0;i<SampleRate;i++)             {
    double x                                 ;
    double y                                 ;
    x  = i                                   ;
    x *= s . width ( )                       ;
    x /= SampleRate                          ;
    switch ( format )                        {
      case CiosAudio::cafInt16               :
        y  = ((short *)c)[i]                 ;
        y /= 32768.0                         ;
        y *= H                               ;
        y += H                               ;
        p  . drawPoint ( x , y )             ;
      break                                  ;
    }                                        ;
  }                                          ;
  p . end ( )                                ;
  ////////////////////////////////////////////
  if ( s == w->Image.size()) w->Image = I    ;
}

void CaPlayFile::run (void)
{
  CiosAudio::Core c1                                                         ;
  CiosAudio::Core c2                                                         ;
  c1 . setDebugger ( (CiosAudio::Debugger *)debugger )                       ;
  c2 . setDebugger ( (CiosAudio::Debugger *)debugger )                       ;
  c1 . Initialize  (                                 )                       ;
  c2 . Initialize  (                                 )                       ;
  ////////////////////////////////////////////////////////////////////////////
  if ( decodeId < 0 )                                                        {
    for (int i=0;(decodeId < 0) && i<c1.DeviceCount();i++)                   {
      CiosAudio::DeviceInfo * dev                                            ;
      dev = c1.GetDeviceInfo((CiosAudio::CaDeviceIndex)i)                    ;
      if ( NULL != dev && dev->maxInputChannels>0)                           {
        CiosAudio::CaHostApiTypeId hid = dev->hostType                       ;
        if ( CiosAudio::FFMPEG == hid ) decodeId = i                         ;
        if ( CiosAudio::VLC    == hid ) decodeId = i                         ;
      }                                                                      ;
    }                                                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  if ( decodeId < 0 )                                                        {
    debugger -> printf ( "No media decoder supported\n" )                    ;
    c1 . Terminate ( )                                                       ;
    c2 . Terminate ( )                                                       ;
    return                                                                   ;
  }                                                                          ;
  debugger -> printf ( "Decoder is %d\n" , decodeId )                        ;
  ////////////////////////////////////////////////////////////////////////////
  if ( deviceId < 0 ) deviceId = c1 . DefaultOutputDevice ( )                ;
  debugger -> printf ( "Output device is %d\n" , deviceId )                  ;
  ////////////////////////////////////////////////////////////////////////////
  CiosAudio::StreamParameters INSP ( decodeId )                              ;
  CiosAudio::StreamParameters ONSP                                           ;
  CiosAudio::Stream         * stream   = NULL                                ;
  ////////////////////////////////////////////////////////////////////////////
  mc  = INSP . CreateCodec ( )                                               ;
  mc -> setFilename ( Filename.toUtf8().constData() )                        ;
  mc -> setInterval ( 100                           )                        ;
  mc -> setWaiting  ( true                          )                        ;
  debugger -> printf ( "MediaCodec created\n" )                              ;
  ////////////////////////////////////////////////////////////////////////////
  CiosAudio::CaError rt                                                      ;
  rt = c1 . Open ( &stream                                                   ,
                   &INSP                                                     ,
                   NULL                                                      ,
                   44100  /* actually, this parameter does nothing */        ,
                   2205   /* actually, this parameter does nothing */        ,
                   0                                                         ,
                   (CiosAudio::Conduit *)&WC                               ) ;
  if ( ( rt != CiosAudio::NoError ) || ( NULL == stream ) )                  {
    debugger -> printf ( "Open media input %s failure\n"                     ,
                         Filename.toUtf8().constData()                     ) ;
    return                                                                   ;
  }                                                                          ;
  debugger -> printf ( "Media decoder opened\n" )                            ;
  long long ds = mc->Length()/1000000                                        ;
  timebar -> setMinimum ( 0  )                                               ;
  timebar -> setMaximum ( ds )                                               ;
  timebar -> setValue   ( 0  )                                               ;
  timebar -> setToolTip ( tr("%1 seconds").arg(ds) )                         ;
  ////////////////////////////////////////////////////////////////////////////
  int bpf = mc->SampleRate() * mc->BytesPerSample()                          ;
  WC . setBufferSize ( bpf * 5 , bpf )                                       ;
  WC . setTime       ( bpf           )                                       ;
  C1 = new unsigned char [ bpf ]                                             ;
  C2 = new unsigned char [ bpf ]                                             ;
  if ( mc->Channels() < 2 )                                                  {
    w2 -> deleteLater ( )                                                    ;
    w2  = NULL                                                               ;
  }                                                                          ;
  debugger -> printf ( "Conduit buffer created\n" )                          ;
  ////////////////////////////////////////////////////////////////////////////
  ONSP . device           = deviceId                                         ;
  ONSP . channelCount     = mc -> Channels     ( )                           ;
  ONSP . sampleFormat     = mc -> SampleFormat ( )                           ;
  ONSP . suggestedLatency = c2.GetDeviceInfo(deviceId)->defaultLowOutputLatency ;
  ONSP . streamInfo       = NULL                                             ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c2. Open ( &out                                                       ,
                  NULL                                                       ,
                  &ONSP                                                      ,
                  mc->SampleRate()                                           ,
                  mc->PeriodSize()                                           ,
                  0                                                          ,
                  (CiosAudio::Conduit *)&WC                                ) ;
  ////////////////////////////////////////////////////////////////////////////
  if ( ( rt != CiosAudio::NoError ) || ( NULL == out ) )                     {
    debugger -> printf ( "Open audio output failure\n" )                     ;
    return                                                                   ;
  }                                                                          ;
  debugger -> printf ( "Audio output device opened\n" )                      ;
  left     -> setEnabled ( out -> hasVolume ( ) )                            ;
  if ( mc->Channels() < 2 )                                                  {
    right  -> deleteLater ( )                                                ;
    right   = NULL                                                           ;
    left   -> setValue    ( (int)(out->Volume() / 100) )                     ;
    left ->setToolTip(tr("Master volume %1").arg((int)(out->Volume()/100)))  ;
  } else                                                                     {
    int LV = (int)(out->Volume(0) / 100)                                     ;
    int RV = (int)(out->Volume(1) / 100)                                     ;
    right  -> setEnabled ( out -> hasVolume ( )           )                  ;
    left   -> setValue   ( LV                             )                  ;
    right  -> setValue   ( RV                             )                  ;
    left   -> setToolTip ( tr("Left channel %1" ).arg(LV) )                  ;
    right  -> setToolTip ( tr("Right channel %1").arg(RV) )                  ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c1 . Start ( stream )                                                 ;
  if ( ( rt != CiosAudio::NoError ) )                                        {
    debugger -> printf ( "Audio input can not start\n" )                     ;
    return                                                                   ;
  }                                                                          ;
  debugger -> printf ( "Decoder started\n" )                                 ;
  ////////////////////////////////////////////////////////////////////////////
  rt = c2 . Start ( out    )                                                 ;
  if ( ( rt != CiosAudio::NoError ) )                                        {
    debugger -> printf ( "Audio output can not start\n" )                    ;
    return                                                                   ;
  }                                                                          ;
  debugger -> printf ( "Audio output started\n" )                            ;
  ////////////////////////////////////////////////////////////////////////////
  while ( stopit ->isChecked (        )                                     &&
          ( 1 == c1 . IsActive ( stream )                                   ||
            1 == c2 . IsActive ( out    )                                ) ) {
    long long All = WC.Total()                                               ;
    All /= bpf                                                               ;
    currentTime = (int)All                                                   ;
    CiosAudio :: Timer::Sleep ( 100 )                                        ;
  }                                                                          ;
  ////////////////////////////////////////////////////////////////////////////
  debugger -> printf ( "Stream stopped\n" )                                  ;
  if ( 0 != c1 . IsStopped ( stream ) ) c1 . Stop ( stream )                 ;
  if ( 0 != c2 . IsStopped ( out    ) ) c2 . Stop ( out    )                 ;
  debugger -> printf ( "Stream closing\n" )                                  ;
  c1 . Close ( stream )                                                      ;
  c2 . Close ( out    )                                                      ;
  debugger -> printf ( "Stream closed\n" )                                   ;
  ////////////////////////////////////////////////////////////////////////////
  c1 . Terminate ( )                                                         ;
  c2 . Terminate ( )                                                         ;
  debugger -> printf ( "Core terminated\n" )                                 ;
  ////////////////////////////////////////////////////////////////////////////
  emit final ( )                                                             ;
}

//////////////////////////////////////////////////////////////////////////////

CaWaveFreqFFT:: CaWaveFreqFFT     ( QWidget * parent      )
              : QMainWindow       (           parent      )
              , CiosAudio::Thread (                       )
              , ui                ( new Ui::CaWaveFreqFFT )
{
  ui -> setupUi ( this ) ;
  TABs     = NULL        ;
  devices  = NULL        ;
  dgui     = NULL        ;
  debugger = NULL        ;
}

CaWaveFreqFFT::~CaWaveFreqFFT(void)
{
  delete ui ;
}

void CaWaveFreqFFT::Quit(void)
{
  qApp -> closeAllWindows        ( ) ;
  qApp -> quitOnLastWindowClosed ( ) ;
}

void CaWaveFreqFFT::Open(void)
{
  QString filename = QFileDialog::getOpenFileName     (
                       this                           ,
                       tr("Open audio file")          ,
                       ""                             ,
                       "*.*"                        ) ;
  if ( filename.length() <= 0 ) return                ;
  int          id                                     ;
  CaPlayFile * pf                                     ;
  pf    = new CaPlayFile   ( TABs          )          ;
  TABs -> addTab           ( pf , filename )          ;
  TABs -> setCurrentWidget ( pf            )          ;
  pf   -> Filename = filename                         ;
  pf   -> debugger = debugger                         ;
  id    = devices->OutputDevices->currentIndex()      ;
  if ( id >=0 )                                       {
    id = devices->OutputDevices->itemData(id).toInt() ;
    pf -> deviceId = id                               ;
  }                                                   ;
  qApp -> processEvents ( )                           ;
  pf   -> start         ( )                           ;
}

void CaWaveFreqFFT::startup(void)
{
  //////////////////////////////////////////////////////////
  TABs      = new QTabWidget ( this                      ) ;
  TABs     -> setTabPosition ( QTabWidget::South         ) ;
  setCentralWidget           ( TABs                      ) ;
  dgui      = new QWidget    ( TABs                      ) ;
  devices   = new Ui::CiosDevices                          ;
  devices  -> setupUi        ( dgui                      ) ;
  TABs     -> addTab         ( dgui , tr("Devices")      ) ;
  //////////////////////////////////////////////////////////
  debugger  = new CiosAudio::CaQtDebugger ( TABs         ) ;
  TABs     -> addTab         ( debugger , tr("Debugger") ) ;
  //////////////////////////////////////////////////////////
  debugger -> start          ( 100                       ) ;
  //////////////////////////////////////////////////////////
  start                      ( 10001                     ) ;
}

void CaWaveFreqFFT::run(int type,CiosAudio::ThreadData * data)
{
  switch ( type )                     {
    case 10001                        :
      ListDetails (                 ) ;
    break                             ;
    case 10002                        :
      Play        ( data->Arguments ) ;
    break                             ;
  }                                   ;
}

void CaWaveFreqFFT::ListDetails (void)
{
  int             defaultInput  = -1                                    ;
  int             defaultOutput = -1                                    ;
  int             inputIndex    = -1                                    ;
  int             outputIndex   = -1                                    ;
  CiosAudio::Core core                                                  ;
  core . setDebugger ( debugger )                                       ;
  core . Initialize  (          )                                       ;
  defaultInput  = core . DefaultInputDevice  ( )                        ;
  defaultOutput = core . DefaultOutputDevice ( )                        ;
  for (int i=0;i<core.HostApiCount();i++)                               {
    CiosAudio::HostApiInfo * hai = core.GetHostApiInfo(i)               ;
    if ( NULL != hai )                                                  {
      QString s(hai->name)                                              ;
      devices -> HostAPIs -> addItem ( s , (int)hai->type )             ;
    }                                                                   ;
  }                                                                     ;
  for (int i=0;i<core.DeviceCount();i++)                                {
    CiosAudio::DeviceInfo * dev                                         ;
    dev = core.GetDeviceInfo(i)                                         ;
    if ( NULL != dev && dev->maxInputChannels>0)                        {
      QString s = CiosAudio::GetDeviceName(core,*dev)                   ;
      if ( defaultInput == i )                                          {
        inputIndex = devices->InputDevices->currentIndex()              ;
      }                                                                 ;
      devices -> InputDevices -> addItem ( s , i )                      ;
    }                                                                   ;
  }                                                                     ;
  for (int i=0;i<core.DeviceCount();i++)                                {
    CiosAudio::DeviceInfo * dev                                         ;
    dev = core.GetDeviceInfo(i)                                         ;
    if ( NULL != dev && dev->maxOutputChannels>0)                       {
      QString s = CiosAudio::GetDeviceName(core,*dev)                   ;
      if ( defaultOutput == i )                                         {
        outputIndex = devices->OutputDevices->currentIndex()            ;
      }                                                                 ;
      devices ->OutputDevices -> addItem ( s , i )                      ;
    }                                                                   ;
  }                                                                     ;
  ///////////////////////////////////////////////////////////////////////
  #ifdef FFMPEGLIB
  int             z     = 0                                             ;
  AVCodecID       id                                                    ;
  AVCodec       * codec = NULL                                          ;
  ///////////////////////////////////////////////////////////////////////
  while ( AV_CODEC_ID_NONE != CiosAudio::AllFFmpegCodecIDs[z] )         {
    id = CiosAudio::AllFFmpegCodecIDs[z]                                ;
    codec = ::avcodec_find_encoder(id)                                  ;
    if ( NULL != codec )                                                {
      if ( AVMEDIA_TYPE_AUDIO == ( codec->type & AVMEDIA_TYPE_AUDIO ) ) {
        QString s(codec->long_name)                                     ;
        devices -> Encoders -> addItem ( s , (int)id )                  ;
      }                                                                 ;
    }                                                                   ;
    z++                                                                 ;
  }                                                                     ;
  ///////////////////////////////////////////////////////////////////////
  z     = 0                                                             ;
  codec = NULL                                                          ;
  while ( AV_CODEC_ID_NONE != CiosAudio::AllFFmpegCodecIDs[z] )         {
    id = CiosAudio::AllFFmpegCodecIDs[z]                                ;
    codec = ::avcodec_find_decoder(id)                                  ;
    if ( NULL != codec )                                                {
      if ( AVMEDIA_TYPE_AUDIO == ( codec->type & AVMEDIA_TYPE_AUDIO ) ) {
        QString s(codec->long_name)                                     ;
        devices -> Decoders -> addItem ( s , (int)id )                  ;
      }                                                                 ;
    }                                                                   ;
    z++                                                                 ;
  }                                                                     ;
  #else
  devices -> Decoders -> hide ( )                                       ;
  devices -> Encoders -> hide ( )                                       ;
  #endif
  ///////////////////////////////////////////////////////////////////////
  core . Terminate  ( )                                                 ;
  ///////////////////////////////////////////////////////////////////////
  devices -> HostAPIs      -> setCurrentIndex ( 0 )                     ;
  devices -> InputDevices  -> setCurrentIndex ( 0 )                     ;
  devices -> OutputDevices -> setCurrentIndex ( 0 )                     ;
  devices -> Decoders      -> setCurrentIndex ( 0 )                     ;
  devices -> Encoders      -> setCurrentIndex ( 0 )                     ;
  if ( inputIndex  >= 0 )                                               {
    devices->InputDevices->setCurrentIndex(inputIndex)                  ;
  }                                                                     ;
  if ( outputIndex >= 0 )                                               {
    devices->OutputDevices->setCurrentIndex(outputIndex)                ;
  }                                                                     ;
}

void CaWaveFreqFFT::Play(char * filename)
{
  int id = devices->OutputDevices->currentIndex()          ;
  if (id < 0 ) return                                      ;
  id = devices->OutputDevices->itemData(id).toInt()        ;
  CiosAudio :: MediaPlay ( filename , id , -1 , debugger ) ;
}

void CaWaveFreqFFT::Speaker(void)
{
}

void CaWaveFreqFFT::Record(void)
{
}

void CaWaveFreqFFT::Wave(void)
{
}

void CaWaveFreqFFT::Display(void)
{
}

//////////////////////////////////////////////////////////////////////////////
