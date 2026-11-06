#include "backstageview.h"

#include "backstagenavbutton.h"
#include "backstagepages.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <business/projectcontext.h>
#include <uisignalmanager.h>

namespace doclife::ui {

    namespace {
        constexpr int kNavWidth = 220;
        constexpr int kBackBarHeight = 52;
    }

    BackstageView::BackstageView(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("BackstageView"));
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(QStringLiteral(
            "#BackstageView { background: #FFFFFF; }"
            "#BackstageLeftPane { background: #F3F3F3; }"
            "#BackstageStack { background: #FFFFFF; }"));
        setFocusPolicy(Qt::StrongFocus);

        buildUi();
        hide();

        if (parentWidget()) {
            parentWidget()->installEventFilter(this);
            setGeometry(parentWidget()->rect());
        }
    }

    void BackstageView::buildUi()
    {
        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // ---------------- 左侧导航 ----------------
        auto* leftPane = new QWidget(this);
        leftPane->setObjectName(QStringLiteral("BackstageLeftPane"));
        leftPane->setFixedWidth(kNavWidth);
        leftPane->setAttribute(Qt::WA_StyledBackground, true);

        auto* leftColumn = new QVBoxLayout(leftPane);
        leftColumn->setContentsMargins(0, 0, 0, 8);
        leftColumn->setSpacing(0);

        // 顶部返回栏
        auto* backBar = new QWidget(leftPane);
        backBar->setFixedHeight(kBackBarHeight);
        auto* backLayout = new QHBoxLayout(backBar);
        backLayout->setContentsMargins(8, 6, 8, 6);

        auto* backButton = new QToolButton(backBar);
        backButton->setText(QStringLiteral("← 文件"));
        backButton->setCursor(Qt::PointingHandCursor);
        backButton->setStyleSheet(QStringLiteral(
            "QToolButton { border: none; padding: 8px 12px; font-size: 15px;"
            " color: #202020; background: transparent; border-radius: 4px; }"
            "QToolButton:hover { background: rgba(0,0,0,0.06); }"));
        connect(backButton, &QToolButton::clicked, this, &BackstageView::closeBackstage);
        backLayout->addWidget(backButton);
        backLayout->addStretch(1);
        leftColumn->addWidget(backBar);

        // 导航项容器（可滚动）
        m_leftScroll = new QScrollArea(leftPane);
        m_leftScroll->setWidgetResizable(true);
        m_leftScroll->setFrameShape(QFrame::NoFrame);
        m_leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_leftScroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));

        auto* navHost = new QWidget(m_leftScroll);
        navHost->setStyleSheet(QStringLiteral("background: transparent;"));
        m_leftLayout = new QVBoxLayout(navHost);
        m_leftLayout->setContentsMargins(0, 0, 0, 0);
        m_leftLayout->setSpacing(0);
        m_leftScroll->setWidget(navHost);
        leftColumn->addWidget(m_leftScroll, 1);

        root->addWidget(leftPane);

        // ---------------- 右侧页面栈 ----------------
        m_stack = new QStackedWidget(this);
        m_stack->setObjectName(QStringLiteral("BackstageStack"));
        m_stack->setAttribute(Qt::WA_StyledBackground, true);
        root->addWidget(m_stack, 1);

        // ---------------- 页面 ----------------
        auto* newPage = new BackstageNewPage(this);
        auto* openPage = new BackstageOpenPage(this);
        auto* printPage = new BackstagePlaceholderPage(QStringLiteral("打印"),
            QStringLiteral("打印与批量输出还在接入中，暂时可以从表格工具栏导出。"), this);
        auto* settingsPage = new BackstageSettingsPage(this);
        auto* userPage = new BackstagePlaceholderPage(QStringLiteral("用户"),
            QStringLiteral("登录、单位信息与授权状态会放在这里。"), this);

        addPageItem(QStringLiteral("ic_fluent_add_24_regular"), QStringLiteral("新建"), newPage, true);
        addPageItem(QStringLiteral("ic_fluent_folder_open_24_regular"), QStringLiteral("打开"), openPage);
        addActionItem(QStringLiteral("ic_fluent_save_24_regular"), QStringLiteral("保存"), []() {
            emit UISignalManager::instance()->saveProject();
            });
        addPageItem(QStringLiteral("ic_fluent_print_24_regular"), QStringLiteral("打印"), printPage);
        addActionItem(QStringLiteral("ic_fluent_dismiss_24_regular"), QStringLiteral("关闭"), [this]() {
            doclife::core::ProjectContext::instance()->closeProject();
            closeBackstage();
            });
        addSeparatorItem();
        addPageItem(QStringLiteral("ic_fluent_settings_24_regular"), QStringLiteral("设置"), settingsPage);
        addPageItem(QStringLiteral("ic_fluent_person_24_regular"), QStringLiteral("用户"), userPage);

        // ---------------- 页面动作 ----------------
        connect(newPage, &BackstageNewPage::requestBlank, this, [this]() {
            closeBackstage();
            emit UISignalManager::instance()->newProject();
            });
        connect(newPage, &BackstageNewPage::requestOpen, this, [this]() {
            closeBackstage();
            emit UISignalManager::instance()->openProject();
            });
        connect(newPage, &BackstageNewPage::requestOpenPath, this, [this](const QString& path) {
            closeBackstage();
            doclife::core::ProjectContext::instance()->openProject(path);
            });

        connect(openPage, &BackstageOpenPage::requestBrowse, this, [this]() {
            closeBackstage();
            emit UISignalManager::instance()->openProject();
            });
        connect(openPage, &BackstageOpenPage::requestOpenPath, this, [this](const QString& path) {
            closeBackstage();
            doclife::core::ProjectContext::instance()->openProject(path);
            });

        connect(settingsPage, &BackstageSettingsPage::requestSettings, this, []() {
            emit UISignalManager::instance()->popSettings();
            });
        connect(settingsPage, &BackstageSettingsPage::requestProjectInfo, this, []() {
            emit UISignalManager::instance()->popProjectsetting();
            });
        connect(settingsPage, &BackstageSettingsPage::requestAbout, this, []() {
            emit UISignalManager::instance()->popAbout();
            });

        if (!m_navButtons.isEmpty()) select(0);
    }

    void BackstageView::addPageItem(const QString& glyph, const QString& text,
                                    BackstagePageBase* page, bool checked)
    {
        auto* button = new BackstageNavButton(glyph, text, BackstageNavButton::Role::Page, this);
        m_leftLayout->addWidget(button);
        m_navButtons.append(button);
        m_navToPage.append(m_stack->count());
        m_stack->addWidget(page);

        const int navIndex = m_navButtons.size() - 1;
        connect(button, &QAbstractButton::clicked, this, [this, navIndex]() { select(navIndex); });

        if (checked) select(navIndex);
    }

    void BackstageView::addActionItem(const QString& glyph, const QString& text,
                                      const std::function<void()>& action)
    {
        auto* button = new BackstageNavButton(glyph, text, BackstageNavButton::Role::Action, this);
        m_leftLayout->addWidget(button);
        m_navButtons.append(button);
        m_navToPage.append(-1);
        connect(button, &QAbstractButton::clicked, this, action);
    }

    void BackstageView::addSeparatorItem()
    {
        m_leftLayout->addWidget(BackstageNavButton::makeSeparator(this));
    }

    void BackstageView::select(int navIndex)
    {
        if (navIndex < 0 || navIndex >= m_navToPage.size()) return;
        const int pageIndex = m_navToPage.at(navIndex);
        if (pageIndex < 0) return;   // 动作项不切页

        for (int i = 0; i < m_navButtons.size(); ++i) {
            auto* button = m_navButtons.at(i);
            if (button->role() == BackstageNavButton::Role::Page) {
                button->setChecked(i == navIndex);
            }
        }

        if (auto* current = qobject_cast<BackstagePageBase*>(m_stack->currentWidget())) {
            current->onHidden();
        }
        m_stack->setCurrentIndex(pageIndex);
        if (auto* next = qobject_cast<BackstagePageBase*>(m_stack->widget(pageIndex))) {
            next->onShown();
        }
        m_current = navIndex;
    }

    void BackstageView::moveFocus(int delta)
    {
        if (m_navButtons.isEmpty()) return;
        int index = m_navButtons.indexOf(qobject_cast<BackstageNavButton*>(focusWidget()));
        if (index < 0) index = m_current;
        for (int step = 0; step < m_navButtons.size(); ++step) {
            index += delta;
            if (index < 0) index = m_navButtons.size() - 1;
            if (index >= m_navButtons.size()) index = 0;
            auto* button = m_navButtons.at(index);
            if (button->role() == BackstageNavButton::Role::Separator) continue;
            button->setFocus(Qt::OtherFocusReason);
            return;
        }
    }

    void BackstageView::toggle()
    {
        isVisible() ? closeBackstage() : openBackstage();
    }

    void BackstageView::openBackstage()
    {
        if (parentWidget()) setGeometry(parentWidget()->rect());
        if (auto* page = qobject_cast<BackstagePageBase*>(m_stack->currentWidget())) {
            page->onShown();
        }
        show();
        raise();
        if (auto* button = m_navButtons.value(m_current)) button->setFocus(Qt::OtherFocusReason);
        setFocus();
    }

    void BackstageView::closeBackstage()
    {
        if (!isVisible()) return;
        hide();
        if (parentWidget()) parentWidget()->setFocus();
    }

    bool BackstageView::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            if (parentWidget()) setGeometry(parentWidget()->rect());
        }
        return QWidget::eventFilter(watched, event);
    }

    void BackstageView::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        setFocus();
    }

    void BackstageView::keyPressEvent(QKeyEvent* event)
    {
        switch (event->key()) {
        case Qt::Key_Escape:
            closeBackstage();
            return;
        case Qt::Key_Up:
            moveFocus(-1);
            return;
        case Qt::Key_Down:
            moveFocus(1);
            return;
        default:
            break;
        }
        QWidget::keyPressEvent(event);
    }

} // namespace doclife::ui
