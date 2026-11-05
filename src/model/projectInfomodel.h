#ifndef PROJECTINFOMODEL_H
#define PROJECTINFOMODEL_H

#include <QJsonObject>
#include <utils.h>

namespace doclife::model {

    struct ProjectInfoModel {
        QString id;
        QString projectnodeId;
        // 项目名称
        QString projectName;

        // 工程编号（合同段编号、标段号之类，打印在表头的工程标识）
        QString projectCode;

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
            o["projectnodeId"] = projectnodeId;
            o["projectName"] = projectName;
            o["projectCode"] = projectCode;
            o["ownerUnitName"] = ownerUnitName;
            o["constructionUnitName"] = constructionUnitName;
            o["designUnitName"] = designUnitName;
            o["supervisionUnitName"] = supervisionUnitName;

            return o;
        }
        static ProjectInfoModel fromJson(const QJsonObject& o) {
            ProjectInfoModel p;
            p.id = o.value("id").toString();
            p.projectnodeId = o.value("projectnodeId").toString();
            p.projectName = o.value("projectName").toString();
            p.projectCode = o.value("projectCode").toString();
            p.ownerUnitName = o.value("ownerUnitName").toString();
            p.constructionUnitName = o.value("constructionUnitName").toString();
            p.designUnitName = o.value("designUnitName").toString();
            p.supervisionUnitName = o.value("supervisionUnitName").toString();

            return p;
        }
    };
}
#endif // PROJECTINFOMODEL_H
