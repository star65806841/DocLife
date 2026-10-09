#include "projiectitem.h"

#include <QBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QFontMetrics>

#include "design/Typography.h"

using namespace fluent;

// ============================================================
// NavigationItemRow
// ============================================================

NavigationItemRow::NavigationItemRow(const QString& iconGlyph,
                                     const QString& text,
                                     bool selected,
                                     QWidget* parent)
    : QWidget(parent)
    , m_iconGlyph(iconGlyph)
    , m_text(text)
    , m_selected(selected)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(40);
}

QSize NavigationItemRow::sizeHint() const
{
    if (m_compact)
        return QSize(48, 40);
    const QFontMetrics metrics(
        themeFont(Typography::FontRole::Body).toQFont());
    return QSize(qMax(84, metrics.horizontalAdvance(m_text) + 68), 40);
}

void NavigationItemRow::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    update();
}

// void NavigationItemRow::setCompact(bool compact)
// {
//     if (m_compact == compact)
//         return;
//     m_compact = compact;
//     updateGeometry();
//     update();
// }

void NavigationItemRow::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColors();
    const auto& radius = themeRadius();
    const QRectF itemRect = rect().adjusted(4, 2, -4, -2);

    QColor fill = Qt::transparent;
    if (m_pressed)
        fill = colors.subtleTertiary;
    else if (m_selected || m_hovered)
        fill = colors.subtleSecondary;

    if (fill.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(itemRect, radius.control, radius.control);
    }

    if (selectedIndicatorVisible()) {
        const QRectF indicator(itemRect.left() + 4,
                               itemRect.center().y() - 8, 3, 16);
        painter.setBrush(colors.accentDefault);
        painter.drawRoundedRect(indicator, 1.5, 1.5);
    }

    const int iconX = m_compact ? (width() - 20) / 2 : 22;
    const QRect iconRect(iconX, 0, 20, height());
    QFont iconFont(Typography::FontFamily::FluentIcons);
    iconFont.setPixelSize(16);
    painter.setFont(iconFont);
    painter.setPen(m_selected ? colors.textPrimary : colors.textSecondary);
    painter.drawText(iconRect, Qt::AlignCenter, m_iconGlyph);

    if (!m_compact) {
        QFont textFont = themeFont(m_selected
                                       ? Typography::FontRole::BodyStrong
                                       : Typography::FontRole::Body)
                             .toQFont();
        painter.setFont(textFont);
        painter.setPen(colors.textPrimary);
        painter.drawText(
            QRect(52, 0, qMax(0, width() - 64), height()),
            Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, m_text);
    }
}

void NavigationItemRow::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
    m_hovered = true;
    update();
}

void NavigationItemRow::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    m_pressed = false;
    update();
}

void NavigationItemRow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void NavigationItemRow::mouseReleaseEvent(QMouseEvent* event)
{
    const bool activate = m_pressed && rect().contains(event->pos());
    m_pressed = false;
    update();
    if (activate && onActivated) {
        onActivated();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// ============================================================
// MainSection
// ============================================================

MainSection::MainSection(const QVector<Entry>& entries, QWidget* parent)
    : QWidget(parent)
    , m_layout(new QBoxLayout(QBoxLayout::TopToBottom, this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(4);

    for (const Entry& entry : entries) {
        const int rowIndex = m_rows.size();
        auto* row = new NavigationItemRow(entry.icon, entry.text,
                                          entry.selected, this);
        row->onActivated = [this, rowIndex]() {
            setSelectedIndex(rowIndex);
            if (onActivated)
                onActivated(rowIndex);
        };
        m_rows.append(row);
        m_layout->addWidget(row);
    }
    m_layout->addStretch();
}

// QSize MainSection::sizeHint() const
// {
//     return QSize(280, 16 + m_rows.size() * 44);
// }

// QSize MainSection::minimumSizeHint() const
// {
//     return QSize(48, 16 + m_rows.size() * 40);
// }

// void MainSection::setCompact(bool compact)
// {
//     m_compact = compact;
//     for (NavigationItemRow* row : m_rows)
//         row->setCompact(compact);
//     updateGeometry();
// }

void MainSection::setSelectedIndex(int index)
{
    for (int i = 0; i < m_rows.size(); ++i)
        m_rows.at(i)->setSelected(i == index);
}

// void MainSection::clearSelection()
// {
//     setSelectedIndex(-1);
// }

// void MainSection::onThemeUpdated()
// {
//     for (NavigationItemRow* row : m_rows)
//         row->update();
//     update();
// }