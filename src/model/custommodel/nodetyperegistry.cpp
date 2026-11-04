#include "NodeTypeRegistry.h"

namespace doclife::model {

    NodeTypeRegistry& NodeTypeRegistry::instance()
    {
        static NodeTypeRegistry registry;
        return registry;
    }

    NodeTypeRegistry::NodeTypeRegistry()
    {
        // ---- 工程实体划分树 ----
        // 最后一项是“允许哪些子类型挂在自己下面”，它同时决定了层级和拖拽规则：
        // 单位/分部/分项只出现在唯一的父级下，所以它们天然只能同级排序。
        registerType({ QStringLiteral("project"), NodeRole::Root,
                      QStringLiteral("项目"),     {}, QStringLiteral("unit"),
                      { QStringLiteral("unit"), QStringLiteral("folder"), QStringLiteral("sheet") } });

        registerType({ QStringLiteral("unit"),    NodeRole::Container,
                      QStringLiteral("单位工程"), {}, QStringLiteral("sub"),
                      { QStringLiteral("sub"), QStringLiteral("folder"), QStringLiteral("sheet") } });

        registerType({ QStringLiteral("sub"),     NodeRole::Container,
                      QStringLiteral("分部工程"), {}, QStringLiteral("item"),
                      { QStringLiteral("item"), QStringLiteral("folder"), QStringLiteral("sheet") } });

        registerType({ QStringLiteral("item"),    NodeRole::Container,
                      QStringLiteral("分项工程"), {}, QStringLiteral("folder"),
                      { QStringLiteral("folder"), QStringLiteral("sheet") } });

        // ---- 资料分类目录（用于装载各类资料） ----
        registerType({ QStringLiteral("folder"),  NodeRole::Container,
                      QStringLiteral("资料分类"), {}, QStringLiteral("sheet"),
                      { QStringLiteral("folder"), QStringLiteral("sheet") } });

        // ---- 表格叶子节点：可以再挂表格，但不能挂目录 ----
        registerType({ QStringLiteral("sheet"),   NodeRole::Leaf,
                      QStringLiteral("表格"),
                      QIcon(QStringLiteral(":/icons/logo/Flash.png")), {},
                      { QStringLiteral("sheet") } });
    }

    void NodeTypeRegistry::registerType(const NodeTypeSpec& spec)
    {
        if (spec.type.isEmpty()) return;
        m_specs.insert(spec.type, spec);
    }

    const NodeTypeSpec* NodeTypeRegistry::spec(const QString& type) const
    {
        auto it = m_specs.constFind(type);
        return it == m_specs.constEnd() ? nullptr : &it.value();
    }

    bool NodeTypeRegistry::isLeaf(const QString& type) const
    {
        const auto* s = spec(type);
        return s && s->role == NodeRole::Leaf;
    }

    bool NodeTypeRegistry::isRoot(const QString& type) const
    {
        const auto* s = spec(type);
        return s && s->role == NodeRole::Root;
    }

    bool NodeTypeRegistry::canHaveChildren(const QString& type) const
    {
        const auto* s = spec(type);
        return s && !s->allowedChildTypes.isEmpty();
    }

    bool NodeTypeRegistry::canAcceptChild(const QString& parentType, const QString& childType) const
    {
        const auto* s = spec(parentType);
        return s && s->allowedChildTypes.contains(childType);
    }

    QString NodeTypeRegistry::displayLabel(const QString& type) const
    {
        const auto* s = spec(type);
        return s ? s->displayLabel : type;
    }

} // namespace doclife::model