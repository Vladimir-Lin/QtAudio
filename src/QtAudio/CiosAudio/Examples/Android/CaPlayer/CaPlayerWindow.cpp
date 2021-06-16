#include "CaPlayerWindow.hpp"
#include "ui_CaPlayerWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CaPlayerWindow w;
    w.show();

    return a.exec();
}

CaPlayerWindow:: CaPlayerWindow ( QWidget * parent       )
               : QMainWindow    (           parent       )
               , ui             ( new Ui::CaPlayerWindow )
{
  ui -> setupUi ( this ) ;
}

CaPlayerWindow::~CaPlayerWindow(void)
{
  delete ui ;
}
