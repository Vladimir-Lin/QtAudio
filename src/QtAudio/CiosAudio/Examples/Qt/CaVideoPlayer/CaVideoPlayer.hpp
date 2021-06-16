#ifndef CAVIDEOPLAYER_HPP
#define CAVIDEOPLAYER_HPP

#include <QMainWindow>

namespace Ui {
class CaVideoPlayer;
}

class CaVideoPlayer : public QMainWindow
{
    Q_OBJECT

public:
    explicit CaVideoPlayer(QWidget *parent = 0);
    ~CaVideoPlayer();

private:
    Ui::CaVideoPlayer *ui;
};

#endif // CAVIDEOPLAYER_HPP
