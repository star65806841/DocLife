#pragma once

#include <QAbstractItemModel>
#include <QVariant>
#include <QHash>

namespace doclife::model { struct ProjectNode; }

namespace doclife::ui {

// 自定义模型：内部持有 ProjectNode 树的拷贝，
// 支持全量 reset 和单节点增删改。
class ProjectTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    // 与原有 QStandardItem 角色值保持一致
    enum Role {
        NodeTypeRole = Qt::UserRole + 1,   // "project"/"unit"/"sub"/"item"/"sheet"
        NodeIdRole   = Qt::UserRole + 2,   // 节点 id（QVariant）
        SubtitleRole = Qt::UserRole + 3,   // 副标题
    };
    Q_ENUM(Role)

    explicit ProjectTreeModel(QObject* parent = nullptr);
    ~ProjectTreeModel() override;

    // ---- QAbstractItemModel 接口 ----
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // ---- 全量更新（打开/关闭/创建项目） ----
    void setRootNode(const doclife::model::ProjectNode& root);
    void clear();

    // ---- 增量更新（供 ProjectContext 节点级信号调用） ----
    // 追加到 parentId 节点的末尾；parentId 无效时追加到虚拟根（即顶级）。
    bool insertNode(const QVariant& parentId,
                    const doclife::model::ProjectNode& node);
    // 删除节点及其子树。
    bool removeNode(const QVariant& nodeId);
    // 只更新该节点的 data（不处理 children 的变化）；children 变化请用 remove+insert。
    bool updateNode(const QVariant& nodeId,
                    const doclife::model::ProjectNode& newNode);

    // ---- 查找 ----
    QModelIndex indexForId(const QVariant& nodeId) const;
    QString nodeTypeForId(const QVariant& nodeId) const;

private:
    struct Node;

    Node* nodeFromIndex(const QModelIndex& index) const;
    QModelIndex createIndexForNode(Node* node) const;
    void destroyTree();
    void registerSubtree(Node* node);
    void unregisterSubtree(Node* node);

    static QString titleFor(const doclife::model::ProjectNode& n);
    static QString subtitleFor(const doclife::model::ProjectNode& n);
    static QString labelFor(const QString& type);

    Node* m_root = nullptr;                 // 虚拟根，不显示
    QHash<QVariant, Node*> m_idIndex;       // id → 节点
};

} // namespace doclife::ui