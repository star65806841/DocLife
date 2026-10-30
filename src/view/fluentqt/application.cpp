#include "application.h"

#include <QApplication>
#include <QFile>
#include <singleinstanceguard.h>
#include <FluentQt/FluentQt.h>

#include "view/fluentqt/MyFluentWindow.h"

namespace fluentqtApplication {

    int runApplication(int argc, char** argv)
    {
        fluent::prepareHighDpiApplication();
        QApplication application(argc, argv);

        // ★★★ 必须放在任何 QStandardPaths / UserTheme 调用之前 ★★★
        QCoreApplication::setOrganizationName(QStringLiteral("OnePoint"));
        QCoreApplication::setApplicationName(QStringLiteral("DocLife"));

        // 从这里往下才允许访问 QStandardPaths / 用户主题
        fluent::initializeResources();

        const QString themePath = fluent::UserTheme::filePath();
        if (!QFile::exists(themePath))
            fluent::UserTheme::exportTemplate();
        fluent::UserTheme::apply();

        application.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());
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
