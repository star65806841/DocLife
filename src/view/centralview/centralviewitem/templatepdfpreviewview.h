#pragma once

#include <QWidget>
#include <memory>

class QPdfDocument;
class QPdfView;
class QLabel;
class QSpinBox;
class QToolButton;
class QTemporaryFile;

namespace doclife::ui {

    class TemplatePdfPreviewView : public QWidget
    {
        Q_OBJECT
    public:
        explicit TemplatePdfPreviewView(QWidget* parent = nullptr);
        ~TemplatePdfPreviewView() override;

        void loadPdf(const QString& pdfPath, int page);

    private:
        void goToPage(int page1Based);                  // spinbox(1-based) → navigator(0-based)
        void syncPageFromNavigator(int page0Based);     // navigator(0-based) → spinbox(1-based)
        void refreshFromDoc();                          // 根据 document 状态刷新 UI
        void resetToErrorState(const QString& msg);

        QPdfDocument* m_doc = nullptr;
        QPdfView* m_view = nullptr;
        QLabel* m_pathLabel = nullptr;
        QLabel* m_pageCount = nullptr;
        QSpinBox* m_pageSpin = nullptr;
        QToolButton* m_prevBtn = nullptr;
        QToolButton* m_nextBtn = nullptr;

        // qrc 路径兜底：QTemporaryFile 必须长期存活
        std::unique_ptr<QTemporaryFile> m_tmpPdf;

        // spinbox ↔ navigator 回环保护
        bool m_syncing = false;
    };

} // namespace doclife::ui