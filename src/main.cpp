#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <singleinstanceguard.h>
#include <FluentQt/FluentQt.h>
#include "view/mainwindow.h"

int main(int argc, char *argv[])
{
    // return fluentqtApplication::runApplication(argc, argv);

    QCoreApplication::setOrganizationName("OnePoint");
    QCoreApplication::setApplicationName("DocLife");

    fluent::prepareHighDpiApplication();
    QApplication a(argc, argv);
    fluent::initializeResources();
    a.setFont(
        Typography::fontStyle(Typography::FontRole::Body).toQFont());

    doclife::core::SingleInstanceGuard guard;
    if (!guard.enforce(QStringLiteral("OnePoint.DocLife.SingleInstance")))
        return 0;

    doclife::ui::MainWindow w;

    w.show();
    return a.exec();
}