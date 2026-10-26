#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>

class QTreeWidget;
class QTreeWidgetItem;

namespace doclife::ui {

class CatalogView : public QWidget
{
    Q_OBJECT
public:
    explicit CatalogView(QWidget *parent = nullptr);

    void setRootPath(const QString &qrcRoot);
    void reload();

private:
    void initUi();
    void buildTree();
    void fillTreeItem(QTreeWidgetItem *parent, const QString &path);

    void onItemExpanded(QTreeWidgetItem *item);
    void onItemClicked(QTreeWidgetItem *item, int column);

    void ensureSheetsLoaded(QTreeWidgetItem *fileItem);
    QStringList readSheetNames(const QString &resourcePath) const;

    QTreeWidget *m_tree = nullptr;
    QString m_rootPath = QStringLiteral(":/data");
};

} // namespace doclife::ui