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
#include <QApplication>
#include <QClipboard>
#include <QShortcut>
#include <QUndoStack>
#include <QSet>
#include <climits>
#include <utils.h>
#include <uisignalmanager.h>
#include "sheetformatbar.h"

#include <components/menus_toolbars/CommandBar.h>
#include <business/projectcontext.h>

namespace doclife::ui {

    class CellBorderDelegate : public QStyledItemDelegate
    {
    public:
        explicit CellBorderDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

        // 文本真正写入模型前记录旧值，供撤销栈使用
        std::function<void(int, int, const QString&, const QString&)> onTextEdited;

        void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
        {
            const QString before = index.data(Qt::EditRole).toString();
            QStyledItemDelegate::setModelData(editor, model, index);
            const QString after = model->data(index, Qt::EditRole).toString();
            if (before != after && onTextEdited)
                onTextEdited(index.row(), index.column(), before, after);
        }

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
    static inline quint64 cellKey(int row, int col)
    {
        return (quint64(quint32(row)) << 32) | quint32(col);
    }

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

        auto* delegate = new CellBorderDelegate(m_table);
        delegate->onTextEdited = [this](int row, int col, const QString& before, const QString& after) {
            if (!m_isProjectMode || m_isApplyingSettings) return;
            QVector<CellSnapshot> beforeSnapshots{ snapshotOf(row, col) };
            QVector<CellSnapshot> afterSnapshots{ beforeSnapshots.first() };
            beforeSnapshots[0].text = before;
            afterSnapshots[0].text = after;
            pushEdit(beforeSnapshots, afterSnapshots, QStringLiteral("输入内容"));
        };
        m_table->setItemDelegate(delegate);

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

        m_formatBar = new SheetFormatBar(this);
        m_formatBar->setVisible(false);
        layout->addWidget(m_formatBar);

        layout->addWidget(m_table, 1);

        m_undoStack = new QUndoStack(this);
        m_undoStack->setUndoLimit(100);
        setupFormatBar();

        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &TemplateSheetView::onSelectionChanged);

        connect(m_table->horizontalHeader(), &QHeaderView::sectionResized, this, &TemplateSheetView::onSectionResized);
        connect(m_table->verticalHeader(), &QHeaderView::sectionResized, this, &TemplateSheetView::onSectionResized);
        connect(m_table, &QTableWidget::itemChanged, this, &TemplateSheetView::onItemChanged);
        connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this, &TemplateSheetView::onHorizontalHeaderClicked);
        connect(m_table->verticalHeader(), &QHeaderView::sectionClicked, this, &TemplateSheetView::onVerticalHeaderClicked);

        connect(m_table->horizontalHeader()->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TemplateSheetView::onHeaderSelectionChanged);
        connect(m_table->verticalHeader()->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TemplateSheetView::onHeaderSelectionChanged);

        connect(m_resetAction, &QAction::triggered, this, &TemplateSheetView::onResetTriggered);

        // 编辑区快捷键只在表格本身有焦点时生效，避免抢走单元格编辑器里的 Ctrl+Z
        auto addShortcut = [this](const QKeySequence& sequence, const std::function<void()>& slot) {
            auto* shortcut = new QShortcut(sequence, m_table);
            shortcut->setContext(Qt::WidgetShortcut);
            connect(shortcut, &QShortcut::activated, this, slot);
            };
        addShortcut(QKeySequence::Undo, [this]() { onUndoTriggered(); });
        addShortcut(QKeySequence::Redo, [this]() { onRedoTriggered(); });
        addShortcut(QKeySequence::Cut, [this]() { onCut(); });
        addShortcut(QKeySequence::Copy, [this]() { onCopy(); });
        addShortcut(QKeySequence::Paste, [this]() { onPaste(); });
        addShortcut(QKeySequence::Bold, [this]() {
            applyToSelection(QStringLiteral("加粗"), [](CellSnapshot& s) {
                QFont font = s.font;
                font.setBold(!font.bold());
                s.font = font;
                });
            });
        addShortcut(QKeySequence::Italic, [this]() {
            applyToSelection(QStringLiteral("斜体"), [](CellSnapshot& s) {
                QFont font = s.font;
                font.setItalic(!font.italic());
                s.font = font;
                });
            });
        addShortcut(QKeySequence::Underline, [this]() {
            applyToSelection(QStringLiteral("下划线"), [](CellSnapshot& s) {
                QFont font = s.font;
                font.setUnderline(!font.underline());
                s.font = font;
                });
            });

        connect(m_undoStack, &QUndoStack::canUndoChanged, this, [this](bool canUndo) {
            if (m_formatBar) m_formatBar->setUndoRedoEnabled(canUndo, m_undoStack->canRedo());
            });
        connect(m_undoStack, &QUndoStack::canRedoChanged, this, [this](bool canRedo) {
            if (m_formatBar) m_formatBar->setUndoRedoEnabled(m_undoStack->canUndo(), canRedo);
            });

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
                for (auto it = m_pendingFormats.begin(); it != m_pendingFormats.end(); ) {
                    if (it.key().sheetId == sheetId) it = m_pendingFormats.erase(it);
                    else ++it;
                }
                clearView();
                UISignalManager::instance()->projectBlankPageActivated();
            });
    }

    TemplateSheetView::~TemplateSheetView()
    {
        // 析构期间撤销栈的信号可能回调到已经销毁的子控件，先静默清空
        if (m_undoStack) {
            m_undoStack->blockSignals(true);
            m_undoStack->clear();
        }
    }

    QString TemplateSheetView::columnName(int col) const    {
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
        if (m_undoStack) m_undoStack->clear();
        m_isApplyingSettings = false;
    }

    void TemplateSheetView::clearPendingEdits()
    {
        m_pendingEdits.clear();
        m_pendingFormats.clear();
    }

    void TemplateSheetView::flushPendingEdits()
    {
        if (m_pendingEdits.isEmpty() && m_pendingFormats.isEmpty()) return;

        auto* ctx = doclife::core::ProjectContext::instance();
        QSqlDatabase db = ctx->getProjectDb();
        if (!db.isOpen()) {
            qWarning() << "★ 填报内容保存失败：工程数据库未打开，待写入单元格数:"
                << m_pendingEdits.size() << m_pendingFormats.size();
            return;
        }

        const bool inTransaction = db.transaction();

        bool ok = true;

        // 1. 填报的文本
        if (!m_pendingEdits.isEmpty()) {
            QSqlQuery q(db);
            q.prepare("INSERT OR REPLACE INTO form_values (sheet_id, row, col, value) VALUES (?,?,?,?)");
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
        }

        // 2. 单元格格式：只更新格式列，已有单元格的文本与边框保持不变
        if (ok && !m_pendingFormats.isEmpty()) {
            QSqlQuery q(db);
            q.prepare("INSERT INTO sheet_cells (sheet_id, row, col, value, font_family, font_size, "
                "font_bold, font_italic, font_underline, font_color, bg_color, h_align, v_align, "
                "row_span, col_span, border_top, border_bottom, border_left, border_right, wrap_text) "
                "VALUES (?,?,?,'',?,?,?,?,?,?,?,?,?,?,?,0,0,0,0,1) "
                "ON CONFLICT(sheet_id, row, col) DO UPDATE SET "
                "font_family=excluded.font_family, font_size=excluded.font_size, "
                "font_bold=excluded.font_bold, font_italic=excluded.font_italic, "
                "font_underline=excluded.font_underline, font_color=excluded.font_color, "
                "bg_color=excluded.bg_color, h_align=excluded.h_align, v_align=excluded.v_align, "
                "row_span=excluded.row_span, col_span=excluded.col_span");

            for (auto it = m_pendingFormats.cbegin(); it != m_pendingFormats.cend(); ++it) {
                const PendingCellFormat& f = it.value();
                q.bindValue(0, it.key().sheetId);
                q.bindValue(1, it.key().row);
                q.bindValue(2, it.key().col);
                q.bindValue(3, f.fontFamily);
                q.bindValue(4, f.fontSize);
                q.bindValue(5, f.bold);
                q.bindValue(6, f.italic);
                q.bindValue(7, f.underline);
                q.bindValue(8, f.fontColor);
                q.bindValue(9, f.fillColor);
                q.bindValue(10, f.hAlign);
                q.bindValue(11, f.vAlign);
                q.bindValue(12, f.rowSpan);
                q.bindValue(13, f.colSpan);
                if (!q.exec()) {
                    qWarning() << "★ 写入单元格格式失败:" << q.lastError().text();
                    ok = false;
                    break;
                }
            }
        }

        if (inTransaction) {
            if (ok) ok = db.commit();
            else    db.rollback();
        }

        if (ok) {
            qDebug() << "★ 填报内容已保存，文本" << m_pendingEdits.size()
                << "格，格式" << m_pendingFormats.size() << "格";
            m_pendingEdits.clear();
            m_pendingFormats.clear();
        }
    }

    void TemplateSheetView::loadSheet(const QString& dbPath, int sheetId, const QString& title)
    {
        if (m_undoStack) m_undoStack->clear();
        m_currentDbPath = dbPath;
        m_currentSheetId = sheetId;
        m_isProjectMode = false;
        m_titleAction->setText(title);
        setupTemplate(dbPath, sheetId);
        setMode(false);
    }

    void TemplateSheetView::loadProjectSheet(const QString& nodeId, const QString& dbPath, int sheetId, const QString& title)
    {
        if (m_undoStack) m_undoStack->clear();
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
            if (m_formatBar) m_formatBar->setVisible(true);

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
            if (m_formatBar) m_formatBar->setVisible(false);
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
        m_spanAnchors.clear();

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
                        m_spanAnchors.insert(cellKey(r, c));
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

    // =====================================================================
    // 编辑区：格式工具栏、选区快照、撤销/重做、剪贴板、合并
    // =====================================================================

    namespace {

        int horizontalAlignOf(int align)
        {
            if (align & Qt::AlignHCenter) return 1;
            if (align & Qt::AlignRight) return 2;
            return 0;
        }

        int verticalAlignOf(int align)
        {
            if (align & Qt::AlignTop) return 0;
            if (align & Qt::AlignBottom) return 2;
            return 1;
        }

        int alignmentFlags(int hAlign, int vAlign)
        {
            int flags = 0;
            flags |= (hAlign == 1) ? Qt::AlignHCenter : (hAlign == 2) ? Qt::AlignRight : Qt::AlignLeft;
            flags |= (vAlign == 0) ? Qt::AlignTop : (vAlign == 2) ? Qt::AlignBottom : Qt::AlignVCenter;
            return flags;
        }

    } // namespace

    class SheetEditCommand : public QUndoCommand
    {
    public:
        SheetEditCommand(TemplateSheetView* view,
                         QVector<CellSnapshot> before,
                         QVector<CellSnapshot> after,
                         const QString& label)
            : m_view(view)
            , m_before(std::move(before))
            , m_after(std::move(after))
        {
            setText(label);
        }

        void undo() override { if (m_view) m_view->applySnapshots(m_before); }
        void redo() override { if (m_view) m_view->applySnapshots(m_after); }

    private:
        TemplateSheetView* m_view = nullptr;
        QVector<CellSnapshot> m_before;
        QVector<CellSnapshot> m_after;
    };

    void TemplateSheetView::setupFormatBar()
    {
        if (!m_formatBar) return;

        connect(m_formatBar, &SheetFormatBar::undoRequested, this, &TemplateSheetView::onUndoTriggered);
        connect(m_formatBar, &SheetFormatBar::redoRequested, this, &TemplateSheetView::onRedoTriggered);
        connect(m_formatBar, &SheetFormatBar::cutRequested, this, &TemplateSheetView::onCut);
        connect(m_formatBar, &SheetFormatBar::copyRequested, this, &TemplateSheetView::onCopy);
        connect(m_formatBar, &SheetFormatBar::pasteRequested, this, &TemplateSheetView::onPaste);
        connect(m_formatBar, &SheetFormatBar::mergeRequested, this, &TemplateSheetView::onMergeCells);

        connect(m_formatBar, &SheetFormatBar::fontFamilyChanged, this, &TemplateSheetView::onFontFamilyChanged);
        connect(m_formatBar, &SheetFormatBar::fontSizeChanged, this, &TemplateSheetView::onFontSizeChanged);
        connect(m_formatBar, &SheetFormatBar::boldToggled, this, &TemplateSheetView::onBoldToggled);
        connect(m_formatBar, &SheetFormatBar::italicToggled, this, &TemplateSheetView::onItalicToggled);
        connect(m_formatBar, &SheetFormatBar::underlineToggled, this, &TemplateSheetView::onUnderlineToggled);
        connect(m_formatBar, &SheetFormatBar::fontColorChanged, this, &TemplateSheetView::onFontColorChanged);
        connect(m_formatBar, &SheetFormatBar::fillColorChanged, this, &TemplateSheetView::onFillColorChanged);
        connect(m_formatBar, &SheetFormatBar::hAlignChanged, this, &TemplateSheetView::onHAlignChanged);
        connect(m_formatBar, &SheetFormatBar::vAlignChanged, this, &TemplateSheetView::onVAlignChanged);
    }

    CellSnapshot TemplateSheetView::snapshotOf(int row, int col) const
    {
        CellSnapshot snapshot;
        snapshot.row = row;
        snapshot.col = col;

        if (const auto* item = m_table->item(row, col)) {
            snapshot.text = item->text();
            snapshot.font = item->font();
            snapshot.foreground = item->foreground().color();
            snapshot.hasFill = item->background().style() != Qt::NoBrush;
            snapshot.background = item->background().color();
            snapshot.alignment = int(item->textAlignment());
        }
        // QTableView::rowSpan() 对被合并覆盖的单元格也会返回整块跨度，
        // 只有锚点才应该记录跨度，否则回放快照时会重复合并、报重叠。
        const quint64 key = cellKey(row, col);
        if (m_spanAnchors.contains(key)) {
            snapshot.rowSpan = qMax(1, m_table->rowSpan(row, col));
            snapshot.colSpan = qMax(1, m_table->columnSpan(row, col));
        }
        return snapshot;
    }

    QVector<CellSnapshot> TemplateSheetView::snapshotRanges(
        const QList<QTableWidgetSelectionRange>& ranges) const
    {
        QVector<CellSnapshot> snapshots;
        for (const auto& range : ranges) {
            for (int row = range.topRow(); row <= range.bottomRow(); ++row) {
                if (row < 0 || row >= m_table->rowCount()) continue;
                for (int col = range.leftColumn(); col <= range.rightColumn(); ++col) {
                    if (col < 0 || col >= m_table->columnCount()) continue;
                    snapshots.append(snapshotOf(row, col));
                }
            }
        }
        return snapshots;
    }

    void TemplateSheetView::applySnapshots(const QVector<CellSnapshot>& snapshots)
    {
        if (!m_isProjectMode || snapshots.isEmpty()) return;

        const bool guard = m_isApplyingSettings;
        m_isApplyingSettings = true;

        // 先清掉旧合并，避免被合并覆盖的单元格带着旧跨度
        for (const auto& s : snapshots) {
            const quint64 key = cellKey(s.row, s.col);
            if (!m_spanAnchors.contains(key)) continue;
            m_table->setSpan(s.row, s.col, 1, 1);
            m_spanAnchors.remove(key);
        }

        for (const CellSnapshot& s : snapshots) {
            if (s.row < 0 || s.row >= m_table->rowCount()) continue;
            if (s.col < 0 || s.col >= m_table->columnCount()) continue;

            auto* item = m_table->item(s.row, s.col);
            if (!item) {
                item = new QTableWidgetItem;
                m_table->setItem(s.row, s.col, item);
            }

            if (item->text() != s.text) {
                item->setText(s.text);
                m_pendingEdits.insert({ m_currentSheetId, s.row, s.col }, s.text);
            }

            item->setFont(s.font);
            item->setForeground(s.foreground);
            if (s.hasFill) item->setBackground(s.background);
            else item->setBackground(QBrush());
            item->setTextAlignment(Qt::Alignment(s.alignment));
            item->setFlags(item->flags() | Qt::ItemIsEditable);

            PendingCellFormat format;
            format.fontFamily = s.font.family();
            format.fontSize = s.font.pointSizeF() > 0 ? s.font.pointSizeF() : 11.0;
            format.bold = s.font.bold() ? 1 : 0;
            format.italic = s.font.italic() ? 1 : 0;
            format.underline = s.font.underline() ? 1 : 0;
            format.fontColor = s.foreground.name();
            format.fillColor = s.hasFill ? s.background.name() : QString();
            format.hAlign = horizontalAlignOf(s.alignment);
            format.vAlign = verticalAlignOf(s.alignment);
            format.rowSpan = s.rowSpan;
            format.colSpan = s.colSpan;
            m_pendingFormats.insert({ m_currentSheetId, s.row, s.col }, format);
        }

        // 最后再按快照建立新的合并
        for (const CellSnapshot& s : snapshots) {
            if (s.rowSpan > 1 || s.colSpan > 1) {
                m_table->setSpan(s.row, s.col, s.rowSpan, s.colSpan);
                m_spanAnchors.insert(cellKey(s.row, s.col));
            }
        }

        m_isApplyingSettings = guard;
        doclife::core::ProjectContext::instance()->markDirty();
        m_table->viewport()->update();
        updateFormatBarFromSelection();
    }

    void TemplateSheetView::pushEdit(const QVector<CellSnapshot>& before,
                                     const QVector<CellSnapshot>& after,
                                     const QString& label)
    {
        if (!m_undoStack || before.isEmpty()) return;
        if (before == after) return;
        m_undoStack->push(new SheetEditCommand(this, before, after, label));
    }

    void TemplateSheetView::applyToSelection(const QString& label,
                                             const std::function<void(CellSnapshot&)>& mutate)
    {
        if (!m_isProjectMode) return;
        const auto ranges = m_table->selectedRanges();
        if (ranges.isEmpty()) return;

        const QVector<CellSnapshot> before = snapshotRanges(ranges);
        QVector<CellSnapshot> after;
        after.reserve(before.size());
        for (CellSnapshot snapshot : before) {
            mutate(snapshot);
            after.append(snapshot);
        }
        pushEdit(before, after, label);
    }

    void TemplateSheetView::updateFormatBarFromSelection()
    {
        if (!m_formatBar || !m_isProjectMode) return;
        const auto ranges = m_table->selectedRanges();
        if (ranges.isEmpty()) return;

        const auto range = ranges.first();
        const CellSnapshot snapshot = snapshotOf(range.topRow(), range.leftColumn());
        m_formatBar->syncState(snapshot.font,
            snapshot.foreground,
            snapshot.hasFill,
            snapshot.background,
            horizontalAlignOf(snapshot.alignment),
            verticalAlignOf(snapshot.alignment));
        m_formatBar->setMergeChecked(snapshot.rowSpan > 1 || snapshot.colSpan > 1);
    }

    void TemplateSheetView::onSelectionChanged()
    {
        if (!m_isProjectMode || m_isApplyingSettings) return;
        updateFormatBarFromSelection();
    }

    void TemplateSheetView::onUndoTriggered()
    {
        if (m_undoStack) m_undoStack->undo();
    }

    void TemplateSheetView::onRedoTriggered()
    {
        if (m_undoStack) m_undoStack->redo();
    }

    QString TemplateSheetView::selectionAsText() const
    {
        const auto ranges = m_table->selectedRanges();
        if (ranges.isEmpty()) return {};

        int top = INT_MAX, left = INT_MAX, bottom = -1, right = -1;
        for (const auto& range : ranges) {
            top = qMin(top, range.topRow());
            left = qMin(left, range.leftColumn());
            bottom = qMax(bottom, range.bottomRow());
            right = qMax(right, range.rightColumn());
        }

        QStringList lines;
        for (int row = top; row <= bottom; ++row) {
            QStringList cells;
            for (int col = left; col <= right; ++col) {
                const auto* item = m_table->item(row, col);
                cells << (item ? item->text() : QString());
            }
            lines << cells.join(QLatin1Char('\t'));
        }
        return lines.join(QLatin1Char('\n'));
    }

    void TemplateSheetView::onCopy()
    {
        if (!m_isProjectMode) return;
        const QString text = selectionAsText();
        if (!text.isEmpty()) QApplication::clipboard()->setText(text);
    }

    void TemplateSheetView::onCut()
    {
        if (!m_isProjectMode) return;
        const auto ranges = m_table->selectedRanges();
        if (ranges.isEmpty()) return;

        const QString text = selectionAsText();
        if (!text.isEmpty()) QApplication::clipboard()->setText(text);

        const QVector<CellSnapshot> before = snapshotRanges(ranges);
        QVector<CellSnapshot> after = before;
        for (auto& snapshot : after) snapshot.text.clear();
        pushEdit(before, after, QStringLiteral("剪切"));
    }

    void TemplateSheetView::onPaste()
    {
        if (!m_isProjectMode) return;
        const QString text = QApplication::clipboard()->text();
        if (text.isEmpty()) return;
        pasteText(text);
    }

    void TemplateSheetView::pasteText(const QString& text)
    {
        const auto ranges = m_table->selectedRanges();
        int top = 0, left = 0;
        if (!ranges.isEmpty()) {
            top = ranges.first().topRow();
            left = ranges.first().leftColumn();
        }

        const QStringList lines = text.split(QLatin1Char('\n'));
        QVector<CellSnapshot> before;
        QVector<CellSnapshot> after;

        for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            QString line = lines.at(lineIndex);
            if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
            const QStringList cells = line.split(QLatin1Char('\t'));

            for (int cellIndex = 0; cellIndex < cells.size(); ++cellIndex) {
                const int row = top + lineIndex;
                const int col = left + cellIndex;
                if (row < 0 || row >= m_table->rowCount()) continue;
                if (col < 0 || col >= m_table->columnCount()) continue;
                if (!m_table->item(row, col)) continue;

                CellSnapshot current = snapshotOf(row, col);
                CellSnapshot updated = current;
                updated.text = cells.at(cellIndex);
                before.append(current);
                after.append(updated);
            }
        }

        pushEdit(before, after, QStringLiteral("粘贴"));
    }

    void TemplateSheetView::onMergeCells()
    {
        if (!m_isProjectMode) return;
        const auto ranges = m_table->selectedRanges();
        if (ranges.isEmpty()) return;

        const auto range = ranges.first();
        const int top = range.topRow();
        const int left = range.leftColumn();
        const int rows = range.rowCount();
        const int cols = range.columnCount();

        // 选区正好等于当前合并块时才是“取消合并”，否则一律按合并处理。
        // （Excel 也是这个行为：点在合并块内部任意位置点一下会拆开，但这里只在完全选中时拆）
        const bool sameBlock = m_spanAnchors.contains(cellKey(top, left))
            && m_table->rowSpan(top, left) == rows
            && m_table->columnSpan(top, left) == cols;
        if (!sameBlock && rows < 2 && cols < 2) return;

        // 与选区相交的旧合并块要先拆开，否则 setSpan 会因重叠而失败。
        // 这些块一并放进快照里，撤销时能原样恢复。
        QVector<CellSnapshot> before;
        QSet<quint64> seen;
        auto addSnapshot = [&](int row, int col) {
            const quint64 key = (quint64(row) << 32) | quint32(col);
            if (seen.contains(key)) return;
            seen.insert(key);
            before.append(snapshotOf(row, col));
            };

        const int bottom = top + rows - 1;
        const int right = left + cols - 1;
        for (quint64 key : m_spanAnchors) {
            const int row = int(key >> 32);
            const int col = int(key & 0xffffffffULL);
            const int rowSpan = m_table->rowSpan(row, col);
            const int colSpan = m_table->columnSpan(row, col);
            const bool intersects = !(row + rowSpan - 1 < top || row > bottom
                || col + colSpan - 1 < left || col > right);
            if (intersects) addSnapshot(row, col);
        }
        for (int row = top; row <= bottom; ++row) {
            for (int col = left; col <= right; ++col) addSnapshot(row, col);
        }

        QVector<CellSnapshot> after = before;
        for (auto& snapshot : after) {
            snapshot.rowSpan = 1;
            snapshot.colSpan = 1;
            if (!sameBlock && snapshot.row == top && snapshot.col == left) {
                snapshot.rowSpan = rows;
                snapshot.colSpan = cols;
                // Excel 的“合并居中”会同时水平和垂直居中
                snapshot.alignment = alignmentFlags(1, 1);
            }
        }

        pushEdit(before, after, sameBlock ? QStringLiteral("取消合并") : QStringLiteral("合并居中"));
    }

    void TemplateSheetView::onFontFamilyChanged(const QString& family)
    {
        if (family.isEmpty()) return;
        applyToSelection(QStringLiteral("字体"), [&family](CellSnapshot& snapshot) {
            QFont font = snapshot.font;
            font.setFamily(family);
            snapshot.font = font;
            });
    }

    void TemplateSheetView::onFontSizeChanged(int pointSize)
    {
        if (pointSize <= 0) return;
        applyToSelection(QStringLiteral("字号"), [pointSize](CellSnapshot& snapshot) {
            QFont font = snapshot.font;
            font.setPointSize(pointSize);
            snapshot.font = font;
            });
    }

    void TemplateSheetView::onBoldToggled(bool on)
    {
        applyToSelection(QStringLiteral("加粗"), [on](CellSnapshot& snapshot) {
            QFont font = snapshot.font;
            font.setBold(on);
            snapshot.font = font;
            });
    }

    void TemplateSheetView::onItalicToggled(bool on)
    {
        applyToSelection(QStringLiteral("斜体"), [on](CellSnapshot& snapshot) {
            QFont font = snapshot.font;
            font.setItalic(on);
            snapshot.font = font;
            });
    }

    void TemplateSheetView::onUnderlineToggled(bool on)
    {
        applyToSelection(QStringLiteral("下划线"), [on](CellSnapshot& snapshot) {
            QFont font = snapshot.font;
            font.setUnderline(on);
            snapshot.font = font;
            });
    }

    void TemplateSheetView::onFontColorChanged(const QColor& color)
    {
        if (!color.isValid()) return;
        applyToSelection(QStringLiteral("字体颜色"), [&color](CellSnapshot& snapshot) {
            snapshot.foreground = color;
            });
    }

    void TemplateSheetView::onFillColorChanged(const QColor& color)
    {
        applyToSelection(color.isValid() ? QStringLiteral("填充颜色") : QStringLiteral("清除填充"),
            [&color](CellSnapshot& snapshot) {
                snapshot.hasFill = color.isValid();
                if (color.isValid()) snapshot.background = color;
            });
    }

    void TemplateSheetView::onHAlignChanged(int align)
    {
        applyToSelection(QStringLiteral("水平对齐"), [align](CellSnapshot& snapshot) {
            snapshot.alignment = alignmentFlags(align, verticalAlignOf(snapshot.alignment));
            });
    }

    void TemplateSheetView::onVAlignChanged(int align)
    {
        applyToSelection(QStringLiteral("垂直对齐"), [align](CellSnapshot& snapshot) {
            snapshot.alignment = alignmentFlags(horizontalAlignOf(snapshot.alignment), align);
            });
    }

} // namespace doclife::ui