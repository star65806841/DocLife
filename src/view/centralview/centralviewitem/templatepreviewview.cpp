#include "templatepreviewview.h"
#include "excelsheetmodel.h"
#include "excelcelldelegate.h"
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QtConcurrent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QMutexLocker>

#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"
#include <uisignalmanager.h>

namespace doclife::ui {
    // 全局锁：让 QXlsx 在底层安全排队
    static QMutex g_xlsxParseMutex;

    TemplatePreviewView::TemplatePreviewView(QWidget* parent) : QWidget(parent) {
        m_view = new QTableView(this);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_view->setSelectionMode(QAbstractItemView::SingleSelection);
        m_view->setAlternatingRowColors(false);
        m_view->setWordWrap(false);
        m_view->setShowGrid(false);
        m_view->horizontalHeader()->setStretchLastSection(false);
        m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_view->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_view->horizontalHeader()->setMinimumSectionSize(1);
        m_view->verticalHeader()->setMinimumSectionSize(1);
        m_view->setItemDelegate(new ExcelCellDelegate(m_view));

        m_loadingLabel = new QLabel("正在加载...", this);
        m_loadingLabel->setAlignment(Qt::AlignCenter);
        m_loadingLabel->setStyleSheet("font-size: 16px; color: #888;");
        m_loadingLabel->hide();

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->addWidget(m_view);
        layout->addWidget(m_loadingLabel);

        connect(UISignalManager::instance(), &UISignalManager::excelFileActivated, this, &TemplatePreviewView::showFile, Qt::UniqueConnection);
        connect(UISignalManager::instance(), &UISignalManager::excelSheetActivated, this, &TemplatePreviewView::showSheet, Qt::UniqueConnection);
    }

    TemplatePreviewView::~TemplatePreviewView() {
        clearCurrent();
    }

    void TemplatePreviewView::clearCurrent() {
        m_view->setModel(nullptr);
        m_view->clearSpans();
        m_model.reset();
        m_doc.reset();
        m_currentWorksheet = nullptr;
    }

    void TemplatePreviewView::showFile(const QString& xlsxPath) {
        // 默认显示第一个 sheet
        requestLoad(xlsxPath, "");
    }

    void TemplatePreviewView::showSheet(const QString& xlsxPath, const QString& sheetName) {
        requestLoad(xlsxPath, sheetName);
    }

    void TemplatePreviewView::requestLoad(const QString& xlsxPath, const QString& sheetName) {
        int currentId = ++m_requestId;
        m_currentXlsxPath = xlsxPath;
        m_currentSheetName = sheetName;

        // 1. 检查缓存：如果已经在内存里，直接秒切
        if (m_docCache.contains(xlsxPath)) {
            qDebug() << "[TemplateView] Cache hit:" << xlsxPath;
            m_doc = m_docCache[xlsxPath];
            m_currentSheetNames = m_doc->sheetNames();
            m_loadingLabel->hide();
            m_view->show();

            QString target = sheetName;
            if (target.isEmpty() || !m_currentSheetNames.contains(target)) {
                target = m_currentSheetNames.first();
            }
            applySheetToView(target);
            return;
        }

        // 2. 检查是否已经在后台排队/解析中
        if (m_pendingParses.contains(xlsxPath)) {
            qDebug() << "[TemplateView] Already parsing in background:" << xlsxPath;
            m_loadingLabel->setText(QString("文件已在后台解析队列中，请稍候...\n%1").arg(QFileInfo(xlsxPath).fileName()));
            m_loadingLabel->show();
            m_view->hide();
            return; // 直接返回，等后台解析完的 finished 信号来处理
        }

        // 3. 发起全新的后台解析任务
        m_pendingParses.insert(xlsxPath);
        m_loadingLabel->setText(QString("正在后台解析文件...\n%1").arg(QFileInfo(xlsxPath).fileName()));
        m_loadingLabel->show();
        m_view->hide();
        clearCurrent();

        qDebug() << "[TemplateView] Dispatch background task:" << xlsxPath;

        // 独立 Watcher 防止相互覆盖
        auto* watcher = new QFutureWatcher<ExcelLoadResult>(this);
        connect(watcher, &QFutureWatcher<ExcelLoadResult>::finished, this, [this, watcher]() {
            auto res = watcher->result();
            watcher->deleteLater();

            m_pendingParses.remove(res.xlsxPath);

            if (!res.error.isEmpty() || !res.doc || res.sheetNames.isEmpty()) {
                qWarning() << "[TemplateView] Parse failed:" << res.error;
                // 如果失败的是当前界面等的文件，才显示错误
                if (res.requestId == m_requestId.load()) {
                    m_loadingLabel->setText(res.error.isEmpty() ? "解析失败或空文件" : res.error);
                }
                return;
            }

            qDebug() << "[TemplateView] Background parse finished:" << res.xlsxPath;

            // 无论是不是当前要展示的，只要解析成功，全部塞进缓存，并更新左侧树！
            if (m_docCache.size() > 10) m_docCache.clear(); // 控制缓存上限
            m_docCache.insert(res.xlsxPath, res.doc);
            emit UISignalManager::instance()->sheetNamesLoaded(res.xlsxPath, res.sheetNames);

            // 如果这个任务不是用户最后点击的（被后续点击覆盖了），就不需要渲染
            if (res.requestId != m_requestId.load()) {
                qDebug() << "[TemplateView] Discard UI update for old task:" << res.xlsxPath;
                return;
            }

            // 渲染到屏幕
            m_doc = res.doc;
            m_currentXlsxPath = res.xlsxPath;
            m_currentSheetNames = res.sheetNames;
            m_loadingLabel->hide();
            m_view->show();

            QString target = res.sheetName;
            if (target.isEmpty() || !res.sheetNames.contains(target)) {
                target = res.sheetNames.first();
            }
            applySheetToView(target);
            });

        watcher->setFuture(QtConcurrent::run([xlsxPath, sheetName, currentId]() {
            ExcelLoadResult res;
            res.requestId = currentId;
            res.xlsxPath = xlsxPath;
            res.sheetName = sheetName;

            try {
                // 加锁：绝对保证底层排队，不并发解析
                QMutexLocker locker(&g_xlsxParseMutex);

                qDebug() << "[Thread] Lock acquired, start parsing:" << xlsxPath;

                QFile file(xlsxPath);
                if (file.open(QIODevice::ReadOnly)) {
                    res.fileData = std::make_shared<QByteArray>(file.readAll());
                    file.close();

                    res.fileBuffer = std::make_shared<QBuffer>();
                    res.fileBuffer->setData(*res.fileData);
                    res.fileBuffer->open(QIODevice::ReadOnly);

                    res.doc = std::make_shared<QXlsx::Document>(res.fileBuffer.get());
                    if (res.doc) {
                        res.sheetNames = res.doc->sheetNames();
                    }
                }
                else {
                    res.error = "无法打开文件";
                }
            }
            catch (const std::exception& e) {
                res.error = QString("异常: %1").arg(e.what());
            }
            catch (...) {
                res.error = "未知解析异常";
            }
            return res;
            }));
    }

    void TemplatePreviewView::applySheetToView(QString sheetName) {
        if (!m_doc) return;

        if (m_view->model()) {
            for (int r = 0; r < m_view->model()->rowCount(); ++r) m_view->showRow(r);
            for (int c = 0; c < m_view->model()->columnCount(); ++c) m_view->showColumn(c);
        }
        m_view->setModel(nullptr);
        m_view->clearSpans();
        m_model.reset();
        m_currentWorksheet = nullptr;

        try {
            if (!m_doc->selectSheet(sheetName)) return;
            QXlsx::Worksheet* ws = m_doc->currentWorksheet();
            if (!ws) return;

            m_currentWorksheet = ws;
            m_model = std::make_unique<detail::ExcelSheetModel>(ws, this);

            m_view->setUpdatesEnabled(false);
            m_view->setModel(m_model.get());

            applyMergedCells(ws);
            applyPaddingSpans();
            applyRowHeights(ws);
            applyColumnWidths(ws);

            m_view->setUpdatesEnabled(true);
            m_view->viewport()->update();
            qDebug() << "[TemplateView] View updated:" << sheetName;
        }
        catch (...) {
            qWarning() << "[TemplateView] Crash caught applying sheet:" << sheetName;
        }
    }

    // ---------------- 排版辅助函数 ----------------

    void TemplatePreviewView::applyMergedCells(QXlsx::Worksheet* ws) {
        m_view->clearSpans();
        if (!ws || !m_model) return;
        const int maxRow = m_model->realRowCount();
        const int maxCol = m_model->realColumnCount();
        for (const auto& range : ws->mergedCells()) {
            const int startRow = range.firstRow() - m_model->firstRow();
            const int startCol = range.firstColumn() - m_model->firstCol();
            if (startRow < 0 || startCol < 0 || startRow >= maxRow || startCol >= maxCol) continue;
            const int rows = qMin((int)(range.lastRow() - range.firstRow() + 1), maxRow - startRow);
            const int cols = qMin((int)(range.lastColumn() - range.firstColumn() + 1), maxCol - startCol);
            if (rows > 1 || cols > 1) m_view->setSpan(startRow, startCol, rows, cols);
        }
    }

    void TemplatePreviewView::applyPaddingSpans() {
        if (!m_model) return;
        const int realRows = m_model->realRowCount();
        const int realCols = m_model->realColumnCount();
        const int totalRows = m_model->rowCount();
        const int totalCols = m_model->columnCount();
        if (totalRows > realRows) m_view->setSpan(realRows, 0, totalRows - realRows, totalCols);
        if (totalCols > realCols) m_view->setSpan(0, realCols, realRows, totalCols - realCols);
    }

    void TemplatePreviewView::applyRowHeights(QXlsx::Worksheet* ws) {
        if (!ws || !m_model) return;
        const int firstRow = m_model->firstRow();
        const int realRows = m_model->realRowCount();
        m_view->verticalHeader()->setDefaultSectionSize(22);
        for (int r = 0; r < realRows; ++r) {
            const double h = ws->rowHeight(r + firstRow);
            if (h > 0) m_view->setRowHeight(r, qMax(18, static_cast<int>(h * 1.333)));
        }
        for (int r = realRows; r < m_model->rowCount(); ++r) m_view->setRowHeight(r, 6);
    }

    void TemplatePreviewView::applyColumnWidths(QXlsx::Worksheet* ws) {
        if (!ws || !m_model) return;
        const int firstCol = m_model->firstCol();
        const int realCols = m_model->realColumnCount();

        double dpiScale = 1.0;
        if (m_view->screen()) dpiScale = m_view->screen()->devicePixelRatio();
        double mdw = m_view->fontMetrics().horizontalAdvance(QLatin1Char('0'));
        if (mdw <= 0) mdw = 7.0;

        for (int c = 0; c < realCols; ++c) {
            const double w = ws->columnWidth(c + firstCol);
            double basePx = (w > 0) ? (w * mdw + 5.0) : 64.0;
            m_view->setColumnWidth(c, qMax(1, static_cast<int>(basePx / dpiScale)));
        }
        for (int c = realCols; c < m_model->columnCount(); ++c) m_view->setColumnWidth(c, 6);
    }

} // namespace doclife::ui