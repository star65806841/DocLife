#include "excelcelldelegate.h"
#include "excelsheetmodel.h"

#include <QPainter>
#include <QAbstractItemModel>
#include <QTextOption>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QFont>

#include "xlsxformat.h"

namespace doclife::ui {

    namespace {

        Qt::Alignment toHAlign(QXlsx::Format::HorizontalAlignment a)
        {
            switch (a) {
            case QXlsx::Format::AlignHCenter:      return Qt::AlignHCenter;
            case QXlsx::Format::AlignRight:        return Qt::AlignRight;
            case QXlsx::Format::AlignHJustify:     return Qt::AlignJustify;
            case QXlsx::Format::AlignHDistributed: return Qt::AlignJustify;
            default:                               return Qt::AlignLeft;
            }
        }

        Qt::Alignment toVAlign(QXlsx::Format::VerticalAlignment a)
        {
            switch (a) {
            case QXlsx::Format::AlignTop:          return Qt::AlignTop;
            case QXlsx::Format::AlignBottom:       return Qt::AlignBottom;
            case QXlsx::Format::AlignVJustify:     return Qt::AlignTop;
            case QXlsx::Format::AlignVDistributed: return Qt::AlignTop;
            default:                               return Qt::AlignVCenter;
            }
        }

        QColor resolveFillColor(const QXlsx::Format& fmt)
        {
            if (!fmt.hasFillData()) return QColor();

            if (fmt.fillPattern() == QXlsx::Format::PatternSolid) {
                const QColor fg = fmt.patternForegroundColor();
                if (fg.isValid()) return fg;
                const QColor bg = fmt.patternBackgroundColor();
                if (bg.isValid()) return bg;
                return QColor();
            }
            if (fmt.fillPattern() == QXlsx::Format::PatternNone)
                return QColor();

            const QColor bg = fmt.patternBackgroundColor();
            if (bg.isValid() && bg != Qt::white) return bg;
            const QColor fg = fmt.patternForegroundColor();
            if (fg.isValid() && fg != Qt::white) return fg;
            return QColor();
        }

        int borderWidthFromStyle(QXlsx::Format::BorderStyle style)
        {
            switch (style) {
            case QXlsx::Format::BorderThin:   return 1;
            case QXlsx::Format::BorderMedium: return 2;
            case QXlsx::Format::BorderThick:  return 3;
            case QXlsx::Format::BorderHair:   return 1;
            case QXlsx::Format::BorderDouble: return 3;
            default:                          return 0;
            }
        }

        Qt::PenStyle penStyleFromBorder(QXlsx::Format::BorderStyle style)
        {
            switch (style) {
            case QXlsx::Format::BorderDashed:           return Qt::DashLine;
            case QXlsx::Format::BorderDotted:           return Qt::DotLine;
            case QXlsx::Format::BorderMediumDashed:     return Qt::DashLine;
            case QXlsx::Format::BorderDashDot:          return Qt::DashDotLine;
            case QXlsx::Format::BorderMediumDashDot:    return Qt::DashDotLine;
            case QXlsx::Format::BorderDashDotDot:       return Qt::DashDotDotLine;
            case QXlsx::Format::BorderMediumDashDotDot: return Qt::DashDotDotLine;
            case QXlsx::Format::BorderSlantDashDot:     return Qt::DashDotLine;
            case QXlsx::Format::BorderDouble:           return Qt::SolidLine;
            default:                                    return Qt::SolidLine;
            }
        }

        // 按位置决定绘制哪些边，避免相邻格重叠：
        //   第一行  -> 上、右、下
        //   第一列  -> 左、右、下
        //   其余    -> 右、下
        // 每条边都只在 Excel 里设置了才真正画
        void drawBorders(QPainter* p, const QRect& rect, const QXlsx::Format& fmt,
            int row, int col)
        {
            auto drawSide = [&](QXlsx::Format::BorderStyle style,
                const QColor& color,
                QPoint p1, QPoint p2) {
                    if (style == QXlsx::Format::BorderNone) return;
                    const int w = borderWidthFromStyle(style);
                    if (w <= 0) return;
                    QPen pen(color.isValid() ? color : QColor(Qt::black));
                    pen.setWidth(w);
                    pen.setStyle(penStyleFromBorder(style));
                    pen.setCapStyle(Qt::FlatCap);
                    p->setPen(pen);
                    p->drawLine(p1, p2);
                };

            if (row == 0)
                drawSide(fmt.topBorderStyle(), fmt.topBorderColor(),
                    rect.topLeft(), rect.topRight());
            if (col == 0)
                drawSide(fmt.leftBorderStyle(), fmt.leftBorderColor(),
                    rect.topLeft(), rect.bottomLeft());

            drawSide(fmt.rightBorderStyle(), fmt.rightBorderColor(),
                rect.topRight(), rect.bottomRight());
            drawSide(fmt.bottomBorderStyle(), fmt.bottomBorderColor(),
                rect.bottomLeft(), rect.bottomRight());
        }

    } // namespace

    ExcelCellDelegate::ExcelCellDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
        m_textDoc.setDocumentMargin(0);
    }

    void ExcelCellDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const
    {
        painter->save();
        painter->setClipRect(option.rect);

        auto* model = qobject_cast<const detail::ExcelSheetModel*>(index.model());
        QXlsx::Format fmt;
        if (model)
            fmt = model->cellFormat(index.row(), index.column());

        // 1) 背景填充
        const QColor bg = resolveFillColor(fmt);
        if (bg.isValid())
            painter->fillRect(option.rect, bg);

        // 2) 边框：只有 Excel 里设了边框的格才画
        if (fmt.hasBorderData()) {
            painter->setRenderHint(QPainter::Antialiasing, false);
            drawBorders(painter, option.rect, fmt, index.row(), index.column());
        }

        // 3) 字体
        QFont font = option.font;
        if (fmt.hasFontData()) {
            if (!fmt.fontName().isEmpty()) font.setFamily(fmt.fontName());
            if (fmt.fontSize() > 0)        font.setPointSize(fmt.fontSize());
            font.setBold(fmt.fontBold());
            font.setItalic(fmt.fontItalic());
            font.setStrikeOut(fmt.fontStrikeOut());
            if (fmt.fontUnderline() != QXlsx::Format::FontUnderlineNone)
                font.setUnderline(true);
        }
        painter->setFont(font);

        // 4) 文字颜色
        QColor fg = Qt::black;
        if (fmt.hasFontData() && fmt.fontColor().isValid())
            fg = fmt.fontColor();

        // 5) 对齐与换行
        Qt::Alignment halign = Qt::AlignLeft;
        Qt::Alignment valign = Qt::AlignVCenter;
        bool wrap = false;
        if (fmt.hasAlignmentData()) {
            halign = toHAlign(fmt.horizontalAlignment());
            valign = toVAlign(fmt.verticalAlignment());
            wrap = fmt.textWrap();
        }

        // 6) 绘制文字
        const QString text = index.data(Qt::DisplayRole).toString();
        if (!text.isEmpty()) {
            const QRect content = option.rect.adjusted(3, 2, -3, -2);

            if (wrap) {
                m_textDoc.clear();
                m_textDoc.setDefaultFont(font);

                QTextOption opt;
                opt.setAlignment(halign);
                opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
                m_textDoc.setDefaultTextOption(opt);

                QTextCharFormat charFmt;
                charFmt.setForeground(fg);
                QTextCursor cur(&m_textDoc);
                cur.insertText(text, charFmt);

                m_textDoc.setTextWidth(content.width());

                const int contentH = static_cast<int>(m_textDoc.size().height());
                int dy = 0;
                if (valign & Qt::AlignVCenter)
                    dy = qMax(0, (content.height() - contentH) / 2);
                else if (valign & Qt::AlignBottom)
                    dy = qMax(0, content.height() - contentH);

                painter->translate(content.left(), content.top() + dy);
                m_textDoc.drawContents(
                    painter,
                    QRectF(0, 0, content.width(), qMin(content.height() - dy, contentH)));
            }
            else {
                painter->setPen(fg);
                painter->drawText(content, halign | valign, text);
            }
        }

        painter->restore();
    }

} // namespace doclife::ui