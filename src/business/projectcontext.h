#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QList>

#include "projectNode.h"
#include "projectInfomodel.h"

// 只前向声明，避免把 QXlsx 头文件拖进所有包含者
namespace QXlsx { class Document; }

namespace doclife::core {

    enum class RenameConflictPolicy { Fail, AutoSuffix, Overwrite };

    // 用于批量挂载表格的数据结构
    struct SheetNodeData {
        QString displayName;  // 实例名（默认用模板名）
        QString templateName; // 模板表名（如：明洞浇筑现场质量检验表）
        int sheetId = -1;
        QString dbPath;
    };

    class ProjectContext : public QObject
    {
        Q_OBJECT
    public:
        static ProjectContext* instance();
        ProjectContext(const ProjectContext&) = delete;
        ProjectContext& operator=(const ProjectContext&) = delete;

        // ---- 状态 ----
        bool    hasOpenProject() const { return !m_filePath.isEmpty(); }
        bool    isUntitled()     const { return !m_tempFilePath.isEmpty(); }
        bool    isDirty()        const { return m_dirty || m_businessDirty; }
        QString filePath()       const { return m_filePath; }
        QString projectName()    const;
        QString displayName()    const;
        QString getUuid()        const;

        // ---- 树访问 ----
        const doclife::model::Node* rootNode() const { return m_root; }
        doclife::model::Node* rootNode() { return m_root; }

        const doclife::model::Node* findNodeById(const QString& id) const;
        doclife::model::Node* findNodeById(const QString& id);

        const doclife::model::ProjectInfoModel& projectInfo() const { return m_projectInfo; }
        doclife::model::ProjectInfoModel& projectInfo() { return m_projectInfo; }
        void setProjectInfo(doclife::model::ProjectInfoModel& projectInfo);

        // ---- 业务表格（懒加载） ----
        QXlsx::Document* businessDocument();
        void             markBusinessDirty();
        void             releaseBusinessDocument();

        // ---- 生命周期 ----
        bool newProject();
        bool openProject(const QString& filePath);
        bool saveProject();
        bool saveProjectAs(const QString& filePath);
        bool closeProject(bool saveBeforeClose = true);
        bool printAllSheets();

        void markClean();
        void markDirty();

        // ---- 增量操作 ----
        bool addChild(const QString& parentId, const QString& childType,
            QString* newId = nullptr);

        // ★ 新增：批量添加表格实例到指定节点
        bool addSheetsToNode(const QString& parentId, const QList<SheetNodeData>& sheets);

        bool removeNode(const QString& nodeId);
        bool renameNode(const QString& nodeId, const QString& newName,
            RenameConflictPolicy policy = RenameConflictPolicy::Fail);
        bool renameProjectFile(const QString& newBaseName,
            RenameConflictPolicy policy = RenameConflictPolicy::Fail,
            QString* actualBaseName = nullptr);
        bool setNodeAttr(const QString& nodeId, const QString& key, const QVariant& value);

    signals:
        void projectCreated(const QString& filePath);
        void projectOpened(const QString& filePath);
        void projectSaved(const QString& filePath);
        void projectClosed();
        void dirtyChanged(bool dirty);
        void errorOccurred(const QString& message);
        void projectFilePathChanged(const QString& oldPath, const QString& newPath);
        void nodeAdded(const QString& parentId, const QString& nodeId);
        void nodeRemoved(const QString& parentId, const QString& nodeId);
        void nodeUpdated(const QString& nodeId);

    public slots:
        void onNewProject();

    private:
        explicit ProjectContext(QObject* parent = nullptr);
        ~ProjectContext() override;
        void initConnect();

        QString makeTempFilePath() const;
        QString writeZip(const QString& targetPath);
        QString readZip(const QString& zipPath);

        QJsonObject nodeToJson(const doclife::model::Node& n) const;
        doclife::model::Node* nodeFromJson(const QJsonObject& o) const;

        QString acquireLock(const QString& filePath);
        QString doRenameProjectFile(const QString& newBaseName,
            RenameConflictPolicy policy,
            QString* actualBaseName);

        void releaseLock();
        void cleanupTempFile();
        void resetState();

    private:
        doclife::model::Node* m_root = nullptr;
        doclife::model::ProjectInfoModel  m_projectInfo;
        QString m_filePath;
        QString m_tempFilePath;
        QString m_lockedPath;
        bool    m_dirty = false;

        QXlsx::Document* m_businessDoc = nullptr;
        bool             m_businessDirty = false;
    };

} // namespace doclife::core