#pragma once

#include <QAbstractItemModel>
#include <QVariant>
#include <QHash>
#include <QString>

#include "projectNode.h"

namespace doclife::model {

class ProjectTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Role {
        NodeTypeRole = Qt::UserRole + 1,
        NodeIdRole   = Qt::UserRole + 2,
        SubtitleRole = Qt::UserRole + 3,
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

    // ---- 根节点（非拥有，所有权在 ProjectContext） ----
    void setRootNode(Node* root);
    Node* rootNode() const { return m_root; }
    void clear();

    // ---- 查找 ----
    QModelIndex indexForId(const QString& nodeId) const;
    QString     nodeTypeForId(const QString& nodeId) const;

public slots:
    // 业务层同步入口（connect 到 ProjectContext 的信号）
    void onNodeAdded(const QString& parentId, const QString& nodeId);
    void onNodeRemoved(const QString& parentId, const QString& nodeId);
    void onNodeUpdated(const QString& nodeId);

signals:
    // 视图触发重命名 → 业务层响应（业务层成功后 emit nodeUpdated 回来刷新）
    void renameRequested(const QString& nodeId, const QString& newName);

private:
    Node*        nodeFromIndex(const QModelIndex& index) const;
    QModelIndex  createIndexForNode(Node* node) const;
    QString      titleForNode(const QModelIndex& index) const;
    QString      subtitleForNode(const QModelIndex& index) const;
    void         registerSubtree(Node* node);
    void         unregisterSubtree(Node* node);
    void         resetIndex();

    Node*                  m_root = nullptr;   // 非拥有
    QHash<QString, Node*>  m_idIndex;
};

} // namespace doclife::model