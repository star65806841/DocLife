#pragma once

#include <QWidget>
#include <QString>
#include <QList>

class QLabel;
class QTableWidget;
class QTableWidgetItem;
class QAction;

namespace fluent::menus_toolbars { class CommandBar; }

namespace doclife::ui {

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
    };

} // namespace doclife::ui