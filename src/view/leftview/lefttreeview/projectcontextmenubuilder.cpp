#include "projectcontextmenubuilder.h"
#include "model/custommodel/NodeTypeRegistry.h"

#include <QAction>
#include <QWidget>
#include <functional>

#include <FluentQt/MenusToolbars.h>

#include <custommodel/NodeTypeRegistry.h>

namespace doclife::ui {

    namespace {

        using Handler = std::function<void()>;

        QAction* addCommand(fluent::menus_toolbars::CommandBarFlyout* flyout, const QString& text, Handler handler)
        {
            auto* action = new QAction(text, flyout);
            // QObject::connect(action, &QAction::triggered, flyout,
            //                  [h = std::move(handler)]() { if (h) h(); });
            flyout->addAction(action);
            return action;
        }

        QString displayLabelOf(const QString& type)
        {
            return doclife::model::NodeTypeRegistry::instance().displayLabel(type);
        }

        // ---- Root：添加子 + 重命名 + 属性 + 刷新 + 关闭 ----
        void buildForRoot(fluent::menus_toolbars::CommandBarFlyout* flyout, const QVariant& nodeId, const doclife::model::NodeTypeSpec* spec)
        {
            if (!spec->defaultChildType.isEmpty()) {
                const QString childType = spec->defaultChildType;
                addCommand(flyout,
                    QStringLiteral("添加%1").arg(displayLabelOf(childType)),
                    [nodeId, childType] {
                        // TODO: doclife::core::ProjectContext::instance()
                        //         ->addChild(nodeId, childType);
                    });
            }
            addCommand(flyout, QStringLiteral("重命名"), [nodeId] {
                // TODO: 触发重命名对话框
                });
            addCommand(flyout, QStringLiteral("项目属性"), [nodeId] {
                // TODO: 打开属性对话框
                });
            addCommand(flyout, QStringLiteral("刷新项目"), [] {
                // TODO: doclife::core::ProjectContext::instance()->refreshProject();
                });
            addCommand(flyout, QStringLiteral("关闭项目"), [] {
                // TODO: doclife::core::ProjectContext::instance()->closeProject();
                });
        }

        // ---- Container：添加子 + 重命名 + 属性 + 删除 ----
        void buildForContainer(fluent::menus_toolbars::CommandBarFlyout* flyout, const QVariant& nodeId, const doclife::model::NodeTypeSpec* spec)
        {
            if (!spec->defaultChildType.isEmpty()) {
                const QString childType = spec->defaultChildType;
                addCommand(flyout,
                    QStringLiteral("添加%1").arg(displayLabelOf(childType)),
                    [nodeId, childType] {
                        // TODO: doclife::core::ProjectContext::instance()
                        //         ->addChild(nodeId, childType);
                    });
            }
            addCommand(flyout, QStringLiteral("重命名"), [nodeId] {
                // TODO
                });
            addCommand(flyout, QStringLiteral("属性"), [nodeId] {
                // TODO
                });
            addCommand(flyout, QStringLiteral("删除"), [nodeId] {
                // TODO: doclife::core::ProjectContext::instance()->removeNode(nodeId);
                });
        }

        // ---- Leaf：打开 + 重命名 + 删除 ----
        void buildForLeaf(fluent::menus_toolbars::CommandBarFlyout* flyout, const QVariant& nodeId, const doclife::model::NodeTypeSpec* spec)
        {
            Q_UNUSED(spec)
                addCommand(flyout, QStringLiteral("打开"), [nodeId] {
                // TODO: doclife::core::ProjectContext::instance()->openLeaf(nodeId);
                    });
            addCommand(flyout, QStringLiteral("重命名"), [nodeId] {
                // TODO
                });
            addCommand(flyout, QStringLiteral("删除"), [nodeId] {
                // TODO
                });
        }

    } // namespace

    // =====================================================================
    fluent::menus_toolbars::CommandBarFlyout* ProjectContextMenuBuilder::build(QWidget* parent,
        const QString& nodeType,
        const QVariant& nodeId)
    {
        auto* flyout = new fluent::menus_toolbars::CommandBarFlyout(parent);
        flyout->setAttribute(Qt::WA_DeleteOnClose);

        const auto* spec = doclife::model::NodeTypeRegistry::instance().spec(nodeType);
        if (!spec) { delete flyout; return nullptr; }

        switch (spec->role) {
        case doclife::model::NodeRole::Root:      buildForRoot(flyout, nodeId, spec);      break;
        case doclife::model::NodeRole::Container: buildForContainer(flyout, nodeId, spec); break;
        case doclife::model::NodeRole::Leaf:      buildForLeaf(flyout, nodeId, spec);      break;
        }

        return flyout;
    }

} // namespace doclife::ui