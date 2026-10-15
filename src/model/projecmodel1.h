// ============================ 具体用表 ============================

#include <QJsonObject>
#include <utils.h>

namespace doclife::model {

    struct Sheet {
        QString id;
        QString tableName;
        QString sheetName;
        QString filePath;

        QJsonObject toJson() const {
            QJsonObject o;
            o["id"] = tableName;
            o["tableName"] = tableName;
            o["sheetName"] = sheetName;
            o["filePath"] = filePath;
            return o;
        }
        static Sheet fromJson(const QJsonObject& o) {
            return {
                o.value("id").toString(),
                o.value("tableName").toString(),
                o.value("sheetName").toString(),
                o.value("filePath").toString()
            };
        }
    };

    // ========================= 分项工程 ==========================
    struct ProjectItem {
        QString id;
        QString itemName;
        QStringList recommendTables;   // 推荐表格集合
        QList<Sheet> sheets;

        QJsonObject toJson() const {
            QJsonObject o;
            o["id"] = id;
            o["itemName"] = itemName;
            o["recommendTables"] = doclife::core::Utils::strListToJson(recommendTables);

            QJsonArray arr;
            for (const auto& s : sheets) arr.append(s.toJson());
            o["sheets"] = arr;
            return o;
        }
        static ProjectItem fromJson(const QJsonObject& o) {
            ProjectItem it;
            it.id = o.value("id").toString();
            it.itemName = o.value("itemName").toString();
            it.recommendTables = doclife::core::Utils::jsonToStrList(o.value("recommendTables").toArray());
            for (const auto& v : o.value("sheets").toArray())
                it.sheets.append(Sheet::fromJson(v.toObject()));
            return it;
        }
    };

    // ========================== 分部工程 ==========================
    struct ProjectSub {
        QString id;
        QString subName;
        QStringList recommendItems;
        QList<ProjectItem> items;

        QJsonObject toJson() const {
            QJsonObject o;
            o["id"] = id;
            o["subName"] = subName;
            o["recommendItems"] = doclife::core::Utils::strListToJson(recommendItems);

            QJsonArray arr;
            for (const auto& i : items) arr.append(i.toJson());
            o["items"] = arr;
            return o;
        }
        static ProjectSub fromJson(const QJsonObject& o) {
            ProjectSub s;
            s.id = o.value("id").toString();
            s.subName = o.value("subName").toString();
            s.recommendItems = doclife::core::Utils::jsonToStrList(o.value("recommendItems").toArray());
            for (const auto& v : o.value("items").toArray())
                s.items.append(ProjectItem::fromJson(v.toObject()));
            return s;
        }
    };

    // ========================= 单位工程 ==========================
    struct ProjectUnit {
        QString id;
        QString unitName;
        QStringList recommendSubs;
        QList<ProjectSub> subs;

        QJsonObject toJson() const {
            QJsonObject o;
            o["id"] = id;
            o["unitName"] = unitName;
            o["recommendSubs"] = doclife::core::Utils::strListToJson(recommendSubs);

            QJsonArray arr;
            for (const auto& s : subs) arr.append(s.toJson());
            o["subs"] = arr;
            return o;
        }
        static ProjectUnit fromJson(const QJsonObject& o) {
            ProjectUnit u;
            u.id = o.value("id").toString();
            u.unitName = o.value("unitName").toString();
            u.recommendSubs = doclife::core::Utils::jsonToStrList(o.value("recommendSubs").toArray());
            for (const auto& v : o.value("subs").toArray())
                u.subs.append(ProjectSub::fromJson(v.toObject()));
            return u;
        }
    };

    // ========================= 档案名 ==========================
    struct ProjectHead {
        QString id;
        QString docName;
        QString overview;
        // 项目名称
        QString projectName;
        // 建设单位（业主/甲方）
        QString ownerUnitName;
        // 施工单位（承包商/乙方）
        QString constructionUnitName;
        // 设计单位
        QString designUnitName;
        // 监理单位
        QString supervisionUnitName;

        QList<ProjectUnit> units;

        QJsonObject toJson() const {
            QJsonObject o;
            o["id"] = id;
            o["docName"] = docName;
            o["overview"] = overview;

            o["projectName"] = projectName;
            o["ownerUnitName"] = ownerUnitName;
            o["constructionUnitName"] = constructionUnitName;
            o["designUnitName"] = designUnitName;
            o["supervisionUnitName"] = supervisionUnitName;

            QJsonArray arr;
            for (const auto& u : units) arr.append(u.toJson());
            o["units"] = arr;
            return o;
        }
        static ProjectHead fromJson(const QJsonObject& o) {
            ProjectHead p;
            p.id = o.value("id").toString();
            p.docName = o.value("docName").toString();
            p.overview = o.value("overview").toString();

            p.projectName = o.value("projectName").toString();
            p.ownerUnitName = o.value("ownerUnitName").toString();
            p.constructionUnitName = o.value("constructionUnitName").toString();
            p.designUnitName = o.value("designUnitName").toString();
            p.supervisionUnitName = o.value("supervisionUnitName").toString();

            for (const auto& v : o.value("units").toArray())
                p.units.append(ProjectUnit::fromJson(v.toObject()));
            return p;
        }
    };
}