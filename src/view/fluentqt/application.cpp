#include "application.h"

#include <QApplication>
#include <singleinstanceguard.h>
#include <FluentQt/FluentQt.h>

#include "centralview/MyFluentWindow.h"

namespace fluentqtApplication {

    int runApplication(int argc, char** argv)
    {
        fluent::prepareHighDpiApplication();
        QApplication application(argc, argv);
        fluent::initializeResources();
        application.setFont(
            Typography::fontStyle(Typography::FontRole::Body).toQFont());
        application.setWindowIcon(QIcon(":/icons/logo/logo.svg"));
        application.setApplicationDisplayName(QStringLiteral("DocLife"));

        doclife::core::SingleInstanceGuard guard;
        if (!guard.enforce(QStringLiteral("OnePoint.DocLife.SingleInstance")))
            return 0;

        doclife::ui::MyFluentWindow window;

        window.showAndActivate();
        return application.exec();
    }
}
