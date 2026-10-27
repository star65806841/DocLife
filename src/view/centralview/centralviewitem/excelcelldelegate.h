#pragma once

#include <QStyledItemDelegate>
#include <QTextDocument>

namespace doclife::ui {

    class ExcelCellDelegate : public QStyledItemDelegate
    {
    public:
        explicit ExcelCellDelegate(QObject* parent = nullptr);

        void paint(QPainter* painter, const QStyleOptionViewItem& option,
            const QModelIndex& index) const override;

    private:
        mutable QTextDocument m_textDoc;
    };

} // namespace doclife::ui