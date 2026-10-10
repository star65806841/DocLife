#include "utils.h"

#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <projecmodel.h>

using namespace doclife::model;

int Utils::add(int a, int b) {
    return a + b;
}

QString Utils::formatName(const QString &name) {
    return name.trimmed();
}

QString Utils::openExcelFile(QWidget *parent)
{
    QMessageBox::StandardButton choice = QMessageBox::question(
        parent,
        "Information",
        "If the xlsx file is large, opening it may take a significant amount of time.\n\n"
        "Do you want to continue and select a file?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
        );

    if (choice != QMessageBox::Yes)
        return QString();

    return QFileDialog::getOpenFileName(
        parent,
        "Select Excel File",
        QString(),
        "Excel File (*.xlsx)"
        );
}

QString Utils::selectDir(QWidget *parent)
{
    return QString();
}

// QStringList <-> QJsonArray
QJsonArray Utils::strListToJson(const QStringList& list) {
    QJsonArray arr;
    for (const auto& s : list) arr.append(s);
    return arr;
}
QStringList Utils::jsonToStrList(const QJsonArray& arr) {
    QStringList list;
    list.reserve(arr.size());
    for (const auto& v : arr) list << v.toString();
    return list;
}

bool Utils::saveProject(const ProjectHead& proj, const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "打开失败:" << f.errorString();
        return false;
    }
    f.write(QJsonDocument(proj.toJson()).toJson(QJsonDocument::Indented));
    return true;
}

bool Utils::loadProject(const QString& filePath, ProjectHead& out) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "打开失败:" << f.errorString();
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "JSON 解析失败:" << err.errorString();
        return false;
    }
    out = ProjectHead::fromJson(doc.object());
    return true;
}