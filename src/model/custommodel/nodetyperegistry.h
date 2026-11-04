#pragma once

#include <QString>
#include <QStringList>
#include <QIcon>
#include <QMap>

namespace doclife::model {

    // 结构角色：决定"能不能有子节点"这件事，与业务类型无关
    enum class NodeRole { Root, Container, Leaf };

    struct NodeTypeSpec {
        QString   type;                 // 业务类型："project"/"unit"/"sub"/"item"/"sheet"
        NodeRole  role = NodeRole::Container;
        QString   displayLabel;         // 兜底显示名："单位工程"
        QIcon     icon;                 // 可空
        QString   defaultChildType;     // 右键"添加"时默认创建的子类型，可空
        QStringList allowedChildTypes;  // 允许挂在下面的子类型；空表示不能有子节点
    };

    class NodeTypeRegistry {
    public:
        static NodeTypeRegistry& instance();

        void registerType(const NodeTypeSpec& spec);
        const NodeTypeSpec* spec(const QString& type) const;

        bool isLeaf(const QString& type) const;
        bool isRoot(const QString& type) const;
        bool canHaveChildren(const QString& type) const;

        // 结构规则：childType 能否挂在 parentType 下面（拖拽与新建共用）
        bool canAcceptChild(const QString& parentType, const QString& childType) const;

        QString displayLabel(const QString& type) const;

    private:
        NodeTypeRegistry();
        Q_DISABLE_COPY(NodeTypeRegistry)

            QMap<QString, NodeTypeSpec> m_specs;
    };

} // namespace doclife::model