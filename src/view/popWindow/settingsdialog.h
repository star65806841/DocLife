#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include <components/dialogs_flyouts/Dialog.h>

class QLabel;

namespace fluent::basicinput {
    class ToggleButton;
    class ColorPicker;
}
namespace fluent::navigation {
    class NavigationView;
    class StackContentHost;
}

namespace doclife::ui {

    // 编辑区外观设置（持久化在 QSettings）
    struct EditorSettings {
        bool   selectionEnabled = true;
        QColor selectionColor = QColor(0x00, 0x78, 0xD4);
    };

    EditorSettings loadEditorSettings();
    void saveEditorSettings(const EditorSettings& settings);

    // 表格样式（含选中态），供 TemplateSheetView 直接 setStyleSheet
    QString editorTableStyleSheet(const EditorSettings& settings = loadEditorSettings());

    /**
     * 设置弹窗：左侧导航 + 右侧页面，UI 结构与「工程信息」弹窗保持一致
     * （NavigationView + StackContentHost + MainSection）。
     * 页面：编辑区域设置（选中背景开关 + 自定义背景色）、诊断日志（一键导出）。
     */
    class SettingsDialog : public fluent::dialogs_flyouts::Dialog
    {
        Q_OBJECT
    public:
        explicit SettingsDialog(QWidget* parent = nullptr);

    private:
        QWidget* buildEditorPage();
        QWidget* buildDiagnosticsPage();
        void applySettings();
        void updateColorEnabled();

        fluent::basicinput::ToggleButton* m_selectionToggle = nullptr;
        fluent::basicinput::ColorPicker* m_colorPicker = nullptr;
        QLabel* m_colorLabel = nullptr;
    };

} // namespace doclife::ui
