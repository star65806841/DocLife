#include "templatesheetview.h"

#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QColor>
#include <QFont>
#include <QDebug>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QAction>
#include <QItemSelectionModel>
#include <utils.h>
#include <uisignalmanager.h>

#include <components/menus_toolbars/CommandBar.h>
#include <business/projectcontext.h>

namespace doclife::ui {

    class CellBorderDelegate : public QStyledItemDelegate
    {
    public:
        explicit CellBorderDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyledItemDelegate::paint(painter, option, index);

            const int top = index.data(Qt::UserRole + 10).toInt();
            const int bottom = index.data(Qt::UserRole + 11).toInt();
            const int left = index.data(Qt::UserRole + 12).toInt();
            const int right = index.data(Qt::UserRole + 13).toInt();

            if (top == 0 && bottom == 0 && left == 0 && right == 0)
                return;

            painter->save();
            QPen pen(QColor("#000000"));
            pen.setCosmetic(true);

            QPoint tl = option.rect.topLeft();
            QPoint tr(option.rect.right() + 1, option.rect.top());
            QPoint bl = option.rect.bottomLeft() + QPoint(0, 1);
            QPoint br = option.rect.bottomRight() + QPoint(1, 1);

            if (top > 0) { pen.setWidth(top > 1 ? 2 : 1); painter->setPen(pen); painter->drawLine(tl, tr); }
            if (bottom > 0) { pen.setWidth(bottom > 1 ? 2 : 1); painter->setPen(pen); painter->drawLine(bl, br); }
            if (left > 0) { pen.setWidth(left > 1 ? 2 : 1); painter->setPen(pen); painter->drawLine(tl, bl); }
            if (right > 0) { pen.setWidth(right > 1 ? 2 : 1); painter->setPen(pen); painter->drawLine(tr, br); }

            painter->restore();
        }
    };

    static inline int ptToPx(qreal pt) { return qRound(pt * 1.3333) + 2; }
    static inline int charsToPx(qreal chars) { return qRound(chars * 7.0) + 5; }

    TemplateSheetView::TemplateSheetView(QWidget* parent)
        : QWidget(parent)
    {
        m_topBar = new fluent::menus_toolbars::CommandBar(this);
        m_topBar->setLabelPosition(fluent::menus_toolbars::CommandBar::LabelPosition::Collapsed);
        m_topBar->setBackgroundVisible(false);

        m_titleAction = new QAction("加载中...", m_topBar);
        m_resetAction = new QAction("重置宽高", m_topBar);
        m_resetAction->setIcon(QIcon(":/icons/edit/resetSheet.svg"));

        m_topBar->addPrimaryAction(m_titleAction);
        m_topBar->addPrimaryAction(m_resetAction);

        m_table = new QTableWidget(this);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_table->setWordWrap(true);
        m_table->setFrameShape(QFrame::NoFrame);
        m_table->setShowGrid(false);

        m_table->horizontalHeader()->setMinimumSectionSize(1);
        m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_table->horizontalHeader()->setStretchLastSection(false);
        m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

        // ★ 1.开启点击行/列头选中，并统一居中
        m_table->horizontalHeader()->setSectionsClickable(true);
        m_table->verticalHeader()->setSectionsClickable(true);
        m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
        m_table->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

        // ★ 2.必须设置在 viewport 上才生效
        m_table->horizontalHeader()->viewport()->setCursor(doclife::core::Utils::createSvgCursor(":/icons/cursor/arrow-down.svg",16));
        m_table->verticalHeader()->viewport()->setCursor(doclife::core::Utils::createSvgCursor(":/icons/cursor/arrow-right.svg",16));
        m_table->viewport()->setCursor(doclife::core::Utils::createSvgCursor(":/icons/cursor/cross-society.svg",16));
        m_table->setItemDelegate(new CellBorderDelegate(m_table));

        m_table->setStyleSheet(R"(
            QTableWidget {
                background-color: #FFFFFF;
                border: none;
            }
            QTableWidget::item {
                padding: 0px;
                margin: 0px;
                border: none;
            }
            QTableWidget::item:selected {
                background-color: #E5F1FB;
                color: #000000;
            }
            QHeaderView::section {
                background-color: #F8F8F8;
                border: 1px solid #D0D0D0;
                font-weight: bold;
                padding: 4px;
            }
            QHeaderView::section:hover {
                background-color: #E5F1FB;
            }
        )");

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(m_topBar);
        layout->addWidget(m_table, 1);

        connect(m_table->horizontalHeader(), &QHeaderView::sectionResized, this, &TemplateSheetView::onSectionResized);
        connect(m_table->verticalHeader(), &QHeaderView::sectionResized, this, &TemplateSheetView::onSectionResized);
        connect(m_table, &QTableWidget::itemChanged, this, &TemplateSheetView::onItemChanged);
        connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this, &TemplateSheetView::onHorizontalHeaderClicked);
        connect(m_table->verticalHeader(), &QHeaderView::sectionClicked, this, &TemplateSheetView::onVerticalHeaderClicked);

        connect(m_table->horizontalHeader()->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TemplateSheetView::onHeaderSelectionChanged);
        connect(m_table->verticalHeader()->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TemplateSheetView::onHeaderSelectionChanged);

        connect(m_resetAction, &QAction::triggered, this, &TemplateSheetView::onResetTriggered);

        // 保存工程前，把内存里的填报内容写入数据库
        auto* ctx = doclife::core::ProjectContext::instance();
        connect(ctx, &doclife::core::ProjectContext::projectSaving,
            this, &TemplateSheetView::flushPendingEdits);
        // 工程关闭后丢弃未保存的填报缓存，避免串到下一个工程
        connect(ctx, &doclife::core::ProjectContext::projectClosed,
            this, &TemplateSheetView::clearPendingEdits);
        // 正在填报的表格被删除时，退回空白页并丢弃该表的缓存
        connect(ctx, &doclife::core::ProjectContext::nodeRemoved, this,
            [this](const QString&, const QString& nodeId) {
                if (nodeId.isEmpty() || nodeId != m_currentNodeId) return;
                const int sheetId = m_currentSheetId;
                for (auto it = m_pendingEdits.begin(); it != m_pendingEdits.end(); ) {
                    if (it.key().sheetId == sheetId) it = m_pendingEdits.erase(it);
                    else ++it;
                }
                clearView();
                UISignalManager::instance()->projectBlankPageActivated();
            });
    }

    QString TemplateSheetView::columnName(int col) const
    {
        QString name;
        while (col >= 0) {
            name.prepend(QChar('A' + col % 26));
            col = col / 26 - 1;
        }
        return name;
    }

    void TemplateSheetView::clearView()
    {
        m_isApplyingSettings = true;
        m_table->clear();
        m_table->setRowCount(0);
        m_table->setColumnCount(0);
        m_table->clearSpans();
        m_currentNodeId.clear();
        m_currentSheetId = -1;
        m_currentDbPath.clear();
        m_isApplyingSettings = false;
    }

    void TemplateSheetView::clearPendingEdits()
    {
        m_pendingEdits.clear();
    }

    void TemplateSheetView::flushPendingEdits()
    {
        if (m_pendingEdits.isEmpty()) return;

        auto* ctx = doclife::core::ProjectContext::instance();
        QSqlDatabase db = ctx->getProjectDb();
        if (!db.isOpen()) {
            qWarning() << "★ 填报内容保存失败：工程数据库未打开，待写入单元格数:" << m_pendingEdits.size();
            return;
        }

        const bool inTransaction = db.transaction();

        QSqlQuery q(db);
        q.prepare("INSERT OR REPLACE INTO form_values (sheet_id, row, col, value) VALUES (?,?,?,?)");

        bool ok = true;
        for (auto it = m_pendingEdits.cbegin(); it != m_pendingEdits.cend(); ++it) {
            q.bindValue(0, it.key().sheetId);
            q.bindValue(1, it.key().row);
            q.bindValue(2, it.key().col);
            q.bindValue(3, it.value());
            if (!q.exec()) {
                qWarning() << "★ 写入填报数据失败:" << q.lastError().text();
                ok = false;
                break;
            }
        }

        if (inTransaction) {
            if (ok) ok = db.commit();
            else    db.rollback();
        }

        if (ok) {
            qDebug() << "★ 填报内容已保存，共" << m_pendingEdits.size() << "个单元格";
            m_pendingEdits.clear();
        }
    }

    void TemplateSheetView::loadSheet(const QString& dbPath, int sheetId, const QString& title)
    {
        m_currentDbPath = dbPath;
        m_currentSheetId = sheetId;
        m_isProjectMode = false;
        m_titleAction->setText(title);
        setupTemplate(dbPath, sheetId);
        setMode(false);
    }

    void TemplateSheetView::loadProjectSheet(const QString& nodeId, const QString& dbPath, int sheetId, const QString& title)
    {
        m_currentNodeId = nodeId;
        m_currentDbPath = dbPath;
        m_currentSheetId = sheetId;
        m_isProjectMode = true;
        m_titleAction->setText(title);

        setupTemplate(dbPath, sheetId);
        setMode(true);
        applyUserSettings();
    }

    void TemplateSheetView::setMode(bool isProject)
    {
        m_isApplyingSettings = true;
        if (isProject) {
            m_table->horizontalHeader()->setVisible(true);
            m_table->verticalHeader()->setVisible(true);
            m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
            m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
            m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
            m_resetAction->setVisible(true);

            for (int c = 0; c < m_table->columnCount(); ++c) {
                m_table->setHorizontalHeaderItem(c, new QTableWidgetItem(columnName(c)));
            }
            for (int r = 0; r < m_table->rowCount(); ++r) {
                m_table->setVerticalHeaderItem(r, new QTableWidgetItem(QString::number(r + 1)));
            }
        }
        else {
            m_table->horizontalHeader()->setVisible(false);
            m_table->verticalHeader()->setVisible(false);
            m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
            m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
            m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            m_resetAction->setVisible(false);
        }
        m_isApplyingSettings = false;
    }

    void TemplateSheetView::setupTemplate(const QString& dbPath, int sheetId)
    {
        // 整个构建过程都屏蔽 itemChanged，否则模板内容会被当成用户输入缓存起来
        const bool guardBefore = m_isApplyingSettings;
        m_isApplyingSettings = true;
        auto restoreGuard = [this, guardBefore]() { m_isApplyingSettings = guardBefore; };

        m_table->clear();
        m_table->setRowCount(0);
        m_table->setColumnCount(0);
        m_table->clearSpans();
        m_defaultColWidths.clear();
        m_defaultRowHeights.clear();

        QSqlDatabase db;
        bool ownsConnection = false;

        if (m_isProjectMode) {
            // 填报模式复用工程上下文里的长连接：同一个文件不重复打开，另存后也自动跟随
            db = doclife::core::ProjectContext::instance()->getProjectDb();
            if (!db.isOpen()) { restoreGuard(); return; }
        }
        else {
            if (!QFile::exists(dbPath)) { restoreGuard(); return; }

            const QString connName = QStringLiteral("sheetview_main");
            db = QSqlDatabase::contains(connName) ? QSqlDatabase::database(connName)
                : QSqlDatabase::addDatabase("QSQLITE", connName);
            if (db.isOpen()) db.close();
            db.setDatabaseName(dbPath);
            if (!db.open()) { restoreGuard(); return; }
            ownsConnection = true;
        }

        // 统一收尾：预览模式关掉自己的连接，填报模式保留长连接
        auto finish = [&]() {
            if (ownsConnection && db.isValid()) db.close();
            restoreGuard();
            };

        int rowCount = 0, colCount = 0;
        {
            QSqlQuery q(db);
            const QString tableName = m_isProjectMode ? QStringLiteral("project_sheets")
                : QStringLiteral("template_sheets");
            q.prepare(QStringLiteral("SELECT row_count, col_count FROM %1 WHERE id = ?").arg(tableName));
            q.addBindValue(sheetId);
            if (q.exec() && q.next()) {
                rowCount = q.value(0).toInt();
                colCount = q.value(1).toInt();
            }
        }

        if (rowCount <= 0 || colCount <= 0) { finish(); return; }

        m_table->setUpdatesEnabled(false);
        m_table->setRowCount(rowCount);
        m_table->setColumnCount(colCount);

        // 1. 恢复默认宽高
        {
            QSqlQuery q(db);
            q.prepare("SELECT col, width FROM sheet_col_widths WHERE sheet_id = ? ORDER BY col");
            q.addBindValue(sheetId);
            if (q.exec()) {
                while (q.next()) {
                    int c = q.value(0).toInt();
                    int px = charsToPx(q.value(1).toDouble());
                    if (c >= 0 && c < colCount) m_table->setColumnWidth(c, px);
                }
            }
            for (int c = 0; c < colCount; ++c) m_defaultColWidths.append(m_table->columnWidth(c));
        }
        {
            QSqlQuery q(db);
            q.prepare("SELECT row, height FROM sheet_row_heights WHERE sheet_id = ?");
            q.addBindValue(sheetId);
            if (q.exec()) {
                while (q.next()) {
                    int r = q.value(0).toInt();
                    int px = ptToPx(q.value(1).toDouble());
                    if (r >= 0 && r < rowCount) m_table->setRowHeight(r, px);
                }
            }
            for (int r = 0; r < rowCount; ++r) m_defaultRowHeights.append(m_table->rowHeight(r));
        }

        // 2. 渲染单元格
        QHash<QString, int> fontVotes;   // "字体|字号" -> 出现次数，用来推断空白格的默认字体
        {
            QSqlQuery q(db);
            q.prepare("SELECT row, col, row_span, col_span, value, font_family, font_size, font_bold, font_italic, font_underline, font_color, bg_color, h_align, v_align, border_top, border_bottom, border_left, border_right FROM sheet_cells WHERE sheet_id = ?");
            q.addBindValue(sheetId);
            if (q.exec()) {
                while (q.next()) {
                    const int r = q.value(0).toInt();
                    const int c = q.value(1).toInt();
                    if (r < 0 || r >= rowCount || c < 0 || c >= colCount) continue;

                    const int rowSpan = qMax(1, q.value(2).toInt());
                    const int colSpan = qMax(1, q.value(3).toInt());
                    auto* item = new QTableWidgetItem(q.value(4).toString());
                    // item->setData(Qt::CursorRole, Utils::createSvgCursor(":/icons/cursor/cross-society.svg", 16));
                    // item->setData(Qt::CursorRole, Qt::CrossCursor);

                    qreal fontSize = q.value(6).toDouble();
                    if (fontSize <= 0.0) fontSize = 10.5;
                    const QString fontFamily = q.value(5).toString();
                    QFont font(fontFamily, qRound(fontSize + 0.5));
                    font.setBold(q.value(7).toInt() != 0);
                    font.setItalic(q.value(8).toInt() != 0);
                    font.setUnderline(q.value(9).toInt() != 0);
                    item->setFont(font);
                    if (!fontFamily.isEmpty()) {
                        fontVotes[fontFamily + QLatin1Char('|') + QString::number(qRound(fontSize))]++;
                    }

                    QColor fg(q.value(10).toString()); if (fg.isValid()) item->setForeground(fg);
                    QColor bg(q.value(11).toString()); if (bg.isValid() && bg != QColor("#FFFFFF")) item->setBackground(bg);

                    const int h = q.value(12).toInt(), v = q.value(13).toInt();
                    Qt::Alignment align;
                    if (h == 1) align |= Qt::AlignHCenter; else if (h == 2) align |= Qt::AlignRight; else align |= Qt::AlignLeft;
                    if (v == 0) align |= Qt::AlignTop; else if (v == 2) align |= Qt::AlignBottom; else align |= Qt::AlignVCenter;
                    item->setTextAlignment(align);

                    item->setData(Qt::UserRole + 10, q.value(14).toInt());
                    item->setData(Qt::UserRole + 11, q.value(15).toInt());
                    item->setData(Qt::UserRole + 12, q.value(16).toInt());
                    item->setData(Qt::UserRole + 13, q.value(17).toInt());

                    if (m_isProjectMode) item->setFlags(item->flags() | Qt::ItemIsEditable);
                    m_table->setItem(r, c, item);

                    if (rowSpan > 1 || colSpan > 1) {
                        m_table->setSpan(r, c, qMin(rowCount - r, rowSpan), qMin(colCount - c, colSpan));
                    }
                }
            }
        }

        // 3. 填报模式：补齐空白单元格并叠加已填写的内容
        if (m_isProjectMode) {
            QString dominantKey;
            int bestVotes = 0;
            for (auto it = fontVotes.cbegin(); it != fontVotes.cend(); ++it) {
                if (it.value() > bestVotes) { bestVotes = it.value(); dominantKey = it.key(); }
            }
            QFont dominantFont(QStringLiteral("宋体"), 11);
            const QStringList parts = dominantKey.split(QLatin1Char('|'));
            if (parts.size() == 2 && !parts.at(0).isEmpty()) {
                dominantFont = QFont(parts.at(0), parts.at(1).toInt());
            }

            ensureBlankCellsEditable(rowCount, colCount, dominantFont);
            applyFormValues(db, rowCount, colCount);
        }

        // ★ 关键修复：去掉了 resizeSections(QHeaderView::Fixed)，防止覆盖用户数据
        m_table->setUpdatesEnabled(true);
        finish();
    }

    void TemplateSheetView::ensureBlankCellsEditable(int rowCount, int colCount, const QFont& defaultFont)
    {
        // 模板只记录了有内容的单元格，空白格没有 QTableWidgetItem，
        // 而 QTableWidget 只允许编辑已经有 item 的格子，所以这里补齐。
        for (int r = 0; r < rowCount; ++r) {
            for (int c = 0; c < colCount; ++c) {
                if (m_table->item(r, c)) continue;
                auto* item = new QTableWidgetItem;
                item->setFont(defaultFont);
                item->setFlags(item->flags() | Qt::ItemIsEditable);
                m_table->setItem(r, c, item);
            }
        }
    }

    void TemplateSheetView::applyFormValues(QSqlDatabase& db, int rowCount, int colCount)
    {
        QSqlQuery q(db);
        q.prepare("SELECT row, col, value FROM form_values WHERE sheet_id = ?");
        q.addBindValue(m_currentSheetId);
        if (!q.exec()) {
            qWarning() << "★ 读取填报数据失败:" << q.lastError().text();
            return;
        }
        while (q.next()) {
            const int r = q.value(0).toInt();
            const int c = q.value(1).toInt();
            if (r < 0 || r >= rowCount || c < 0 || c >= colCount) continue;
            if (auto* item = m_table->item(r, c)) item->setText(q.value(2).toString());
        }
    }

    void TemplateSheetView::onSectionResized(int logicalIndex, int oldSize, int newSize)
    {
        if (!m_isProjectMode || m_isApplyingSettings || m_currentNodeId.isEmpty()) return;

        auto* ctx = doclife::core::ProjectContext::instance();
        auto* node = ctx->findNodeById(m_currentNodeId);
        if (!node) return;

        QJsonObject widths = node->data.attrs.value("col_widths").toObject();
        QJsonObject heights = node->data.attrs.value("row_heights").toObject();

        if (sender() == m_table->horizontalHeader()) {
            widths[QString::number(logicalIndex)] = newSize;
            node->data.attrs.insert("col_widths", widths);
            qDebug() << "★ 已记录列宽保存请求，列：" << logicalIndex << " 宽度：" << newSize;
        }
        else if (sender() == m_table->verticalHeader()) {
            heights[QString::number(logicalIndex)] = newSize;
            node->data.attrs.insert("row_heights", heights);
            qDebug() << "★ 已记录行高保存请求，行：" << logicalIndex << " 高度：" << newSize;
        }
        ctx->markDirty();
    }

    void TemplateSheetView::onItemChanged(QTableWidgetItem* item)
    {
        if (!m_isProjectMode || m_isApplyingSettings || item == nullptr) return;
        if (m_currentNodeId.isEmpty() || m_currentSheetId < 0) return;

        const int row = item->row();
        const int col = item->column();
        if (row < 0 || col < 0) return;

        // 先只记在内存里，保存工程时统一落库，避免未保存的改动偷偷写进文件
        PendingCellKey key;
        key.sheetId = m_currentSheetId;
        key.row = row;
        key.col = col;
        m_pendingEdits.insert(key, item->text());

        doclife::core::ProjectContext::instance()->markDirty();
    }

    void TemplateSheetView::applyUserSettings()
    {
        m_isApplyingSettings = true;
        auto* ctx = doclife::core::ProjectContext::instance();
        auto* node = ctx->findNodeById(m_currentNodeId);
        if (node) {
            QJsonObject widths = node->data.attrs.value("col_widths").toObject();
            QJsonObject heights = node->data.attrs.value("row_heights").toObject();
            for (auto it = widths.begin(); it != widths.end(); ++it) {
                int col = it.key().toInt();
                if (col >= 0 && col < m_table->columnCount()) m_table->setColumnWidth(col, it.value().toInt());
            }
            for (auto it = heights.begin(); it != heights.end(); ++it) {
                int row = it.key().toInt();
                if (row >= 0 && row < m_table->rowCount()) m_table->setRowHeight(row, it.value().toInt());
            }
        }
        m_isApplyingSettings = false;
    }

    void TemplateSheetView::onHorizontalHeaderClicked(int logicalIndex)
    {
        if (!m_isProjectMode) return;
        m_table->selectColumn(logicalIndex);
    }

    void TemplateSheetView::onVerticalHeaderClicked(int logicalIndex)
    {
        if (!m_isProjectMode) return;
        m_table->selectRow(logicalIndex);
    }

    // ★ 核心修复：彻底利用 selectionModel 来判断当前选中状态，防止行/列判断串号
    void TemplateSheetView::onHeaderSelectionChanged()
    {
        auto selModel = m_table->selectionModel();
        int selectedCols = selModel->selectedColumns().size();
        int selectedRows = selModel->selectedRows().size();

        if (selectedCols > 0) {
            if (selectedCols == 1) {
                int col = selModel->selectedColumns().first().column();
                m_resetAction->setText(QString("还原%1列宽度").arg(columnName(col)));
            }
            else {
                m_resetAction->setText(QString("还原%1列宽度").arg(selectedCols));
            }
        }
        else if (selectedRows > 0) {
            if (selectedRows == 1) {
                int row = selModel->selectedRows().first().row();
                m_resetAction->setText(QString("还原%1行高度").arg(row + 1));
            }
            else {
                m_resetAction->setText(QString("还原%1行高度").arg(selectedRows));
            }
        }
        else {
            m_resetAction->setText("重置宽高");
        }
    }

    // ★ 核心修复：完全隔离行高和列宽的恢复
    void TemplateSheetView::onResetTriggered()
    {
        m_isApplyingSettings = true;

        auto* ctx = doclife::core::ProjectContext::instance();
        auto* node = ctx->findNodeById(m_currentNodeId);
        if (!node) { m_isApplyingSettings = false; return; }

        auto selModel = m_table->selectionModel();
        QModelIndexList selCols = selModel->selectedColumns();
        QModelIndexList selRows = selModel->selectedRows();

        QJsonObject widths = node->data.attrs.value("col_widths").toObject();
        QJsonObject heights = node->data.attrs.value("row_heights").toObject();

        if (!selCols.isEmpty()) {
            // 还原选中列
            for (auto& index : selCols) {
                int col = index.column();
                if (col < m_defaultColWidths.size()) m_table->setColumnWidth(col, m_defaultColWidths[col]);
                widths.remove(QString::number(col));
            }
            node->data.attrs.insert("col_widths", widths);
            qDebug() << "还原了" << selCols.size() << "列的宽度";
        }
        else if (!selRows.isEmpty()) {
            // 还原选中行
            for (auto& index : selRows) {
                int row = index.row();
                if (row < m_defaultRowHeights.size()) m_table->setRowHeight(row, m_defaultRowHeights[row]);
                heights.remove(QString::number(row));
            }
            node->data.attrs.insert("row_heights", heights);
            qDebug() << "还原了" << selRows.size() << "行的高度";
        }
        else {
            // 全量还原
            node->data.attrs.remove("col_widths");
            node->data.attrs.remove("row_heights");
            for (int c = 0; c < m_table->columnCount(); ++c) {
                if (c < m_defaultColWidths.size()) m_table->setColumnWidth(c, m_defaultColWidths[c]);
            }
            for (int r = 0; r < m_table->rowCount(); ++r) {
                if (r < m_defaultRowHeights.size()) m_table->setRowHeight(r, m_defaultRowHeights[r]);
            }
            qDebug() << "全量还原了宽高";
        }

        ctx->markDirty();
        m_isApplyingSettings = false;
        m_table->clearSelection();
    }

} // namespace doclife::ui