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

        // 0=空白页 1=模板表格视图
        m_blankPage = new QWidget(m_host);
        m_sheetView = new TemplateSheetView(m_host);
        m_sheetView->setContentsMargins(0, 0, 9, 9);

        m_host->insertPage(0, m_blankPage);
        m_host->insertPage(1, m_sheetView);

        m_host->setCurrentIndex(0, 0, false);

        layout->addWidget(m_host, 1);

        // 只保留表格激活的监听
        connect(UISignalManager::instance(), &UISignalManager::templateSheetActivated,
            this, &MainContentView::onTemplateSheetActivated);
    }

    void MainContentView::onTemplateSheetActivated(const QString& dbPath,
        int sheetId,
        const QString& title)
    {
        // 将表格数据传给渲染组件
        m_sheetView->loadSheet(dbPath, sheetId, title);
        // 切换到表格页
        m_host->setCurrentIndex(1, 0, true);
    }

} // namespace doclife::ui