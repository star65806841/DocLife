#include "utils.h"

#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <projecmodel1.h>

using namespace doclife::core;

QString Utils::makeProjectId()
{
    // 形如 "proj-3f2a9b1c-4e5d-6f78-9abc-def012345678"
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return QStringLiteral("proj-") + uuid;
}

QString Utils::formatName(const QString& name) {
    return name.trimmed();
}

QString Utils::openExcelFile(QWidget* parent)
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

QString Utils::selectDir(QWidget* parent)
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

bool Utils::saveProject(const doclife::model::ProjectHead& proj, const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "打开失败:" << f.errorString();
        return false;
    }
    f.write(QJsonDocument(proj.toJson()).toJson(QJsonDocument::Indented));
    return true;
}

bool Utils::loadProject(const QString& filePath, doclife::model::ProjectHead& out) {
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
    out = doclife::model::ProjectHead::fromJson(doc.object());
    return true;
}

inline QString Utils::jsonToStr(const QJsonObject& o, bool pretty) {
    return QString::fromUtf8(
        QJsonDocument(o).toJson(pretty ? QJsonDocument::Indented
                                       : QJsonDocument::Compact));
}

inline QJsonObject Utils::jsonToStr(const QString& s) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "解析失败:" << err.errorString();
        return QJsonObject{};
    }
    return doc.object();
}