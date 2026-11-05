#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <QHash>
#include <QHashFunctions>
#include <QFont>
#include <QColor>
#include <QVector>
#include <QTableWidget>
#include <QSet>

#include <functional>

class QLabel;
class QTableWidgetItem;
class QAction;
class QSqlDatabase;
class QUndoStack;

namespace fluent::menus_toolbars { class CommandBar; }

namespace doclife::ui {

    class SheetFormatBar;
    class SheetEditCommand;
    class SheetTextCommand;

    // 填报内容的缓存键：同一张工程表 + 同一行列只保留最后一次输入
    struct PendingCellKey {
        int sheetId = -1;
        int row = -1;
        int col = -1;

        bool operator==(const PendingCellKey& other) const
        {
            return sheetId == other.sheetId && row == other.row && col == other.col;
        }
    };

    inline size_t qHash(const PendingCellKey& key, size_t seed = 0) noexcept
    {
        return qHashMulti(seed, key.sheetId, key.row, key.col);
    }

    // 单元格的完整可视状态：撤销/重做靠它在“改前/改后”之间来回切换
    struct CellSnapshot {
        int row = 0;
        int col = 0;
        QString text;
        QFont font;
        QColor foreground;
        QColor background;
        bool hasFill = false;
        int alignment = 0;
        int rowSpan = 1;
        int colSpan = 1;
        // 排版与锁定：跟着单元格一起存进 sheet_cells
        bool wrap = true;      // 折行显示（关掉则单行 + 省略号）
        bool autoFit = false;  // 文字缩放到单元格大小
        bool locked = false;   // 锁定只读，且不参与自动赋值（同步工程信息、打勾等）

        bool operator==(const CellSnapshot& other) const
        {
            return row == other.row && col == other.col
                && text == other.text
                && font == other.font
                && foreground == other.foreground
                && hasFill == other.hasFill
                && background == other.background
                && alignment == other.alignment
                && rowSpan == other.rowSpan
                && colSpan == other.colSpan
                && wrap == other.wrap
                && autoFit == other.autoFit
                && locked == other.locked;
        }
        bool operator!=(const CellSnapshot& other) const { return !(*this == other); }
    };

    // 待写入 sheet_cells 的单元格格式
    struct PendingCellFormat {
        QString fontFamily;
        qreal   fontSize = 0;
        int     bold = 0;
        int     italic = 0;
        int     underline = 0;
        QString fontColor;
        QString fillColor;   // 空字符串表示无填充
        int     hAlign = 0;
        int     vAlign = 1;
        int     rowSpan = 1;
        int     colSpan = 1;
        int     wrap = 1;
        int     autoFit = 0;
        int     locked = 0;

        bool operator==(const PendingCellFormat& other) const
        {
            return fontFamily == other.fontFamily && fontSize == other.fontSize
                && bold == other.bold && italic == other.italic && underline == other.underline
                && fontColor == other.fontColor && fillColor == other.fillColor
                && hAlign == other.hAlign && vAlign == other.vAlign
                && rowSpan == other.rowSpan && colSpan == other.colSpan
                && wrap == other.wrap && autoFit == other.autoFit && locked == other.locked;
        }
    };

    class TemplateSheetView : public QWidget
    {
        Q_OBJECT
    public:
        explicit TemplateSheetView(QWidget* parent = nullptr);
        ~TemplateSheetView() override;

        // 模板预览模式
        void loadSheet(const QString& dbPath, int sheetId, const QString& title);

        // 项目填报模式
        void loadProjectSheet(const QString& nodeId, const QString& dbPath, int sheetId, const QString& title);

        // 清空页面
        void clearView();

        // 把内存中的填报内容与格式立即写入工程数据库（保存工程时由 ProjectContext 触发）
        void flushPendingEdits();

        // 丢弃尚未落盘的内容（关闭工程时调用）
        void clearPendingEdits();

    private slots:
        void onSectionResized(int logicalIndex, int oldSize, int newSize);
        void onItemChanged(QTableWidgetItem* item);
        void onHeaderSelectionChanged();
        void onResetTriggered();
        void onHorizontalHeaderClicked(int logicalIndex);
        void onVerticalHeaderClicked(int logicalIndex);
        // 只读单元格双击 → 确认后解锁编辑
        void onCellDoubleClicked(const QModelIndex& index);
        // 一键勾选/取消整张表的方格（「不合格」前面的方格不勾）
        void onCheckAllBoxesToggled(bool checked);

        // 编辑区命令
        void onSelectionChanged();
        void onUndoTriggered();
        void onRedoTriggered();
        void onCut();
        void onCopy();
        void onPaste();
        void onMergeCells();
        void onFontFamilyChanged(const QString& family);
        void onFontSizeChanged(int pointSize);
        void onBoldToggled(bool on);
        void onItalicToggled(bool on);
        void onUnderlineToggled(bool on);
        void onFontColorChanged(const QColor& color);
        void onFillColorChanged(const QColor& color);
        void onHAlignChanged(int align);
        void onVAlignChanged(int align);
        void onAutoFitToggled(bool on);
        void onWrapToggled(bool on);
        void onLockToggled(bool on);

    private:
        friend class SheetEditCommand;
        friend class SheetTextCommand;

        void setupTemplate(const QString& dbPath, int sheetId);
        void applyUserSettings();
        void setMode(bool isProject);
        QString columnName(int col) const;

        // 填报模式：补齐模板里没有样式记录的空白单元格，保证整张表都能录入
        void ensureBlankCellsEditable(int rowCount, int colCount, const QFont& defaultFont);
        // 填报模式：叠加用户已经填写过的内容
        void applyFormValues(QSqlDatabase& db, int rowCount, int colCount);
        // 按角色设置只读/底色（灰色只读、黄色工程信息、蓝色汇总）
        void applyCellRole(QTableWidgetItem* item, const QString& roleKey);
        // 按「角色 + 锁定」统一决定这格能不能编辑
        void applyCellEditableState(QTableWidgetItem* item) const;
        // 该格是否被用户锁定（锁定后不吃自动赋值）
        static bool isCellLocked(const QTableWidgetItem* item);
        // 用当前工程/节点信息刷新表格里的 #字段 占位符
        void refreshFieldValues();

        // ---- 编辑区 ----
        void setupFormatBar();
        void updateFormatBarFromSelection();
        CellSnapshot snapshotOf(int row, int col) const;
        QVector<CellSnapshot> snapshotRanges(const QList<QTableWidgetSelectionRange>& ranges) const;
        void applySnapshots(const QVector<CellSnapshot>& snapshots);
        void pushEdit(const QVector<CellSnapshot>& before,
                      const QVector<CellSnapshot>& after,
                      const QString& label);
        // 只改单元格文本（不碰字体/底色），供勾选方格这类纯文字开关使用
        void setCellTextOnly(int row, int col, const QString& text);
        // 让「勾选方格」按钮的按下状态与表里实际的 ☑ 一致（不触发信号）
        void syncCheckBoxesAction();
        // 折行打开后，把行高长到能放下换行后的文字（合并行跳过）
        void autoGrowRow(int row);
        void applyToSelection(const QString& label, const std::function<void(CellSnapshot&)>& mutate);
        QString selectionAsText() const;
        void pasteText(const QString& text);

        fluent::menus_toolbars::CommandBar* m_topBar = nullptr;
        SheetFormatBar* m_formatBar = nullptr;
        QUndoStack* m_undoStack = nullptr;

        QAction* m_titleAction = nullptr;
        QAction* m_resetAction = nullptr;
        QAction* m_refreshAction = nullptr;
        QAction* m_checkBoxesAction = nullptr;

        QTableWidget* m_table = nullptr;

        bool m_isProjectMode = false;
        QString m_currentNodeId;
        int m_currentSheetId = -1;
        QString m_currentDbPath;

        bool m_isApplyingSettings = false;

        QList<int> m_defaultColWidths;
        QList<int> m_defaultRowHeights;

        // 尚未落盘的填报内容：(填报页id, 行, 列) -> 文本
        QHash<PendingCellKey, QString> m_pendingEdits;
        // 尚未落盘的单元格格式
        QHash<PendingCellKey, PendingCellFormat> m_pendingFormats;
        // 当前表里所有合并块的左上角（QTableView 无法可靠区分锚点与被覆盖的单元格）
        QSet<quint64> m_spanAnchors;
    };

} // namespace doclife::ui
