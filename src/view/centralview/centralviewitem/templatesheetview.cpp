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
#include <QStyle>
#include <QFontMetrics>
#include <QAction>
#include <QItemSelectionModel>
#include <QApplication>
#include <QClipboard>
#include <QShortcut>
#include <QUndoStack>
#include <QSet>
#include <QTimer>
#include <QPointer>
#include <QSignalBlocker>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QElapsedTimer>
#include <popWindow/dialog/dialogbase.h>
#include <popWindow/settingsdialog.h>
#include <popWindow/toast/apptoast.h>
#include <components/date_time/DatePicker.h>
#include <components/dialogs_flyouts/ContentDialog.h>
#include <components/textfields/Label.h>
#include <climits>
#include <utils.h>
#include <uisignalmanager.h>
#include <logging.h>

#include "sheetformatbar.h"
#include "model/cellrole.h"
#include "model/fieldtokens.h"
#include "model/boxmark.h"

#include <components/menus_toolbars/CommandBar.h>
#include <business/projectcontext.h>

namespace doclife::ui {

    // 单元格在 QTableWidgetItem 上挂的数据键（20 以后是本模块专用）
    enum CellDataRole {
        CellRoleKeyRole = Qt::UserRole + 20,   // 角色 key：title/readonly/project/input/summary/date
        CellRawTextRole = Qt::UserRole + 21,   // 占位符原文
        CellLockedRole = Qt::UserRole + 22,    // 锁定：只读 + 不参与自动赋值
        CellAutoFitRole = Qt::UserRole + 23,   // 自适应：文字缩放到单元格大小
        CellWrapRole = Qt::UserRole + 24,      // 折行显示
        CellSaveRole = Qt::UserRole + 25,      // 这格已经有保存过的内容（不再自动同步）
    };

    namespace {
        // 性能/行为日志：统一走 logging.h（glog 落文件，用户可通过「导出诊断日志」回传）
        void perfLog(const QString& line)
        {
            DOC_INFO(QStringLiteral("perf  ") + line);
        }

        // 约定的只读底色：与模板标记色/文档一致（RGB 217,217,217）。
        // 手动锁定的格子也用它，所以「只读」和「锁定」看起来是同一个灰。
        QColor readOnlyTint() { return QColor(0xD9, 0xD9, 0xD9); }
        // 表名/标题用更浅的一层，免得整张表头和只读说明混成一片
        QColor titleTint() { return QColor(0xF2, 0xF2, 0xF2); }

        // 角色底色：用户「无填充」时回落到这一层，与模板标记色一致
        QColor cellRoleTint(const QString& roleKey)
        {
            switch (doclife::model::cellRoleFromKey(roleKey)) {
            case doclife::model::CellRole::Title:    return titleTint();
            case doclife::model::CellRole::ReadOnly: return readOnlyTint();
            case doclife::model::CellRole::Project:  return QColor(0xFF, 0xF2, 0xCC);
            case doclife::model::CellRole::Summary:  return QColor(0xDD, 0xEB, 0xF7);
            case doclife::model::CellRole::Date:     return QColor(0xE2, 0xEF, 0xDA);
            default:                                 return QColor();
            }
        }

        // 「自适应单元格大小」：只缩小、不放大，二分找能放下的最大字号
        QFont shrinkFontToFit(const QFont& font, const QString& text, const QRect& rect, bool wrap)
        {
            if (text.isEmpty()) return font;

            const QRect avail = rect.adjusted(4, 2, -4, -2);
            if (avail.width() <= 0 || avail.height() <= 0) return font;

            const qreal original = font.pointSizeF() > 0 ? font.pointSizeF() : 10.5;
            const qreal lo0 = 4.0;
            if (original <= lo0) return font;

            const int flags = wrap ? int(Qt::TextWordWrap) : int(Qt::TextSingleLine);

            auto fits = [&](qreal pt) {
                QFont probe(font);
                probe.setPointSizeF(pt);
                const QFontMetricsF fm(probe);
                const QRectF needed = fm.boundingRect(QRectF(0, 0, avail.width(), 0), flags, text);
                return needed.width() <= avail.width() + 0.5
                    && needed.height() <= avail.height() + 0.5;
            };

            if (fits(original)) return font;

            qreal lo = lo0, hi = original;
            for (int i = 0; i < 8; ++i) {
                const qreal mid = (lo + hi) / 2.0;
                if (fits(mid)) lo = mid;
                else hi = mid;
            }

            QFont fitted(font);
            fitted.setPointSizeF(lo);
            return fitted;
        }
    } // namespace

    class CellBorderDelegate : public QStyledItemDelegate
    {
    public:
        explicit CellBorderDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

        // 文本真正写入模型前记录旧值，供撤销栈使用
        std::function<void(int, int, const QString&, const QString&)> onTextEdited;

        // 选中高亮由本委托叠加绘制：样式表的 item:selected 会盖掉单元格自身的填充色，
        // 导致"填了色看不见"。这里只留一层半透明色，填充色和字体色都能透出来。
        bool selectionEnabled = true;
        QColor selectionColor = QColor(0x00, 0x78, 0xD4);

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
            // 先绕开样式表的 item:selected：带着选中态绘制时，Qt 会忽略单元格自身的填充色，
            // 填色在选中期间完全看不见。去掉标志让背景正常绘制，选中层再由下面手动叠加。
            QStyleOptionViewItem opt = option;

            // 自己走一遍 initStyleOption，改成需要的排版参数后再交给样式绘制，
            // 这样折行/省略/自适应/锁定底色都能逐格生效。
            initStyleOption(&opt, index);

            // 摘掉选中态必须在 initStyleOption 之后：它会从选区模型把 State_Selected 重新写回来，
            // 先清后写等于没清，样式表的 item:selected 仍会接管背景，单元格填充色照样看不见。
            // 选中高亮改由下面叠加半透明层绘制。
            const bool selected = (opt.state & QStyle::State_Selected) != 0;
            opt.state &= ~QStyle::State_Selected;
            opt.state &= ~QStyle::State_HasFocus;

            const QVariant wrapValue = index.data(CellWrapRole);
            const bool wrapAllowed = wrapValue.isValid() ? wrapValue.toBool() : true;

            // 只有真的放不下（或正文里本来就有换行）才走换行排版。
            // 换行要走 QTextLayout，很贵；绝大多数格子一行就放得下，跳过能明显加快整表重绘。
            bool wrap = false;
            if (wrapAllowed) {
                wrap = opt.text.contains(QLatin1Char('\n'))
                    || QFontMetrics(opt.font).horizontalAdvance(opt.text) > qMax(8, opt.rect.width() - 8);
            }

            opt.features = wrap
                ? (opt.features | QStyleOptionViewItem::WrapText)
                : (opt.features & ~QStyleOptionViewItem::WrapText);
            opt.textElideMode = Qt::ElideNone;   // 不折行就是硬切，不加省略号

            if (index.data(CellLockedRole).toBool()) {
                opt.backgroundBrush = QBrush(readOnlyTint());   // 锁定 = 约定的只读底色
            }

            if (index.data(CellAutoFitRole).toBool()) {
                const QFont fitted = shrinkFontToFit(opt.font, opt.text, opt.rect, wrap);
                opt.font = fitted;
                opt.fontMetrics = QFontMetrics(fitted);
            }

            QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

            // 不折行时样式不会自己裁剪，这里夹一下：超出单元格的部分是硬切，不是溢出到隔壁
            painter->save();
            painter->setClipRect(opt.rect);

            // 背景自己铺：表格带 QTableWidget::item 样式规则时，绘制由 QStyleSheetStyle 接管，
            // 它不会把 Qt::BackgroundRole 画出来，于是填充色、角色底色（黄/蓝/灰）、锁定灰全部丢失。
            // 在样式绘制之前先填一层，选中高亮仍在最后叠加，所以选中色照旧。
            QBrush backgroundBrush = index.data(Qt::BackgroundRole).value<QBrush>();
            if (index.data(CellLockedRole).toBool()) {
                backgroundBrush = QBrush(readOnlyTint());   // 锁定 = 约定的只读底色
            }
            if (backgroundBrush.style() != Qt::NoBrush) {
                painter->fillRect(opt.rect, backgroundBrush);
            }

            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
            painter->restore();

            if (selected && selectionEnabled) {
                painter->save();
                QColor tint = selectionColor;
                tint.setAlpha(56);
                painter->fillRect(opt.rect, tint);
                painter->restore();
            }

            const int top = index.data(Qt::UserRole + 10).toInt();
            const int bottom = index.data(Qt::UserRole + 11).toInt();
            const int left = index.data(Qt::UserRole + 12).toInt();
            const int right = index.data(Qt::UserRole + 13).toInt();

            if (top == 0 && bottom == 0 && left == 0 && right == 0)
                return;

            painter->save();
            QPen pen(QColor("#000000"));
            pen.setCosmetic(true);

            QPoint tl = opt.rect.topLeft();
            QPoint tr(opt.rect.right() + 1, opt.rect.top());
            QPoint bl = opt.rect.bottomLeft() + QPoint(0, 1);
            QPoint br = opt.rect.bottomRight() + QPoint(1, 1);

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

    // 纯文本改动（如勾选方格）：只回放文本，不碰字体/底色/合并，
    // 避免把角色底色当成用户填充写进数据库。
    class SheetTextCommand : public QUndoCommand
    {
    public:
        SheetTextCommand(TemplateSheetView* view,
                         QVector<QPair<int, int>> cells,
                         QStringList before,
                         QStringList after,
                         const QString& label)
            : m_view(view)
            , m_cells(std::move(cells))
            , m_before(std::move(before))
            , m_after(std::move(after))
        {
            setText(label);
        }

        void undo() override { applyTexts(m_before); }
        void redo() override { applyTexts(m_after); }

    private:
        void applyTexts(const QStringList& texts)
        {
            if (!m_view) return;
            for (int i = 0; i < m_cells.size() && i < texts.size(); ++i) {
                m_view->setCellTextOnly(m_cells.at(i).first, m_cells.at(i).second, texts.at(i));
            }
            m_view->syncCheckBoxesAction();
        }

        TemplateSheetView* m_view = nullptr;
        QVector<QPair<int, int>> m_cells;
        QStringList m_before;
        QStringList m_after;
    };

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

        m_refreshAction = new QAction("同步工程信息", m_topBar);
        m_refreshAction->setIcon(QIcon(":/icons/menu/project_info.svg"));
        m_topBar->addPrimaryAction(m_refreshAction);
        connect(m_refreshAction, &QAction::triggered, this, &TemplateSheetView::refreshFieldValues);

        // 一键勾选：把整张表的方格打上勾（「不合格」前面的方格保持不勾），再点一次取消
        m_checkBoxesAction = new QAction("勾选方格", m_topBar);
        m_checkBoxesAction->setIcon(QIcon(":/icons/menu/check.svg"));
        m_checkBoxesAction->setCheckable(true);
        m_checkBoxesAction->setToolTip(
            QStringLiteral("批量勾选/取消勾选"));
        m_topBar->addPrimaryAction(m_checkBoxesAction);
        connect(m_checkBoxesAction, &QAction::toggled, this, &TemplateSheetView::onCheckAllBoxesToggled);

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

        m_table->setStyleSheet(editorTableStyleSheet());
        // 选中高亮改由委托自绘（样式表的 item:selected 会盖掉单元格填充色），
        // 这里把编辑器设置里的选中色同步给委托，并在设置变更后刷新重绘。
        auto syncSelectionToDelegate = [delegate, this] {
            const auto settings = loadEditorSettings();
            delegate->selectionEnabled = settings.selectionEnabled;
            delegate->selectionColor = settings.selectionColor;
            m_table->viewport()->update();
        };
        syncSelectionToDelegate();
        connect(UISignalManager::instance(), &UISignalManager::editorSettingsChanged, this,
            [this, syncSelectionToDelegate] {
                m_table->setStyleSheet(editorTableStyleSheet());
                syncSelectionToDelegate();
            });

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
        connect(m_table, &QTableWidget::doubleClicked, this, &TemplateSheetView::onCellDoubleClicked);

        // 日期选择窗回执：确认属于当前表才写入；回执日期无效（用户取消）则保持单元格原值
        connect(UISignalManager::instance(), &UISignalManager::datePickConfirmed, this,
            [this](int sheetId, int row, int col, const QDate& date) {
                if (!m_isProjectMode || sheetId != m_currentSheetId) return;
                if (!date.isValid()) return;
                auto* cell = m_table->item(row, col);
                if (!cell) return;
                if (isCellLocked(cell)) return;   // 锁定格不接受日期回填
                cell->setText(date.toString(QStringLiteral("yyyy年M月d日")));
                m_pendingEdits.insert({ sheetId, row, col }, cell->text());
                doclife::core::ProjectContext::instance()->markDirty();
            });

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
        if (m_checkBoxesAction) {
            QSignalBlocker blocker(m_checkBoxesAction);
            m_checkBoxesAction->setChecked(false);
        }
        m_isApplyingSettings = false;
    }

    void TemplateSheetView::clearPendingEdits()
    {
        m_pendingEdits.clear();
        m_pendingFormats.clear();
    }

    void TemplateSheetView::flushPendingEdits()
    {
        flushPending(true);
    }

    void TemplateSheetView::flushCurrentSheet()
    {
        flushPending(false);
    }

    void TemplateSheetView::flushPending(bool allSheets)
    {
        if (m_pendingEdits.isEmpty() && m_pendingFormats.isEmpty()) return;

        // 切表时只写当前这张表，别的表留在缓存里等切到它 / 保存工程时再写
        auto wanted = [this, allSheets](const PendingCellKey& key) {
            return allSheets || key.sheetId == m_currentSheetId;
            };

        // 没有任何要写的就别开事务：桌面/同步盘上 commit 的 fsync 很贵，空事务也会付这个代价
        int matched = 0;
        for (auto it = m_pendingEdits.cbegin(); it != m_pendingEdits.cend(); ++it) {
            if (wanted(it.key())) ++matched;
        }
        for (auto it = m_pendingFormats.cbegin(); it != m_pendingFormats.cend(); ++it) {
            if (wanted(it.key())) ++matched;
        }
        if (matched == 0) return;

        QElapsedTimer timer;
        timer.start();

        auto* ctx = doclife::core::ProjectContext::instance();
        QSqlDatabase db = ctx->getProjectDb();
        if (!db.isOpen()) {
            qWarning() << "★ 填报内容保存失败：工程数据库未打开，待写入单元格数:"
                << m_pendingEdits.size() << m_pendingFormats.size();
            return;
        }

        const bool inTransaction = db.transaction();

        bool ok = true;
        int writtenTexts = 0;
        int writtenFormats = 0;

        // 1. 填报的文本
        {
            QSqlQuery q(db);
            q.prepare("INSERT OR REPLACE INTO form_values (sheet_id, row, col, value) VALUES (?,?,?,?)");
            for (auto it = m_pendingEdits.cbegin(); it != m_pendingEdits.cend(); ++it) {
                if (!wanted(it.key())) continue;
                q.bindValue(0, it.key().sheetId);
                q.bindValue(1, it.key().row);
                q.bindValue(2, it.key().col);
                q.bindValue(3, it.value());
                if (!q.exec()) {
                    qWarning() << "★ 写入填报数据失败:" << q.lastError().text();
                    ok = false;
                    break;
                }
                ++writtenTexts;
            }
        }

        // 2. 单元格格式：只更新格式列，已有单元格的文本与边框保持不变
        if (ok) {
            QSqlQuery q(db);
            q.prepare("INSERT INTO sheet_cells (sheet_id, row, col, value, font_family, font_size, "
                "font_bold, font_italic, font_underline, font_color, bg_color, h_align, v_align, "
                "row_span, col_span, border_top, border_bottom, border_left, border_right, wrap_text, "
                "auto_fit, locked) "
                "VALUES (?,?,?,'',?,?,?,?,?,?,?,?,?,?,?,0,0,0,0,?,?,?) "
                "ON CONFLICT(sheet_id, row, col) DO UPDATE SET "
                "font_family=excluded.font_family, font_size=excluded.font_size, "
                "font_bold=excluded.font_bold, font_italic=excluded.font_italic, "
                "font_underline=excluded.font_underline, font_color=excluded.font_color, "
                "bg_color=excluded.bg_color, h_align=excluded.h_align, v_align=excluded.v_align, "
                "row_span=excluded.row_span, col_span=excluded.col_span, "
                "wrap_text=excluded.wrap_text, auto_fit=excluded.auto_fit, locked=excluded.locked");

            for (auto it = m_pendingFormats.cbegin(); it != m_pendingFormats.cend(); ++it) {
                if (!wanted(it.key())) continue;
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
                q.bindValue(14, f.wrap);
                q.bindValue(15, f.autoFit);
                q.bindValue(16, f.locked);
                if (!q.exec()) {
                    qWarning() << "★ 写入单元格格式失败:" << q.lastError().text();
                    ok = false;
                    break;
                }
                ++writtenFormats;
            }
        }

        if (inTransaction) {
            if (ok) ok = db.commit();
            else    db.rollback();
        }

        if (!ok) return;

        qDebug() << "★ 填报内容已保存，文本" << writtenTexts << "格，格式" << writtenFormats << "格";
        perfLog(QStringLiteral("flush all=%1 文本=%2 格式=%3 耗时=%4ms")
            .arg(allSheets ? 1 : 0).arg(writtenTexts).arg(writtenFormats).arg(timer.elapsed()));

        if (allSheets) {
            m_pendingEdits.clear();
            m_pendingFormats.clear();
            return;
        }

        for (auto it = m_pendingEdits.begin(); it != m_pendingEdits.end(); ) {
            it = (it.key().sheetId == m_currentSheetId) ? m_pendingEdits.erase(it) : std::next(it);
        }
        for (auto it = m_pendingFormats.begin(); it != m_pendingFormats.end(); ) {
            it = (it.key().sheetId == m_currentSheetId) ? m_pendingFormats.erase(it) : std::next(it);
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
        QElapsedTimer timer;
        timer.start();

        // 切表前先把上一张表的填报内容落库：否则锁定、打勾、折行这些状态
        // 只存在内存里，切走再回来就丢了。
        flushCurrentSheet();
        const qint64 flushMs = timer.elapsed();

        if (m_undoStack) m_undoStack->clear();
        m_currentNodeId = nodeId;
        m_currentDbPath = dbPath;
        m_currentSheetId = sheetId;
        m_isProjectMode = true;
        m_titleAction->setText(title);

        setupTemplate(dbPath, sheetId);
        const qint64 setupMs = timer.elapsed() - flushMs;

        setMode(true);
        applyUserSettings();
        syncCheckBoxesAction();

        perfLog(QStringLiteral("切表 sheet=%1 \"%2\" 总=%3ms flush=%4ms setup=%5ms 其它=%6ms 行=%7 列=%8")
            .arg(sheetId).arg(title).arg(timer.elapsed()).arg(flushMs).arg(setupMs)
            .arg(timer.elapsed() - flushMs - setupMs)
            .arg(m_table->rowCount()).arg(m_table->columnCount()));
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
            if (m_refreshAction) m_refreshAction->setVisible(true);
            if (m_checkBoxesAction) m_checkBoxesAction->setVisible(true);

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
            if (m_refreshAction) m_refreshAction->setVisible(false);
            if (m_checkBoxesAction) m_checkBoxesAction->setVisible(false);
        }
        m_isApplyingSettings = false;
    }

    void TemplateSheetView::setupTemplate(const QString& dbPath, int sheetId)
    {
        QElapsedTimer perfTimer;
        perfTimer.start();

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
            q.prepare("SELECT row, col, row_span, col_span, value, font_family, font_size, font_bold, font_italic, font_underline, font_color, bg_color, h_align, v_align, border_top, border_bottom, border_left, border_right, wrap_text FROM sheet_cells WHERE sheet_id = ?");
            q.addBindValue(sheetId);
            if (q.exec()) {
                while (q.next()) {
                    const int r = q.value(0).toInt();
                    const int c = q.value(1).toInt();
                    if (r < 0 || r >= rowCount || c < 0 || c >= colCount) continue;

                    const int rowSpan = qMax(1, q.value(2).toInt());
                    const int colSpan = qMax(1, q.value(3).toInt());
                    auto* item = new QTableWidgetItem(q.value(4).toString());
                    // 占位符原文（sheet_cells.value）常驻数据库：它就是"这格该填什么"的信息，
                    // 用户手填、解锁都不会删掉它，点"刷新信息"就按它 + 当前工程信息重算回填。
                    if (m_isProjectMode && doclife::model::containsFieldToken(q.value(4).toString())) {
                        item->setData(Qt::UserRole + 21, q.value(4).toString());
                    }
                    // item->setData(Qt::CursorRole, Utils::createSvgCursor(":/icons/cursor/cross-society.svg", 16));
                    // item->setData(Qt::CursorRole, Qt::CrossCursor);

                    qreal fontSize = q.value(6).toDouble();
                    if (fontSize <= 0.0) fontSize = 10.5;
                    const QString fontFamily = q.value(5).toString();
                    // 用 setPointSizeF 保留 10.5 这类半磅字号；四舍五入会把 9 变成 10
                    QFont font(fontFamily);
                    font.setPointSizeF(fontSize);
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

                    const QVariant wrapValue = q.value(18);
                    item->setData(CellWrapRole, wrapValue.isNull() ? true : (wrapValue.toInt() != 0));

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

            // 排版与锁定标记（老工程库没有这些列时读不到，按默认值处理）
            {
                QSqlQuery layoutQuery(db);
                layoutQuery.prepare(QStringLiteral(
                    "SELECT row, col, locked, auto_fit FROM sheet_cells WHERE sheet_id = ?"));
                layoutQuery.addBindValue(sheetId);
                if (layoutQuery.exec()) {
                    while (layoutQuery.next()) {
                        auto* item = m_table->item(layoutQuery.value(0).toInt(), layoutQuery.value(1).toInt());
                        if (!item) continue;
                        item->setData(CellLockedRole, layoutQuery.value(2).toInt() != 0);
                        item->setData(CellAutoFitRole, layoutQuery.value(3).toInt() != 0);
                    }
                }
            }

            // 字段占位符：#projectName / #unitName 等，用当前工程与节点信息替换显示
            const QHash<QString, QString> fieldValues =
                doclife::core::ProjectContext::instance()->fieldValues(m_currentNodeId);
            for (int row = 0; row < rowCount; ++row) {
                for (int col = 0; col < colCount; ++col) {
                    auto* item = m_table->item(row, col);
                    if (!item) continue;
                    // 锁定的格子不吃自动赋值：保留它自己已有的内容
                    if (isCellLocked(item)) continue;
                    // 已经有保存内容的格子也不自动同步：切页/重开保持原样，等用户点「同步工程信息」
                    if (item->data(CellSaveRole).toBool()) continue;
                    // 占位符原文就是这格的字段绑定（数据源永远是 sheet_cells.value）：
                    // 有绑定的格子，即使 form_values 里存过手填/旧值，打开时也按绑定重算，
                    // 保证「改完工程信息 → 刷新/重开」显示的是当前项目信息。
                    QString raw = item->data(Qt::UserRole + 21).toString();
                    if (raw.isEmpty()) {
                        const QString shown = item->text();
                        if (!doclife::model::containsFieldToken(shown)) continue;
                        raw = shown;
                        item->setData(Qt::UserRole + 21, raw);
                    }
                    item->setText(doclife::model::resolveFieldTokens(raw, fieldValues));
                }
            }

            // 单元格角色：模板库可能没有 cell_role 列，读不到就按普通单元格处理
            QSqlQuery roleQuery(db);
            roleQuery.prepare(QStringLiteral(
                "SELECT row, col, cell_role FROM sheet_cells WHERE sheet_id = ? AND cell_role IS NOT NULL"));
            roleQuery.addBindValue(sheetId);
            if (roleQuery.exec()) {
                while (roleQuery.next()) {
                    const int row = roleQuery.value(0).toInt();
                    const int col = roleQuery.value(1).toInt();
                    if (row < 0 || row >= rowCount || col < 0 || col >= colCount) continue;
                    if (auto* item = m_table->item(row, col)) {
                        applyCellRole(item, roleQuery.value(2).toString());
                    }
                }
            }
        }

        // ★ 关键修复：去掉了 resizeSections(QHeaderView::Fixed)，防止覆盖用户数据
        m_table->setUpdatesEnabled(true);
        finish();
        qDebug() << "setupTemplate 耗时:" << perfTimer.elapsed() << "ms";
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
                item->setData(CellWrapRole, true);
                item->setFlags(item->flags() | Qt::ItemIsEditable);
                m_table->setItem(r, c, item);
            }
        }
    }

    void TemplateSheetView::refreshFieldValues()
    {
        if (!m_isProjectMode) return;

        const QHash<QString, QString> values =
            doclife::core::ProjectContext::instance()->fieldValues(m_currentNodeId);

        const bool guard = m_isApplyingSettings;
        m_isApplyingSettings = true;
        int refreshed = 0;
        for (int row = 0; row < m_table->rowCount(); ++row) {
            for (int col = 0; col < m_table->columnCount(); ++col) {
                auto* item = m_table->item(row, col);
                if (!item) continue;
                // 锁定的格子不参与同步
                if (isCellLocked(item)) continue;
                QString raw = item->data(Qt::UserRole + 21).toString();
                if (raw.isEmpty()) {
                    // 兼容：没有记录原始文本时，直接对当前文本再解析一次
                    raw = item->text();
                    if (!doclife::model::containsFieldToken(raw)) continue;
                }
                const QString resolved = doclife::model::resolveFieldTokens(raw, values);
                if (resolved == item->text()) continue;
                item->setText(resolved);
                ++refreshed;
                // 把重算结果同步进待保存的填报文本，保存后重开/打印口径一致
                m_pendingEdits.insert({ m_currentSheetId, row, col }, resolved);
            }
        }
        m_isApplyingSettings = guard;
        m_table->viewport()->update();
        if (refreshed > 0) {
            doclife::core::ProjectContext::instance()->markDirty();
        }
        AppToast::showSuccess(this, QStringLiteral("已按当前工程信息刷新 %1 个单元格").arg(refreshed));
    }

    void TemplateSheetView::onCheckAllBoxesToggled(bool checked)
    {
        if (!m_isProjectMode || m_currentSheetId < 0) {
            syncCheckBoxesAction();
            return;
        }

        QVector<QPair<int, int>> cells;
        QStringList before;
        QStringList after;

        for (int row = 0; row < m_table->rowCount(); ++row) {
            for (int col = 0; col < m_table->columnCount(); ++col) {
                auto* item = m_table->item(row, col);
                if (!item) continue;
                if (isCellLocked(item)) continue;   // 锁定格不参与批量打勾
                const QString oldText = item->text();
                const QString newText = doclife::model::applyBoxMarks(oldText, checked);
                if (newText == oldText) continue;
                cells.append({ row, col });
                before.append(oldText);
                after.append(newText);
            }
        }

        if (cells.isEmpty()) {
            // 没有可改的方格：按钮状态按实际内容弹回去，别停在“已勾选”的假状态
            syncCheckBoxesAction();
            AppToast::showSuccess(this, checked
                ? QStringLiteral("本表没有可勾选的方格")
                : QStringLiteral("本表没有已勾选的方格"));
            return;
        }

        if (m_undoStack) {
            m_undoStack->push(new SheetTextCommand(this, cells, before, after,
                checked ? QStringLiteral("勾选方格") : QStringLiteral("取消勾选")));
        }
        else {
            for (int i = 0; i < cells.size(); ++i) {
                setCellTextOnly(cells.at(i).first, cells.at(i).second, after.at(i));
            }
        }
        syncCheckBoxesAction();
        AppToast::showSuccess(this, checked ? QStringLiteral("已勾选") : QStringLiteral("已取消勾选"));
    }

    void TemplateSheetView::setCellTextOnly(int row, int col, const QString& text)
    {
        auto* item = m_table->item(row, col);
        if (!item) return;
        if (item->text() == text) return;

        const bool guard = m_isApplyingSettings;
        m_isApplyingSettings = true;
        item->setText(text);
        m_isApplyingSettings = guard;

        // 与「同步工程信息」一致：先进待保存缓存，保存工程时统一落库，重开/打印口径一致
        m_pendingEdits.insert({ m_currentSheetId, row, col }, text);
        doclife::core::ProjectContext::instance()->markDirty();
    }

    void TemplateSheetView::syncCheckBoxesAction()
    {
        if (!m_checkBoxesAction) return;

        bool anyChecked = false;
        for (int row = 0; row < m_table->rowCount() && !anyChecked; ++row) {
            for (int col = 0; col < m_table->columnCount(); ++col) {
                const auto* item = m_table->item(row, col);
                if (item && doclife::model::containsCheckedBox(item->text())) {
                    anyChecked = true;
                    break;
                }
            }
        }

        QSignalBlocker blocker(m_checkBoxesAction);
        m_checkBoxesAction->setChecked(anyChecked);
    }

    void TemplateSheetView::autoGrowRow(int row)
    {
        if (!m_isProjectMode || m_isApplyingSettings) return;
        if (row < 0 || row >= m_table->rowCount()) return;

        // 这一行涉及的合并块，每块只处理一次（合并块里的格子要按整块算宽高）
        QSet<quint64> blocks;
        for (int col = 0; col < m_table->columnCount(); ++col) {
            if (!m_table->item(row, col)) continue;
            const QPoint anchor = spanAnchorOf(row, col);
            blocks.insert(cellKey(anchor.y(), anchor.x()));
        }

        for (const quint64 key : blocks) {
            const int ar = int(key >> 32);
            const int ac = int(key & 0xffffffffULL);

            auto* item = m_table->item(ar, ac);
            if (!item || !item->data(CellWrapRole).toBool()) continue;

            const QString text = item->text();
            if (text.isEmpty()) continue;

            const int rs = qMax(1, m_table->rowSpan(ar, ac));
            const int cs = qMax(1, m_table->columnSpan(ar, ac));

            int width = 0;
            for (int i = 0; i < cs && ac + i < m_table->columnCount(); ++i) {
                width += m_table->columnWidth(ac + i);
            }
            width -= 8;   // 与 shrinkFontToFit 的左右边距一致
            if (width <= 8) continue;

            int current = 0;
            for (int i = 0; i < rs && ar + i < m_table->rowCount(); ++i) {
                current += m_table->rowHeight(ar + i);
            }

            const QFontMetrics fm(item->font());
            const int needed = fm.boundingRect(QRect(0, 0, width, 0), Qt::TextWordWrap, text).height() + 6;
            if (needed <= current) continue;

            // 差量加在合并块的最后一行：前面的行不动，版面变化最小
            const int last = qMin(ar + rs - 1, m_table->rowCount() - 1);
            const int before = m_table->rowHeight(last);
            m_table->setRowHeight(last, before + (needed - current));
            perfLog(QStringLiteral("行高自适应 row=%1(块 %2,%3 %4x%5) %6 → %7px")
                .arg(last).arg(ar).arg(ac).arg(rs).arg(cs)
                .arg(before).arg(before + (needed - current)));
        }
    }

    QPoint TemplateSheetView::spanAnchorOf(int row, int col) const
    {
        for (const quint64 key : m_spanAnchors) {
            const int r = int(key >> 32);
            const int c = int(key & 0xffffffffULL);
            const int rs = qMax(1, m_table->rowSpan(r, c));
            const int cs = qMax(1, m_table->columnSpan(r, c));
            if (row >= r && row < r + rs && col >= c && col < c + cs) {
                return QPoint(c, r);
            }
        }
        return QPoint(col, row);
    }

    void TemplateSheetView::applyCellRole(QTableWidgetItem* item, const QString& roleKey)
    {
        if (!item) return;

        const auto role = doclife::model::cellRoleFromKey(roleKey);
        if (role == doclife::model::CellRole::Unknown) return;

        item->setData(CellRoleKeyRole, roleKey);

        const QColor tint = cellRoleTint(roleKey);
        if (tint.isValid() && item->background().style() == Qt::NoBrush) {
            // 用户已手动填充过的格子保留自己的颜色，不被角色色覆盖
            item->setBackground(tint);
        }

        applyCellEditableState(item);
    }

    bool TemplateSheetView::isCellLocked(const QTableWidgetItem* item)
    {
        return item && item->data(CellLockedRole).toBool();
    }

    void TemplateSheetView::applyCellEditableState(QTableWidgetItem* item) const
    {
        if (!item) return;

        const auto role = doclife::model::cellRoleFromKey(item->data(CellRoleKeyRole).toString());
        const bool editable = !isCellLocked(item) && doclife::model::cellRoleEditable(role);

        Qt::ItemFlags flags = item->flags();
        if (editable) flags |= Qt::ItemIsEditable;
        else flags &= ~Qt::ItemIsEditable;
        item->setFlags(flags);
    }

    void TemplateSheetView::onCellDoubleClicked(const QModelIndex& index)
    {
        if (!m_isProjectMode || !index.isValid()) return;

        auto* item = m_table->item(index.row(), index.column());
        if (!item) return;

        // 锁定的格子：先问一次是否解锁，解锁后按普通单元格编辑
        if (isCellLocked(item)) {
            DialogBase::show(this,
                QStringLiteral("单元格已锁定"),
                QStringLiteral("该单元格已锁定为只读，不参与工程信息同步等自动填充。\n解锁后即可编辑，是否解锁？"),
                [this, row = index.row(), col = index.column()]() {
                    CellSnapshot before = snapshotOf(row, col);
                    CellSnapshot after = before;
                    after.locked = false;
                    pushEdit({ before }, { after }, QStringLiteral("解锁单元格"));
                    if (auto* cell = m_table->item(row, col)) {
                        m_table->edit(m_table->model()->index(row, col));
                    }
                },
                nullptr);
            return;
        }

        const QString roleKey = item->data(Qt::UserRole + 20).toString();
        const auto role = doclife::model::cellRoleFromKey(roleKey);

        // 时间区域：双击只发全局信号，由主界面统一弹日期选择窗。
        // 不在双击事件里直接弹浮层——轻关闭浮层会把本次双击的按压当成“点外部”，刚弹出就被关掉。
        if (role == doclife::model::CellRole::Date) {
            QDate current = QDate::fromString(item->text(), QStringLiteral("yyyy年M月d日"));
            if (!current.isValid()) current = QDate::fromString(item->text(), QStringLiteral("yyyy-MM-dd"));
            emit UISignalManager::instance()->datePickRequested(
                m_currentSheetId, index.row(), index.column(),
                current.isValid() ? current : QDate::currentDate());
            return;
        }

        if (doclife::model::cellRoleEditable(role)) return;   // 正常可编辑，交给默认编辑

        // 用统一弹窗确认，风格与「保存确认」等保持一致
        DialogBase::show(this,
            QStringLiteral("只读单元格"),
            QStringLiteral("该单元格是只读的（灰色/黄色/蓝色底纹）。\n解锁后可以编辑，是否继续？"),
            [this, row = index.row(), col = index.column()]() {
                auto* cell = m_table->item(row, col);
                if (!cell) return;

                cell->setData(Qt::UserRole + 20,
                    doclife::model::cellRoleKey(doclife::model::CellRole::Input));
                cell->setFlags(cell->flags() | Qt::ItemIsEditable);
                cell->setBackground(QBrush());
                // 注意：不清除 UserRole+21 的占位符绑定。
                // 占位符原文是「这格该填什么」的字段级信息，随 sheet_cells.value 常驻数据库；
                // 用户解锁手填只是临时覆盖显示，点「刷新信息」仍按当前工程信息重算回填。

                // 只改本工程里的这份表格副本，不动模板
                auto* ctx = doclife::core::ProjectContext::instance();
                QSqlDatabase db = ctx->getProjectDb();
                if (db.isOpen()) {
                    QSqlQuery q(db);
                    q.prepare(QStringLiteral(
                        "UPDATE sheet_cells SET cell_role = 'input' WHERE sheet_id = ? AND row = ? AND col = ?"));
                    q.addBindValue(m_currentSheetId);
                    q.addBindValue(row);
                    q.addBindValue(col);
                    q.exec();
                }
                ctx->markDirty();

                m_table->edit(m_table->model()->index(row, col));
            },
            nullptr);
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
            if (auto* item = m_table->item(r, c)) {
                item->setText(q.value(2).toString());
                // 这格已经有保存过的内容：下次打开不再自动同步工程信息
                item->setData(CellSaveRole, true);
            }
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

        // 折行格子里输入长文本后，把行高撑到能放下
        autoGrowRow(row);

        // 改完这一格顺手把工具栏状态对齐（否则按钮可能还停在上一个格子的状态）
        updateFormatBarFromSelection();

        perfLog(QStringLiteral("单元格改动 row=%1 col=%2 字数=%3 折行=%4 行高=%5px")
            .arg(row).arg(col).arg(item->text().size())
            .arg(item->data(CellWrapRole).toBool() ? 1 : 0)
            .arg(m_table->rowHeight(row)));

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
        connect(m_formatBar, &SheetFormatBar::autoFitToggled, this, &TemplateSheetView::onAutoFitToggled);
        connect(m_formatBar, &SheetFormatBar::wrapToggled, this, &TemplateSheetView::onWrapToggled);
        connect(m_formatBar, &SheetFormatBar::lockToggled, this, &TemplateSheetView::onLockToggled);
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

            const QVariant wrapValue = item->data(CellWrapRole);
            snapshot.wrap = wrapValue.isValid() ? wrapValue.toBool() : true;
            snapshot.autoFit = item->data(CellAutoFitRole).toBool();
            snapshot.locked = item->data(CellLockedRole).toBool();
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
            if (s.hasFill) {
                item->setBackground(s.background);
            }
            else {
                // 无填充 = 回落到角色底色（文档里的约定）：
                // 取消填充不该把工程信息黄底、时间区域绿底这些一起抹掉
                item->setBackground(QBrush(cellRoleTint(item->data(CellRoleKeyRole).toString())));
            }
            item->setTextAlignment(Qt::Alignment(s.alignment));
            item->setData(CellWrapRole, s.wrap);
            item->setData(CellAutoFitRole, s.autoFit);
            item->setData(CellLockedRole, s.locked);
            // 锁定 / 只读角色决定这格能不能编辑，不能一律放开
            applyCellEditableState(item);

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
            format.wrap = s.wrap ? 1 : 0;
            format.autoFit = s.autoFit ? 1 : 0;
            format.locked = s.locked ? 1 : 0;
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
        m_formatBar->syncLayoutState(snapshot.wrap, snapshot.autoFit, snapshot.locked);
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
                auto* target = m_table->item(row, col);
                if (!target) continue;
                if (isCellLocked(target)) continue;   // 锁定格不接受粘贴

                CellSnapshot current = snapshotOf(row, col);
                CellSnapshot updated = current;
                updated.text = cells.at(cellIndex);
                before.append(current);
                after.append(updated);
            }
        }

        pushEdit(before, after, QStringLiteral("粘贴"));

        // 粘贴进来的长文本同样要把行高撑开（只处理受影响的非合并行）
        for (const CellSnapshot& s : after) {
            autoGrowRow(s.row);
        }
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

    void TemplateSheetView::onAutoFitToggled(bool on)
    {
        applyToSelection(QStringLiteral("自适应单元格大小"), [on](CellSnapshot& snapshot) {
            snapshot.autoFit = on;
            });
    }

    void TemplateSheetView::onWrapToggled(bool on)
    {
        int selectedCells = 0;
        const auto ranges = m_table->selectedRanges();
        for (const auto& range : ranges) {
            selectedCells += range.rowCount() * range.columnCount();
        }

        applyToSelection(QStringLiteral("折行显示"), [on](CellSnapshot& snapshot) {
            snapshot.wrap = on;
            });

        // 打开折行后把行高长到能放下，否则固定行高会把换出来的行裁掉，看着像没换行
        int grownRows = 0;
        if (on) {
            for (const auto& range : m_table->selectedRanges()) {
                for (int row = range.topRow(); row <= range.bottomRow(); ++row) {
                    const int before = m_table->rowHeight(row);
                    autoGrowRow(row);
                    if (m_table->rowHeight(row) != before) ++grownRows;
                }
            }
        }

        perfLog(QStringLiteral("折行=%1 选区=%2格 行高变化=%3行")
            .arg(on ? 1 : 0).arg(selectedCells).arg(grownRows));
    }

    void TemplateSheetView::onLockToggled(bool on)
    {
        applyToSelection(on ? QStringLiteral("锁定单元格") : QStringLiteral("解锁单元格"),
            [on](CellSnapshot& snapshot) {
                snapshot.locked = on;
            });
    }

} // namespace doclife::ui