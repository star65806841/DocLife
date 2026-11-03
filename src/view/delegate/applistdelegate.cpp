#include "applistdelegate.h"
#include <QPainter>
#include <QMouseEvent>

namespace doclife::ui {

    AppListDelegate::AppListDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
    }

    void AppListDelegate::setCheckBoxVisible(bool visible)
    {
        m_checkBoxVisible = visible;
    }

    QRect AppListDelegate::getCheckBoxRect(const QStyleOptionViewItem& option) const
    {
        return QRect(option.rect.left() + 8, option.rect.center().y() - 8, 16, 16);
    }

    void AppListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        if (!index.isValid()) return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // 1. 绘制背景
        QRect bgRect = option.rect.adjusted(4, 2, -4, -2);
        const bool isSelected = option.state & QStyle::State_Selected;
        const bool isHovered = option.state & QStyle::State_MouseOver;

        if (isSelected && isHovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0xD0, 0xE8, 0xFF));
            painter->drawRoundedRect(bgRect, 4, 4);
        }
        else if (isSelected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0xE5, 0xF1, 0xFB));
            painter->drawRoundedRect(bgRect, 4, 4);
        }
        else if (isHovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0xF0, 0xF0, 0xF0));
            painter->drawRoundedRect(bgRect, 4, 4);
        }

        // 2. 计算文字起始位置
        int textLeft = option.rect.left() + 16;

        if (m_checkBoxVisible) {
            QRect cbRect = getCheckBoxRect(option);
            bool isChecked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
            bool isEnabled = option.state & QStyle::State_Enabled;

            if (isChecked) {
                painter->setPen(Qt::NoPen);
                // 禁用的勾选框使用灰色
                painter->setBrush(isEnabled ? QColor(0x00, 0x78, 0xD4) : QColor(0xC0, 0xC0, 0xC0));
                painter->drawRoundedRect(cbRect, 3, 3);
                painter->setPen(QPen(Qt::white, 2));
                painter->drawText(cbRect, Qt::AlignCenter, "✓");
            }
            else {
                painter->setPen(QPen(QColor(0xC0, 0xC0, 0xC0), 1));
                painter->setBrush(Qt::NoBrush);
                painter->drawRoundedRect(cbRect, 3, 3);
            }
            textLeft = cbRect.right() + 8;
        }

        // 3. 绘制文字
        QString text = index.data(Qt::DisplayRole).toString();
        bool isEnabled = option.state & QStyle::State_Enabled;
        painter->setPen(isEnabled ? QColor(0x33, 0x33, 0x33) : QColor(0x99, 0x99, 0x99));
        painter->setFont(option.font);

        QRect textRect = option.rect.adjusted(textLeft - option.rect.left(), 0, -8, 0);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

        painter->restore();
    }

    QSize AppListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        return QSize(0, 32);
    }

    bool AppListDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
    {
        if (event->type() == QEvent::MouseButtonRelease && m_checkBoxVisible) {
            auto* me = static_cast<QMouseEvent*>(event);

            if (option.rect.contains(me->pos())) {
                // ★ 核心修复：如果是禁用状态（系统缺失），直接吞掉事件，不响应任何勾选
                if (!(index.flags() & Qt::ItemIsEnabled)) {
                    return true;
                }

                QVariant checkData = index.data(Qt::CheckStateRole);
                Qt::CheckState curState = checkData.isValid() ? static_cast<Qt::CheckState>(checkData.toInt()) : Qt::Unchecked;
                Qt::CheckState newState = (curState == Qt::Checked) ? Qt::Unchecked : Qt::Checked;

                model->setData(index, newState, Qt::CheckStateRole);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

} // namespace doclife::ui