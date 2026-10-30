#include "maincontentview.h"
#include "centralview/centralviewitem/templatepreviewview.h"
#include "centralview/centralviewitem/templatepdfpreviewview.h"
#include "centralview/centralviewitem/templatesheetview.h"    // ★ 新增

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

        // 0=Excel预览 1=空白页 2=PDF预览 3=模板表格视图 ★
        m_preview = new TemplatePreviewView(m_host);
        m_blankPage = new QWidget(m_host);
        m_pdfPreview = new TemplatePdfPreviewView(m_host);
        m_sheetView = new TemplateSheetView(m_host);

        m_host->insertPage(0, m_preview);
        m_host->insertPage(1, m_blankPage);
        m_host->insertPage(2, m_pdfPreview);
        m_host->insertPage(3, m_sheetView);

        m_host->setCurrentIndex(1, 0, false);

        layout->addWidget(m_host, 1);

        connect(UISignalManager::instance(), &UISignalManager::excelSheetActivated,
            this, &MainContentView::onSheetActivated);
        connect(UISignalManager::instance(), &UISignalManager::pdfPreviewRequested,
            this, &MainContentView::onPdfActivated);
        // ★ 新增
        connect(UISignalManager::instance(), &UISignalManager::templateSheetActivated,
            this, &MainContentView::onTemplateSheetActivated);
    }

    void MainContentView::onSheetActivated(const QString& xlsxPath, const QString& sheetName)
    {
        m_preview->showSheet(xlsxPath, sheetName);
        m_host->setCurrentIndex(0, 0, true);
    }

    void MainContentView::onPdfActivated(const QString& pdfPath, int page)
    {
        m_pdfPreview->loadPdf(pdfPath, page);
        m_host->setCurrentIndex(2, 0, true);
    }

    void MainContentView::onTemplateSheetActivated(const QString& dbPath,
        int sheetId,
        const QString& title)
    {
        m_sheetView->loadSheet(dbPath, sheetId, title);
        m_host->setCurrentIndex(3, 0, true);
    }

} // namespace doclife::ui