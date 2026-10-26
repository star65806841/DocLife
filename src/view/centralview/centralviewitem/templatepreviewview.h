#pragma once

#include <QWidget>
#include <QString>
#include <memory>

class QTableView;
class QBuffer;

namespace QXlsx { class Document; }

namespace doclife::ui {
class ExcelSheetModel;
// 只读预览：一个 QTableView，负责 QXlsx 的加载与切换
class TemplatePreviewView : public QWidget
{
    Q_OBJECT
public:
    explicit TemplatePreviewView(QWidget *parent = nullptr);
    ~TemplatePreviewView() override;

public slots:
    void showSheet(const QString &xlsxPath, const QString &sheetName);
    void clearCurrent();

private:
    QTableView *m_view = nullptr;
    QBuffer    *m_buffer = nullptr;
    std::unique_ptr<QXlsx::Document>         m_doc;
    std::unique_ptr<ExcelSheetModel> m_model;
    QString m_currentXlsxPath;
};

} // namespace doclife::ui