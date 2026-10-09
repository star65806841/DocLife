#include "application.h"

#include <QApplication>
#include <FluentQt/FluentQt.h>

#include <QWindow>
#include <QVBoxLayout>
#include <QWidget>

namespace fluentqtApplication {

int runApplication(int argc, char** argv)
{
    fluent::prepareHighDpiApplication();
    QApplication application(argc, argv);
    fluent::initializeResources();
    application.setFont(
        Typography::fontStyle(Typography::FontRole::Body).toQFont());
    application.setWindowIcon(QIcon(":/icons/logo/logo.ico"));
    // application.setApplicationDisplayName(QStringLiteral("FluentQt"));
    // fluent::windowing::Window window;
    QWindow window;
    window.setTitle(QStringLiteral("FluentQt Hello World"));
    window.resize(720, 520);

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);

    auto* button = new fluent::basicinput::Button(QStringLiteral("Hello from FluentQt"), content);
    button->setFluentStyle(fluent::basicinput::Button::Accent);
    QObject::connect(button, &fluent::basicinput::Button::clicked, button,
                     [button] { button->setText(QStringLiteral("Hello, you!")); });
    layout->addWidget(button, 0, Qt::AlignCenter);
    window;

    window.show();
    return application.exec();
}

}
