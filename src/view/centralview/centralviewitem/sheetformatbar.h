#pragma once

#include <QWidget>
#include <QColor>
#include <QFont>

class QToolButton;
class QComboBox;
class QFontComboBox;

namespace doclife::ui {

    /**
     * 表格填报的格式工具栏，布局与交互对齐 Excel：
     * 撤销/重做 · 剪切/复制/粘贴 · 字体/字号 · 加粗/斜体/下划线 ·
     * 字体颜色/填充颜色 · 横向/纵向对齐 · 合并居中。
     */
    class SheetFormatBar : public QWidget
    {
        Q_OBJECT
    public:
        explicit SheetFormatBar(QWidget* parent = nullptr);

        // 撤销/重做按钮的可用状态
        void setUndoRedoEnabled(bool canUndo, bool canRedo);
        // 合并按钮的按下状态
        void setMergeChecked(bool merged);

        // 用当前选区回填控件状态（内部屏蔽信号，不会反过来触发修改）
        void syncState(const QFont& font,
                       const QColor& fontColor,
                       bool hasFill,
                       const QColor& fillColor,
                       int hAlign,
                       int vAlign);
        // 排版/锁定按钮的按下状态：折行、自适应、锁定
        void syncLayoutState(bool wrap, bool autoFit, bool locked);

    signals:
        void undoRequested();
        void redoRequested();
        void cutRequested();
        void copyRequested();
        void pasteRequested();
        void mergeRequested();

        void fontFamilyChanged(const QString& family);
        void fontSizeChanged(int pointSize);
        void boldToggled(bool on);
        void italicToggled(bool on);
        void underlineToggled(bool on);
        void fontColorChanged(const QColor& color);
        void fillColorChanged(const QColor& color);   // 无效颜色 = 无填充
        void hAlignChanged(int align);                // 0 左 / 1 中 / 2 右
        void vAlignChanged(int align);                // 0 上 / 1 中 / 2 下

        void autoFitToggled(bool on);                 // 自适应单元格大小
        void wrapToggled(bool on);                    // 折行显示
        void lockToggled(bool on);                    // 锁定/解锁单元格

    private:
        QToolButton* makeButton(const QString& glyphName, const QString& tooltip, bool checkable = false);
        void refreshColorIcons();
        void showColorMenu(QToolButton* anchor, bool fill);

        QToolButton* m_undo = nullptr;
        QToolButton* m_redo = nullptr;
        QToolButton* m_cut = nullptr;
        QToolButton* m_copy = nullptr;
        QToolButton* m_paste = nullptr;
        QToolButton* m_bold = nullptr;
        QToolButton* m_italic = nullptr;
        QToolButton* m_underline = nullptr;
        QToolButton* m_fontColor = nullptr;
        QToolButton* m_fillColor = nullptr;
        QToolButton* m_hLeft = nullptr;
        QToolButton* m_hCenter = nullptr;
        QToolButton* m_hRight = nullptr;
        QToolButton* m_vTop = nullptr;
        QToolButton* m_vMiddle = nullptr;
        QToolButton* m_vBottom = nullptr;
        QToolButton* m_merge = nullptr;
        QToolButton* m_autoFit = nullptr;
        QToolButton* m_wrap = nullptr;
        QToolButton* m_lock = nullptr;

        QFontComboBox* m_fontFamily = nullptr;
        QComboBox* m_fontSize = nullptr;

        QColor m_currentFontColor = QColor(QStringLiteral("#000000"));
        QColor m_currentFillColor = QColor(QStringLiteral("#FFFFFF"));
        bool   m_hasFill = false;
    };

} // namespace doclife::ui
