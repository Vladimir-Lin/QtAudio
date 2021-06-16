#include "CaVideoPlayer.hpp"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CaVideoPlayer w;
    w.show();

    return a.exec();
}
