#include "mainwindow.h"

#include <centralview/centralview.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->setWindowTitle("_公路资料管理");
    this->setWindowIcon(QIcon(":/icons/logo/logo.svg"));
    this->setMinimumSize(QSize(850,550));
    this->setWindowState(Qt::WindowMaximized);
    auto* cenWidget = new doclife::ui::CentralView();
    setCentralWidget(cenWidget);
}