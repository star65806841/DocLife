#include "utils.h"

#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <projecmodel1.h>

using namespace doclife::core;

QString Utils::sanitizeFileName(const QString& name) {
    QString s = name;
    s.replace(QRegularExpression(R"([\\/:*?"<>|\x00-\x1F])"), QStringLiteral("_"));
    s = s.trimmed();
    while (s.endsWith('.')) s.chop(1);
    s = s.trimmed();
    if (s.isEmpty()) return {};

    static const QSet<QString> reserved = {
        QStringLiteral("CON"),  QStringLiteral("PRN"),
        QStringLiteral("AUX"),  QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"),
        QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"),
        QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"),
        QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9"),
    };
    if (reserved.contains(s.toUpper())) s += QLatin1Char('_');

    if (s.length() > 200) s.truncate(200);
    return s;
}

QString Utils::makeProjectId()
{
    // 形如 "proj-3f2a9b1c-4e5d-6f78-9abc-def012345678"
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return QStringLiteral("proj-") + uuid;
}

QString Utils::formatName(const QString& name) {
    return name.trimmed();
}

QString Utils::saveFileDialog(const QString& headName, QWidget* parent)
{
    QFileDialog dlg(parent, QObject::tr("保存文件"));
    // 设置为保存模式
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    // 允许输入一个不存在的文件名
    dlg.setFileMode(QFileDialog::AnyFile);
    // 文件过滤器
    dlg.setNameFilter(QObject::tr("文档 (*.doclife)"));

    // 如果用户没有输入后缀，自动添加 .doclife
    dlg.setDefaultSuffix("doclife");

    // 默认文件名
    dlg.selectFile(headName + ".doclife");

    // 弹出对话框
    if (dlg.exec() != QDialog::Accepted)
        return QString(); // 用户取消

    QStringList files = dlg.selectedFiles();
    if (files.isEmpty())
        return QString();

    QString fileName = files.first();

    // 强制固定格式：检查后缀是不是 .json
    QFileInfo fi(fileName);

    // compare 忽略大小写
    if (fi.suffix().compare("doclife", Qt::CaseInsensitive) != 0) {

        // 方案 A：自动改成
        fileName = QDir(fi.absolutePath()).filePath(fi.completeBaseName() + ".doclife");
    }

    return fileName;
}

QString Utils::openFileDialog(QWidget* parent)
{
    // QMessageBox::StandardButton choice = QMessageBox::question(
    //     parent,
    //     "Information",
    //     "If the xlsx file is large, opening it may take a significant amount of time.\n\n"
    //     "Do you want to continue and select a file?",
    //     QMessageBox::Yes | QMessageBox::No,
    //     QMessageBox::Yes
    // );

    // if (choice != QMessageBox::Yes)
    //     return QString();

    return QFileDialog::getOpenFileName(
        parent,
        "Select doclife File",
        QString(),
        "DocLife File (*.doclife)"
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

QString Utils::displayNameForFile(const QString& fileName)
{
    static const QHash<QString, QString> kMap = {
        { QStringLiteral("sichuan"),    QStringLiteral("四川省公路工程施工监理统一用表") },
        { QStringLiteral("tunnel"), QStringLiteral("隧道工程质检用表") },
        { QStringLiteral("sub_project"), QStringLiteral("分部工程") },
        { QStringLiteral("item_project"), QStringLiteral("分项工程") },
        { QStringLiteral("tunnel_qualityinspectionform.xlsx"), QStringLiteral("隧道工程分项工程质量检验评定表") },
        // { QStringLiteral("population.xlsx"), QStringLiteral("隧道工程分项工程质量检验评定表") },现场质量检验表
    };
    return kMap.value(fileName, fileName);   // 未映射则回退文件名
}