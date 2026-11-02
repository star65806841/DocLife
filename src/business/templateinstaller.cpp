#include "templateinstaller.h"
#include "utils.h" // 引用 AppConfig::TemplateSuffix

#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

namespace doclife::ui {

    QString TemplateInstaller::dataRoot()
    {
        // 单次初始化：AppLocalData/data
        static const QString cached = [] {
            QString base = QStandardPaths::writableLocation(
                QStandardPaths::AppLocalDataLocation);
            if (base.isEmpty()) {
                // 极端兜底：org/app name 未设置
                base = QDir::homePath() + QStringLiteral("/.doclife");
            }
            const QString root = base + QStringLiteral("/data");
            QDir().mkpath(root);
            return root;
            }();
        return cached;
    }

    QString TemplateInstaller::collectionFilePath(const QString& code)
    {
        // 直接返回 {dataRoot}/{code}.dlt
        return dataRoot() + QLatin1Char('/') + code + QLatin1Char('.') + doclife::core::AppConfig::TemplateSuffix;
    }

    QString TemplateInstaller::seedFilePath(const QString& code)
    {
        // 种子包路径：{appDir}/data/{code}.dlt
        return QCoreApplication::applicationDirPath()
            + QStringLiteral("/data/") + code + QLatin1Char('.') + doclife::core::AppConfig::TemplateSuffix;
    }

    QStringList TemplateInstaller::installedCollections()
    {
        QDir root(dataRoot());
        if (!root.exists())
            return {};

        QStringList codes;
        // 只扫描 .dlt 文件
        const QString suffix = QStringLiteral(".") + QString(doclife::core::AppConfig::TemplateSuffix);
        for (const QFileInfo& fi : root.entryInfoList({ "*" + suffix }, QDir::Files)) {
            codes << fi.completeBaseName();
        }
        return codes;
    }

    bool TemplateInstaller::ensureSeedInstalled(QString* err)
    {
        const QString code = QStringLiteral("examples");
        const QString targetFile = collectionFilePath(code);
        const QString seedFile = seedFilePath(code);

        // 如果目标文件已经存在，直接返回成功
        if (QFile::exists(targetFile)) {
            return true;
        }

        // 如果种子文件不存在，报错
        if (!QFile::exists(seedFile)) {
            qWarning() << "[TemplateInstaller] 种子文件不存在:" << seedFile;
            if (err) *err = QStringLiteral("种子包未找到");
            return false;
        }

        // 直接复制文件即可，无需解压
        if (!QFile::copy(seedFile, targetFile)) {
            qWarning() << "[TemplateInstaller] 无法复制种子文件到:" << targetFile;
            if (err) *err = QStringLiteral("复制种子包失败");
            return false;
        }

        qDebug() << "[TemplateInstaller] 种子包已复制到:" << targetFile;
        return true;
    }

    bool TemplateInstaller::uninstall(const QString& code)
    {
        if (code.isEmpty() || code.contains(QLatin1Char('/'))
            || code == QLatin1String(".."))
            return false;

        const QString filePath = collectionFilePath(code);
        if (QFile::exists(filePath)) {
            return QFile::remove(filePath);
        }
        return true; // 文件不存在视为已卸载成功
    }

} // namespace doclife::ui