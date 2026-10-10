// ============================ Sheet ============================

#include <QJsonObject>
#include <utils.h>

namespace doclife::model {

struct Sheet {
    QString tableName;
    QString sheetName;
    QString filePath;

    QJsonObject toJson() const {
        QJsonObject o;
        o["tableName"] = tableName;
        o["sheetName"] = sheetName;
        o["filePath"]  = filePath;
        return o;
    }
    static Sheet fromJson(const QJsonObject& o) {
        return {
            o.value("tableName").toString(),
            o.value("sheetName").toString(),
            o.value("filePath").toString()
        };
    }
};

// ========================= ProjectItem ==========================
struct ProjectItem {
    QString id;
    QString itemName;
    QStringList recommendTables;   // 推荐表格集合
    QList<Sheet> sheets;

    QJsonObject toJson() const {
        QJsonObject o;
        o["id"]              = id;
        o["itemName"]        = itemName;
        o["recommendTables"] = Utils::strListToJson(recommendTables);

        QJsonArray arr;
        for (const auto& s : sheets) arr.append(s.toJson());
        o["sheets"] = arr;
        return o;
    }
    static ProjectItem fromJson(const QJsonObject& o) {
        ProjectItem it;
        it.id              = o.value("id").toString();
        it.itemName        = o.value("itemName").toString();
        it.recommendTables = Utils::jsonToStrList(o.value("recommendTables").toArray());
        for (const auto& v : o.value("sheets").toArray())
            it.sheets.append(Sheet::fromJson(v.toObject()));
        return it;
    }
};

// ========================== ProjectSub ==========================
struct ProjectSub {
    QString id;
    QString subName;
    QStringList recommendItems;
    QList<ProjectItem> items;

    QJsonObject toJson() const {
        QJsonObject o;
        o["id"]             = id;
        o["subName"]        = subName;
        o["recommendItems"] = Utils::strListToJson(recommendItems);

        QJsonArray arr;
        for (const auto& i : items) arr.append(i.toJson());
        o["items"] = arr;
        return o;
    }
    static ProjectSub fromJson(const QJsonObject& o) {
        ProjectSub s;
        s.id             = o.value("id").toString();
        s.subName        = o.value("subName").toString();
        s.recommendItems = Utils::jsonToStrList(o.value("recommendItems").toArray());
        for (const auto& v : o.value("items").toArray())
            s.items.append(ProjectItem::fromJson(v.toObject()));
        return s;
    }
};

// ========================= ProjectUnit ==========================
struct ProjectUnit {
    QString id;
    QString unitName;
    QStringList recommendSubs;
    QList<ProjectSub> subs;

    QJsonObject toJson() const {
        QJsonObject o;
        o["id"]            = id;
        o["unitName"]      = unitName;
        o["recommendSubs"] = Utils::strListToJson(recommendSubs);

        QJsonArray arr;
        for (const auto& s : subs) arr.append(s.toJson());
        o["subs"] = arr;
        return o;
    }
    static ProjectUnit fromJson(const QJsonObject& o) {
        ProjectUnit u;
        u.id            = o.value("id").toString();
        u.unitName      = o.value("unitName").toString();
        u.recommendSubs = Utils::jsonToStrList(o.value("recommendSubs").toArray());
        for (const auto& v : o.value("subs").toArray())
            u.subs.append(ProjectSub::fromJson(v.toObject()));
        return u;
    }
};

// ========================= ProjectHead ==========================
struct ProjectHead {
    QString id;
    QString docName;
    QString projectName;
    QString constructionUnitName;
    QString buildingUnitName;
    QString designUnitName;
    QString supervisionUnitName;
    QString overview;
    QList<ProjectUnit> units;

    QJsonObject toJson() const {
        QJsonObject o;
        o["version"]              = 1;
        o["id"]                   = id;
        o["docName"]              = docName;
        o["projectName"]          = projectName;
        o["constructionUnitName"] = constructionUnitName;
        o["buildingUnitName"]     = buildingUnitName;
        o["designUnitName"]       = designUnitName;
        o["supervisionUnitName"]  = supervisionUnitName;
        o["overview"]             = overview;

        QJsonArray arr;
        for (const auto& u : units) arr.append(u.toJson());
        o["units"] = arr;
        return o;
    }
    static ProjectHead fromJson(const QJsonObject& o) {
        ProjectHead p;
        p.id                   = o.value("id").toString();
        p.docName              = o.value("docName").toString();
        p.projectName          = o.value("projectName").toString();
        p.constructionUnitName = o.value("constructionUnitName").toString();
        p.buildingUnitName     = o.value("buildingUnitName").toString();
        p.designUnitName       = o.value("designUnitName").toString();
        p.supervisionUnitName  = o.value("supervisionUnitName").toString();
        p.overview             = o.value("overview").toString();
        for (const auto& v : o.value("units").toArray())
            p.units.append(ProjectUnit::fromJson(v.toObject()));
        return p;
    }
};
}