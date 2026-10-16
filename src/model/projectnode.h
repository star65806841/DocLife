#pragma once
// ============================ 具体用表 ============================

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

namespace doclife::model {

struct ProjectNode {
    QString      id;         // 节点 id（Sheet 等没有 id 的层可留空）
    QString      type;       // "project" / "unit" / "sub" / "item" / "sheet"
    QString      name;
    QJsonObject  attrs;      // 各层差异化字段（原 projectName、filePath 等）
    QList<ProjectNode> children;

    // ---- 便捷访问（可选） ----
    QString attr(const QString& key) const { return attrs.value(key).toString(); }
    QStringList attrList(const QString& key) const {
        QStringList out;
        for (const auto& v : attrs.value(key).toArray()) out << v.toString();
        return out;
    }
    // ---------- 序列化 ----------
    static QJsonObject toJson(const ProjectNode& n) {
        QJsonObject o = n.attrs;          // 先把差异化字段平铺进来
        o["id"]   = n.id;
        o["type"] = n.type;
        o["name"] = n.name;

        if (!n.children.isEmpty()) {
            QJsonArray arr;
            for (const auto& c : n.children) arr.append(toJson(c));
            o["children"] = arr;
        }
        return o;
    }

    // ---------- 反序列化 ----------
    static ProjectNode fromJson(const QJsonObject& o) {
        ProjectNode n;
        for (auto it = o.begin(); it != o.end(); ++it) {
            const QString& k = it.key();
            if      (k == "id")       n.id   = it.value().toString();
            else if (k == "type")     n.type = it.value().toString();
            else if (k == "name")     n.name = it.value().toString();
            else if (k == "children") {
                for (const auto& v : it.value().toArray())
                    n.children.append(fromJson(v.toObject()));
            }
            else n.attrs.insert(k, it.value());   // 其余字段全进 attrs
        }
        return n;
    }
};
}