#include "catalogview.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <uisignalmanager.h>

namespace {
    enum NodeType { NodeDir = 0, NodeFile = 1, NodeSheet = 2, NodePlaceholder = -1 };
    inline QString nodePath(const QTreeWidgetItem* item) { return item->data(0, Qt::UserRole).toString(); }
    inline int nodeType(const QTreeWidgetItem* item) { return item->data(0, Qt::UserRole + 1).toInt(); }
    inline QString nodeSheet(const QTreeWidgetItem* item) { return item->data(0, Qt::UserRole + 2).toString(); }
}

namespace doclife::ui {

    CatalogView::CatalogView(QWidget* parent) : QWidget(parent) {
        initUi();
        reload();
        connect(UISignalManager::instance(), &UISignalManager::sheetNamesLoaded, this, &CatalogView::onSheetNamesLoaded);
        connect(m_tree, &QTreeWidget::itemExpanded, this, &CatalogView::onItemExpanded);
    }

    void CatalogView::setRootPath(const QString& qrcRoot) { m_rootPath = qrcRoot; reload(); }
    void CatalogView::reload() { buildTree(); }

    void CatalogView::initUi() {
        m_tree = new QTreeWidget(this);
        m_tree->setHeaderHidden(true);
        m_tree->setColumnCount(1);
        m_tree->setUniformRowHeights(true);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_tree);
        connect(m_tree, &QTreeWidget::itemClicked, this, &CatalogView::onItemClicked);
    }

    void CatalogView::buildTree() {
        m_tree->clear();
        if (m_rootPath.isEmpty()) return;
        auto* root = new QTreeWidgetItem(m_tree);
        root->setText(0, QStringLiteral("资料库"));
        root->setData(0, Qt::UserRole, m_rootPath);
        root->setData(0, Qt::UserRole + 1, NodeDir);
        fillTreeItem(root, m_rootPath);
        root->setExpanded(true);
        m_tree->expandToDepth(1);
    }

    void CatalogView::fillTreeItem(QTreeWidgetItem* parent, const QString& path) {
        const QDir dir(path);
        const auto entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
        for (const QFileInfo& fi : entries) {
            if (fi.isDir()) {
                auto* child = new QTreeWidgetItem(parent);
                child->setText(0, fi.fileName());
                child->setData(0, Qt::UserRole, fi.filePath());
                child->setData(0, Qt::UserRole + 1, NodeDir);
                fillTreeItem(child, fi.filePath());
            }
            else if (fi.suffix().compare(QLatin1String("xlsx"), Qt::CaseInsensitive) == 0) {
                auto* fileItem = new QTreeWidgetItem(parent);
                fileItem->setText(0, fi.fileName());
                fileItem->setData(0, Qt::UserRole, fi.filePath());
                fileItem->setData(0, Qt::UserRole + 1, NodeFile);
                auto* ph = new QTreeWidgetItem(fileItem);
                ph->setText(0, QStringLiteral("加载中..."));
                ph->setData(0, Qt::UserRole + 1, NodePlaceholder);
            }
        }
    }

    void CatalogView::onItemClicked(QTreeWidgetItem* item, int) {
        if (!item) return;
        qDebug() << "[CatalogView] Item clicked:" << item->text(0) << "Type:" << nodeType(item);
        const int type = nodeType(item);

        if (type == NodeFile) {
            qDebug() << "[CatalogView] File clicked, emitting signal:" << nodePath(item);
            emit UISignalManager::instance()->excelFileActivated(nodePath(item));
            if (!item->isExpanded()) item->setExpanded(true);
        }
        else if (type == NodeSheet) {
            qDebug() << "[CatalogView] Sheet clicked, emitting signal:" << nodePath(item) << nodeSheet(item);
            emit UISignalManager::instance()->excelSheetActivated(nodePath(item), nodeSheet(item));
        }
    }

    void CatalogView::onItemExpanded(QTreeWidgetItem* item) {
        if (!item) return;

        // 只有当展开的是文件节点，且它下面还挂着“加载中...”占位符时，才发起真正的加载请求
        if (nodeType(item) == NodeFile) {
            if (item->childCount() > 0 && nodeType(item->child(0)) == NodePlaceholder) {
                qDebug() << "[CatalogView] Expanded file, emitting signal:" << nodePath(item);
                emit UISignalManager::instance()->excelFileActivated(nodePath(item));
            }
        }
    }

    void CatalogView::onSheetNamesLoaded(const QString& xlsxPath, const QStringList& sheetNames) {
        QTreeWidgetItemIterator it(m_tree);
        while (*it) {
            if (nodeType(*it) == NodeFile && nodePath(*it) == xlsxPath) {
                QTreeWidgetItem* fileItem = *it;

                // 防重复
                if (fileItem->childCount() > 0) {
                    if (nodeType(fileItem->child(0)) != NodePlaceholder) {
                        break;
                    }
                }

                // 移除占位符
                if (fileItem->childCount() == 1 && nodeType(fileItem->child(0)) == NodePlaceholder) {
                    delete fileItem->takeChild(0);
                }

                // 👇 添加这里：生成真正的 Sheet 节点
                for (const QString& name : sheetNames) {
                    auto* sheetItem = new QTreeWidgetItem(fileItem);
                    sheetItem->setText(0, name);
                    sheetItem->setData(0, Qt::UserRole, xlsxPath);
                    sheetItem->setData(0, Qt::UserRole + 1, NodeSheet);
                    sheetItem->setData(0, Qt::UserRole + 2, name);
                }
                break;
            }
            ++it;
        }
    }
} // namespace doclife::ui