#include "NodeTypeRegistry.h"

namespace doclife::model {

NodeTypeRegistry& NodeTypeRegistry::instance()
{
    // Meyers 单例：首次调用时构造，此时 QApplication 已存在，QIcon 安全
    static NodeTypeRegistry registry;
    return registry;
}

NodeTypeRegistry::NodeTypeRegistry()
{
    // ---- 内建类型（初始 5 种） ----
    registerType({ QStringLiteral("project"), NodeRole::Root,
                  QStringLiteral("项目"),     {}, QStringLiteral("unit")  });
    registerType({ QStringLiteral("unit"),    NodeRole::Container,
                  QStringLiteral("单位工程"), {}, QStringLiteral("sub")   });
    registerType({ QStringLiteral("sub"),     NodeRole::Container,
                  QStringLiteral("分部工程"), {}, QStringLiteral("item")  });
    registerType({ QStringLiteral("item"),    NodeRole::Container,
                  QStringLiteral("分项工程"), {}, QStringLiteral("sheet") });
    registerType({ QStringLiteral("sheet"),   NodeRole::Leaf,
                  QStringLiteral("表格"),
                  QIcon(QStringLiteral(":/icons/logo/Flash.png")), {} });

    // ---- 将来扩展：只加一行 ----
    // registerType({ "attachment", NodeRole::Leaf, "附件", {}, {} });
    // registerType({ "subpackage", NodeRole::Container, "分包", {}, "unit" });
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