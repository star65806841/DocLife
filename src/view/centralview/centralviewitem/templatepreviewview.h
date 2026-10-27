#pragma once

#include <QWidget>
#include <QString>
#include <memory>

class QTableView;
class QBuffer;

namespace QXlsx {
    class Document;
    class Worksheet;
}

namespace doclife::ui::detail { class ExcelSheetModel; }

namespace doclife::ui {

    class TemplatePreviewView : public QWidget
    {
        Q_OBJECT
    public:
        explicit TemplatePreviewView(QWidget* parent = nullptr);
        ~TemplatePreviewView() override;

    public slots:
        void showSheet(const QString& xlsxPath, const QString& sheetName);
        void clearCurrent();

    private:
        void applyMergedCells(QXlsx::Worksheet* ws);
        void applyPaddingSpans();
        void applyHiddenRowsAndColumns(QXlsx::Worksheet* ws);
        void applyRowHeights(QXlsx::Worksheet* ws);
        void applyColumnWidths(QXlsx::Worksheet* ws);

        QTableView* m_view = nullptr;
        QBuffer* m_buffer = nullptr;
        std::unique_ptr<QXlsx::Document>         m_doc;
        std::unique_ptr<detail::ExcelSheetModel> m_model;
        QXlsx::Worksheet* m_currentWorksheet = nullptr;
        QString m_currentXlsxPath;
    };

} // namespace doclife::ui