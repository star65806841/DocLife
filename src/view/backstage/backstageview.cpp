#include "backstageview.h"

#include "backstagepages.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

#include <components/navigation/NavigationView.h>
#include <components/navigation/StackContentHost.h>
#include <design/Typography.h>
#include <popWindow/infoItem/projectitem.h>   // MainSection 导航项列表

#include <business/projectcontext.h>
#include <uisignalmanager.h>

namespace doclife::ui {

    BackstageView::BackstageView(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("BackstagePanel"));
        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet(QStringLiteral("#BackstagePanel { background: #FFFFFF; }"));
        buildUi();
    }

    void BackstageView::buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(20, 14, 20, 18);
        root->setSpacing(12);

        // ---------------- 顶部：返回 + 标题 ----------------
        auto* topRow = new QHBoxLayout;
        topRow->setSpacing(12);

        auto* backButton = new QToolButton(this);
        backButton->setText(QStringLiteral("← 文件"));
        backButton->setCursor(Qt::PointingHandCursor);
        backButton->setAutoRaise(true);
        backButton->setStyleSheet(QStringLiteral(
            "QToolButton { border: none; padding: 6px 10px; font-size: 14px;"
            " color: #202020; border-radius: 4px; background: transparent; }"
            "QToolButton:hover { background: rgba(0,0,0,0.06); }"));
        connect(backButton, &QToolButton::clicked, this, &BackstageView::requestClose);
        topRow->addWidget(backButton);

        auto* title = new QLabel(QStringLiteral("文件"), this);
        title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600; color: #202020;"));
        topRow->addWidget(title);
        topRow->addStretch(1);
        root->addLayout(topRow);

        // ---------------- 导航（与工程信息设置页同一套） ----------------
        auto* navView = new fluent::navigation::NavigationView(this);
        navView->setAnimationEnabled(true);
        navView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Left);
        navView->setExpandedPaneWidth(180);
        navView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        m_host = navView->contentHost();

        auto* newPage = new BackstageNewPage(this);
        auto* openPage = new BackstageOpenPage(this);
        auto* settingsPage = new BackstageSettingsPage(this);

        m_pages = { newPage, openPage, settingsPage };
        for (int i = 0; i < m_pages.size(); ++i) {
            m_host->insertPage(i, m_pages.at(i), fluent::WidgetOwnership::Reparented);
        }
        m_host->setTransitionEffect(fluent::navigation::StackContentHost::TransitionEffect::SlideFromBottom);

        QVector<MainSection::Entry> entries =
        {
            { Typography::Icons::glyph(QStringLiteral("ic_fluent_add_24_regular")),
              QStringLiteral("新建"), true },
            { Typography::Icons::glyph(QStringLiteral("ic_fluent_history_24_regular")),
              QStringLiteral("打开"), false },
            { Typography::Icons::glyph(QStringLiteral("ic_fluent_settings_24_regular")),
              QStringLiteral("设置"), false },
        };

        auto* mainSection = new MainSection(entries, navView);
        navView->setMainChromeWidget(mainSection, fluent::WidgetOwnership::Borrowed);

        mainSection->onActivated = [this](int pageIndex) { showPage(pageIndex); };

        m_host->setCurrentIndex(0, 0, false);
        showPage(0);

        root->addWidget(navView, 1);

        // ---------------- 页面动作 ----------------
        connect(newPage, &BackstageNewPage::requestBlank, this, [this]() {
            emit UISignalManager::instance()->newProject();
            emit requestClose();
            });
        connect(newPage, &BackstageNewPage::requestOpen, this, [this]() {
            emit UISignalManager::instance()->openProject();
            emit requestClose();
            });

        // 打开历史里的文件：先关抽屉再打开工程，避免文件对话框被压在抽屉下面
        connect(openPage, &BackstageOpenPage::requestBrowse, this, [this]() {
            emit UISignalManager::instance()->openProject();
            emit requestClose();
            });
        connect(openPage, &BackstageOpenPage::requestOpenPath, this, [this](const QString& path) {
            emit requestClose();
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
    }

    void BackstageView::showPage(int index)
    {
        if (!m_host || index < 0 || index >= m_pages.size()) return;

        const int direction = index > m_host->currentIndex() ? 1 : -1;
        m_host->setCurrentIndex(index, direction, true);

        if (auto* page = m_pages.at(index)) page->onShown();
    }

} // namespace doclife::ui
