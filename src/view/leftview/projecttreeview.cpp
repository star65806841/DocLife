#include "ProjectTreeView.h"
#include "fluenttreeitemdelegate.h"

#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QStandardItem>

#include <components/collections/TreeView.h>
#include <business/ProjectContext.h>

namespace doclife::ui {

// ---------- 自定义数据角色 ----------
namespace {
constexpr int RoleNodeType = Qt::UserRole + 1;   // "project"/"unit"/"sub"/"item"/"sheet"
constexpr int RoleNodeId   = Qt::UserRole + 2;   // 节点的 id
constexpr int RoleSubtitle = Qt::UserRole + 3;   // 副标题（显示在第二列）
}

// ===============================================================
ProjectTreeView::ProjectTreeView(QWidget* parent)
    : QWidget(parent)
{
    // ---- Model ----
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("信息")});

    // ---- TreeView ----
    m_tree = new fluent::collections::TreeView(this);
    m_tree->setModel(m_model);
    m_tree->setIndicatorMotionAnimationEnabled(true);
    m_tree->setSelectionMode(fluent::collections::SelectionMode::Single);
    m_tree->setHeaderHidden(false);

    // auto* delegate = new treeview_test::FluentTreeItemDelegate(m_tree, 42, m_tree);
    // auto* delegate = new FluentTreeItemDelegate();
    // delegate->setCheckBoxVisible(true);   // 如需复选框
    m_tree->setItemDelegate(new FluentTreeItemDelegate(
        static_cast<fluent::FluentElement*>(m_tree), 24, m_tree, m_tree));
    // m_tree->setItemDelegate(delegate);
    // ---- 布局 ----
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tree);
    m_tree->setCanReorderItems(true);
    // ---- 监听 ProjectContext ----
    auto* ctx = doclife::core::ProjectContext::instance();

    // 结构变化（增/删/清空）→ 全量重建
    connect(ctx, &doclife::core::ProjectContext::projectsChanged,
            this, &ProjectTreeView::rebuildAll);

    // 内容变化（单个项目更新）→ 局部重建
    connect(ctx, &doclife::core::ProjectContext::projectUpdated,
            this, &ProjectTreeView::onProjectUpdated);

    // 初始构建
    rebuildAll();
}

// ===============================================================
// 全量重建：清空模型，为每个项目追加一个顶层节点
// ===============================================================
void ProjectTreeView::rebuildAll()
{
    m_model->removeRows(0, m_model->rowCount());   // 清空顶层

    const auto ctx = doclife::core::ProjectContext::instance();
    for (const auto& proj : ctx->projects()) {
        auto* root = makeProjectItem(proj.id);
        if (root) {
            m_model->appendRow(root);
        }
    }
    m_tree->expandAll();
}

// ===============================================================
// 局部重建：只替换某个项目对应的顶层节点
// ===============================================================
void ProjectTreeView::onProjectUpdated(const QString& projectId)
{
    const int row = findProjectRow(projectId);
    if (row < 0) {
        // 不存在则追加（防止漏加）
        auto* root = makeProjectItem(projectId);
        if (root) m_model->appendRow(root);
        return;
    }

    auto* old = m_model->item(row);
    auto* fresh = makeProjectItem(projectId);
    if (!fresh) return;

    // 替换整行
    m_model->removeRow(row);
    m_model->insertRow(row, fresh);

    Q_UNUSED(old);
    // expandAll 只影响新节点，如果不想每次都展开，可省
    // m_tree->expandAll();
}

// ===============================================================
// 找项目在顶层的位置
// ===============================================================
int ProjectTreeView::findProjectRow(const QString& projectId) const
{
    for (int i = 0; i < m_model->rowCount(); ++i) {
        auto* item = m_model->item(i);
        if (item && item->data(RoleNodeId).toString() == projectId) {
            return i;
        }
    }
    return -1;
}

// ===============================================================
// ProjectHead → QStandardItem（递归构建整棵子树）
// ===============================================================
QStandardItem* ProjectTreeView::makeProjectItem(const QString& projectId)
{
    const auto* proj = doclife::core::ProjectContext::instance()->head(projectId);
    if (!proj) return nullptr;
    // ---------- 根：项目 ----------
    auto* root = new QStandardItem(
        proj->docName.isEmpty() ? QStringLiteral("未命名项目") : proj->docName);
    root->setData(QStringLiteral("project"), RoleNodeType);
    root->setData(proj->id,                  RoleNodeId);
    root->setData(proj->docName,             RoleSubtitle);
    root->setEditable(false);

    // ---------- 单位 ----------
    QList<doclife::model::ProjectUnit> units = proj->units;
    int nuitsCount = units.count();
    qDebug() << " units has : " << nuitsCount;
    for (doclife::model::ProjectUnit& unit : units) {
    // for (int var = 0; var < 2; ++var) {
        auto* unitItem = new QStandardItem("unit.unitName");
        unitItem->setData(QStringLiteral("unit"), RoleNodeType);
        unitItem->setData(unit.id,                RoleNodeId);
        unitItem->setData(QStringLiteral("%1 个分部").arg(2),
                          RoleSubtitle);
        unitItem->setEditable(false);

        // ---------- 分部 ----------
        int nuitsSubs = unit.subs.count();
        qDebug() << " subs has : " << nuitsCount;
        for (doclife::model::ProjectSub & sub : unit.subs) {
        // for (int var = 0; var < 2; ++var){
            auto* subItem = new QStandardItem("sub.subName");
            subItem->setText("setText");
            subItem->setData(QStringLiteral("sub"), RoleNodeType);
            subItem->setData(sub.id,                RoleNodeId);
            subItem->setData(QStringLiteral("%1 个项目").arg(2),
                             RoleSubtitle);
            subItem->setEditable(false);

            // ---------- 项目 ----------
            int itemsCount = sub.items.count();
            qDebug() << " items has : " << itemsCount;
            for (doclife::model::ProjectItem& it : sub.items) {
                auto* itemItem = new QStandardItem(it.itemName);
                itemItem->setData(QStringLiteral("item"), RoleNodeType);
                itemItem->setData(it.id,                  RoleNodeId);
                itemItem->setData(QStringLiteral("%1 张表格").arg(it.sheets.size()),
                                  RoleSubtitle);
                itemItem->setEditable(false);

                // ---------- 表格 ----------
                int sheetsCount = it.sheets.count();
                qDebug() << " sheets has : " << sheetsCount;
                for (doclife::model::Sheet& sh : it.sheets) {
                    auto* sheetItem = new QStandardItem(sh.sheetName);
                    sheetItem->setData(QStringLiteral("sheet"), RoleNodeType);
                    sheetItem->setData(sh.sheetName,            RoleSubtitle);
                    sheetItem->setEditable(false);
                    sheetItem->setFlags(sheetItem->flags() & ~Qt::ItemIsDropEnabled);
                    itemItem->appendRow(sheetItem);
                }
                subItem->appendRow(itemItem);
            }
            unitItem->appendRow(subItem);
        }
        root->appendRow(unitItem);
    }

    return root;
}

} // namespace doclife::ui