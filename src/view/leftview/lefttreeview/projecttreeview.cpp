#include "ProjectTreeView.h"

#include <QVBoxLayout>
#include <QVariant>
#include <QSet>
#include <QModelIndex>
#include <QRect>
#include <functional>

#include "business/ProjectContext.h"
 #include "ProjectContextMenuBuilder.h"

#include "fluenttreeitemdelegate.h"
#include "model/custommodel/ProjectTreeModel.h"

#include "components/collections/TreeView.h"
#include <components/menus_toolbars/CommandBarFlyout.h>

namespace doclife::ui {

    // ===============================================================
    ProjectTreeView::ProjectTreeView(QWidget* parent)
        : QWidget(parent)
    {
        m_model = new doclife::model::ProjectTreeModel(this);

        m_tree = new fluent::collections::TreeView(this);
        m_tree->setModel(m_model);
        m_tree->setIndicatorMotionAnimationEnabled(true);
        m_tree->setSelectionMode(fluent::collections::SelectionMode::Single);
        m_tree->setHeaderHidden(true);
        // 将右键菜单策略设置为自定义上下文菜单 右键发出customContextMenuRequested
        m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

        auto* delegate = new FluentTreeItemDelegate();
        m_tree->setItemDelegate(delegate);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_tree);

        // 拖放重排需要 ProjectTreeModel 实现 mimeData/dropMimeData，
        // 当前版本暂不启用。
        // m_tree->setCanReorderItems(true);

        // 右键激发
        connect(m_tree, &fluent::collections::TreeView::customContextMenuRequested,
            this, &ProjectTreeView::onCustomContextMenuRequested);

        // ---- 全量信号：打开 / 创建 / 关闭 ----
        auto* ctx = doclife::core::ProjectContext::instance();
        connect(ctx, &doclife::core::ProjectContext::projectOpened,
            this, &ProjectTreeView::rebuildAll);
        connect(ctx, &doclife::core::ProjectContext::projectCreated,
            this, &ProjectTreeView::rebuildAll);
        connect(ctx, &doclife::core::ProjectContext::projectClosed,
            this, &ProjectTreeView::rebuildAll);

        // ---- 增量信号：节点增删改，直接驱动模型 ----
        // connect(ctx, &doclife::core::ProjectContext::nodeAdded,
        //         this, [this](const QString& parentId, const QString& nodeId) {
        //             const auto* n = doclife::core::ProjectContext::instance()->findNodeById(nodeId);
        //             if (!n) return;
        //             m_model->insertNode(parentId, *n);
        //             const QModelIndex pi = m_model->indexForId(parentId);
        //             if (pi.isValid()) m_tree->expand(pi);
        //         });
        // connect(ctx, &doclife::core::ProjectContext::nodeRemoved,
        //         this, [this](const QString& /*parentId*/, const QString& nodeId) {
        //             m_model->removeNode(nodeId);
        //         });
        // connect(ctx, &doclife::core::ProjectContext::nodeUpdated,
        //         this, [this](const QString& nodeId) {
        //             const auto* n = doclife::core::ProjectContext::instance()->findNodeById(nodeId);
        //             if (!n) return;
        //             m_model->updateNode(nodeId, *n);
        //         });

        // 初始状态（可能已经打开着项目）
        rebuildAll();
    }

    // ===============================================================
    void ProjectTreeView::rebuildAll()
    {
        // ---- 1. 快照：展开 / 折叠 / 选中 ----
        QSet<QString> expandedIds;
        QSet<QString> collapsedIds;

        std::function<void(const QModelIndex&)> collect =
            [&](const QModelIndex& parent) {
            const int rows = m_model->rowCount(parent);
            for (int r = 0; r < rows; ++r) {
                const QModelIndex idx = m_model->index(r, 0, parent);
                const QString id = idx.data(doclife::model::ProjectTreeModel::NodeIdRole).toString();
                if (m_tree->isExpanded(idx))
                    expandedIds.insert(id);
                else
                    collapsedIds.insert(id);
                collect(idx);
            }
            };
        collect(QModelIndex());

        QString selectedId;
        {
            const QModelIndex cur = m_tree->currentIndex();
            if (cur.isValid())
                selectedId = cur.data(doclife::model::ProjectTreeModel::ProjectTreeModel::NodeIdRole).toString();
        }

        // ---- 2. 数据更新 ----
        auto* ctx = doclife::core::ProjectContext::instance();
        if (!ctx->hasOpenProject()) {
            m_model->clear();
            return;
        }
        const auto& root = ctx->rootNode();
        if (root.type.isEmpty()) {
            m_model->clear();
            return;
        }
        m_model->setRootNode(root);

        // ---- 3. 恢复展开 / 选中 ----
        std::function<void(const QModelIndex&)> restore =
            [&](const QModelIndex& parent) {
            const int rows = m_model->rowCount(parent);
            for (int r = 0; r < rows; ++r) {
                const QModelIndex idx = m_model->index(r, 0, parent);
                const QString id = idx.data(doclife::model::ProjectTreeModel::ProjectTreeModel::NodeIdRole).toString();
                if (expandedIds.contains(id))
                    m_tree->expand(idx);
                if (id == selectedId)
                    m_tree->setCurrentIndex(idx);
                restore(idx);
            }
            };
        restore(QModelIndex());

        // 首次构建（没有任何历史快照）时全部展开
        if (expandedIds.isEmpty() && collapsedIds.isEmpty())
            m_tree->expandAll();
    }

    // ===============================================================
    void ProjectTreeView::onCustomContextMenuRequested(const QPoint& pos)
    {
        const QModelIndex index = m_tree->indexAt(pos);
        if (!index.isValid()) return;

        const QString  nodeType = index.data(doclife::model::ProjectTreeModel::ProjectTreeModel::NodeTypeRole).toString();
        const QVariant nodeId = index.data(doclife::model::ProjectTreeModel::ProjectTreeModel::NodeIdRole);
        if (nodeType.isEmpty()) return;

        m_tree->setCurrentIndex(index);

        fluent::menus_toolbars::CommandBarFlyout* flyout = ProjectContextMenuBuilder::build(this, nodeType, nodeId);
        if (!flyout) return;

        // 锚点：节点水平中点、下边缘
        const QRect rect = m_tree->visualRect(index);
        const QPoint anchor(rect.center().x(), rect.bottom());
        flyout->showAtPoint(m_tree->viewport(), anchor);
    }

} // namespace doclife::ui