#ifndef MY_FLUENT_WINDOW_H
#define MY_FLUENT_WINDOW_H

#include <FluentQt/Windowing.h>
#include <QCloseEvent>
#include <QString>

namespace fluent::windowing {
    enum class BackdropEffect; // 前向声明，避免在头文件引入过多依赖
}
namespace doclife::ui {

class CentralView;
    class MyFluentWindow : public fluent::windowing::Window {
        Q_OBJECT

    public:
        explicit MyFluentWindow(QWidget* parent = nullptr);
        ~MyFluentWindow() override;

        void showAndActivate();
    signals:

    protected:
        void closeEvent(QCloseEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        void setCaptionTexts();
        void setContent();
        void setTitleBar();

        void onClosedCleanup();       // 关闭后的资源清理

        CentralView* m_content = nullptr;
    };
}
#endif // MY_FLUENT_WINDOW_H