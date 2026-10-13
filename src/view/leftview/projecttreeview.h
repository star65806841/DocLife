#pragma once

#include <QWidget>

class QStandardItemModel;
class QStandardItem;

namespace fluent::collections {
class TreeView;
}

namespace doclife::ui {

/**
 * @brief 用一个 TreeView 展示 ProjectContext 中所有项目
 *        —— 每个项目是树的一个顶层节点
 */
class ProjectTreeView : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectTreeView(QWidget *parent = nullptr);
    ~ProjectTreeView() override = default;

private slots:
    // ProjectContext 变化时调用
    void rebuildAll();                              // 全量重建
    void onProjectUpdated(const QString& projectId); // 局部重建单棵子树

private:
    // 递归构建：ProjectHead → QStandardItem
    QStandardItem* makeProjectItem(const QString& projectId);

    // 找到某项目在 model 顶层的行号（找不到返回 -1）
    int findProjectRow(const QString& projectId) const;

private:
    fluent::collections::TreeView* m_tree  = nullptr;
    QStandardItemModel*            m_model = nullptr;
};

} // namespace doclife::ui