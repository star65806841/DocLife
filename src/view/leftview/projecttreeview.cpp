#include "ProjectTreeView.h"
#include "fluenttreeitemdelegate.h"

#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QIcon>

#include <components/collections/TreeView.h>
#include <business/ProjectContext.h>

namespace doclife::ui {

namespace {

constexpr int RoleNodeType = Qt::UserRole + 1;   // "project"/"unit"/"sub"/"item"/"sheet"
constexpr int RoleNodeId   = Qt::UserRole + 2;
constexpr int RoleSubtitle = Qt::UserRole + 3;

// ---- 类型常量（与 ProjectNode.type 保持一致） ----
const QString TypeProject = QStringLiteral("project");
const QString TypeUnit    = QStringLiteral("unit");
const QString TypeSub     = QStringLiteral("sub");
const QString TypeItem    = QStringLiteral("item");
const QString TypeSheet   = QStringLiteral("sheet");

// 名称字段的 key
QString nameKeyFor(const QString& type) {
    if (type == TypeProject) return QStringLiteral("projectName");
    if (type == TypeUnit)    return QStringLiteral("unitName");
    if (type == TypeSub)     return QStringLiteral("subName");
    if (type == TypeItem)    return QStringLiteral("itemName");
    if (type == TypeSheet)   return QStringLiteral("tableName");
    return QStringLiteral("name");
}

// 类型的中文标签（用于兜底名称）
QString labelFor(const QString& type) {
    if (type == TypeProject) return QStringLiteral("项目");
    if (type == TypeUnit)    return QStringLiteral("单位工程");
    if (type == TypeSub)     return QStringLiteral("分部工程");
    if (type == TypeItem)    return QStringLiteral("分项工程");
    if (type == TypeSheet)   return QStringLiteral("表格");
    return type;
}

// 副标题（第二列显示）
QString subtitleFor(const doclife::model::ProjectNode& n) {
    if (n.type == TypeProject)
        return n.attr(QStringLiteral("docName"));

    if (n.type == TypeUnit)
        return QStringLiteral("%1 个分部").arg(n.children.size());

    if (n.type == TypeSub)
        return QStringLiteral("%1 个分项").arg(n.children.size());

    if (n.type == TypeItem)
        return QStringLiteral("%1 张表格").arg(n.children.size());

    if (n.type == TypeSheet) {
        const QString sheet = n.attr(QStringLiteral("sheetName"));
        const QString path  = n.attr(QStringLiteral("filePath"));
        return sheet.isEmpty() ? path
                               : QStringLiteral("%1 | %2").arg(sheet, path);
    }
    return {};
}

} // namespace

// ===============================================================
ProjectTreeView::ProjectTreeView(QWidget* parent)
    : QWidget(parent)
{
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("名称")});

    m_tree = new fluent::collections::TreeView(this);
    m_tree->setModel(m_model);
    m_tree->setIndicatorMotionAnimationEnabled(true);
    m_tree->setSelectionMode(fluent::collections::SelectionMode::Single);
    m_tree->setHeaderHidden(false);

    auto* delegate = new FluentTreeItemDelegate();
    m_tree->setItemDelegate(delegate);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tree);
    m_tree->setCanReorderItems(true);

    // ---- 监听 ProjectContext（单项目模型） ----
    auto* ctx = doclife::core::ProjectContext::instance();

    // 打开 / 创建 / 关闭 → 全量重建
    connect(ctx, &doclife::core::ProjectContext::projectOpened,
            this, &ProjectTreeView::rebuildAll);
    connect(ctx, &doclife::core::ProjectContext::projectCreated,
            this, &ProjectTreeView::rebuildAll);
    connect(ctx, &doclife::core::ProjectContext::projectClosed,
            this, &ProjectTreeView::rebuildAll);

    // 初始状态（可能已经打开着项目）
    rebuildAll();
}

// ===============================================================
void ProjectTreeView::rebuildAll()
{
    m_model->removeRows(0, m_model->rowCount());

    auto* ctx = doclife::core::ProjectContext::instance();
    if (!ctx->hasOpenProject()) return;

    const auto& root = ctx->rootNode();
    if (root.type.isEmpty()) return;

    if (auto* rootItem = makeItem(root)) {
        m_model->appendRow(rootItem);
    }
    m_tree->expandAll();
}

// ===============================================================
QStandardItem* ProjectTreeView::makeItem(const doclife::model::ProjectNode& node)
{
    // ---- 名称：优先 type 对应字段 → 通用 name → 兜底中文标签 ----
    QString title = node.attr(nameKeyFor(node.type));
    if (title.isEmpty()) title = node.attr(QStringLiteral("name"));
    if (title.isEmpty()) title = labelFor(node.type);

    auto* item = new QStandardItem(title);
    item->setData(node.type,          RoleNodeType);
    item->setData(node.id,            RoleNodeId);
    item->setData(subtitleFor(node),  RoleSubtitle);
    item->setEditable(false);

    applyFlags(item, node.type);

    // ---- 递归子节点 ----
    for (const auto& child : node.children) {
        if (auto* childItem = makeItem(child)) {
            item->appendRow(childItem);
        }
    }
    return item;
}

// ===============================================================
void ProjectTreeView::applyFlags(QStandardItem* item, const QString& type)
{
    if (type == TypeSheet) {
        // 表格是最底层，不允许作为拖放目标
        item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
        item->setIcon(QIcon(":/icons/logo/Flash.png"));
    }
}

} // namespace doclife::ui