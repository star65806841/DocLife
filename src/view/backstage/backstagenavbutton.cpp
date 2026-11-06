#include "backstagenavbutton.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>

#include <design/Typography.h>

namespace doclife::ui {

    namespace {
        constexpr int kNavWidth = 220;
        constexpr int kNavHeight = 44;
        constexpr int kIconSize = 16;
        constexpr int kPadX = 16;
        constexpr int kIconTextGap = 12;

        const QColor kTextPrimary(0x20, 0x20, 0x20);
        const QColor kHoverFill(0, 0, 0, 10);
        const QColor kCheckedFill(0, 0, 0, 20);
        const QColor kAccent(0x00, 0x78, 0xD4);
        const QColor kSeparator(0xE0, 0xE0, 0xE0);
    }

    BackstageNavButton::BackstageNavButton(const QString& glyphName,
                                           const QString& text,
                                           Role role,
                                           QWidget* parent)
        : QAbstractButton(parent)
        , m_glyph(glyphName)
        , m_text(text)
        , m_role(role)
    {
        setCheckable(role == Role::Page);
        setAutoExclusive(false);           // 互斥由 BackstageView 统一管理
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(role == Role::Separator ? Qt::NoFocus : Qt::TabFocus);
        if (role == Role::Separator) setEnabled(false);
    }

    BackstageNavButton* BackstageNavButton::makeSeparator(QWidget* parent)
    {
        return new BackstageNavButton(QString(), QString(), Role::Separator, parent);
    }

    QSize BackstageNavButton::sizeHint() const
    {
        if (m_role == Role::Separator) return QSize(kNavWidth, 13);
        return QSize(kNavWidth, kNavHeight);
    }

    void BackstageNavButton::paintEvent(QPaintEvent*)
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        if (m_role == Role::Separator) {
            const int y = height() / 2;
            p.setPen(QPen(kSeparator, 1));
            p.drawLine(kPadX, y, width() - kPadX, y);
            return;
        }

        if (isChecked()) {
            p.fillRect(rect(), kCheckedFill);
            p.fillRect(QRect(0, 0, 3, height()), kAccent);   // 选中左条
        }
        else if (m_hover) {
            p.fillRect(rect(), kHoverFill);
        }

        const int iconY = (height() - kIconSize) / 2;
        int textX = kPadX;
        if (!m_glyph.isEmpty()) {
            Typography::Icons::paintGlyph(p, QRectF(kPadX, iconY, kIconSize, kIconSize),
                m_glyph, kIconSize, Qt::AlignCenter);
            textX = kPadX + kIconSize + kIconTextGap;
        }

        QFont f = font();
        f.setPixelSize(14);
        f.setBold(isChecked());
        p.setFont(f);
        p.setPen(kTextPrimary);
        p.drawText(QRect(textX, 0, width() - textX - kPadX, height()),
            Qt::AlignVCenter | Qt::AlignLeft, m_text);

        if (hasFocus()) {
            p.setPen(QPen(kAccent, 1));
            p.setBrush(Qt::NoBrush);
            p.drawRect(rect().adjusted(1, 1, -2, -2));
        }
    }

    void BackstageNavButton::enterEvent(QEnterEvent*)
    {
        m_hover = true;
        update();
    }

    void BackstageNavButton::leaveEvent(QEvent*)
    {
        m_hover = false;
        update();
    }

    void BackstageNavButton::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            click();
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }

} // namespace doclife::ui
