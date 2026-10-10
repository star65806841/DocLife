#pragma once
#include <QJsonArray>
#include <QString>

class QWidget;

namespace doclife::model{

class ProjectHead;

class Utils {
public:
    static int add(int a, int b);
    static QString formatName(const QString &name);
    static QString openExcelFile(QWidget *parent = nullptr);
    static QString selectDir(QWidget *parent = nullptr);
    static QJsonArray strListToJson(const QStringList &list);
    static QStringList jsonToStrList(const QJsonArray &arr);
    static bool saveProject(const ProjectHead &proj, const QString &filePath);
    static bool loadProject(const QString &filePath, ProjectHead &out);
};
}