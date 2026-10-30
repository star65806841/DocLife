#include "templatesheetview.h"

#include <QVBoxLayout>
#include <QLabel>
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

namespace doclife::ui {

    // ============================================================================
    // 自定义 Delegate：画线
    // ============================================================================
    class CellBorderDelegate : public QStyledItemDelegate
    {
    public:
        explicit CellBorderDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent) {
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option,
            const QModelIndex& index) const override
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
            QPoint bl(option.rect.left(), option.rect.bottom() + 1);
            QPoint br(option.rect.right() + 1, option.rect.bottom() + 1);

            if (top > 0) {
                pen.setWidth(top > 1 ? 2 : 1);
                painter->setPen(pen);
                painter->drawLine(tl, tr);
            }
            if (bottom > 0) {
                pen.setWidth(bottom > 1 ? 2 : 1);
                painter->setPen(pen);
                painter->drawLine(bl, br);
            }
            if (left > 0) {
                pen.setWidth(left > 1 ? 2 : 1);
                painter->setPen(pen);
                painter->drawLine(tl, bl);
            }
            if (right > 0) {
                pen.setWidth(right > 1 ? 2 : 1);
                painter->setPen(pen);
                painter->drawLine(tr, br);
            }

            painter->restore();
        }
    };

    // ============================================================================
    // 尺寸换算
    // ============================================================================
    static inline int ptToPx(qreal pt) {
        return qRound(pt * 1.3333) + 2;
    }

    static inline int charsToPx(qreal chars) {
        return qRound(chars * 7.0) + 5;
    }

    TemplateSheetView::TemplateSheetView(QWidget* parent)
        : QWidget(parent)
    {
        m_titleLabel = new QLabel(this);
        m_titleLabel->setStyleSheet(
            "font-size: 14pt; font-weight: bold; padding: 8px;");

        m_table = new QTableWidget(this);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setWordWrap(true);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setVisible(false);
        m_table->setFrameShape(QFrame::NoFrame);
        m_table->setShowGrid(false);

        m_table->horizontalHeader()->setMinimumSectionSize(1);
        m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        m_table->horizontalHeader()->setStretchLastSection(false);

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
    )");

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(m_titleLabel);
        layout->addWidget(m_table, 1);
    }

    void TemplateSheetView::loadSheet(const QString& dbPath,
        int sheetId,
        const QString& title)
    {
        m_titleLabel->setText(title);
        m_table->clear();
        m_table->setRowCount(0);
        m_table->setColumnCount(0);
        m_table->clearSpans();

        if (!QFile::exists(dbPath)) return;

        const QString connName = QStringLiteral("sheetview_main");
        QSqlDatabase db = QSqlDatabase::contains(connName)
            ? QSqlDatabase::database(connName)
            : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);

        if (db.isOpen()) db.close();
        db.setDatabaseName(dbPath);
        if (!db.open()) return;

        int rowCount = 0, colCount = 0;
        qreal defRowHeight = 14.25, defColWidth = 8.43;

        {
            {
                QSqlQuery q(db);
                q.prepare(QStringLiteral(
                    "SELECT row_count, col_count, default_row_height, default_col_width"
                    " FROM template_sheets WHERE id = ?"));
                q.addBindValue(sheetId);
                if (q.exec() && q.next()) {
                    rowCount = q.value(0).toInt();
                    colCount = q.value(1).toInt();
                    defRowHeight = q.value(2).toDouble();
                    defColWidth = q.value(3).toDouble();
                }
            }

            if (rowCount <= 0 || colCount <= 0) { db.close(); return; }

            m_table->setUpdatesEnabled(false);
            m_table->setRowCount(rowCount);
            m_table->setColumnCount(colCount);

            // 1. 读取行高
            {
                QSqlQuery q(db);
                q.prepare("SELECT row, height FROM sheet_row_heights WHERE sheet_id = ?");
                q.addBindValue(sheetId);
                if (q.exec()) {
                    while (q.next()) {
                        const int r = q.value(0).toInt();
                        if (r >= 0 && r < rowCount)
                            m_table->setRowHeight(r, qMax(18, ptToPx(q.value(1).toDouble())));
                    }
                }
            }

            // 2. 读取列宽
            {
                QSqlQuery q(db);
                q.prepare("SELECT col, width FROM sheet_col_widths WHERE sheet_id = ? ORDER BY col");
                q.addBindValue(sheetId);
                if (q.exec()) {
                    while (q.next()) {
                        const int c = q.value(0).toInt();
                        if (c >= 0 && c < colCount) {
                            m_table->setColumnWidth(c, charsToPx(q.value(1).toDouble()));
                        }
                    }
                }
            }

            // 3. 读取内容并设置字体
            {
                QSqlQuery q(db);
                q.prepare(QStringLiteral(
                    "SELECT row, col, row_span, col_span, value,"
                    " font_family, font_size, font_bold, font_italic, font_underline,"
                    " font_color, bg_color, h_align, v_align,"
                    " border_top, border_bottom, border_left, border_right"
                    " FROM sheet_cells WHERE sheet_id = ?"));
                q.addBindValue(sheetId);
                if (q.exec()) {
                    while (q.next()) {
                        const int r = q.value(0).toInt();
                        const int c = q.value(1).toInt();
                        if (r < 0 || r >= rowCount || c < 0 || c >= colCount) continue;

                        const int rowSpan = qMax(1, q.value(2).toInt());
                        const int colSpan = qMax(1, q.value(3).toInt());
                        const QString value = q.value(4).toString();
                        const QString fontFamily = q.value(5).toString();
                        qreal fontSize = q.value(6).toDouble();

                        auto* item = new QTableWidgetItem(value);

                        if (fontSize <= 0.0) fontSize = 9.0; // 兜底

                        QFont font(fontFamily);

                        // ★ 字体平滑：如果字号包含 x.5，微调一下，避免 Qt 渲染出半像素导致模糊
                        if (fontSize == 10.5) fontSize = 10.5;
                        else if (fontSize > 8.5 && fontSize < 9.5) fontSize = 9.0;

                        font.setPointSizeF(fontSize);

                        // 强制抗锯齿和优质渲染
                        font.setStyleStrategy(QFont::PreferAntialias);
                        font.setHintingPreference(QFont::PreferFullHinting);

                        font.setBold(q.value(7).toInt() != 0);
                        font.setItalic(q.value(8).toInt() != 0);
                        font.setUnderline(q.value(9).toInt() != 0);
                        item->setFont(font);

                        QColor fg(q.value(10).toString());
                        if (fg.isValid()) item->setForeground(fg);
                        QColor bg(q.value(11).toString());
                        if (bg.isValid() && bg != QColor("#FFFFFF"))
                            item->setBackground(bg);

                        const int h = q.value(12).toInt();
                        const int v = q.value(13).toInt();
                        Qt::Alignment align;
                        if (h == 1) align |= Qt::AlignHCenter;
                        else if (h == 2) align |= Qt::AlignRight;
                        else        align |= Qt::AlignLeft;
                        if (v == 0) align |= Qt::AlignTop;
                        else if (v == 2) align |= Qt::AlignBottom;
                        else        align |= Qt::AlignVCenter;
                        item->setTextAlignment(align);

                        item->setData(Qt::UserRole + 10, q.value(14).toInt());
                        item->setData(Qt::UserRole + 11, q.value(15).toInt());
                        item->setData(Qt::UserRole + 12, q.value(16).toInt());
                        item->setData(Qt::UserRole + 13, q.value(17).toInt());

                        m_table->setItem(r, c, item);

                        if (rowSpan > 1 || colSpan > 1) {
                            const int maxRow = qMin(rowCount - r, rowSpan);
                            const int maxCol = qMin(colCount - c, colSpan);
                            if (maxRow > 1 || maxCol > 1)
                                m_table->setSpan(r, c, maxRow, maxCol);
                        }
                    }
                }
            }

            m_table->horizontalHeader()->resizeSections(QHeaderView::Fixed);
            m_table->setUpdatesEnabled(true);
        }

        db.close();
    }

} // namespace doclife::ui