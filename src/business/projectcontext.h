#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include "projectnode.h"
#include "projectInfomodel.h"

namespace doclife::core {
    // 文件重命名时的冲突策略
    enum class RenameConflictPolicy {
        Fail,          // 冲突时返回 false，不修改（默认）
        AutoSuffix,    // 冲突时自动加 " (2)" " (3)" ……
        Overwrite,     // 覆盖已有文件（危险，慎用）
    };
    /**
         * @brief 全局唯一的工程上下文（单例）
         *
         * 模型：
         *  - 同一时间只打开一个项目（对应一个 .doclife / .xlsx 文件）
         *  - 项目文件结构（扁平化，永不超过单元格 32767 字符）：
         *      Sheet "_meta_"
         *          A1 : {"version":1, "info": {...ProjectInfoModel...}}
         *          A2 : {"parentRow":0, "id":"...", "type":"project", "attrs":{...}}
         *          A3 : {"parentRow":2, "id":"...", "type":"unit",    "attrs":{...}}
         *          ...
         *      Sheet[其他]  → 各个业务表格
         *  - 内存中只保留一棵 ProjectNode 树 + 一份 ProjectInfoModel
         *  - 新建项目时使用系统临时目录下的 UUID 命名 .doclife 文件；
         *    用户首次「另存为」后切换到目标文件，临时文件删除。
         *  - 打开/保存已落盘的文件时创建 <file>.lock（内含 PID），
         *    若锁被活进程持有则拒绝打开。
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
        bool    hasOpenProject() const { return !m_filePath.isEmpty(); }
        // 没有保存过的项目一定是新建空白，尚未命名
        bool    isUntitled()     const { return !m_tempFilePath.isEmpty(); }
        bool    isDirty()        const { return m_dirty; }
        QString filePath()  const { return m_filePath; }
        QString projectName()    const;
        QString displayName()    const;    // 标题栏用
        QString getUuid()        const;

        // ============================================================
        // 数据访问
        // ============================================================
        const doclife::model::ProjectNode& rootNode() const { return m_root; }
        doclife::model::ProjectNode& rootNode() { return m_root; }

        const doclife::model::ProjectNode* findNodeById(const QString& id) const;
        doclife::model::ProjectNode* findNodeById(const QString& id);

        const doclife::model::ProjectInfoModel& projectInfo() const { return m_projectInfo; }
        doclife::model::ProjectInfoModel& projectInfo() { return m_projectInfo; }
        void setProjectInfo(doclife::model::ProjectInfoModel& projectInfo);
        // ============================================================
        // 操作
        // ============================================================
        // 新建（内部使用临时文件，UUID 命名）
        bool newProject(QString* errorMsg = nullptr);

        // 打开项目：若已有打开的项目，会先尝试保存再关闭
        bool openProject(const QString& filePath, QString* errorMsg = nullptr);

        // 保存当前项目到磁盘（已保存过 → 直接覆盖；未保存过 → 返回 false，请用 saveProjectAs）
        bool saveProject(QString* errorMsg = nullptr);

        // 另存为 / 首次保存：把临时文件转正
        bool saveProjectAs(const QString& filePath, QString* errorMsg = nullptr);

        // 关闭项目：saveBeforeClose=true 且非临时文件时先保存；释放内存与锁
        bool closeProject(bool saveBeforeClose = true, QString* errorMsg = nullptr);

        // 打印（除 _meta_ 外的所有工作表）
        bool printAllSheets(QString* errorMsg = nullptr);

        // 业务侧改动树之后调用，标记为"有未保存修改"
        void markDirty();

        // ---- 增量操作 ----
        bool addChild(const QString& parentId, const QString& childType,
                      QString* newId = nullptr, QString* errorMsg = nullptr);
        bool removeNode(const QString& nodeId, QString* errorMsg = nullptr);
        // 若 nodeId 是 root 且项目已保存，会同步重命名磁盘文件
        bool renameNode(const QString& nodeId, const QString& newName,
                        QString* errorMsg = nullptr,
                        RenameConflictPolicy policy = RenameConflictPolicy::Fail);
        // 单独暴露：重命名项目文件（不改 root.name，仅磁盘操作）
        //   newBaseName    —— 不带扩展名的新 basename
        //   actualBaseName —— AutoSuffix 时回填实际使用的 basename（可空）
        bool renameProjectFile(const QString& newBaseName,
                               RenameConflictPolicy policy = RenameConflictPolicy::Fail,
                               QString* actualBaseName = nullptr,
                               QString* errorMsg = nullptr);
        bool setNodeAttr(const QString& nodeId, const QString& key,
                         const QVariant& value, QString* errorMsg = nullptr);
    signals:
        void projectCreated(const QString& filePath);
        void projectOpened(const QString& filePath);
        void projectSaved(const QString& filePath);
        void projectClosed();

        void dirtyChanged(bool dirty);
        void errorOccurred(const QString& message);

        void projectFilePathChanged(const QString& oldPath, const QString& newPath);
        // ---- 增量信号（视图层据此局部刷新，无需整树重建） ----
        void nodeAdded(const QString& parentId, const QString& nodeId);
        void nodeRemoved(const QString& parentId, const QString& nodeId);
        void nodeUpdated(const QString& nodeId);
    public slots:
        void onNewProject();

    private:
        explicit ProjectContext(QObject* parent = nullptr);
        ~ProjectContext() override;

        void initConnect();

        // ---- 元数据读写 ----
        bool writeMetadata(const QString& filePath, QString* errorMsg);
        bool readMetadata(const QString& filePath, QString* errorMsg);

        // ---- 锁 ----
        bool acquireLock(const QString& filePath, QString* errorMsg);
        void releaseLock();

        // ---- 临时文件 ----
        QString makeTempFilePath() const;
        void    cleanupTempFile();

        // ---- 状态 ----
        void resetState();

    private:
        doclife::model::ProjectNode       m_root;
        doclife::model::ProjectInfoModel  m_projectInfo;

        QString m_filePath;   // 当前操作的真实文件（可能是临时 .doclife）
        QString m_tempFilePath;    // 非空表示"还没保存过、用的临时文件"
        QString m_lockedPath;      // 已加锁的文件路径（空 = 未锁）
        // "内存 vs 磁盘是否一致"的标志，用于退出前提示保存。
        bool m_dirty = false;
    };

} // namespace doclife::core