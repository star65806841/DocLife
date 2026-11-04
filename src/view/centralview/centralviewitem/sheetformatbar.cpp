#include "sheetformatbar.h"

#include <QComboBox>
#include <QColorDialog>
#include <QFontComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QToolButton>
#include <QWidgetAction>

#include <design/Typography.h>

namespace doclife::ui {

    namespace {

        const QColor kIconColor(0x32, 0x31, 0x30);

        QIcon glyphIcon(const QString& name)
        {
            const int px = 16;
            QPixmap pm(px * 2, px * 2);
            pm.setDevicePixelRatio(2.0);
            pm.fill(Qt::transparent);

            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(kIconColor);
            Typography::Icons::paintGlyph(p, QRectF(0, 0, px, px), name, px, Qt::AlignCenter);
            p.end();
            return QIcon(pm);
        }

        // 颜色按钮：上半部分是图标，下半部分是当前颜色的色条（Excel 的 A/填充桶样式）
        QIcon colorGlyphIcon(const QString& name, const QColor& color, bool showBar)
        {
            const int px = 20;
            QPixmap pm(px * 2, px * 2);
            pm.setDevicePixelRatio(2.0);
            pm.fill(Qt::transparent);

            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(kIconColor);
            Typography::Icons::paintGlyph(p, QRectF(0, 0, px, px - 4), name, 16, Qt::AlignCenter);

            if (showBar && color.isValid()) {
                const QRectF bar(3, px - 4, px - 6, 3);
                p.fillRect(bar, color);
                p.setPen(QColor(0x8A, 0x88, 0x86));
                p.drawRect(bar);
            }
            p.end();
            return QIcon(pm);
        }

        QFrame* separator(QWidget* parent)
        {
            auto* line = new QFrame(parent);
            line->setFrameShape(QFrame::VLine);
            line->setFrameShadow(QFrame::Plain);
            line->setFixedHeight(18);
            line->setStyleSheet(QStringLiteral("color:#DDDDDD;"));
            return line;
        }

        const QStringList& paletteColors()
        {
            static const QStringList colors = {
                "#FFFFFF", "#000000", "#E7E6E6", "#44546A", "#4472C4",
                "#ED7D31", "#A5A5A5", "#FFC000", "#5B9BD5", "#70AD47",
                "#F2F2F2", "#808080", "#D9D9D9", "#262626", "#2F5597",
                "#C55A11", "#7F7F7F", "#BF9000", "#2E75B6", "#548235",
                "#D9E2F3", "#FBE5D6", "#EDEDED", "#DEEAF6", "#E2F0D9",
                "#FFF2CC", "#FCE4D6", "#E4DFEC", "#DDEBF7", "#E2EFDA",
            };
            return colors;
        }

    } // namespace

    SheetFormatBar::SheetFormatBar(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("SheetFormatBar"));
        setFixedHeight(38);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(2);

        // ---- 撤销 / 重做 ----
        m_undo = makeButton(QStringLiteral("ic_fluent_arrow_undo_20_regular"), QStringLiteral("撤销 (Ctrl+Z)"));
        m_redo = makeButton(QStringLiteral("ic_fluent_arrow_redo_20_regular"), QStringLiteral("重做 (Ctrl+Y)"));
        connect(m_undo, &QToolButton::clicked, this, &SheetFormatBar::undoRequested);
        connect(m_redo, &QToolButton::clicked, this, &SheetFormatBar::redoRequested);
        layout->addWidget(m_undo);
        layout->addWidget(m_redo);
        layout->addWidget(separator(this));

        // ---- 剪切 / 复制 / 粘贴 ----
        m_cut = makeButton(QStringLiteral("ic_fluent_cut_20_regular"), QStringLiteral("剪切 (Ctrl+X)"));
        m_copy = makeButton(QStringLiteral("ic_fluent_copy_20_regular"), QStringLiteral("复制 (Ctrl+C)"));
        m_paste = makeButton(QStringLiteral("ic_fluent_clipboard_paste_20_regular"), QStringLiteral("粘贴 (Ctrl+V)"));
        connect(m_cut, &QToolButton::clicked, this, &SheetFormatBar::cutRequested);
        connect(m_copy, &QToolButton::clicked, this, &SheetFormatBar::copyRequested);
        connect(m_paste, &QToolButton::clicked, this, &SheetFormatBar::pasteRequested);
        layout->addWidget(m_cut);
        layout->addWidget(m_copy);
        layout->addWidget(m_paste);
        layout->addWidget(separator(this));

        // ---- 字体 / 字号 ----
        m_fontFamily = new QFontComboBox(this);
        m_fontFamily->setFixedHeight(26);
        m_fontFamily->setMinimumWidth(132);
        m_fontFamily->setMaximumWidth(180);
        m_fontFamily->setToolTip(QStringLiteral("字体"));
        connect(m_fontFamily, &QFontComboBox::currentFontChanged, this,
            [this](const QFont& font) {
                emit fontFamilyChanged(font.family());
            });
        layout->addWidget(m_fontFamily);

        m_fontSize = new QComboBox(this);
        m_fontSize->setFixedHeight(26);
        m_fontSize->setFixedWidth(64);
        m_fontSize->setEditable(true);
        m_fontSize->setInsertPolicy(QComboBox::NoInsert);
        m_fontSize->setToolTip(QStringLiteral("字号"));
        for (int size : { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36, 48, 72 }) {
            m_fontSize->addItem(QString::number(size));
        }
        connect(m_fontSize, &QComboBox::currentTextChanged, this,
            [this](const QString& text) {
                bool ok = false;
                const int size = text.toInt(&ok);
                if (ok && size > 0 && size < 200) emit fontSizeChanged(size);
            });
        layout->addWidget(m_fontSize);
        layout->addWidget(separator(this));

        // ---- 加粗 / 斜体 / 下划线 ----
        m_bold = makeButton(QStringLiteral("ic_fluent_text_bold_20_regular"), QStringLiteral("加粗 (Ctrl+B)"), true);
        m_italic = makeButton(QStringLiteral("ic_fluent_text_italic_20_regular"), QStringLiteral("斜体 (Ctrl+I)"), true);
        m_underline = makeButton(QStringLiteral("ic_fluent_text_underline_20_regular"), QStringLiteral("下划线 (Ctrl+U)"), true);
        connect(m_bold, &QToolButton::toggled, this, &SheetFormatBar::boldToggled);
        connect(m_italic, &QToolButton::toggled, this, &SheetFormatBar::italicToggled);
        connect(m_underline, &QToolButton::toggled, this, &SheetFormatBar::underlineToggled);
        layout->addWidget(m_bold);
        layout->addWidget(m_italic);
        layout->addWidget(m_underline);
        layout->addWidget(separator(this));

        // ---- 字体颜色 / 填充颜色 ----
        m_fontColor = makeButton(QStringLiteral("ic_fluent_text_color_20_regular"), QStringLiteral("字体颜色"));
        m_fillColor = makeButton(QStringLiteral("ic_fluent_paint_bucket_20_regular"), QStringLiteral("填充颜色"));
        connect(m_fontColor, &QToolButton::clicked, this, [this]() { showColorMenu(m_fontColor, false); });
        connect(m_fillColor, &QToolButton::clicked, this, [this]() { showColorMenu(m_fillColor, true); });
        layout->addWidget(m_fontColor);
        layout->addWidget(m_fillColor);
        layout->addWidget(separator(this));

        // ---- 横向对齐 ----
        m_hLeft = makeButton(QStringLiteral("ic_fluent_text_align_left_20_regular"), QStringLiteral("左对齐"), true);
        m_hCenter = makeButton(QStringLiteral("ic_fluent_text_align_center_20_regular"), QStringLiteral("居中对齐"), true);
        m_hRight = makeButton(QStringLiteral("ic_fluent_text_align_right_20_regular"), QStringLiteral("右对齐"), true);
        connect(m_hLeft, &QToolButton::clicked, this, [this]() { emit hAlignChanged(0); });
        connect(m_hCenter, &QToolButton::clicked, this, [this]() { emit hAlignChanged(1); });
        connect(m_hRight, &QToolButton::clicked, this, [this]() { emit hAlignChanged(2); });
        layout->addWidget(m_hLeft);
        layout->addWidget(m_hCenter);
        layout->addWidget(m_hRight);

        // ---- 纵向对齐 ----
        m_vTop = makeButton(QStringLiteral("ic_fluent_align_top_20_regular"), QStringLiteral("顶端对齐"), true);
        m_vMiddle = makeButton(QStringLiteral("ic_fluent_align_center_vertical_20_regular"), QStringLiteral("垂直居中"), true);
        m_vBottom = makeButton(QStringLiteral("ic_fluent_align_bottom_20_regular"), QStringLiteral("底端对齐"), true);
        connect(m_vTop, &QToolButton::clicked, this, [this]() { emit vAlignChanged(0); });
        connect(m_vMiddle, &QToolButton::clicked, this, [this]() { emit vAlignChanged(1); });
        connect(m_vBottom, &QToolButton::clicked, this, [this]() { emit vAlignChanged(2); });
        layout->addWidget(m_vTop);
        layout->addWidget(m_vMiddle);
        layout->addWidget(m_vBottom);
        layout->addWidget(separator(this));

        // ---- 合并居中 ----
        m_merge = makeButton(QStringLiteral("ic_fluent_table_cells_merge_20_regular"), QStringLiteral("合并居中 / 取消合并"), true);
        connect(m_merge, &QToolButton::clicked, this, &SheetFormatBar::mergeRequested);
        layout->addWidget(m_merge);

        layout->addStretch(1);

        setStyleSheet(QStringLiteral(R"(
            #SheetFormatBar {
                background: #F7F7F7;
                border-bottom: 1px solid #E3E3E3;
            }
            #SheetFormatBar QToolButton {
                border: 1px solid transparent;
                border-radius: 3px;
                background: transparent;
            }
            #SheetFormatBar QToolButton:hover {
                background: #E5F1FB;
                border-color: #B7D6F0;
            }
            #SheetFormatBar QToolButton:checked {
                background: #CDE7F8;
                border-color: #7FB8E0;
            }
            #SheetFormatBar QToolButton:pressed {
                background: #B5D9F2;
            }
            #SheetFormatBar QComboBox, #SheetFormatBar QFontComboBox {
                border: 1px solid #D8D8D8;
                border-radius: 3px;
                padding-left: 6px;
                background: #FFFFFF;
            }
        )"));

        refreshColorIcons();
    }

    QToolButton* SheetFormatBar::makeButton(const QString& glyphName, const QString& tooltip, bool checkable)
    {
        auto* button = new QToolButton(this);
        button->setAutoRaise(true);
        button->setCheckable(checkable);
        button->setToolTip(tooltip);
        button->setIcon(glyphIcon(glyphName));
        button->setIconSize(QSize(16, 16));
        button->setFixedSize(30, 28);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    }

    void SheetFormatBar::refreshColorIcons()
    {
        m_fontColor->setIcon(colorGlyphIcon(QStringLiteral("ic_fluent_text_color_20_regular"),
            m_currentFontColor, true));
        m_fillColor->setIcon(colorGlyphIcon(QStringLiteral("ic_fluent_paint_bucket_20_regular"),
            m_currentFillColor, m_hasFill));
    }

    void SheetFormatBar::setUndoRedoEnabled(bool canUndo, bool canRedo)
    {
        m_undo->setEnabled(canUndo);
        m_redo->setEnabled(canRedo);
    }

    void SheetFormatBar::setMergeChecked(bool merged)
    {
        const QSignalBlocker blocker(m_merge);
        m_merge->setChecked(merged);
    }

    void SheetFormatBar::syncState(const QFont& font,
                                   const QColor& fontColor,
                                   bool hasFill,
                                   const QColor& fillColor,
                                   int hAlign,
                                   int vAlign)
    {
        {
            const QSignalBlocker b1(m_fontFamily);
            m_fontFamily->setCurrentFont(font);
        }
        {
            const QSignalBlocker b2(m_fontSize);
            m_fontSize->setCurrentText(QString::number(font.pointSize() > 0 ? font.pointSize() : 11));
        }
        {
            const QSignalBlocker b3(m_bold);
            const QSignalBlocker b4(m_italic);
            const QSignalBlocker b5(m_underline);
            m_bold->setChecked(font.bold());
            m_italic->setChecked(font.italic());
            m_underline->setChecked(font.underline());
        }
        {
            const QSignalBlocker b6(m_hLeft);
            const QSignalBlocker b7(m_hCenter);
            const QSignalBlocker b8(m_hRight);
            m_hLeft->setChecked(hAlign == 0);
            m_hCenter->setChecked(hAlign == 1);
            m_hRight->setChecked(hAlign == 2);
        }
        {
            const QSignalBlocker b9(m_vTop);
            const QSignalBlocker b10(m_vMiddle);
            const QSignalBlocker b11(m_vBottom);
            m_vTop->setChecked(vAlign == 0);
            m_vMiddle->setChecked(vAlign == 1);
            m_vBottom->setChecked(vAlign == 2);
        }

        if (fontColor.isValid()) m_currentFontColor = fontColor;
        m_hasFill = hasFill;
        if (fillColor.isValid()) m_currentFillColor = fillColor;
        refreshColorIcons();
    }

    void SheetFormatBar::showColorMenu(QToolButton* anchor, bool fill)
    {
        QMenu menu(this);

        auto* grid = new QWidget(&menu);
        auto* layout = new QGridLayout(grid);
        layout->setContentsMargins(8, 8, 8, 4);
        layout->setSpacing(2);

        int index = 0;
        for (const QString& hex : paletteColors()) {
            auto* swatch = new QToolButton(grid);
            swatch->setFixedSize(18, 18);
            swatch->setCursor(Qt::PointingHandCursor);
            swatch->setToolTip(hex);
            swatch->setStyleSheet(QStringLiteral(
                "QToolButton { background:%1; border:1px solid #C8C6C4; }"
                "QToolButton:hover { border:1px solid #0078D4; }").arg(hex));

            const QColor color(hex);
            connect(swatch, &QToolButton::clicked, &menu, [this, &menu, color, fill]() {
                if (fill) {
                    m_hasFill = true;
                    m_currentFillColor = color;
                    emit fillColorChanged(color);
                }
                else {
                    m_currentFontColor = color;
                    emit fontColorChanged(color);
                }
                refreshColorIcons();
                menu.close();
                });

            layout->addWidget(swatch, index / 10, index % 10);
            ++index;
        }
        const int rows = (index + 9) / 10;

        auto* clear = new QToolButton(&menu);
        clear->setText(fill ? QStringLiteral("无填充") : QStringLiteral("自动"));
        clear->setToolButtonStyle(Qt::ToolButtonTextOnly);
        clear->setAutoRaise(true);
        connect(clear, &QToolButton::clicked, &menu, [this, &menu, fill]() {
            if (fill) {
                m_hasFill = false;
                emit fillColorChanged(QColor());
            }
            else {
                m_currentFontColor = QColor(Qt::black);
                emit fontColorChanged(m_currentFontColor);
            }
            refreshColorIcons();
            menu.close();
            });
        layout->addWidget(clear, rows, 0, 1, 4);

        auto* more = new QToolButton(&menu);
        more->setText(QStringLiteral("其他颜色..."));
        more->setToolButtonStyle(Qt::ToolButtonTextOnly);
        more->setAutoRaise(true);
        connect(more, &QToolButton::clicked, &menu, [this, &menu, fill]() {
            menu.close();
            const QColor pick = QColorDialog::getColor(
                fill ? m_currentFillColor : m_currentFontColor, this, QStringLiteral("选择颜色"));
            if (!pick.isValid()) return;
            if (fill) {
                m_hasFill = true;
                m_currentFillColor = pick;
                emit fillColorChanged(pick);
            }
            else {
                m_currentFontColor = pick;
                emit fontColorChanged(pick);
            }
            refreshColorIcons();
            });
        layout->addWidget(more, rows + 1, 0, 1, 10);

        auto* action = new QWidgetAction(&menu);
        action->setDefaultWidget(grid);
        menu.addAction(action);

        menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
    }

} // namespace doclife::ui
