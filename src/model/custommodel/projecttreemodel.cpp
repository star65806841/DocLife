#include "ProjectTreeModel.h"
#include "NodeTypeRegistry.h"

#include <QIcon>
#include <QList>
#include <QtAlgorithms>

#include <business/ProjectContext.h>   // 提供 ProjectNode 完整定义

namespace doclife::model {

using doclife::model::ProjectNode;

// =====================================================================
// 内部节点
// =====================================================================
struct ProjectTreeModel::Node
{
    ProjectNode data;
    Node* parent = nullptr;
    QList<Node*> children;

    ~Node()
    {
        qDeleteAll(children);
        children.clear();
    }
};

namespace {

QString titleForNode(const ProjectNode& n)
{
    QString title = n.name;
    if (title.isEmpty())
        title = n.attr(QStringLiteral("name"));
    if (title.isEmpty())
        title = NodeTypeRegistry::instance().displayLabel(n.type);
    return title;
}

QString subtitleForNode(const ProjectNode& n)
{
    const auto* s = NodeTypeRegistry::instance().spec(n.type);
    if (!s) return {};

    if (s->role == NodeRole::Root)
        return n.attr(QStringLiteral("docName"));

    if (s->role == NodeRole::Container)
        return QStringLiteral("%1 个子项").arg(n.children.size());

    // Leaf：展示 sheetName | filePath
    const QString sheet = n.attr(QStringLiteral("sheetName"));
    const QString path  = n.attr(QStringLiteral("filePath"));
    if (!sheet.isEmpty() && !path.isEmpty())
        return QStringLiteral("%1 | %2").arg(sheet, path);
    if (!sheet.isEmpty()) return sheet;
    return path;
}

} // namespace

// =====================================================================
ProjectTreeModel::ProjectTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    m_root = new Node;
}

ProjectTreeModel::~ProjectTreeModel()
{
    destroyTree();
}

void ProjectTreeModel::destroyTree()
{
    delete m_root;
    m_root = nullptr;
    m_idIndex.clear();
}

// =====================================================================
// 索引辅助
// =====================================================================
ProjectTreeModel::Node*
ProjectTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid()) return m_root;
    return static_cast<Node*>(index.internalPointer());
}

QModelIndex ProjectTreeModel::createIndexForNode(Node* node) const
{
    if (!node || !node->parent) return QModelIndex();
    Node* p = node->parent;
    const int row = p->children.indexOf(node);
    if (row < 0) return QModelIndex();
    return createIndex(row, 0, node);
}

// =====================================================================
// QAbstractItemModel
// =====================================================================
QModelIndex ProjectTreeModel::index(int row, int column,
                                    const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();
    Node* parentNode = parent.isValid() ? nodeFromIndex(parent) : m_root;
    if (!parentNode || row >= parentNode->children.size()) return QModelIndex();
    return createIndex(row, column, parentNode->children[row]);
}

QModelIndex ProjectTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) return QModelIndex();
    Node* childNode = nodeFromIndex(child);
    if (!childNode || !childNode->parent || childNode->parent == m_root)
        return QModelIndex();

    Node* p  = childNode->parent;
    Node* gp = p->parent;
    if (!gp) return QModelIndex();
    const int row = gp->children.indexOf(p);
    if (row < 0) return QModelIndex();
    return createIndex(row, 0, p);
}

int ProjectTreeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0) return 0;
    Node* parentNode = parent.isValid() ? nodeFromIndex(parent) : m_root;
    return parentNode ? parentNode->children.size() : 0;
}

int ProjectTreeModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant ProjectTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return {};
    Node* node = nodeFromIndex(index);
    if (!node) return {};

    const ProjectNode& n = node->data;

    if (index.column() == 0) {
        switch (role) {
        case Qt::DisplayRole:
            return titleForNode(n);
        case Qt::DecorationRole: {
            const auto* s = NodeTypeRegistry::instance().spec(n.type);
            return (s && !s->icon.isNull()) ? QVariant(s->icon) : QVariant();
        }
        case Qt::ToolTipRole:
            return subtitleForNode(n);
        case NodeTypeRole:
            return n.type;
        case NodeIdRole:
            return n.id;
        case SubtitleRole:
            return subtitleForNode(n);
        default:
            break;
        }
    }
    return {};
}

QVariant ProjectTreeModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0)
        return QStringLiteral("名称");
    return QAbstractItemModel::headerData(section, orientation, role);
}

Qt::ItemFlags ProjectTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    Node* node = nodeFromIndex(index);
    if (!node) return Qt::NoItemFlags;

    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    // 能否成为 drop 目标由"角色"决定，与业务类型无关
    if (NodeTypeRegistry::instance().canHaveChildren(node->data.type))
        f |= Qt::ItemIsDropEnabled;

    return f;
}

// =====================================================================
// 全量
// =====================================================================
void ProjectTreeModel::setRootNode(const ProjectNode& root)
{
    beginResetModel();
    destroyTree();
    m_root = new Node;

    Node* realRoot = new Node;
    realRoot->data   = root;
    realRoot->parent = m_root;
    m_root->children.append(realRoot);
    registerSubtree(realRoot);

    endResetModel();
}

void ProjectTreeModel::clear()
{
    beginResetModel();
    destroyTree();
    m_root = new Node;
    endResetModel();
}

// =====================================================================
// 增量
// =====================================================================
bool ProjectTreeModel::insertNode(const QString& parentId,
                                  const doclife::model::ProjectNode& node)
{
    Node* parentNode = parentId.isEmpty()
    ? m_root
    : m_idIndex.value(parentId, nullptr);
    if (!parentNode) return false;

    const int row        = parentNode->children.size();
    const QModelIndex pi = (parentNode == m_root)
                               ? QModelIndex()
                               : createIndexForNode(parentNode);

    beginInsertRows(pi, row, row);

    Node* newNode   = new Node;
    newNode->data   = node;
    newNode->parent = parentNode;
    parentNode->children.append(newNode);
    registerSubtree(newNode);

    endInsertRows();
    return true;
}

bool ProjectTreeModel::removeNode(const QString& nodeId)
{
    Node* node = m_idIndex.value(nodeId, nullptr);
    if (!node || !node->parent) return false;

    Node* parentNode = node->parent;
    const int row    = parentNode->children.indexOf(node);
    if (row < 0) return false;

    const QModelIndex pi = (parentNode == m_root)
                               ? QModelIndex()
                               : createIndexForNode(parentNode);

    beginRemoveRows(pi, row, row);
    unregisterSubtree(node);
    parentNode->children.removeAt(row);
    node->parent = nullptr;
    delete node;
    endRemoveRows();
    return true;
}

bool ProjectTreeModel::updateNode(const QString& nodeId,
                                  const doclife::model::ProjectNode& newNode)
{
    Node* node = m_idIndex.value(nodeId, nullptr);
    if (!node) return false;

    const QString oldId = node->data.id;      // QString，不再 QVariant 包装
    node->data = newNode;
    const QString newId = node->data.id;

    if (oldId != newId) {
        m_idIndex.remove(oldId);
        if (!newId.isEmpty())
            m_idIndex.insert(newId, node);
    }

    const QModelIndex idx = createIndexForNode(node);
    if (idx.isValid())
        emit dataChanged(idx, idx);
    return true;
}
// =====================================================================
// 索引维护
// =====================================================================
void ProjectTreeModel::registerSubtree(Node* node)
{
    if (!node) return;
    const QString id = node->data.id;
    if (!id.isEmpty())
        m_idIndex.insert(id, node);           // ★ 不再 QVariant(id)
    for (Node* c : node->children)
        registerSubtree(c);
}

void ProjectTreeModel::unregisterSubtree(Node* node)
{
    if (!node) return;
    for (Node* c : node->children)
        unregisterSubtree(c);
    const QString id = node->data.id;
    if (!id.isEmpty())
        m_idIndex.remove(id);
}

// =====================================================================
// 查找
// =====================================================================
QModelIndex ProjectTreeModel::indexForId(const QString& nodeId) const
{
    Node* node = m_idIndex.value(nodeId, nullptr);
    if (!node) return QModelIndex();
    return createIndexForNode(node);
}

QString ProjectTreeModel::nodeTypeForId(const QString& nodeId) const
{
    Node* node = m_idIndex.value(nodeId, nullptr);
    return node ? node->data.type : QString();
}

} // namespace doclife::ui