#include "utils.h"

#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>

using namespace doclife::model;

int Utils::add(int a, int b) {
    return a + b;
}

QString Utils::formatName(const QString &name) {
    return name.trimmed();
}

QString Utils::openExcelFile(QWidget *parent)
{
    QMessageBox::StandardButton choice = QMessageBox::question(
        parent,
        "Information",
        "If the xlsx file is large, opening it may take a significant amount of time.\n\n"
        "Do you want to continue and select a file?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
        );

    if (choice != QMessageBox::Yes)
        return QString();

    return QFileDialog::getOpenFileName(
        parent,
        "Select Excel File",
        QString(),
        "Excel File (*.xlsx)"
        );
}

QString Utils::selectDir(QWidget *parent)
{
    return QString();
}