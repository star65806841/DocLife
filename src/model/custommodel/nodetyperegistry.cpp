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
        registerType({ QStringLiteral("project"), NodeRole::Root,
                      QStringLiteral("项目"),     {}, QStringLiteral("unit") });

        registerType({ QStringLiteral("unit"),    NodeRole::Container,
                      QStringLiteral("单位工程"), {}, QStringLiteral("sub") });

        registerType({ QStringLiteral("sub"),     NodeRole::Container,
                      QStringLiteral("分部工程"), {}, QStringLiteral("item") });

        registerType({ QStringLiteral("item"),    NodeRole::Container,
                      QStringLiteral("分项工程"), {}, QStringLiteral("folder") }); // 分项下默认建文件夹

        // ---- 资料分类目录（用于装载各类资料） ----
        registerType({ QStringLiteral("folder"),  NodeRole::Container,
                      QStringLiteral("资料分类"), {}, QStringLiteral("sheet") });

        // ---- 表格叶子节点 ----
        registerType({ QStringLiteral("sheet"),   NodeRole::Leaf,
                      QStringLiteral("表格"),
                      QIcon(QStringLiteral(":/icons/logo/Flash.png")), {} });
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
        return s && s->role != NodeRole::Leaf;
    }

    QString NodeTypeRegistry::displayLabel(const QString& type) const
    {
        const auto* s = spec(type);
        return s ? s->displayLabel : type;
    }

} // namespace doclife::model