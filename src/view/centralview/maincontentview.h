#pragma once

#include <QWidget>

namespace fluent::navigation { class StackContentHost; }

namespace doclife::ui {

    class TemplatePreviewView;
    class TemplatePdfPreviewView;
    class TemplateSheetView;

    class MainContentView : public QWidget
    {
        Q_OBJECT
    public:
        explicit MainContentView(QWidget* parent = nullptr);

    private slots:
        void onSheetActivated(const QString& xlsxPath, const QString& sheetName);
        void onPdfActivated(const QString& pdfPath,int page);
        void onTemplateSheetActivated(const QString& dbPath, int sheetId, const QString& title);
    private:
        fluent::navigation::StackContentHost* m_host = nullptr;
        TemplatePreviewView* m_preview = nullptr;
        QWidget* m_blankPage = nullptr;
        TemplatePdfPreviewView* m_pdfPreview = nullptr;
        TemplateSheetView* m_sheetView = nullptr;
    };

} // namespace doclife::ui