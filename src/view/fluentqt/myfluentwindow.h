#ifndef MY_FLUENT_WINDOW_H
#define MY_FLUENT_WINDOW_H

#include <QString>
#include <QCloseEvent>
#include <FluentQt/Windowing.h>
#include <components/basicinput/ToggleButton.h>

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

        void didCancelChanged();

    private slots:
        void switchDarkLight(bool dark);

    protected:
        void closeEvent(QCloseEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        void initConnect();
        void setCaptionTexts();
        void setContent();
        void setTitleBar();

        void onClosedCleanup();       // 关闭后的资源清理
        bool askUser();               // 关闭前的询问

        bool didCancel() const;
        void setDidCancel(bool newDidCancel);

        fluent::basicinput::ToggleButton* m_toggle = nullptr;
        CentralView* m_content = nullptr;
        bool m_didCancel = false;
    };
}
#endif // MY_FLUENT_WINDOW_H