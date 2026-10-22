#include "projectTreeModel.h"
#include "NodeTypeRegistry.h"
#include "ProjectContext.h"          // ← 新增

#include <QIcon>
#include <QDebug>
#include <QtAlgorithms>

namespace doclife::model {

// ============================================================
// 构造：自动与业务层连线 + 若项目已打开则直接同步
// ============================================================
ProjectTreeModel::ProjectTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    auto* ctx = doclife::core::ProjectContext::instance();

    // ---- 生命周期 ----
    connect(ctx, &doclife::core::ProjectContext::projectOpened,
            this, [this](const QString&) {
                setRootNode(doclife::core::ProjectContext::instance()->rootNode());
            });
    connect(ctx, &doclife::core::ProjectContext::projectCreated,
            this, [this](const QString&) {
                setRootNode(doclife::core::ProjectContext::instance()->rootNode());
            });
    connect(ctx, &doclife::core::ProjectContext::projectClosed,
            this, &ProjectTreeModel::clear);

    // ---- 增量同步 ----
    connect(ctx, &doclife::core::ProjectContext::nodeAdded,
            this, &ProjectTreeModel::onNodeAdded, Qt::UniqueConnection);
    connect(ctx, &doclife::core::ProjectContext::nodeRemoved,
            this, &ProjectTreeModel::onNodeRemoved, Qt::UniqueConnection);
    connect(ctx, &doclife::core::ProjectContext::nodeUpdated,
            this, &ProjectTreeModel::onNodeUpdated, Qt::UniqueConnection);

    // ---- 视图重命名意图 → 业务层 ----
    connect(this, &ProjectTreeModel::renameRequested,
            ctx, [ctx](const QString& id, const QString& newName) {
                QString err;
                if (!ctx->renameNode(id, newName, &err)) {
                    qWarning() << "[ProjectTreeModel] rename failed:" << err;
                }
            });

    // 构造时若项目已打开，直接同步（避免先开项目后建模型）
    if (ctx->hasOpenProject() && ctx->rootNode()) {
        m_root = ctx->rootNode();
        resetIndex();
    }
}

ProjectTreeModel::~ProjectTreeModel()
{
    m_idIndex.clear();
    m_root = nullptr;
}

// ============================================================
// 根 / 索引维护
// ============================================================
void ProjectTreeModel::setRootNode(Node* root)
{
    beginResetModel();
    m_root = root;              // 非拥有
    resetIndex();
    endResetModel();
}

void ProjectTreeModel::clear()
{
    beginResetModel();
    m_root = nullptr;
    m_idIndex.clear();
    endResetModel();
}

void ProjectTreeModel::resetIndex()
{
    m_idIndex.clear();
    if (m_root) registerSubtree(m_root);
}

void ProjectTreeModel::registerSubtree(Node* node)
{
    if (!node) return;
    if (!node->data.id.isEmpty()) m_idIndex.insert(node->data.id, node);
    for (Node* c : node->children) registerSubtree(c);
}

void ProjectTreeModel::unregisterSubtree(Node* node)
{
    if (!node) return;
    for (Node* c : node->children) unregisterSubtree(c);
    if (!node->data.id.isEmpty()) m_idIndex.remove(node->data.id);
}

// ============================================================
// 索引辅助：根节点现在也是有效的
// ============================================================
Node* ProjectTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid()) return nullptr;
    return static_cast<Node*>(index.internalPointer());
}

QModelIndex ProjectTreeModel::createIndexForNode(Node* node) const
{
    if (!node || !m_root) return QModelIndex();

    // 根节点：树中唯一顶层，row = 0
    if (node == m_root) {
        return createIndex(0, 0, node);
    }

    Node* p = node->parent;
    if (!p) return QModelIndex();
    const int row = p->children.indexOf(node);
    if (row < 0) return QModelIndex();
    return createIndex(row, 0, node);
}

// ============================================================
// QAbstractItemModel 接口
// ============================================================
QModelIndex ProjectTreeModel::index(int row, int column,
                                    const QModelIndex& parent) const
{
    if (!m_root || !hasIndex(row, column, parent)) return QModelIndex();
    if (column != 0) return QModelIndex();

    if (!parent.isValid()) {
        // 顶层只有根节点一项
        if (row == 0) return createIndex(0, 0, m_root);
        return QModelIndex();
    }

    Node* parentNode = nodeFromIndex(parent);
    if (!parentNode) return QModelIndex();
    if (row < 0 || row >= parentNode->children.size()) return QModelIndex();
    return createIndex(row, column, parentNode->children[row]);
}

QModelIndex ProjectTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || !m_root) return QModelIndex();

    Node* childNode = nodeFromIndex(child);
    if (!childNode || childNode == m_root) return QModelIndex();

    Node* p = childNode->parent;
    if (!p) return QModelIndex();
    if (p == m_root) return createIndex(0, 0, m_root);  // 父是根节点
    return createIndexForNode(p);
}

int ProjectTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!m_root) return 0;
    if (parent.column() > 0) return 0;

    // 顶层：只有根节点
    if (!parent.isValid()) return 1;

    Node* parentNode = nodeFromIndex(parent);
    if (!parentNode) return 0;
    return parentNode->children.size();
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

    const NodeAttributes& n = node->data;

    if (index.column() == 0) {
        switch (role) {
        case Qt::DisplayRole:
            return titleForNode(index);

        case Qt::EditRole:                       // ← 新增：编辑器初始文本
            // 用真实名字，不用 titleForNode() 的 fallback，
            // 否则首次编辑就会把"单位工程"这类占位文字写死成名字
            return n.name;

        case Qt::DecorationRole: {
            const auto* s = NodeTypeRegistry::instance().spec(n.type);
            return (s && !s->icon.isNull()) ? QVariant(s->icon) : QVariant();
        }
        case Qt::ToolTipRole:
            return subtitleForNode(index);
        case NodeTypeRole:
            return n.type;
        case NodeIdRole:
            return n.id;
        case SubtitleRole:
            return subtitleForNode(index);
        default:
            break;
        }
    }
    return {};
}

bool ProjectTreeModel::setData(const QModelIndex& index,
                               const QVariant& value, int role)
{
    if (!index.isValid() || index.column() != 0)
        return false;

    if (role != Qt::EditRole)
        return QAbstractItemModel::setData(index, value, role);

    Node* node = nodeFromIndex(index);
    if (!node) return false;

    const QString newName = value.toString().trimmed();
    if (newName.isEmpty()) return false;
    if (newName == node->data.name) return true;

    // 交给业务层，成功后由 nodeUpdated → onNodeUpdated 刷新
    emit renameRequested(node->data.id, newName);
    return false;
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

    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    if (NodeTypeRegistry::instance().canHaveChildren(node->data.type))
        f |= Qt::ItemIsDropEnabled;
    return f;
}

// ============================================================
// 查找
// ============================================================
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

QString ProjectTreeModel::titleForNode(const QModelIndex& index) const
{
    Node* node = nodeFromIndex(index);
    if (!node) return QString{};

    QString title = node->data.name;
    if (title.isEmpty())
        title = node->data.attr(QStringLiteral("name"));
    if (title.isEmpty())
        title = NodeTypeRegistry::instance().displayLabel(node->data.type);
    return title;
}

QString ProjectTreeModel::subtitleForNode(const QModelIndex& index) const
{
    Node* node = nodeFromIndex(index);
    if (!node) return QString{};

    const auto* s = NodeTypeRegistry::instance().spec(node->data.type);
    if (!s) return {};

    if (s->role == NodeRole::Root || s->role == NodeRole::Container)
        return QStringLiteral("%1 个子项").arg(node->children.size());

    const QString sheet = node->data.attr(QStringLiteral("sheetName"));
    const QString path  = node->data.attr(QStringLiteral("filePath"));
    if (!sheet.isEmpty() && !path.isEmpty())
        return QStringLiteral("%1 | %2").arg(sheet, path);
    if (!sheet.isEmpty()) return sheet;
    return path;
}

// ============================================================
// 业务层信号同步
// ============================================================
void ProjectTreeModel::onNodeAdded(const QString& parentId, const QString& nodeId)
{
    if (!m_root) return;

    Node* parent = m_idIndex.value(parentId, nullptr);
    if (!parent) return;

    Node* child = nullptr;
    for (Node* c : parent->children) {
        if (c->data.id == nodeId) { child = c; break; }
    }
    if (!child) return;

    const int row = parent->children.indexOf(child);
    // 关键：根节点现在也有有效索引
    const QModelIndex pi = createIndexForNode(parent);

    beginInsertRows(pi, row, row);
    if (!nodeId.isEmpty()) m_idIndex.insert(nodeId, child);
    endInsertRows();
}

void ProjectTreeModel::onNodeRemoved(const QString& /*parentId*/, const QString& nodeId)
{
    if (!m_root) return;

    Node* node = m_idIndex.value(nodeId, nullptr);
    if (!node || !node->parent) return;

    Node* parent = node->parent;
    const int row = parent->children.indexOf(node);
    if (row < 0) return;

    const QModelIndex pi = createIndexForNode(parent);

    beginRemoveRows(pi, row, row);
    unregisterSubtree(node);
    parent->children.removeAt(row);
    node->parent = nullptr;
    endRemoveRows();
}

void ProjectTreeModel::onNodeUpdated(const QString& nodeId)
{
    Node* node = m_idIndex.value(nodeId, nullptr);
    if (!node) return;

    const QModelIndex idx = createIndexForNode(node);
    if (idx.isValid())
        emit dataChanged(idx, idx);
}

} // namespace doclife::model