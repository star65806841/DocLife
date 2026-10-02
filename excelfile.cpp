#include "excelfile.h"

#include <xlsxdocument.h>

ExcelFile::ExcelFile(QWidget *parent)
    : QWidget{parent}
{}

void ExcelFile::openExcelFile(QString filePath)
{
    m_filePath = filePath;

    // // First, read sheet names using QXlsx::Document
    QXlsx::Document doc(filePath);
    if (!doc.isLoadPackage())
    {
        qDebug()<<"Failed to load the Excel file";
        return;
    }

    const QStringList sheetNames = doc.sheetNames();
    if (sheetNames.isEmpty())
    {
        qDebug()<<"The Excel file has no sheets";
        return;
    }

    // clearTabs();

    // // For each sheet, create a model and a view, and add as a tab
    // for (const QString &sheetName : sheetNames)
    // {
    //     ExcelModel *model = new ExcelModel(this);
    //     if (!model->loadFromXlsx(filePath, sheetName))
    //     {
    //         delete model;
    //         continue;
    //     }

    //     QTableView *view = new QTableView(m_tabWidget);
    //     view->setModel(model);


    //     // Change the default Stretch mode to Interactive to allow manual resizing
    //     view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    //     view->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    //     // Apply column width (Excel units -> pixel conversion needed, approx. 7.5x)
    //     for (int c = 0; c < model->columnCount(); ++c) {
    //         view->setColumnWidth(c, static_cast<int>(model->columnWidth(c) * 7.5));
    //     }

    //     // Apply row height (points -> pixel conversion needed, approx. 1.33x)
    //     for (int r = 0; r < model->rowCount(); ++r) {
    //         view->setRowHeight(r, static_cast<int>(model->rowHeight(r) * 1.33));
    //     }

    //     view->setSelectionBehavior(QAbstractItemView::SelectItems);
    //     view->setSelectionMode(QAbstractItemView::SingleSelection);
    //     view->setEditTriggers(QAbstractItemView::NoEditTriggers);

    //     applySpansToView(view, model);

    //     m_tabWidget->addTab(view, sheetName);
    //     m_models.push_back(model);
    // }

    // if (m_tabWidget->count() == 0)
    // {
    //     QMessageBox::warning(
    //         this,
    //         "Error",
    //         "No sheet could be loaded from the Excel file."
    //         );
    // }
}

