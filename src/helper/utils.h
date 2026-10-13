#pragma once
#include <QJsonArray>
#include <QString>

class QWidget;
namespace doclife::model {
    class ProjectHead;
}
namespace doclife::core {

    class Utils {
    public:
        static QString makeProjectId();

        static QString formatName(const QString& name);
        static QString openExcelFile(QWidget* parent = nullptr);
        static QString selectDir(QWidget* parent = nullptr);
        static QJsonArray strListToJson(const QStringList& list);
        static QStringList jsonToStrList(const QJsonArray& arr);
        static bool saveProject(const doclife::model::ProjectHead& proj, const QString& filePath);
        static bool loadProject(const QString& filePath, doclife::model::ProjectHead& out);

    };
}