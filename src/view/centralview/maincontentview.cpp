#include "maincontentview.h"
#include "centralview/centralviewitem/templatesheetview.h"

#include <QVBoxLayout>
#include <components/navigation/StackContentHost.h>

#include <uisignalmanager.h>

namespace doclife::ui {

    MainContentView::MainContentView(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_host = new fluent::navigation::StackContentHost(this);

        m_blankPage = new QWidget(m_host);
        m_sheetView = new TemplateSheetView(m_host);
        m_sheetView->setContentsMargins(0, 0, 9, 9);

        m_host->insertPage(0, m_blankPage);
        m_host->insertPage(1, m_sheetView);

        m_host->setCurrentIndex(0, 0, false);

        layout->addWidget(m_host, 1);

        connect(UISignalManager::instance(), &UISignalManager::templateSheetActivated,
            this, &MainContentView::onTemplateSheetActivated);

        connect(UISignalManager::instance(), &UISignalManager::projectSheetActivated,
            this, &MainContentView::onProjectSheetActivated);

        connect(UISignalManager::instance(), &UISignalManager::projectBlankPageActivated, this, [this]() {
            clearView();
            });
    }

    // ★ 实现清空逻辑
    void MainContentView::clearView()
    {
        m_sheetView->clearView();
        m_host->setCurrentIndex(0, 0, false); // 0 是空白页
    }

    void MainContentView::onTemplateSheetActivated(const QString& dbPath,
        int sheetId,
        const QString& title)
    {
        m_sheetView->loadSheet(dbPath, sheetId, title);
        m_host->setCurrentIndex(1, 0, true);
    }

    void MainContentView::onProjectSheetActivated(const QString& nodeId, const QString& dbPath, int sheetId, const QString& title)
    {
        m_sheetView->loadProjectSheet(nodeId, dbPath, sheetId, title);
        m_host->setCurrentIndex(1, 0, true);
    }

} // namespace doclife::ui