#include "CaVideoPlayer.hpp"
#include "ui_CaVideoPlayer.h"

CaVideoPlayer::CaVideoPlayer(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CaVideoPlayer)
{
    ui->setupUi(this);
}

CaVideoPlayer::~CaVideoPlayer()
{
    delete ui;
}
