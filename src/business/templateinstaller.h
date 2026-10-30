#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace doclife::ui {

    // 模板包安装器：
    //   - 解压 zip 到 {dataRoot}/{code}/
    //   - 种子包 examples.zip 从 {appDir}/data/ 读，首次启动解压
    //   - 下载功能下一阶段接入（接口已预留）
    class TemplateInstaller : public QObject
    {
        Q_OBJECT
    public:
        explicit TemplateInstaller(QObject* parent = nullptr) : QObject(parent) {}

        // ---------- 路径 ----------

        // 唯一的运行时数据根（AppLocalData/data）
        static QString dataRoot();

        // 某个集合的根目录，如 {dataRoot}/examples
        static QString collectionRoot(const QString& code);

        // 某集合 zip 的运行时副本，如 {dataRoot}/examples.zip
        static QString collectionZipPath(const QString& code);

        // 种子 zip，永远在 exe 同级：{appDir}/data/examples.zip
        static QString seedZipPath(const QString& code);

        // 已安装集合（扫 dataRoot 下所有子目录）
        static QStringList installedCollections();

        // ---------- 解压 / 安装 ----------

        // 解压 zip 到 destDir，含 Zip Slip 防护；失败返回 false，err 带原因
        static bool extractZip(const QString& zipPath,
            const QString& destDir,
            QString* err = nullptr);

        // 首次启动调用：若 {dataRoot}/examples/ 不存在，则从 exe 同级解压
        static bool ensureSeedInstalled(QString* err = nullptr);

        // 卸载某个集合：删除目录 + 同名 zip
        static bool uninstall(const QString& code);

    signals:
        // 下一阶段（下载）用：
        // void started(const QString& code);
        // void progress(const QString& code, qint64 recv, qint64 total);
        // void finished(const QString& code);
        // void failed(const QString& code, const QString& error);
    };

} // namespace doclife::ui