#include "ProjectContextMenuBuilder.h"
#include "model/custommodel/NodeTypeRegistry.h"

#include <QAction>
#include <QWidget>
#include <QDebug>
#include <functional>

#include <FluentQt/MenusToolbars.h>
#include <business/ProjectContext.h>

namespace doclife::ui {

    using Flyout = fluent::menus_toolbars::CommandBarFlyout;
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

        QString displayLabelOf(const QString& type) {
            return doclife::model::NodeTypeRegistry::instance().displayLabel(type);
        }

        // ---- Root (项目节点) ----
        void buildForRoot(Flyout* flyout, const QString& nodeId,
            const doclife::model::NodeTypeSpec* spec,
            const ProjectContextMenuBuilder::RenameHandler& onRename)
        {
            if (!spec->defaultChildType.isEmpty()) {
                const QString childType = spec->defaultChildType;
                addCommand(flyout, QStringLiteral("添加%1").arg(displayLabelOf(childType)),
                    [nodeId, childType] {
                        doclife::core::ProjectContext::instance()->addChild(nodeId, childType);
                    });
            }
            addCommand(flyout, QStringLiteral("重命名"), [nodeId, onRename] {
                if (onRename) onRename(nodeId);
                });
            addCommand(flyout, QStringLiteral("关闭项目"), [] {
                doclife::core::ProjectContext::instance()->closeProject(true);
                });
        }

        // ---- Container (单位/分部/分项) ----
        void buildForContainer(Flyout* flyout, const QString& nodeId,
            const doclife::model::NodeTypeSpec* spec,
            const ProjectContextMenuBuilder::RenameHandler& onRename)
        {
            // 如果是"分部工程"，优先提供"一键新建分项(工序)"
            if (spec->type == QLatin1String("sub")) {
                addCommand(flyout, QStringLiteral("一键添加分项工程 (成套表)..."), [nodeId] {
                    // TODO: 下一步接入弹窗
                    qDebug() << "TODO: 弹窗选择工序，一键添加至" << nodeId;
                    });
            }

            if (!spec->defaultChildType.isEmpty()) {
                const QString childType = spec->defaultChildType;
                addCommand(flyout, QStringLiteral("添加%1").arg(displayLabelOf(childType)),
                    [nodeId, childType] {
                        doclife::core::ProjectContext::instance()->addChild(nodeId, childType);
                    });
            }

            // 所有容器节点都可以自由挂载单表
            addCommand(flyout, QStringLiteral("自由添加表格..."), [nodeId] {
                // TODO: 下一步弹窗搜索选表
                qDebug() << "TODO: 弹窗自由选择表格添加至" << nodeId;
                });

            addCommand(flyout, QStringLiteral("重命名"), [nodeId, onRename] {
                if (onRename) onRename(nodeId);
                });

            addCommand(flyout, QStringLiteral("删除节点"), [nodeId] {
                doclife::core::ProjectContext::instance()->removeNode(nodeId);
                });
        }

        // ---- Leaf (表格) ----
        void buildForLeaf(Flyout* flyout, const QString& nodeId,
            const doclife::model::NodeTypeSpec* spec,
            const ProjectContextMenuBuilder::RenameHandler& onRename)
        {
            Q_UNUSED(spec)
                addCommand(flyout, QStringLiteral("打开填报"), [nodeId] {
                // TODO: 打开右侧预览/填报
                qDebug() << "TODO: 打开表格填报" << nodeId;
                    });
            addCommand(flyout, QStringLiteral("重命名实例"), [nodeId, onRename] {
                if (onRename) onRename(nodeId);
                });
            addCommand(flyout, QStringLiteral("删除表格"), [nodeId] {
                doclife::core::ProjectContext::instance()->removeNode(nodeId);
                });
        }

    } // namespace

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