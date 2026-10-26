#include "MyFluentWindow.h"

#include "centralview/centralview.h"
#include "business/projectcontext.h"
#include "view/popWindow/dialog/dialogbase.h"

#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QShowEvent>
#include <QHideEvent>
#include <QWidget>
#include <uisignalmanager.h>
// #include <item/TabViewItem.h>

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
        initConnect();

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
        if(!titleBar){
            return;
        }
        // titleBar->setTitleBarHeight(48);
        // macOS reserves leading traffic-light controls;
        // Windows reserves trailing caption buttons.
        int platformLeadingWidth = 0;
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
        int titleBarHeight = titleBar->height();
        // 创建标题栏内容
        auto* titleContent = new QWidget(titleBar); // 父对象设为 titleBar，随其销毁
        titleBar->setContentWidget(titleContent);
        // titleContent->setStyleSheet("background-color: #a1F1BF;");
        auto* layout = new QHBoxLayout(titleContent);
        layout->setContentsMargins(8, 0, 8, 0);
        layout->setSpacing(8);

        // auto* tabs = new fluent::navigation::TabView(titleContent);
        // tabs->setFixedHeight(titleBarHeight);
        // tabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        // tabs->setTabWidthMode(fluent::navigation::TabView::TabWidthMode::SizeToContent);
        // tabs->setTabReorderEnabled(true);
        // tabs->setTabsClosable(false);
        // tabs->setAddTabButtonVisible(false);

        // tabs->addTab(fluent::navigation::TabViewItem("主页", Typography::Icons::Home));
        // tabs->addTab(fluent::navigation::TabViewItem("编辑", Typography::Icons::Document));

        auto* icon = new QLabel("", titleContent);
        QPixmap pixmap(":/icons/logo/logo.svg");
        icon->setPixmap(pixmap);
        icon->setFixedSize(28, 28);

        auto* titleLabel = new fluent::textfields::Label("xx公路资料管理软件", titleContent);
        titleLabel->setFluentTypography(Typography::FontRole::BodyStrong);

        m_toggle = new fluent::basicinput::ToggleButton("",titleContent);
        m_toggle->setFluentStyle(fluent::basicinput::ToggleButton::Standard);
        m_toggle->setIconGlyph(
            Typography::Icons::glyph(QStringLiteral("ic_fluent_brightness_high_32_regular"))
        );
        // layout->addWidget(tabs);
        layout->addWidget(icon);
        layout->addWidget(titleLabel);
        layout->addStretch();
        layout->addWidget(m_toggle);
    }

    void MyFluentWindow::setCaptionTexts()
    {
        setCaptionButtonToolTips("最小化", "最大化", "关闭", "向下还原");
        setCaptionButtonAccessibleNames("minimize", "maximize", "close", "restore");
    }

    void MyFluentWindow::closeEvent(QCloseEvent* event)
    {
        // 若业务需要“关闭前确认”，可在此弹出对话框
        bool shouldClose = askUser();
        if (!shouldClose) {
            event->ignore();
            return;
        }


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

        // 先关闭文档再关闭窗口
        doclife::core::ProjectContext::instance()->closeProject();
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

    bool MyFluentWindow::askUser()
    {
        if (!doclife::core::ProjectContext::instance()->hasOpenProject()) {
            // 没有打开过项目
            return true;
        }
        else if (doclife::core::ProjectContext::instance()->isUntitled() && !didCancel()) {
            // 项目尚未保存过
            doclife::ui::DialogBase::show(
                this,
                QStringLiteral("保存确认"),
                QStringLiteral("当前文档从未保存，你要保存再关闭吗。"),
                [this]() {
                    qDebug() << "User clicks OK";
                    emit UISignalManager::instance()->saveProject();
                },
                [this]() {
                    qDebug() << "User clicks Cancel";
                    // 点击不保存
                    setDidCancel(true);
                }
            );
            return false;
        }
        else
        {
            return true;
        }
    }

    bool MyFluentWindow::didCancel() const
    {
        return m_didCancel;
    }

    void MyFluentWindow::setDidCancel(bool newDidCancel)
    {
        if (m_didCancel == newDidCancel)
            return;
        m_didCancel = newDidCancel;
        emit didCancelChanged();
    }

    void MyFluentWindow::initConnect() {
        connect(this, &MyFluentWindow::didCancelChanged, this, [this]() {
            QTimer::singleShot(300, this, &MyFluentWindow::close);
            });
        connect(m_toggle, &fluent::basicinput::ToggleButton::toggled, this, &MyFluentWindow::switchDarkLight);
    }

    void MyFluentWindow::switchDarkLight(bool dark)
    {
        if (dark) {
            fluent::FluentElement::setTheme(fluent::FluentElement::Theme::Dark);
            m_toggle->setIconGlyph(
                Typography::Icons::glyph(QStringLiteral("ic_fluent_weather_moon_32_regular"))
            );
        }
        else {
            fluent::FluentElement::setTheme(fluent::FluentElement::Theme::Light);
            m_toggle->setIconGlyph(
                Typography::Icons::glyph(QStringLiteral("ic_fluent_brightness_high_32_regular"))
            );
        }
    }
}