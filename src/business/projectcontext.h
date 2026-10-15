#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include "projectnode.h"
#include "projectInfomodel.h"

namespace doclife::core {

/**
 * @brief 全局唯一的工程上下文（单例）
 *
 * 模型：
 *  - 同一时间只打开一个项目（对应一个 .xlsx 文件）
 *  - 项目文件结构：
 *      Sheet[0] "_meta_"  →  A1 = 整棵树的 JSON，B1 = ProjectInfo 的 JSON
 *      Sheet[1..N]        →  各个业务表格
 *  - 内存中只保留一棵 ProjectNode 树 + 一份 ProjectInfo
 */
class ProjectContext : public QObject
{
    Q_OBJECT

public:
    static ProjectContext* instance();

    ProjectContext(const ProjectContext&) = delete;
    ProjectContext& operator=(const ProjectContext&) = delete;

    // ============================================================
    // 状态
    // ============================================================
    bool    hasOpenProject() const { return !m_excelFilePath.isEmpty(); }
    bool    isDirty()        const { return m_dirty; }
    QString excelFilePath()  const { return m_excelFilePath; }
    QString projectName()    const;

    // ============================================================
    // 数据访问
    // ============================================================
    const doclife::model::ProjectNode& rootNode() const { return m_root; }
    doclife::model::ProjectNode&       rootNode()       { return m_root; }

    const doclife::model::ProjectInfoModel& projectInfo() const { return m_projectInfo; }
    doclife::model::ProjectInfoModel&       projectInfo()       { return m_projectInfo; }

    // ============================================================
    // 操作
    // ============================================================
    // 创建项目：新建 Excel，写 A1（树 JSON）和 B1（ProjectInfoModel JSON）
    bool createProject(const QString& filePath,
                       const doclife::model::ProjectInfoModel& info,
                       QString* errorMsg = nullptr);

    // 打开项目：若已有打开的项目，会先尝试保存再关闭
    bool openProject(const QString& filePath, QString* errorMsg = nullptr);

    // 保存当前项目到磁盘
    bool saveProject(QString* errorMsg = nullptr);

    // 另存为
    bool saveProjectAs(const QString& filePath, QString* errorMsg = nullptr);

    // 关闭项目：saveBeforeClose=true 时先保存；释放内存
    bool closeProject(bool saveBeforeClose = true, QString* errorMsg = nullptr);

    // 打印（除 _meta_ 外的所有工作表）
    bool printAllSheets(QString* errorMsg = nullptr);

    // 业务侧改动树之后调用，标记为"有未保存修改"
    void markDirty();

signals:
    void projectCreated(const QString& filePath);
    void projectOpened (const QString& filePath);
    void projectSaved  (const QString& filePath);
    void projectClosed ();
    void dirtyChanged  (bool dirty);
    void errorOccurred (const QString& message);

private:
    explicit ProjectContext(QObject* parent = nullptr);
    ~ProjectContext() override;

    bool writeMetadata(const QString& filePath, QString* errorMsg);
    bool readMetadata (const QString& filePath, QString* errorMsg);
    void resetState();

    doclife::model::ProjectNode m_root;
    doclife::model::ProjectInfoModel m_projectInfo;
    QString                     m_excelFilePath;
    bool                        m_dirty = false;
};

} // namespace doclife::core