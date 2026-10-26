#include "maincontentview.h"
#include "centralview/centralviewitem/templatepreviewview.h"

#include <QVBoxLayout>
#include <components/navigation/StackContentHost.h>

#include <uisignalmanager.h>

namespace doclife::ui {

MainContentView::MainContentView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_host = new fluent::navigation::StackContentHost(this);

    // index 0 = 预览页；index 1 = 空白页（未来可替换成欢迎页 / 项目概览）
    m_preview   = new TemplatePreviewView(m_host);
    m_blankPage = new QWidget(m_host);

    m_host->insertPage(0, m_preview);
    m_host->insertPage(1, m_blankPage);

    // 默认显示空白页；收到 sheet 激活信号后切到预览页
    m_host->setCurrentIndex(1, 0, false);

    layout->addWidget(m_host, 1);

    connect(UISignalManager::instance(), &UISignalManager::excelSheetActivated,
            this, &MainContentView::onSheetActivated);
}

void MainContentView::onSheetActivated(const QString &xlsxPath, const QString &sheetName)
{
    m_preview->showSheet(xlsxPath, sheetName);
    m_host->setCurrentIndex(0, 0, true);   // 切到 index 0（预览页）
}

} // namespace doclife::ui