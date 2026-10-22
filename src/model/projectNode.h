#pragma once
// ============================ 具体用表 ============================

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

namespace doclife::model {

struct NodeAttributes {
    QString      id;
    QString      type;
    QString      name;

    int          startLinear = 0;   // ← 初始化为 0
    int          endLinear   = 0;   // ← 初始化为 0

    QJsonObject  attrs;

    QString attr(const QString& key) const { return attrs.value(key).toString(); }
    QStringList attrList(const QString& key) const {
        QStringList out;
        for (const auto& v : attrs.value(key).toArray()) out << v.toString();
        return out;
    }
};

struct Node
{
    NodeAttributes data;
    Node* parent = nullptr;
    QList<Node*> children;

    ~Node()
    {
        qDeleteAll(children);
        children.clear();
    }
};

} // namespace doclife::model