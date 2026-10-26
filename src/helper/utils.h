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

        static QString selectDir(QWidget* parent = nullptr);
        static QString jsonToStr(const QJsonObject &o, bool pretty = false);
        static QString formatName(const QString& name);
        static QString saveFileDialog(const QString& headName,QWidget *parent);
        static QString openFileDialog(QWidget* parent = nullptr);
        static QString sanitizeFileName(const QString &name);
        static QString displayNameForFile(const QString &fileName);

        static QJsonObject jsonToStr(const QString &s);
        static QJsonArray strListToJson(const QStringList& list);

        static QStringList jsonToStrList(const QJsonArray& arr);

        static bool saveProject(const doclife::model::ProjectHead& proj, const QString& filePath);
        static bool loadProject(const QString& filePath, doclife::model::ProjectHead& out);

    };
}