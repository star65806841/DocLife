#include "templatepreviewview.h"

#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QBuffer>
#include <QFile>
#include <QDebug>
#include <QAbstractTableModel>

#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"

namespace doclife::ui {

class ExcelSheetModel : public QAbstractTableModel
{
public:
    explicit ExcelSheetModel(QXlsx::Worksheet *sheet, QObject *parent = nullptr)
        : QAbstractTableModel(parent), m_sheet(sheet)
    {
        const QXlsx::CellRange range = sheet->dimension();
        m_firstRow = range.firstRow();
        m_firstCol = range.firstColumn();
        m_rows = qMax(0, qMin(range.rowCount(),    kMaxRows));
        m_cols = qMax(0, qMin(range.columnCount(), kMaxCols));
    }

    int rowCount(const QModelIndex & = QModelIndex()) const override { return m_rows; }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return m_cols; }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || !m_sheet) return {};
        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return m_sheet->read(index.row() + m_firstRow,
                                 index.column() + m_firstCol);
        return {};
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role != Qt::DisplayRole) return {};
        if (orientation == Qt::Horizontal)
            return columnName(section + m_firstCol - 1);
        return QString::number(section + m_firstRow);
    }

private:
    static QString columnName(int col)
    {
        QString name;
        while (col >= 0) {
            name.prepend(QChar('A' + col % 26));
            col = col / 26 - 1;
        }
        return name;
    }

    static constexpr int kMaxRows = 5000;
    static constexpr int kMaxCols = 200;

    QXlsx::Worksheet *m_sheet = nullptr;
    int m_firstRow = 1;
    int m_firstCol = 1;
    int m_rows = 0;
    int m_cols = 0;
};

} // namespace doclife::ui::detail

namespace doclife::ui {

TemplatePreviewView::TemplatePreviewView(QWidget *parent)
    : QWidget(parent)
{
    m_view = new QTableView(this);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setAlternatingRowColors(true);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->verticalHeader()->setDefaultSectionSize(22);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view);
}

TemplatePreviewView::~TemplatePreviewView()
{
    clearCurrent();
}

void TemplatePreviewView::clearCurrent()
{
    m_view->setModel(nullptr);   // 1) 断开视图
    m_model.reset();             // 2) 删 model
    m_doc.reset();               // 3) 销毁工作簿
    delete m_buffer;             // 4) 释放 QRC 字节流
    m_buffer = nullptr;
    m_currentXlsxPath.clear();
}

void TemplatePreviewView::showSheet(const QString &xlsxPath, const QString &sheetName)
{
    // 切换文件才重建 Document；同一文件内只换 model
    if (!m_doc || xlsxPath != m_currentXlsxPath) {
        clearCurrent();

        QFile file(xlsxPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "打开资源失败:" << xlsxPath;
            return;
        }
        const QByteArray data = file.readAll();
        file.close();

        m_buffer = new QBuffer(this);
        m_buffer->setData(data);
        m_buffer->open(QIODevice::ReadOnly);

        m_doc = std::make_unique<QXlsx::Document>(m_buffer);
        if (m_doc->sheetNames().isEmpty()) {
            qWarning() << "解析失败或无 sheet:" << xlsxPath;
            clearCurrent();
            return;
        }
        m_currentXlsxPath = xlsxPath;
    }

    m_view->setModel(nullptr);
    m_model.reset();

    if (!m_doc->selectSheet(sheetName)) return;
    QXlsx::Worksheet *ws = m_doc->currentWorksheet();
    if (!ws) return;

    m_model = std::make_unique<ExcelSheetModel>(ws, this);
    m_view->setModel(m_model.get());
    m_view->resizeColumnsToContents();
}

} // namespace doclife::ui