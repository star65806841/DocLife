#include "excelsheetmodel.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"
#include "xlsxcell.h"

namespace doclife::ui::detail {

    ExcelSheetModel::ExcelSheetModel(QXlsx::Worksheet* sheet, QObject* parent)
        : QAbstractTableModel(parent), m_sheet(sheet)
    {
        const QXlsx::CellRange range = sheet->dimension();
        m_firstRow = range.firstRow();
        m_firstCol = range.firstColumn();
        m_rows = qMax(0, qMin(range.rowCount(), kMaxRows));
        m_cols = qMax(0, qMin(range.columnCount(), kMaxCols));
    }

    int ExcelSheetModel::rowCount(const QModelIndex&) const
    {
        return m_rows + kPaddingRows;
    }

    int ExcelSheetModel::columnCount(const QModelIndex&) const
    {
        return m_cols + kPaddingCols;
    }

    QVariant ExcelSheetModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid() || !m_sheet) return {};
        // padding 区域返回空
        if (index.row() >= m_rows || index.column() >= m_cols) return {};
        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return m_sheet->read(index.row() + m_firstRow, index.column() + m_firstCol);
        return {};
    }

    QVariant ExcelSheetModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (role != Qt::DisplayRole) return {};
        if (orientation == Qt::Horizontal) {
            if (section >= m_cols) return {};          // padding 列头不显示
            return columnName(section + m_firstCol - 1);
        }
        if (section >= m_rows) return {};              // padding 行头不显示
        return QString::number(section + m_firstRow);
    }

    QXlsx::Format ExcelSheetModel::cellFormat(int row, int col) const
    {
        if (!m_sheet) return QXlsx::Format{};
        if (row >= m_rows || col >= m_cols) return QXlsx::Format{};

        const quint64 key = (static_cast<quint64>(row) << 32) | static_cast<quint32>(col);
        auto it = m_formatCache.constFind(key);
        if (it != m_formatCache.constEnd()) return it.value();

        auto cell = m_sheet->cellAt(row + m_firstRow, col + m_firstCol);
        QXlsx::Format fmt = cell ? cell->format() : QXlsx::Format{};
        m_formatCache.insert(key, fmt);
        return fmt;
    }

    QString ExcelSheetModel::columnName(int col)
    {
        QString name;
        while (col >= 0) {
            name.prepend(QChar('A' + col % 26));
            col = col / 26 - 1;
        }
        return name;
    }

} // namespace doclife::ui::detail