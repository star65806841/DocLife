#include "templatepreviewview.h"
#include "excelsheetmodel.h"
#include "excelcelldelegate.h"

#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QBuffer>
#include <QFile>
#include <QDebug>
#include <QScreen>
#include <QFontMetrics>
#include <QtGlobal>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <TargetConditionals.h>
#endif

#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"

namespace doclife::ui {

    TemplatePreviewView::TemplatePreviewView(QWidget* parent)
        : QWidget(parent)
    {
        m_view = new QTableView(this);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_view->setSelectionMode(QAbstractItemView::SingleSelection);
        m_view->setAlternatingRowColors(false);
        m_view->setWordWrap(false);

        m_view->setShowGrid(false);
        //m_view->setGridStyle(Qt::SolidLine);
        //m_view->setStyleSheet("QTableView { gridline-color: #9A000000; }");

        m_view->horizontalHeader()->setStretchLastSection(false);
        m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_view->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);

        m_view->horizontalHeader()->setMinimumSectionSize(1);
        m_view->verticalHeader()->setMinimumSectionSize(1);

        m_view->setItemDelegate(new ExcelCellDelegate(m_view));

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(0);
        layout->addWidget(m_view);
    }

    TemplatePreviewView::~TemplatePreviewView()
    {
        clearCurrent();
    }

    void TemplatePreviewView::clearCurrent()
    {
        if (m_view->model()) {
            for (int r = 0; r < m_view->model()->rowCount(); ++r)
                m_view->showRow(r);
            for (int c = 0; c < m_view->model()->columnCount(); ++c)
                m_view->showColumn(c);
        }

        m_view->setModel(nullptr);
        m_view->clearSpans();
        m_model.reset();
        m_doc.reset();
        delete m_buffer;
        m_buffer = nullptr;
        m_currentWorksheet = nullptr;
        m_currentXlsxPath.clear();
    }

    void TemplatePreviewView::showSheet(const QString& xlsxPath, const QString& sheetName)
    {
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

        if (m_view->model()) {
            for (int r = 0; r < m_view->model()->rowCount(); ++r)
                m_view->showRow(r);
            for (int c = 0; c < m_view->model()->columnCount(); ++c)
                m_view->showColumn(c);
        }
        m_view->setModel(nullptr);
        m_view->clearSpans();
        m_model.reset();
        m_currentWorksheet = nullptr;

        if (!m_doc->selectSheet(sheetName)) return;
        QXlsx::Worksheet* ws = m_doc->currentWorksheet();
        if (!ws) return;

        m_currentWorksheet = ws;
        m_model = std::make_unique<detail::ExcelSheetModel>(ws, this);
        m_view->setModel(m_model.get());

        applyMergedCells(ws);       // Excel 自身的合并
        applyPaddingSpans();        // padding 行列合并成整块
        applyRowHeights(ws);
        applyHiddenRowsAndColumns(ws);
        applyColumnWidths(ws);
    }

    void TemplatePreviewView::applyMergedCells(QXlsx::Worksheet* ws)
    {
        m_view->clearSpans();
        if (!ws || !m_model) return;

        const int firstRow = m_model->firstRow();
        const int firstCol = m_model->firstCol();
        const int maxRow = m_model->realRowCount();
        const int maxCol = m_model->realColumnCount();

        const auto merged = ws->mergedCells();
        for (const QXlsx::CellRange& range : merged) {
            const int startRow = range.firstRow() - firstRow;
            const int startCol = range.firstColumn() - firstCol;
            const int spanRows = range.lastRow() - range.firstRow() + 1;
            const int spanCols = range.lastColumn() - range.firstColumn() + 1;

            if (startRow < 0 || startCol < 0) continue;
            if (startRow >= maxRow || startCol >= maxCol) continue;

            const int rows = qMin(spanRows, maxRow - startRow);
            const int cols = qMin(spanCols, maxCol - startCol);
            if (rows > 1 || cols > 1)
                m_view->setSpan(startRow, startCol, rows, cols);
        }
    }

    void TemplatePreviewView::applyPaddingSpans()
    {
        if (!m_model) return;
        const int realRows = m_model->realRowCount();
        const int realCols = m_model->realColumnCount();
        if (realRows <= 0 || realCols <= 0) return;

        const int totalRows = m_model->rowCount();
        const int totalCols = m_model->columnCount();
        const int padRows = totalRows - realRows;
        const int padCols = totalCols - realCols;

        // 底部整条 padding 行合并成一块，覆盖所有列
        if (padRows > 0)
            m_view->setSpan(realRows, 0, padRows, totalCols);

        // 右侧 padding 列（真实行范围内）合并成一块
        if (padCols > 0)
            m_view->setSpan(0, realCols, realRows, padCols);
    }

    void TemplatePreviewView::applyRowHeights(QXlsx::Worksheet* ws)
    {
        if (!ws || !m_model) return;

        const int firstRow = m_model->firstRow();
        const int realRows = m_model->realRowCount();

        for (int r = 0; r < realRows; ++r) {
            const double h = ws->rowHeight(r + firstRow);
            m_view->setRowHeight(r, h > 0 ? qMax(18, static_cast<int>(h * 1.333)) : 22);
        }
        for (int r = realRows; r < m_model->rowCount(); ++r)
            m_view->setRowHeight(r, 6);
    }

    void TemplatePreviewView::applyColumnWidths(QXlsx::Worksheet* ws)
    {
        if (!ws || !m_model) return;

        const int firstCol = m_model->firstCol();
        const int realCols = m_model->realColumnCount();

        // ------------------- DPI 缩放计算 (宏隔离) -------------------
        double dpiScale = 1.0;

#if defined(Q_OS_WIN)
        // Windows: 优先使用系统原生的 GetDpiForWindow 获取精准缩放比
        // 注意：若不想引入 windows.h，可以直接用 m_view->screen()->logicalDotsPerInch() / 96.0
        HWND hwnd = reinterpret_cast<HWND>(m_view->winId());
        UINT dpi = GetDpiForWindow(hwnd);
        if (dpi > 0) {
            dpiScale = dpi / 96.0;
        } else if (m_view->screen()) {
            dpiScale = m_view->screen()->logicalDotsPerInch() / 96.0;
        }

#elif defined(Q_OS_MACOS)
        // macOS: 逻辑层已经处理了 Retina 缩放，这里保持 1.0，避免列被过度挤压
        // 如果强制除以 2.0，列宽会比原 Excel 窄一半
        dpiScale = 1.0;

#else
        // Linux / 其他平台: 使用 Qt 提供的 devicePixelRatio
        if (m_view->screen()) {
            dpiScale = m_view->screen()->devicePixelRatio();
        }
#endif

        // 兜底保护
        if (dpiScale <= 0.1) dpiScale = 1.0;
        // -----------------------------------------------------------

        // 动态获取当前字体下数字 '0' 的宽度（逻辑像素）
        double mdw = m_view->fontMetrics().horizontalAdvance(QLatin1Char('0'));
        if (mdw <= 0) mdw = 7.0;

        for (int c = 0; c < realCols; ++c) {
            const double w = ws->columnWidth(c + firstCol);

            // Excel 默认列宽公式：字符宽度 * 字符像素 + 5 像素的内边距
            double basePx = (w > 0) ? (w * mdw + 5.0) : 64.0;

            // 核心换算：抵消 DPI 膨胀
            int px = static_cast<int>(basePx / dpiScale);

            // 兜底：最少 1 像素
            px = qMax(1, px);

            m_view->setColumnWidth(c, px);
        }

        // padding 列保持固定小宽度
        for (int c = realCols; c < m_model->columnCount(); ++c) {
            m_view->setColumnWidth(c, 6);
        }
    }

    void TemplatePreviewView::applyHiddenRowsAndColumns(QXlsx::Worksheet* ws)
    {
        if (!ws || !m_model) return;

        const int firstRow = m_model->firstRow();
        const int firstCol = m_model->firstCol();
        const int realRows = m_model->realRowCount();
        const int realCols = m_model->realColumnCount();

        for (int r = 0; r < realRows; ++r) {
            if (ws->rowHeight(r + firstRow) == 0)
                m_view->hideRow(r);
        }
        for (int c = 0; c < realCols; ++c) {
            if (ws->columnWidth(c + firstCol) == 0)
                m_view->hideColumn(c);
        }
    }

} // namespace doclife::ui