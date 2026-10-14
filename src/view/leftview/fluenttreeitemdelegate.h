#pragma once

#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QPainter>
#include <QRectF>
#include <QColor>

#include "components/foundation/FluentElement.h"

namespace doclife::ui {

/**
 * @brief Fluent 风格树视图行代理（无参构造版）
 *
 * 使用方式：
 *   auto* delegate = new FluentTreeItemDelegate(tree);   // 只需传 parent
 *   tree->setItemDelegate(delegate);
 *
 * 内部会自动从 view 获取 FluentElement 来读取主题色，
 * 行高固定为 32px（与官方示例一致）。
 */
class FluentTreeItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit FluentTreeItemDelegate(QObject* parent = nullptr);

    // -------- 可选配置 --------
    void setCheckBoxVisible(bool visible);
    bool checkBoxVisible() const { return m_checkBoxVisible; }

    // -------- QStyledItemDelegate 接口 --------
    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    bool editorEvent(QEvent* event,
                     QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    // -------- 自定义数据角色 --------
    static constexpr int IconGlyphRole = Qt::UserRole + 100;

private:
    // -------- 内部辅助（从 option 动态获取）--------
    fluent::FluentElement* themeHostFromOption(const QStyleOptionViewItem& option) const;
    QAbstractItemView*    viewFromOption(const QStyleOptionViewItem& option) const;

    // -------- 绘制辅助 --------
    void paintCheckBox(QPainter* painter,
                       const QRectF& rect,
                       Qt::CheckState state,
                       const fluent::FluentElement::Colors& colors) const;

    void paintIconGlyph(QPainter* painter,
                        const QRectF& rect,
                        const QString& glyph,
                        const QColor& color) const;

    qreal accentProgress(const QModelIndex& index,
                         QAbstractItemView* view) const;

    // -------- 布局计算 --------
    QRectF bgRectForOption(const QStyleOptionViewItem& option) const;
    QRectF checkBoxRectForOption(const QStyleOptionViewItem& option) const;
    QRectF chevronRectForOption(const QStyleOptionViewItem& option) const;

private:
    bool m_checkBoxVisible = false;
};

} // namespace doclife::ui