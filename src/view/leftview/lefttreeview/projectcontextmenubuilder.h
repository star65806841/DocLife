#pragma once

#include <QString>
#include <QVariant>

class QWidget;

namespace fluent::menus_toolbars {
    class CommandBarFlyout;
}

namespace doclife::ui {

    // 按节点"角色"（Root/Container/Leaf）构造菜单，业务类型只影响文案与默认子类型。
    // 加新类型时无需修改此文件，只需在 NodeTypeRegistry 注册。
    class ProjectContextMenuBuilder
    {
    public:
        // 重命名回调（View 提供，触发 m_tree->edit()）
        using RenameHandler = std::function<void(const QString& nodeId)>;

        static fluent::menus_toolbars::CommandBarFlyout* build(
            QWidget* parent,
            const QString& nodeType,
            const QVariant& nodeId,
            RenameHandler onRename = nullptr);   // 默认 nullptr，保持向后兼容
    };

} // namespace doclife::ui
