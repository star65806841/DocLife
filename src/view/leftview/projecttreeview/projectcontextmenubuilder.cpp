#include "ProjectContextMenuBuilder.h"
#include "model/custommodel/NodeTypeRegistry.h"

#include <QAction>
#include <QWidget>
#include <QDebug>
#include <functional>

#include <FluentQt/MenusToolbars.h>
#include <business/ProjectContext.h>

#include "view/popWindow/projectwizarddialog.h"
#include "view/popWindow/addsheetdialog.h"

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
            addCommand(flyout, QStringLiteral("添加单位工程"), [nodeId] {
                doclife::core::ProjectContext::instance()->addChild(nodeId, QStringLiteral("unit"));
                });
            addCommand(flyout, QStringLiteral("新建资料分类"), [nodeId] {
                doclife::core::ProjectContext::instance()->addChild(nodeId, QStringLiteral("folder"));
                });
            addCommand(flyout, QStringLiteral("重命名"), [nodeId, onRename] {
                if (onRename) onRename(nodeId);
                });
            addCommand(flyout, QStringLiteral("关闭项目"), [] {
                doclife::core::ProjectContext::instance()->closeProject(true);
                });
        }

        // ---- Container (单位/分部/分项/资料分类) ----
        void buildForContainer(Flyout* flyout, const QString& nodeId,
            const doclife::model::NodeTypeSpec* spec,
            const ProjectContextMenuBuilder::RenameHandler& onRename)
        {
            // 1. 分项工程专属：一键质检成套向导
            if (spec->type == QLatin1String("item")) {
                addCommand(flyout, QStringLiteral("⚡ 一键生成质检评定包..."), [nodeId] {
                    ProjectWizardDialog dlg(nodeId);
                    if (dlg.exec() == QDialog::Accepted) {
                        doclife::core::ProjectContext::instance()->createItemWithSheets(
                            nodeId, dlg.itemName(), dlg.selectedSheets());
                    }
                    });
            }

            // 2. 结构扩展
            if (spec->type == QLatin1String("unit")) {
                addCommand(flyout, QStringLiteral("添加分部工程"), [nodeId] {
                    doclife::core::ProjectContext::instance()->addChild(nodeId, QStringLiteral("sub"));
                    });
            }
            else if (spec->type == QLatin1String("sub")) {
                addCommand(flyout, QStringLiteral("添加分项工程"), [nodeId] {
                    doclife::core::ProjectContext::instance()->addChild(nodeId, QStringLiteral("item"));
                    });
            }

            // 3. 资料夹
            addCommand(flyout, QStringLiteral("新建资料分类"), [nodeId] {
                doclife::core::ProjectContext::instance()->addChild(nodeId, QStringLiteral("folder"));
                });

            // 4. ★ 自由添加表格
            addCommand(flyout, QStringLiteral("补充单张资料..."), [nodeId] {
                AddSheetDialog dlg;
                if (dlg.exec() == QDialog::Accepted) {
                    doclife::core::ProjectContext::instance()->addSheetsToNode(
                        nodeId, dlg.selectedSheets());
                }
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