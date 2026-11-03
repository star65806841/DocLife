#pragma once

#include <QStyledItemDelegate>

namespace doclife::ui {

    class AppListDelegate : public QStyledItemDelegate
    {
        Q_OBJECT
    public:
        explicit AppListDelegate(QObject* parent = nullptr);

        // 控制是否显示复选框
        void setCheckBoxVisible(bool visible);
        bool checkBoxVisible() const { return m_checkBoxVisible; }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    private:
        QRect getCheckBoxRect(const QStyleOptionViewItem& option) const;
        bool m_checkBoxVisible = false;
    };

} // namespace doclife::ui