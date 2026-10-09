#include <QApplication>
#include <FluentQt/FluentQt.h>
#include "view/mainwindow.h"

int main(int argc, char *argv[])
{
    // return fluentqtApplication::runApplication(argc, argv);

    fluent::prepareHighDpiApplication();
    QApplication a(argc, argv);
    fluent::initializeResources();
    a.setFont(
        Typography::fontStyle(Typography::FontRole::Body).toQFont());

    MainWindow w;
    w.show();
    return QApplication::exec();
}