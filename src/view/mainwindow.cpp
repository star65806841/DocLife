#include "mainwindow.h"

#include <centralview/centralview.h>
namespace doclife::ui
{
    MainWindow::MainWindow(QWidget* parent)
        : QMainWindow(parent)
    {
        this->setWindowTitle("_公路资料管理");
        this->setWindowIcon(QIcon(":/icons/logo/logo.svg"));
        this->setMinimumSize(QSize(850, 550));
        this->setWindowState(Qt::WindowMaximized);
        m_cenWidget = new doclife::ui::CentralView();
        setCentralWidget(m_cenWidget);
    }

    void MainWindow::closeEvent(QCloseEvent* event)
    {

    }
}