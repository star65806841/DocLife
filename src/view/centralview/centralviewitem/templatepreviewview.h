#pragma once
#include <QWidget>
#include <QStringList>
#include <memory>
#include <atomic>
#include <QMutex>
#include <QFutureWatcher>
#include <QBuffer>
#include <QByteArray>
#include <QSet>

class QTableView;
class QLabel;

namespace QXlsx { class Document; class Worksheet; }
namespace doclife::ui::detail { class ExcelSheetModel; }

namespace doclife::ui {

    struct ExcelLoadResult {
        int requestId = 0;
        QString xlsxPath;
        QString sheetName;
        std::shared_ptr<QByteArray> fileData;
        std::shared_ptr<QBuffer> fileBuffer;
        std::shared_ptr<QXlsx::Document> doc;
        QStringList sheetNames;
        QString error;
    };

    class TemplatePreviewView : public QWidget {
        Q_OBJECT
    public:
        explicit TemplatePreviewView(QWidget* parent = nullptr);
        ~TemplatePreviewView() override;

    public slots:
        void showFile(const QString& xlsxPath);
        void showSheet(const QString& xlsxPath, const QString& sheetName);
        void clearCurrent();

    private:
        void requestLoad(const QString& xlsxPath, const QString& sheetName);
        void applySheetToView(QString sheetName);

        void applyRowHeights(QXlsx::Worksheet* ws);
        void applyColumnWidths(QXlsx::Worksheet* ws);
        void applyMergedCells(QXlsx::Worksheet* ws);
        void applyPaddingSpans();

        QTableView* m_view = nullptr;
        QLabel* m_loadingLabel = nullptr;

        std::shared_ptr<QByteArray> m_fileData;
        std::shared_ptr<QBuffer> m_fileBuffer;

        std::shared_ptr<QXlsx::Document> m_doc;
        std::unique_ptr<detail::ExcelSheetModel> m_model;
        QXlsx::Worksheet* m_currentWorksheet = nullptr;

        QString m_currentXlsxPath;
        QString m_currentSheetName;
        QStringList m_currentSheetNames;

        // 缓存池与任务池
        QHash<QString, std::shared_ptr<QXlsx::Document>> m_docCache;
        QSet<QString> m_pendingParses;
        std::atomic<int> m_requestId{ 0 };
    };
} // namespace doclife::ui