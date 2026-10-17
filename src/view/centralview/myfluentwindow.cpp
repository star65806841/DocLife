#include "MyFluentWindow.h"

#include "centralview.h"

#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QShowEvent>
#include <QHideEvent>
#include <QWidget>

namespace doclife::ui {
    MyFluentWindow::MyFluentWindow(QWidget* parent)
        : fluent::windowing::Window(parent)
    {
        setBackdropEffect(fluent::windowing::BackdropEffect::Mica);
        setCustomWindowChromeEnabled(true);
        // setAttribute(Qt::WA_DeleteOnClose); // 关闭后自动 delete

        setMinimumSize(QSize(850, 550));
        setWindowState(Qt::WindowMaximized);

        setTitleBar();
        setCaptionTexts();
        setContent();
    }

    MyFluentWindow::~MyFluentWindow() = default;

    void MyFluentWindow::setContent()
    {
        m_content = new CentralView(this);
        setContentWidget(m_content);
    }

    void MyFluentWindow::setTitleBar()
    {
       fluent::windowing::TitleBar* titleBar = this->titleBar();
        // titleBar->setTitleBarHeight(48);
        // macOS reserves leading traffic-light controls;
        // Windows reserves trailing caption buttons.
       int platformLeadingWidth  = 0;
       int platformTrailingWidth = 0;

#if defined(Q_OS_WIN)
        // Windows 自绘 caption 按钮总宽度（三个 Button 的宽度之和）
        // 可以从 TitleBar 实际创建按钮后读取，或使用经验值约 138px
        platformTrailingWidth = 138;
#elif defined(Q_OS_MACOS)
        // macOS 交通灯区域约 78px
        platformLeadingWidth = 78;
#endif
        titleBar->setSystemReservedLeadingWidth(platformLeadingWidth);
        titleBar->setSystemReservedTrailingWidth(platformTrailingWidth);

        // 创建标题栏内容
        auto* titleContent = new QWidget(titleBar); // 父对象设为 titleBar，随其销毁
        titleBar->setContentWidget(titleContent);
        // titleContent->setStyleSheet("background-color: #a1F1BF;");
        auto* layout = new QHBoxLayout(titleContent);
        layout->setContentsMargins(8, 0, 8, 0);
        layout->setSpacing(8);

        auto* icon = new QLabel("", titleContent);
        QPixmap pixmap(":/icons/logo/logo.svg");
        icon->setPixmap(pixmap);
        icon->setFixedSize(28, 28);
        auto* titleLabel = new fluent::textfields::Label("xx公路资料管理软件", titleContent);
        titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);

        layout->addWidget(icon);
        layout->addWidget(titleLabel);
        layout->addStretch();
    }

    void MyFluentWindow::setCaptionTexts()
    {
        setCaptionButtonToolTips("最小化", "最大化", "关闭", "向下还原");
        setCaptionButtonAccessibleNames("minimize", "maximize", "close", "restore");
    }

    void MyFluentWindow::closeEvent(QCloseEvent* event)
    {
        // 若业务需要“关闭前确认”，可在此弹出对话框
        // bool shouldClose = askUser();
        // if (!shouldClose) {
        //     event->ignore();
        //     return;
        // }


        // 执行必要的资源释放、配置保存等
        onClosedCleanup();

        // 接受关闭事件，窗口随后将被隐藏/销毁
        event->accept();

        // 调用基类的 closeEvent 以保留 Fluent-Qt 的清理逻辑
        fluent::windowing::Window::closeEvent(event);
    }

    void MyFluentWindow::showAndActivate()
    {
        show();
        requestForegroundActivation();
    }

    void MyFluentWindow::onClosedCleanup()
    {
        // 保存窗口几何信息、释放独占资源等
        // 内容部件由 WA_DeleteOnClose 或父对象析构负责
        m_content->closeProject();
    }

    void MyFluentWindow::showEvent(QShowEvent* event)
    {
        fluent::windowing::Window::showEvent(event);
        // 首次显示时可进行懒初始化
    }

    void MyFluentWindow::hideEvent(QHideEvent* event)
    {
        fluent::windowing::Window::hideEvent(event);
        // 窗口隐藏时暂停动画、计时器等
    }
}