#include "ProjectContextMenuBuilder.h"
#include "model/custommodel/NodeTypeRegistry.h"

#include <QAction>
#include <QWidget>
#include <QDebug>
#include <functional>
#include <uisignalmanager.h>

#include <FluentQt/MenusToolbars.h>
#include <business/ProjectContext.h>

// ★ 引入统一的弹窗
#include "view/popWindow/addsheetdialog.h"
#include "view/popWindow/dialog/dialogbase.h"

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

        // 统一的“新建表格”逻辑
        void triggerNewSheet(const QString& nodeId) {
            // 发出信号，由 CentralView 接管
            UISignalManager::instance()->requestAddSheet(nodeId);
        }

        // 统一的“打开填报”逻辑：工程表节点带着 projectSheetId，直接交给右侧视图
        void openSheetNode(const QString& nodeId) {
            auto* ctx = doclife::core::ProjectContext::instance();
            auto* node = ctx->findNodeById(nodeId);
            if (!node || !ctx->hasOpenProject()) return;

            const int projectSheetId = node->data.attrs.value(QStringLiteral("projectSheetId")).toInt(-1);
            if (projectSheetId < 0) {
                qWarning() << "表格节点缺少 projectSheetId，无法打开填报:" << nodeId;
                return;
            }
            UISignalManager::instance()->projectSheetActivated(
                nodeId, ctx->filePath(), projectSheetId, node->data.name);
        }

        // 统一的“安全删除”逻辑
        void triggerDeleteNode(const QString& nodeId, QWidget* parentWidget) {
            auto* ctx = doclife::core::ProjectContext::instance();
            auto* node = ctx->findNodeById(nodeId);
            if (!node) return;

            // 有子节点时提示影响范围，其余情况只做普通确认
            const bool hasChildren = !node->children.isEmpty();
            const QString content = hasChildren
                ? QStringLiteral("删除当前目录与所有子级资料，且不可恢复！")
                : QStringLiteral("确定要删除“%1”吗？").arg(node->data.name);

            DialogBase::show(parentWidget,
                QStringLiteral("删除确认"),
                content,
                [ctx, nodeId]() { ctx->removeNode(nodeId); },
                nullptr);
        }

        // ---- Root (项目节点) ----
        void buildForRoot(Flyout* flyout, const QString& nodeId, QWidget* p, const ProjectContextMenuBuilder::RenameHandler& onRename)
        {
            addCommand(flyout, "添加单位工程", [nodeId] {
                doclife::core::ProjectContext::instance()->addChild(nodeId, "unit");
                });
            addCommand(flyout, "新建目录", [nodeId] {
                doclife::core::ProjectContext::instance()->addChild(nodeId, "folder");
                });
            addCommand(flyout, "新建表格", [nodeId] {
                triggerNewSheet(nodeId);
                });
            addCommand(flyout, "重命名", [nodeId, onRename] {
                if (onRename) onRename(nodeId);
                });
            addCommand(flyout, "关闭项目", [] {
                doclife::core::ProjectContext::instance()->closeProject(true);
                });
        }

        // ---- Container (单位/分部/分项) ----
        void buildForContainer(Flyout* flyout, const QString& nodeId, const QString& type, QWidget* p, const ProjectContextMenuBuilder::RenameHandler& onRename)
        {
            // 按级别显示“添加下级”
            if (type == "unit") {
                addCommand(flyout, "添加分部工程", [nodeId] {
                    doclife::core::ProjectContext::instance()->addChild(nodeId, "sub");
                    });
            }
            else if (type == "sub") {
                addCommand(flyout, "添加分项工程", [nodeId] {
                    doclife::core::ProjectContext::instance()->addChild(nodeId, "item");
                    });
            }

            addCommand(flyout, "新建目录", [nodeId] {
                doclife::core::ProjectContext::instance()->addChild(nodeId, "folder");
                });
            addCommand(flyout, "新建表格", [nodeId] {
                triggerNewSheet(nodeId);
                });
            addCommand(flyout, "重命名", [nodeId, onRename] {
                if (onRename) onRename(nodeId);
                });
            addCommand(flyout, "删除目录", [nodeId, p] {
                triggerDeleteNode(nodeId, p);
                });
        }

        // ---- Folder (新目录) ----
        void buildForFolder(Flyout* flyout, const QString& nodeId, QWidget* p, const ProjectContextMenuBuilder::RenameHandler& onRename)
        {
            addCommand(flyout, "新建目录", [nodeId] {
                doclife::core::ProjectContext::instance()->addChild(nodeId, "folder");
                });
            addCommand(flyout, "新建表格", [nodeId] {
                triggerNewSheet(nodeId);
                });
            addCommand(flyout, "重命名", [nodeId, onRename] {
                if (onRename) onRename(nodeId);
                });
            addCommand(flyout, "删除目录", [nodeId, p] {
                triggerDeleteNode(nodeId, p);
                });
        }

        // ---- Leaf (表格) ----
        void buildForLeaf(Flyout* flyout, const QString& nodeId, QWidget* p, const ProjectContextMenuBuilder::RenameHandler& onRename)
        {
            // addCommand(flyout, "打开填报", [nodeId] {
            //     openSheetNode(nodeId);
            //     });
            addCommand(flyout, "重命名", [nodeId, onRename] {
                if (onRename) onRename(nodeId);
                });
            addCommand(flyout, "删除表格", [nodeId, p] {
                triggerDeleteNode(nodeId, p);
                });
        }

    } // namespace

    Flyout* ProjectContextMenuBuilder::build(QWidget* parent, const QString& nodeType, const QVariant& nodeId, RenameHandler onRename)
    {
        auto* flyout = new Flyout(parent);
        flyout->setAttribute(Qt::WA_DeleteOnClose);

        const auto* spec = doclife::model::NodeTypeRegistry::instance().spec(nodeType);
        if (!spec) { delete flyout; return nullptr; }

        const QString idStr = nodeId.toString();

        if (spec->role == doclife::model::NodeRole::Root) {
            buildForRoot(flyout, idStr, parent, onRename);
        }
        else if (spec->type == "folder") {
            buildForFolder(flyout, idStr, parent, onRename);
        }
        else if (spec->role == doclife::model::NodeRole::Leaf) {
            buildForLeaf(flyout, idStr, parent, onRename);
        }
        else {
            buildForContainer(flyout, idStr, spec->type, parent, onRename);
        }

        return flyout;
    }

} // namespace doclife::ui