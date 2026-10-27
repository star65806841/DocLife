#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QString>
#include "xlsxformat.h"

namespace QXlsx { class Worksheet; }

namespace doclife::ui::detail {

    class ExcelSheetModel : public QAbstractTableModel
    {
        Q_OBJECT
    public:
        explicit ExcelSheetModel(QXlsx::Worksheet* sheet, QObject* parent = nullptr);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        int firstRow() const { return m_firstRow; }
        int firstCol() const { return m_firstCol; }

        int realRowCount() const { return m_rows; }
        int realColumnCount() const { return m_cols; }

        QXlsx::Format cellFormat(int row, int col) const;

    private:
        static QString columnName(int col);

        static constexpr int kMaxRows = 5000;
        static constexpr int kMaxCols = 200;
        static constexpr int kPaddingRows = 1;   // 底部可滚出的空白行数
        static constexpr int kPaddingCols = 1;   // 右侧可滚出的空白列数

        QXlsx::Worksheet* m_sheet = nullptr;
        int m_firstRow = 1;
        int m_firstCol = 1;
        int m_rows = 0;
        int m_cols = 0;

        // (row << 32) | col -> Format，懒加载缓存，避免每帧重复 cellAt
        mutable QHash<quint64, QXlsx::Format> m_formatCache;
    };

} // namespace doclife::ui::detail