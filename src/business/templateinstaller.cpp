#include "templateinstaller.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace doclife::ui {

    // ===========================================================================
    // 路径
    // ===========================================================================

    QString TemplateInstaller::dataRoot()
    {
        // 单次初始化：AppLocalData/data
        //   便携版: {exe}/../AppData/Local/...
        //   安装版: {exe}/../AppData/Local/...  —— 一致，无需探测
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

    QString TemplateInstaller::collectionRoot(const QString& code)
    {
        return dataRoot() + QLatin1Char('/') + code;
    }

    QString TemplateInstaller::collectionZipPath(const QString& code)
    {
        return dataRoot() + QLatin1Char('/') + code + QStringLiteral(".zip");
    }

    QString TemplateInstaller::seedZipPath(const QString& code)
    {
        return QCoreApplication::applicationDirPath()
            + QStringLiteral("/data/") + code + QStringLiteral(".zip");
    }

    QStringList TemplateInstaller::installedCollections()
    {
        QDir root(dataRoot());
        if (!root.exists())
            return {};
        // 只列目录，zip 文件自动被 QDir::Dirs 过滤
        return root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    }

    // ===========================================================================
    // 解压
    // ===========================================================================

    bool TemplateInstaller::extractZip(const QString& zipPath,
        const QString& destDir,
        QString* err)
    {
        QuaZip zip(zipPath);
        if (!zip.open(QuaZip::mdUnzip)) {
            if (err) *err = QStringLiteral("无法打开 zip: %1").arg(zipPath);
            return false;
        }

        QDir().mkpath(destDir);
        const QDir dest(destDir);
        const QString destPrefix =
            QDir::cleanPath(dest.absolutePath()) + QLatin1Char('/');

        for (bool ok = zip.goToFirstFile(); ok; ok = zip.goToNextFile()) {
            QString name = zip.getCurrentFileName();
            // Windows 打包的 zip 可能用反斜杠
            name.replace(QLatin1Char('\\'), QLatin1Char('/'));

            // ---- Zip Slip 防护 ----
            if (name.isEmpty()
                || name.startsWith(QLatin1Char('/'))
                || name.contains(QStringLiteral("../"))
                || name == QLatin1String("..")) {
                if (err) *err = QStringLiteral("zip 包含非法路径: %1").arg(name);
                return false;
            }

            const QString outPath = QDir::cleanPath(dest.absoluteFilePath(name));
            if (!outPath.startsWith(destPrefix)) {
                if (err) *err = QStringLiteral("zip 路径越界: %1").arg(name);
                return false;
            }

            // 目录条目（结尾带 /）
            if (name.endsWith(QLatin1Char('/'))) {
                QDir().mkpath(outPath);
                continue;
            }

            // 文件的父目录
            QDir().mkpath(QFileInfo(outPath).absolutePath());

            QuaZipFile in(&zip);
            if (!in.open(QIODevice::ReadOnly)) {
                if (err) *err = QStringLiteral("解压失败: %1").arg(name);
                return false;
            }

            QFile out(outPath);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                if (err) *err = QStringLiteral("写入失败: %1").arg(outPath);
                return false;
            }

            // 分块写，避免一次性把大文件读入内存
            constexpr qint64 kChunk = 64 * 1024;
            while (!in.atEnd()) {
                const QByteArray buf = in.read(kChunk);
                if (buf.isEmpty()) break;
                if (out.write(buf) != buf.size()) {
                    if (err) *err = QStringLiteral("写入失败: %1").arg(outPath);
                    return false;
                }
            }

            in.close();
            out.close();
        }

        return true;
    }

    // ===========================================================================
    // 种子安装
    // ===========================================================================

    bool TemplateInstaller::ensureSeedInstalled(QString* err)
    {
        const QString code = QStringLiteral("examples");
        const QString targetDir = collectionRoot(code);
        const QString targetZip = collectionZipPath(code);
        const QString seedZip = seedZipPath(code);

        // 已安装：目标目录存在且非空
        QDir target(targetDir);
        if (target.exists()
            && !target.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
            return true;
        }

        if (!QFile::exists(seedZip)) {
            qWarning() << "[TemplateInstaller] seed zip 不存在:" << seedZip;
            if (err) *err = QStringLiteral("种子包未找到");
            return false;
        }

        qDebug() << "[TemplateInstaller] extracting seed:"
            << seedZip << "->" << targetDir;

        // 解压前先清空目标目录（防止上次残留的半成品）
        if (target.exists())
            target.removeRecursively();

        if (!extractZip(seedZip, targetDir, err))
            return false;

        // 扁平版：把 zip 也复制到 dataRoot，与后续下载的集合保持一致
        if (!QFile::exists(targetZip))
            QFile::copy(seedZip, targetZip);

        return true;
    }

    // ===========================================================================
    // 卸载
    // ===========================================================================

    bool TemplateInstaller::uninstall(const QString& code)
    {
        if (code.isEmpty() || code.contains(QLatin1Char('/'))
            || code == QLatin1String(".."))
            return false;

        bool ok = true;
        QDir dir(collectionRoot(code));
        if (dir.exists())
            ok = dir.removeRecursively() && ok;

        const QString zip = collectionZipPath(code);
        if (QFile::exists(zip))
            ok = QFile::remove(zip) && ok;

        return ok;
    }

} // namespace doclife::ui