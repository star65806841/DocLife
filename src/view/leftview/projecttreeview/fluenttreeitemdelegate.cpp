#include "FluentTreeItemDelegate.h"

#include <QStyle>
#include <QTreeView>
#include <QLineEdit>
#include <functional>
#include <QMouseEvent>
#include <QPainterPath>
#include <QItemSelectionModel>

#include "design/Spacing.h"
#include "design/Typography.h"
#include "design/CornerRadius.h"

#include "compatibility/QtCompat.h"
#include "components/collections/TreeView.h"

namespace doclife::ui {

    // ===============================================================
    // 构造
    // ===============================================================
    FluentTreeItemDelegate::FluentTreeItemDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
    }

    void FluentTreeItemDelegate::setCheckBoxVisible(bool visible)
    {
        if (m_checkBoxVisible == visible) return;
        m_checkBoxVisible = visible;
        if (auto* view = qobject_cast<QAbstractItemView*>(parent())) {
            if (view->viewport()) view->viewport()->update();
        }
    }

    QWidget* FluentTreeItemDelegate::createEditor(QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const
    {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
        auto* lineEdit = qobject_cast<QLineEdit*>(editor);
        if (!lineEdit) return editor;

        // 打开编辑器时默认全选。默认选区色和整行的选中色几乎一样，看上去像没选中，
        // 这里显式给一个更明显的蓝色：点一下可以取消全选，Ctrl+A 可以重新全选。
        const QColor textColor = option.palette.color(QPalette::Text);
        const QColor baseColor = option.palette.color(QPalette::Button);

        lineEdit->setFrame(false);
        lineEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  border: 1px solid #7FB8E0;"
            "  border-radius: 2px;"
            "  padding: 0px 3px;"
            "  background: %1;"
            "  color: %2;"
            "  selection-background-color: #9CCBEB;"
            "  selection-color: #FFFFE0;"
            "}").arg(baseColor.name(), textColor.name()));

        return editor;
    }

    QAbstractItemView* FluentTreeItemDelegate::viewFromOption(
        const QStyleOptionViewItem& option) const
    {
        return qobject_cast<QAbstractItemView*>(
            const_cast<QWidget*>(option.widget));
    }

    fluent::FluentElement* FluentTreeItemDelegate::themeHostFromOption(
        const QStyleOptionViewItem& option) const
    {
        if (auto* view = viewFromOption(option)) {
            return dynamic_cast<fluent::FluentElement*>(view);
        }
        return nullptr;
    }

    qreal FluentTreeItemDelegate::accentProgress(const QModelIndex& index,
        QAbstractItemView* view) const
    {
        auto* tv = qobject_cast<fluent::collections::TreeView*>(view);
        return tv ? tv->selectedIndicatorProgress(index) : 1.0;
    }

    void FluentTreeItemDelegate::paintCheckBox(
        QPainter* painter,
        const QRectF& rect,
        Qt::CheckState state,
        const fluent::FluentElement::Colors& colors) const
    {
        painter->save();

        const qreal boxSize = 18.0;
        const qreal x = rect.center().x() - boxSize / 2.0;
        const qreal y = rect.center().y() - boxSize / 2.0;
        QRectF boxRect(x, y, boxSize, boxSize);
        const qreal r = 3.0;

        QPainterPath path;
        path.addRoundedRect(boxRect, r, r);

        if (state == Qt::Checked || state == Qt::PartiallyChecked) {
            painter->setPen(Qt::NoPen);
            // 如果主题失效，给个默认的系统蓝
            painter->setBrush(colors.accentDefault.isValid()
                ? colors.accentDefault
                : QColor(0x00, 0x78, 0xD4));
            painter->drawPath(path);

            QFont iconFont(Typography::FontFamily::FluentIcons, -1);
            iconFont.setPixelSize(12);
            painter->setFont(iconFont);
            painter->setPen(Qt::white);

            const QString glyph = (state == Qt::Checked)
                ? Typography::Icons::CheckMark
                : Typography::Icons::Hyphen;
            painter->drawText(boxRect, Qt::AlignCenter, glyph);
        }
        else {
            painter->setPen(QPen(colors.strokeDefault.isValid()
                ? colors.strokeDefault
                : QColor(0xC0, 0xC0, 0xC0), 1.5));
            painter->setBrush(Qt::transparent);
            painter->drawPath(path);
        }

        painter->restore();
    }

    void FluentTreeItemDelegate::paintIconGlyph(QPainter* painter,
        const QRectF& rect,
        const QString& glyph,
        const QColor& color) const
    {
        QFont iconFont(Typography::FontFamily::FluentIcons, -1);
        iconFont.setPixelSize(16);
        painter->setFont(iconFont);
        painter->setPen(color);
        painter->drawText(rect, Qt::AlignCenter, glyph);
    }

    namespace {
        constexpr qreal kChevronAreaW = 20.0;
        constexpr qreal kCheckBoxAreaW = 22.0;
        constexpr qreal kIconAreaW = 20.0;
        constexpr qreal kGap = 4.0;
        constexpr qreal kCursorStart = 12.0;
        constexpr int   kRowHeight = 32;
    }

    QRectF FluentTreeItemDelegate::bgRectForOption(
        const QStyleOptionViewItem& option) const
    {
        const QAbstractItemView* viewWidget =
            qobject_cast<const QAbstractItemView*>(option.widget);
        const int vpWidth = (viewWidget && viewWidget->viewport())
            ? viewWidget->viewport()->width()
            : option.rect.right();
        return QRectF(2, option.rect.top() + 2, vpWidth - 4,
            option.rect.height() - 4);
    }

    QRectF FluentTreeItemDelegate::checkBoxRectForOption(
        const QStyleOptionViewItem& option) const
    {
        if (!m_checkBoxVisible) return {};
        QRectF bg = bgRectForOption(option);
        qreal x = qreal(option.rect.left()) + kCursorStart;
        return QRectF(x, bg.top(), kCheckBoxAreaW, bg.height());
    }

    QRectF FluentTreeItemDelegate::chevronRectForOption(
        const QStyleOptionViewItem& option) const
    {
        qreal x = qreal(option.rect.left()) + kCursorStart;
        if (m_checkBoxVisible) x += kCheckBoxAreaW + kGap;
        QRectF bg = bgRectForOption(option);
        return QRectF(x, bg.top(), kChevronAreaW, bg.height());
    }

    void FluentTreeItemDelegate::paint(QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const
    {
        if (!index.isValid()) return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        const bool isFirstColumn = (index.column() == 0);

        auto* view = viewFromOption(option);
        auto* host = themeHostFromOption(option);

        fluent::FluentElement::Colors colors{};
        fluent::FluentElement::Radius radius{};
        if (host) {
            colors = host->themeColors();
            radius = host->themeRadius();
        }
        const int cornerR = radius.control > 0 ? radius.control : 4;

        // ---------- 状态 ----------
        const bool isSelected = option.state & QStyle::State_Selected;
        const bool isHovered = option.state & QStyle::State_MouseOver;
        const bool isPressed = (option.state & QStyle::State_Sunken) && isHovered;
        const bool isEnabled = option.state & QStyle::State_Enabled;

        QColor bgColor = Qt::transparent;
        QColor textColor = colors.textPrimary.isValid()
            ? colors.textPrimary
            : QColor(0x33, 0x33, 0x33);

        // ★★★ 核心修复：彻底锁死浅色背景，绝不使用可能变黑的 colors.xxx
        if (!isEnabled) {
            textColor = QColor(0x99, 0x99, 0x99); // 禁用态浅灰
        }
        else if (isSelected && isPressed) {
            bgColor = QColor(0xD0, 0xE8, 0xFF); // 按下深一点的蓝
        }
        else if (isSelected || isPressed) {
            bgColor = QColor(0xE5, 0xF1, 0xFB); // 选中浅蓝
        }
        else if (isHovered) {
            bgColor = QColor(0xF0, 0xF0, 0xF0); // 悬停浅灰
        }

        if (isFirstColumn) {
            QRectF bgRect = bgRectForOption(option);

            // ---------- 背景 ----------
            if (bgColor.alpha() > 0) {
                QPainterPath path;
                path.addRoundedRect(bgRect, cornerR, cornerR);
                painter->setPen(Qt::NoPen);
                painter->setBrush(bgColor);
                painter->drawPath(path);
            }

            // ---------- 选中指示条 (如果是没有复选框的纯树) ----------
            auto* tv = qobject_cast<fluent::collections::TreeView*>(view);
            if (!tv || !tv->selectionIndicatorVisible()) {
                if (!m_checkBoxVisible && isSelected && isEnabled) {
                    const qreal accentT = qBound(0.0, accentProgress(index, view), 1.0);
                    const bool activeMotion = tv && tv->isIndicatorMotionActiveForIndex(index);

                    const auto direction = activeMotion
                        ? tv->indicatorMotionDirection()
                        : fluent::collections::TreeView::IndicatorVerticalDirection::None;
                    const auto hierarchyTransition = activeMotion
                        ? tv->indicatorHierarchyTransition()
                        : fluent::collections::TreeView::IndicatorHierarchyTransition::None;

                    const qreal indicatorW = 3.0;
                    const qreal fullH = 16.0;
                    const qreal indicatorH = fullH * (0.35 + 0.65 * accentT);
                    const qreal settledX = bgRect.left() + 4.0;
                    const qreal settledY = bgRect.center().y() - fullH / 2.0;
                    const qreal remaining = 1.0 - accentT;

                    qreal indicatorX = settledX;
                    if (hierarchyTransition ==
                        fluent::collections::TreeView::IndicatorHierarchyTransition::Inward)
                        indicatorX += remaining * 4.0;
                    else if (hierarchyTransition ==
                        fluent::collections::TreeView::IndicatorHierarchyTransition::Outward)
                        indicatorX -= remaining * 3.0;

                    qreal indicatorY = bgRect.center().y() - indicatorH / 2.0;
                    if (direction ==
                        fluent::collections::TreeView::IndicatorVerticalDirection::Down)
                        indicatorY = settledY - remaining * 6.0;
                    else if (direction ==
                        fluent::collections::TreeView::IndicatorVerticalDirection::Up)
                        indicatorY = settledY + (fullH - indicatorH) + remaining * 6.0;

                    QRectF indicatorRect(indicatorX, indicatorY, indicatorW, indicatorH);
                    QPainterPath indicatorPath;
                    indicatorPath.addRoundedRect(indicatorRect, indicatorW / 2.0,
                        indicatorW / 2.0);

                    QColor ac = colors.accentDefault.isValid()
                        ? colors.accentDefault
                        : QColor(0x00, 0x78, 0xD4);
                    ac.setAlphaF(ac.alphaF() * accentT);
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(ac);
                    painter->drawPath(indicatorPath);
                }
            }
        }

        if (isFirstColumn) {
            qreal cursorX = qreal(option.rect.left()) + kCursorStart;

            if (m_checkBoxVisible) {
                QRectF bgRect = bgRectForOption(option);
                QRectF cbRect(cursorX, bgRect.top(), kCheckBoxAreaW, bgRect.height());
                QVariant checkData = index.data(Qt::CheckStateRole);
                Qt::CheckState checkState = checkData.isValid()
                    ? static_cast<Qt::CheckState>(checkData.toInt())
                    : Qt::Unchecked;
                paintCheckBox(painter, cbRect, checkState, colors);
                cursorX += kCheckBoxAreaW + kGap;
            }

            auto* tv = qobject_cast<fluent::collections::TreeView*>(view);
            const QAbstractItemModel* m = index.model();
            const bool hasChildren = m && m->hasChildren(index);
            const qreal chevronLeft = cursorX;

            if (hasChildren) {
                const qreal rotation = tv ? tv->chevronRotation(index)
                    : (tv && tv->isExpanded(index) ? 1.0 : 0.0);
                const qreal angle = rotation * 90.0;

                QFont iconFont(Typography::FontFamily::FluentIcons, -1);
                iconFont.setPixelSize(12);
                painter->setFont(iconFont);
                painter->setPen(textColor);

                QRectF bgRect = bgRectForOption(option);
                QRectF chevronRect(chevronLeft, bgRect.top(), kChevronAreaW,
                    bgRect.height());

                painter->save();
                painter->translate(chevronRect.center());
                painter->rotate(angle);
                painter->translate(-chevronRect.center());
                painter->drawText(chevronRect, Qt::AlignCenter,
                    Typography::Icons::ChevronRightMed);
                painter->restore();
            }
            cursorX = chevronLeft + kChevronAreaW + kGap;

            const QString iconGlyph = index.data(IconGlyphRole).toString();
            if (!iconGlyph.isEmpty()) {
                QRectF bgRect = bgRectForOption(option);
                QRectF iconRect(cursorX, bgRect.top(), kIconAreaW, bgRect.height());
                paintIconGlyph(painter, iconRect, iconGlyph, textColor);
                cursorX += kIconAreaW + kGap;
            }

            QRectF bgRect = bgRectForOption(option);
            QRectF textRect(cursorX, bgRect.top(),
                bgRect.right() - cursorX - 8.0, bgRect.height());
            painter->setPen(textColor);
            painter->setFont(option.font);

            const QString text = index.data(Qt::DisplayRole).toString();
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                painter->fontMetrics().elidedText(
                    text, Qt::ElideRight, int(textRect.width())));
        }

        painter->restore();
    }

    bool FluentTreeItemDelegate::editorEvent(QEvent* event,
        QAbstractItemModel* model,
        const QStyleOptionViewItem& option,
        const QModelIndex& index)
    {
        auto* view = viewFromOption(option);

        if (event->type() == QEvent::MouseButtonPress && m_checkBoxVisible) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (checkBoxRectForOption(option).contains(fluentMousePos(me)))
                return true;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            const QPointF pos = fluentMousePos(me);

            if (m_checkBoxVisible) {
                QRectF cbRect = checkBoxRectForOption(option);
                if (cbRect.contains(pos)) {
                    QVariant checkData = index.data(Qt::CheckStateRole);
                    Qt::CheckState curState = checkData.isValid()
                        ? static_cast<Qt::CheckState>(checkData.toInt())
                        : Qt::Unchecked;
                    Qt::CheckState newState = (curState == Qt::Checked)
                        ? Qt::Unchecked : Qt::Checked;

                    auto* tv = qobject_cast<fluent::collections::TreeView*>(view);
                    QItemSelectionModel* selection =
                        view ? view->selectionModel() : nullptr;
                    const bool syncSelection = tv && selection &&
                        (tv->selectionMode() ==
                            fluent::collections::TreeView::SelectionMode::Multiple ||
                            tv->selectionMode() ==
                            fluent::collections::TreeView::SelectionMode::Extended);

                    if (syncSelection) {
                        const QItemSelectionModel::SelectionFlags flags =
                            (newState == Qt::Checked)
                            ? (QItemSelectionModel::Select |
                                QItemSelectionModel::Rows)
                            : (QItemSelectionModel::Deselect |
                                QItemSelectionModel::Rows);
                        selection->select(index, flags);
                        if (newState == Qt::Checked)
                            selection->setCurrentIndex(index,
                                QItemSelectionModel::NoUpdate);
                        return true;
                    }

                    model->setData(index, newState, Qt::CheckStateRole);

                    std::function<void(const QModelIndex&)> cascadeChildren =
                        [&](const QModelIndex& parent) {
                        int rows = model->rowCount(parent);
                        for (int r = 0; r < rows; ++r) {
                            QModelIndex child = model->index(r, 0, parent);
                            model->setData(child, newState, Qt::CheckStateRole);
                            cascadeChildren(child);
                        }
                        };
                    cascadeChildren(index);

                    std::function<void(const QModelIndex&)> updateAncestors =
                        [&](const QModelIndex& child) {
                        QModelIndex parent = child.parent();
                        if (!parent.isValid()) return;

                        int rows = model->rowCount(parent);
                        int checkedCount = 0, uncheckedCount = 0;
                        for (int r = 0; r < rows; ++r) {
                            QVariant v = model->index(r, 0, parent)
                                .data(Qt::CheckStateRole);
                            auto st = v.isValid()
                                ? static_cast<Qt::CheckState>(v.toInt())
                                : Qt::Unchecked;
                            if (st == Qt::Checked) ++checkedCount;
                            else if (st == Qt::Unchecked) ++uncheckedCount;
                        }

                        Qt::CheckState parentState;
                        if (checkedCount == rows) parentState = Qt::Checked;
                        else if (uncheckedCount == rows) parentState = Qt::Unchecked;
                        else parentState = Qt::PartiallyChecked;

                        model->setData(parent, parentState, Qt::CheckStateRole);
                        updateAncestors(parent);
                        };
                    updateAncestors(index);
                    return true;
                }
            }

            if (index.model() && index.model()->hasChildren(index)) {
                QRectF chevRect = chevronRectForOption(option);
                if (chevRect.contains(pos)) {
                    auto* tv = qobject_cast<fluent::collections::TreeView*>(view);
                    if (tv) tv->toggleExpanded(index);
                    return true;
                }
            }
        }

        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    QSize FluentTreeItemDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
        const QModelIndex& /*index*/) const
    {
        return QSize(0, kRowHeight);
    }
} // namespace doclife::ui