#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace doclife::ui {

    // 模板包安装器（纯数据库版）：
    //   - 直接管理 {dataRoot}/{code}.dlt 文件
    //   - 种子包 examples.dlt 从 {appDir}/data/ 读
    class TemplateInstaller : public QObject
    {
        Q_OBJECT
    public:
        explicit TemplateInstaller(QObject* parent = nullptr) : QObject(parent) {}

        // 唯一的运行时数据根（AppLocalData/data）
        static QString dataRoot();

        // 获取某集合的 .dlt 文件全路径，如 {dataRoot}/examples.dlt
        static QString collectionFilePath(const QString& code);

        // 种子包的全路径，永远在 exe 同级：{appDir}/data/examples.dlt
        static QString seedFilePath(const QString& code);

        // 已安装集合（扫描 dataRoot 下所有 .dlt 文件，返回 code）
        static QStringList installedCollections();

        // 首次启动调用：若 {dataRoot}/examples.dlt 不存在，则从 exe 同级拷贝
        static bool ensureSeedInstalled(QString* err = nullptr);

        // 卸载某个集合：直接删除 .dlt 文件
        static bool uninstall(const QString& code);
    };

} // namespace doclife::ui