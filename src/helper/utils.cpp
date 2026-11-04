#include "utils.h"

#include <QIcon>
#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <projecmodel1.h>
#include <QDir>
#include <QFileInfo>
#include <QUuid>

namespace doclife::core {

    // 可以在全局定义常量，方便以后统一修改
    static const QString kProjectSuffix = QStringLiteral("dlf");
    static const QString kProjectFilter = QStringLiteral("DocLife 项目文件 (*.dlf)");

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
        QFileDialog dlg(parent, QObject::tr("保存项目文件"));
        dlg.setAcceptMode(QFileDialog::AcceptSave);
        dlg.setFileMode(QFileDialog::AnyFile);

        // ★ 使用全局配置
        dlg.setNameFilter(QObject::tr(AppConfig::ProjectFilter));
        dlg.setDefaultSuffix(AppConfig::ProjectSuffix);
        dlg.selectFile(headName + "." + AppConfig::ProjectSuffix);

        if (dlg.exec() != QDialog::Accepted) return QString();

        QStringList files = dlg.selectedFiles();
        if (files.isEmpty()) return QString();

        QString fileName = files.first();
        QFileInfo fi(fileName);

        // ★ 强制后缀纠正
        if (fi.suffix().compare(AppConfig::ProjectSuffix, Qt::CaseInsensitive) != 0) {
            fileName = QDir(fi.absolutePath()).filePath(
                fi.completeBaseName() + "." + AppConfig::ProjectSuffix);
        }

        return fileName;
    }

    QString Utils::openFileDialog(QWidget* parent)
    {
        return QFileDialog::getOpenFileName(
            parent,
            "打开 DocLife 项目",
            QString(),
            QObject::tr(AppConfig::ProjectFilter) // ★ 使用全局配置
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
            { QStringLiteral("sub_project"), QStringLiteral("分部工程") },
            { QStringLiteral("item_project"),QStringLiteral("分项工程") },
            { QStringLiteral("examples"),    QStringLiteral("示例表格") },
            { QStringLiteral("sichuan"),     QStringLiteral("四川省公路工程施工监理统一用表") },
            { QStringLiteral("tunnelworks"),      QStringLiteral("隧道工程质检用表") },
            { QStringLiteral("tunnel_qualityinspectionform"), QStringLiteral("隧道工程分项工程质量检验评定表") },
            { QStringLiteral("subgrade__qualityinspectionform"), QStringLiteral("路基工程分项工程质量检验评定表") },
        };

        auto it = kMap.constFind(fileName);
        if (it != kMap.constEnd())
            return it.value();

        const QString base = QFileInfo(fileName).completeBaseName();
        it = kMap.constFind(base);
        if (it != kMap.constEnd())
            return it.value();

        for (auto i = kMap.constBegin(); i != kMap.constEnd(); ++i) {
            if (i.key().compare(fileName, Qt::CaseInsensitive) == 0 ||
                i.key().compare(base, Qt::CaseInsensitive) == 0)
                return i.value();
        }

        return fileName;
    }

    QCursor Utils::createSvgCursor(const QString& path, int size)
    {
        QIcon icon(path);
        if (!icon.isNull()) {
            QPixmap pm = icon.pixmap(QSize(size, size));
            if (!pm.isNull()) {
                return QCursor(pm, pm.width() / 2, pm.height() / 2);
            }
        }
        return QCursor(Qt::ArrowCursor);   // SVG 加载失败时使用系统内置光标
    }
}