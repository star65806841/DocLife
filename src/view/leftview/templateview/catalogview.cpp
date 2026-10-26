#include "catalogview.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QBuffer>
#include <QDebug>

#include "xlsxdocument.h"

#include <uisignalmanager.h>

namespace {

enum NodeType {
    NodeDir         = 0,
    NodeFile        = 1,
    NodeSheet       = 2,
    NodePlaceholder = -1,
    NodeError       = -2
};

inline QString nodePath(const QTreeWidgetItem *item)
{ return item->data(0, Qt::UserRole).toString(); }

inline int nodeType(const QTreeWidgetItem *item)
{ return item->data(0, Qt::UserRole + 1).toInt(); }

inline QString nodeSheet(const QTreeWidgetItem *item)
{ return item->data(0, Qt::UserRole + 2).toString(); }

} // namespace

namespace doclife::ui {

CatalogView::CatalogView(QWidget *parent)
    : QWidget(parent)
{
    initUi();
    reload();
}

void CatalogView::setRootPath(const QString &qrcRoot)
{
    m_rootPath = qrcRoot;
    reload();
}

void CatalogView::reload()
{
    buildTree();
}

void CatalogView::initUi()
{
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setUniformRowHeights(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemExpanded, this, &CatalogView::onItemExpanded);
    connect(m_tree, &QTreeWidget::itemClicked,  this, &CatalogView::onItemClicked);
}

void CatalogView::buildTree()
{
    m_tree->clear();
    if (m_rootPath.isEmpty()) return;

    const QFileInfo rootInfo(m_rootPath);
    const QString rootLabel = rootInfo.fileName().isEmpty()
                                  ? QStringLiteral("资料库")
                                  : rootInfo.fileName();

    auto *root = new QTreeWidgetItem(m_tree);
    root->setText(0, rootLabel);
    root->setData(0, Qt::UserRole,     m_rootPath);
    root->setData(0, Qt::UserRole + 1, NodeDir);

    fillTreeItem(root, m_rootPath);
    root->setExpanded(true);
    m_tree->expandToDepth(1);
}

void CatalogView::fillTreeItem(QTreeWidgetItem *parent, const QString &path)
{
    const QDir dir(path);
    const auto entries = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);

    for (const QFileInfo &fi : entries) {
        if (fi.isDir()) {
            auto *child = new QTreeWidgetItem(parent);
            child->setText(0, fi.fileName());
            child->setData(0, Qt::UserRole,     fi.filePath());
            child->setData(0, Qt::UserRole + 1, NodeDir);
            fillTreeItem(child, fi.filePath());
        } else if (fi.suffix().compare(QLatin1String("xlsx"), Qt::CaseInsensitive) == 0) {
            auto *fileItem = new QTreeWidgetItem(parent);
            fileItem->setText(0, fi.fileName());
            fileItem->setData(0, Qt::UserRole,     fi.filePath());
            fileItem->setData(0, Qt::UserRole + 1, NodeFile);

            auto *ph = new QTreeWidgetItem(fileItem);
            ph->setText(0, QStringLiteral("加载中…"));
            ph->setData(0, Qt::UserRole + 1, NodePlaceholder);
        }
    }
}

void CatalogView::onItemExpanded(QTreeWidgetItem *item)
{
    ensureSheetsLoaded(item);
}

void CatalogView::onItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    const int type = nodeType(item);

    if (type == NodeFile) {
        ensureSheetsLoaded(item);
        if (!item->isExpanded()) item->setExpanded(true);

        if (item->childCount() > 0) {
            QTreeWidgetItem *first = item->child(0);
            if (nodeType(first) == NodeSheet)
                emit UISignalManager::instance()->excelSheetActivated(
                    nodePath(first), nodeSheet(first));
        }
    } else if (type == NodeSheet) {
        emit UISignalManager::instance()->excelSheetActivated(
            nodePath(item), nodeSheet(item));
    }
}

void CatalogView::ensureSheetsLoaded(QTreeWidgetItem *fileItem)
{
    if (!fileItem) return;
    if (nodeType(fileItem) != NodeFile) return;

    if (fileItem->childCount() == 1) {
        if (nodeType(fileItem->child(0)) != NodePlaceholder)
            return;
    } else if (fileItem->childCount() > 1) {
        return;
    }

    const QString path = nodePath(fileItem);
    const QStringList sheets = readSheetNames(path);

    if (fileItem->childCount() == 1)
        delete fileItem->takeChild(0);

    if (sheets.isEmpty()) {
        auto *err = new QTreeWidgetItem(fileItem);
        err->setText(0, QStringLiteral("(无 sheet 或解析失败)"));
        err->setData(0, Qt::UserRole + 1, NodeError);
        return;
    }

    for (const QString &name : sheets) {
        auto *sheetItem = new QTreeWidgetItem(fileItem);
        sheetItem->setText(0, name);
        sheetItem->setData(0, Qt::UserRole,     path);
        sheetItem->setData(0, Qt::UserRole + 1, NodeSheet);
        sheetItem->setData(0, Qt::UserRole + 2, name);
    }
}

QStringList CatalogView::readSheetNames(const QString &resourcePath) const
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "打开资源失败:" << resourcePath;
        return {};
    }
    const QByteArray data = file.readAll();
    file.close();

    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);

    QXlsx::Document doc(&buffer);
    return doc.sheetNames();
}

} // namespace doclife::ui