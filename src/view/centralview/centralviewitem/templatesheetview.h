#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <QHash>
#include <QHashFunctions>

class QLabel;
class QTableWidget;
class QTableWidgetItem;
class QAction;
class QFont;
class QSqlDatabase;

namespace fluent::menus_toolbars { class CommandBar; }

namespace doclife::ui {

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

    class TemplateSheetView : public QWidget
    {
        Q_OBJECT
    public:
        explicit TemplateSheetView(QWidget* parent = nullptr);

        // 模板预览模式
        void loadSheet(const QString& dbPath, int sheetId, const QString& title);

        // 项目填报模式
        void loadProjectSheet(const QString& nodeId, const QString& dbPath, int sheetId, const QString& title);

        // 清空页面
        void clearView();

        // 把内存中的填报内容立即写入工程数据库（保存工程时由 ProjectContext 触发）
        void flushPendingEdits();

        // 丢弃尚未落盘的填报内容（关闭工程时调用）
        void clearPendingEdits();

    private slots:
        void onSectionResized(int logicalIndex, int oldSize, int newSize);
        void onItemChanged(QTableWidgetItem* item);
        void onHeaderSelectionChanged();
        void onResetTriggered();
        void onHorizontalHeaderClicked(int logicalIndex);
        void onVerticalHeaderClicked(int logicalIndex);

    private:
        void setupTemplate(const QString& dbPath, int sheetId);
        void applyUserSettings();
        void setMode(bool isProject);
        QString columnName(int col) const;

        // 填报模式：补齐模板里没有样式记录的空白单元格，保证整张表都能录入
        void ensureBlankCellsEditable(int rowCount, int colCount, const QFont& defaultFont);
        // 填报模式：叠加用户已经填写过的内容
        void applyFormValues(QSqlDatabase& db, int rowCount, int colCount);

        fluent::menus_toolbars::CommandBar* m_topBar = nullptr;
        QAction* m_titleAction = nullptr;
        QAction* m_resetAction = nullptr;

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
    };

} // namespace doclife::ui