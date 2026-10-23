#include "ProjectContextMenuBuilder.h"
#include "model/custommodel/NodeTypeRegistry.h"

#include <QAction>
#include <QWidget>
#include <QDebug>
#include <functional>

#include <FluentQt/MenusToolbars.h>
#include <business/ProjectContext.h>

namespace doclife::ui {

using Flyout  = fluent::menus_toolbars::CommandBarFlyout;
using Handler = std::function<void()>;

namespace {

QAction* addCommand(Flyout* flyout, const QString& text, Handler handler)
{
    auto* action = new QAction(text, flyout);
    QObject::connect(action, &QAction::triggered, flyout,
                     [h = std::move(handler)]() { if (h) h(); });
    flyout->addSecondaryAction(action);
    return action;
}

QString displayLabelOf(const QString& type)
{
    return doclife::model::NodeTypeRegistry::instance().displayLabel(type);
}

// ---- Root ----
void buildForRoot(Flyout* flyout, const QString& nodeId,
                  const doclife::model::NodeTypeSpec* spec,
                  const ProjectContextMenuBuilder::RenameHandler& onRename)
{
    if (!spec->defaultChildType.isEmpty()) {
        const QString childType = spec->defaultChildType;
        addCommand(flyout,
                   QStringLiteral("添加%1").arg(displayLabelOf(childType)),
                   [nodeId, childType] {
                       doclife::core::ProjectContext::instance()
                       ->addChild(nodeId, childType);
                   });
    }
    addCommand(flyout, QStringLiteral("重命名"), [nodeId, onRename] {
        if (onRename) onRename(nodeId);
    });
    addCommand(flyout, QStringLiteral("项目属性"), [nodeId] {
        // TODO: 打开属性对话框
    });
    addCommand(flyout, QStringLiteral("刷新项目"), [] {
        // TODO
    });
    addCommand(flyout, QStringLiteral("关闭项目"), [] {
        doclife::core::ProjectContext::instance()->closeProject(true);
    });
}

// ---- Container ----
void buildForContainer(Flyout* flyout, const QString& nodeId,
                       const doclife::model::NodeTypeSpec* spec,
                       const ProjectContextMenuBuilder::RenameHandler& onRename)
{
    if (!spec->defaultChildType.isEmpty()) {
        const QString childType = spec->defaultChildType;
        addCommand(flyout,
                   QStringLiteral("添加%1").arg(displayLabelOf(childType)),
                   [nodeId, childType] {
                       doclife::core::ProjectContext::instance()
                       ->addChild(nodeId, childType);
                   });
    }
    addCommand(flyout, QStringLiteral("重命名"), [nodeId, onRename] {
        if (onRename) onRename(nodeId);
    });
    addCommand(flyout, QStringLiteral("属性"), [nodeId] {
        // TODO
    });
    addCommand(flyout, QStringLiteral("删除"), [nodeId] {
        doclife::core::ProjectContext::instance()->removeNode(nodeId);
    });
}

// ---- Leaf ----
void buildForLeaf(Flyout* flyout, const QString& nodeId,
                  const doclife::model::NodeTypeSpec* spec,
                  const ProjectContextMenuBuilder::RenameHandler& onRename)
{
    Q_UNUSED(spec)
    addCommand(flyout, QStringLiteral("打开"), [nodeId] {
        // TODO: ProjectContext::instance()->openLeaf(nodeId);
    });
    addCommand(flyout, QStringLiteral("重命名"), [nodeId, onRename] {
        if (onRename) onRename(nodeId);
    });
    addCommand(flyout, QStringLiteral("删除"), [nodeId] {
        doclife::core::ProjectContext::instance()->removeNode(nodeId);
    });
}

} // namespace

// =====================================================================
Flyout* ProjectContextMenuBuilder::build(QWidget* parent,
                                         const QString& nodeType,
                                         const QVariant& nodeId,
                                         RenameHandler onRename)
{
    auto* flyout = new Flyout(parent);
    flyout->setAttribute(Qt::WA_DeleteOnClose);

    const auto* spec = doclife::model::NodeTypeRegistry::instance().spec(nodeType);
    if (!spec) { delete flyout; return nullptr; }

    const QString idStr = nodeId.toString();

    switch (spec->role) {
    case doclife::model::NodeRole::Root:
        buildForRoot(flyout, idStr, spec, onRename);      break;
    case doclife::model::NodeRole::Container:
        buildForContainer(flyout, idStr, spec, onRename); break;
    case doclife::model::NodeRole::Leaf:
        buildForLeaf(flyout, idStr, spec, onRename);      break;
    }

    return flyout;
}

} // namespace doclife::ui