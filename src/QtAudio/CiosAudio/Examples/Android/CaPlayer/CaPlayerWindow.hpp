#ifndef CAPLAYERWINDOW_HPP
#define CAPLAYERWINDOW_HPP

#include <QApplication>
#include <QMainWindow>

namespace Ui {
class CaPlayerWindow;
}

class CaPlayerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CaPlayerWindow(QWidget *parent = 0);
    ~CaPlayerWindow();

private:
    Ui::CaPlayerWindow *ui;
};

#endif // CAPLAYERWINDOW_HPP
