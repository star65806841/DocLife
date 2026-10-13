#ifndef PROJECTINFO_H
#define PROJECTINFO_H

#include <QJsonObject>
#include <utils.h>

namespace doclife::model {

    struct ProjectInfo {
        QString id;
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

        QJsonObject toJson() const {
            QJsonObject o;
            o["id"] = id;
            o["projectName"] = projectName;
            o["ownerUnitName"] = ownerUnitName;
            o["constructionUnitName"] = constructionUnitName;
            o["designUnitName"] = designUnitName;
            o["supervisionUnitName"] = supervisionUnitName;

            return o;
        }
        static ProjectInfo fromJson(const QJsonObject& o) {
            ProjectInfo p;
            p.id = o.value("id").toString();
            p.projectName = o.value("projectName").toString();
            p.ownerUnitName = o.value("ownerUnitName").toString();
            p.constructionUnitName = o.value("constructionUnitName").toString();
            p.designUnitName = o.value("designUnitName").toString();
            p.supervisionUnitName = o.value("supervisionUnitName").toString();

            return p;
        }
    };
}
#endif // PROJECTINFO_H
