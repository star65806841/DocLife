#pragma once

#include <QAbstractItemModel>
#include <QVariant>
#include <QHash>
#include <QString>

namespace doclife::model {
struct ProjectNode;
// 邻接表模型：内部持有 ProjectNode 树拷贝，任意深度、任意业务类型。
// 结构约束（能否有子）由 NodeTypeRegistry 决定，模型自身不硬编码任何类型。
class ProjectTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Role {
        NodeTypeRole = Qt::UserRole + 1,   // 业务类型字符串
        NodeIdRole   = Qt::UserRole + 2,   // 节点 id（QString）
        SubtitleRole = Qt::UserRole + 3,   // 副标题
    };
    Q_ENUM(Role)

    explicit ProjectTreeModel(QObject* parent = nullptr);
    ~ProjectTreeModel() override;

    // ---- QAbstractItemModel ----
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value,
                 int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // ---- 全量 ----
    void setRootNode(const doclife::model::ProjectNode& root);
    void clear();

    // ---- 增量 ----
    // parentId 无效时追加到虚拟根（即顶级）
    bool insertNode(const QString& parentId,
                    const doclife::model::ProjectNode& node);
    bool removeNode(const QString& nodeId);
    bool updateNode(const QString& nodeId,
                    const doclife::model::ProjectNode& newNode);

    // ---- 查找 ----
    QModelIndex indexForId(const QString& nodeId) const;
    QString nodeTypeForId(const QString& nodeId) const;

private:
    struct Node;

    Node* nodeFromIndex(const QModelIndex& index) const;
    QModelIndex createIndexForNode(Node* node) const;
    void destroyTree();
    void registerSubtree(Node* node);
    void unregisterSubtree(Node* node);

    Node* m_root = nullptr;                 // 虚拟根，不显示
    QHash<QString, Node*> m_idIndex;        // id → 节点
};

} // namespace doclife::ui