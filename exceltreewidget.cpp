#include "ExcelTreeWidget.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>

#include "xlsxdocument.h"
#include "xlsxcellrange.h"

ExcelTreeWidget::ExcelTreeWidget(QWidget *parent)
    : QTreeWidget{parent}
{
    setColumnCount(1);
    setHeaderLabel(tr("资源"));
    setUniformRowHeights(true);        // 大目录时性能更好
    setExpandsOnDoubleClick(true);

    connect(this, &QTreeWidget::itemClicked,
            this, &ExcelTreeWidget::onItemClicked);
}

void ExcelTreeWidget::loadDirectory(const QString &rootPath)
{
    clear();

    QFileInfo info(rootPath);
    if (!info.exists() || !info.isDir()) {
        qWarning() << "不是有效目录:" << rootPath;
        return;
    }

    auto *rootItem = new QTreeWidgetItem(this);
    rootItem->setText(0, info.fileName().isEmpty() ? rootPath : info.fileName());
    rootItem->setData(0, Qt::UserRole,     static_cast<int>(ExcelNodeType::Dir));
    rootItem->setData(0, Qt::UserRole + 1, info.absoluteFilePath());

    buildTree(info.absoluteFilePath(), rootItem);
    rootItem->setExpanded(true);
}

void ExcelTreeWidget::buildTree(const QString &dirPath, QTreeWidgetItem *parentItem)
{
    QDir dir(dirPath);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);

    for (const QFileInfo &fi : entries) {
        if (fi.isDir()) {
            auto *dirItem = new QTreeWidgetItem(parentItem);
            dirItem->setText(0, fi.fileName());
            dirItem->setData(0, Qt::UserRole,     static_cast<int>(ExcelNodeType::Dir));
            dirItem->setData(0, Qt::UserRole + 1, fi.absoluteFilePath());
            buildTree(fi.absoluteFilePath(), dirItem);
        } else if (fi.suffix().compare("xlsx", Qt::CaseInsensitive) == 0) {
            addExcelFile(fi.absoluteFilePath(), parentItem);
        }
    }
}

void ExcelTreeWidget::addExcelFile(const QString &filePath, QTreeWidgetItem *parentItem)
{
    QFileInfo fi(filePath);

    auto *fileItem = new QTreeWidgetItem(parentItem);
    fileItem->setText(0, fi.fileName());
    fileItem->setData(0, Qt::UserRole,     static_cast<int>(ExcelNodeType::File));
    fileItem->setData(0, Qt::UserRole + 1, filePath);

    // 打开一次，取出 sheet 名
    QXlsx::Document doc(filePath);
    const QStringList sheets = doc.sheetNames();

    for (const QString &sheetName : sheets) {
        auto *sheetItem = new QTreeWidgetItem(fileItem);
        sheetItem->setText(0, sheetName);
        sheetItem->setData(0, Qt::UserRole,     static_cast<int>(ExcelNodeType::Sheet));
        sheetItem->setData(0, Qt::UserRole + 1, filePath);
        sheetItem->setData(0, Qt::UserRole + 2, sheetName);
    }
}

void ExcelTreeWidget::onItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;

    const int type = item->data(0, Qt::UserRole).toInt();
    if (type != static_cast<int>(ExcelNodeType::Sheet))
        return;

    const QString filePath  = item->data(0, Qt::UserRole + 1).toString();
    const QString sheetName = item->data(0, Qt::UserRole + 2).toString();

    emit sheetSelected(filePath, sheetName);
}