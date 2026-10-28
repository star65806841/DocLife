#pragma once
#include <QWidget>
#include <QString>
#include <QStringList>

class QTreeWidget;
class QTreeWidgetItem;

namespace doclife::ui {
    class CatalogView : public QWidget {
        Q_OBJECT
    public:
        explicit CatalogView(QWidget* parent = nullptr);
        void setRootPath(const QString& qrcRoot);
        void reload();

    private slots:
        void onItemClicked(QTreeWidgetItem* item, int column);
        void onItemExpanded(QTreeWidgetItem* item);
        void onSheetNamesLoaded(const QString& xlsxPath, const QStringList& sheetNames);

    private:
        void initUi();
        void buildTree();
        void fillTreeItem(QTreeWidgetItem* parent, const QString& path);
        QTreeWidget* m_tree = nullptr;
        QString m_rootPath = QStringLiteral(":/data");
    };
}