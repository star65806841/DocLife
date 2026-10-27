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
        fluent::initializeResources();
        // C:/Users/admin/AppData/Local/DocLife/themes/fluent.json
        const QString themePath = fluent::UserTheme::filePath();
        if (!QFile::exists(themePath)) {
            fluent::UserTheme::exportTemplate();
        }

        // 从文件加载全局主题。这会替换之前任何 applyOverrides 的内存配置。
        fluent::UserTheme::apply();

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
