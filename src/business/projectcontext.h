#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QList>
#include <QSqlDatabase>

#include "projectNode.h"
#include "projectInfomodel.h"

namespace doclife::core {

    enum class RenameConflictPolicy { Fail, AutoSuffix, Overwrite };

    struct SheetNodeData {
        QString displayName;
        QString templateName;
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

        static constexpr const char* kFileSuffix = AppConfig::ProjectSuffix;
        static constexpr const char* kFileFilter = AppConfig::ProjectFilter;

        // ---- 状态 ----
        bool    hasOpenProject() const { return !m_filePath.isEmpty(); }
        bool    isUntitled()     const { return !m_tempFilePath.isEmpty(); }
        bool    isDirty()        const { return m_dirty; }
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

        // ---- 生命周期 ----
        bool newProject();
        bool openProject(const QString& filePath);
        bool saveProject();
        bool saveProjectAs(const QString& filePath);
        bool closeProject(bool saveBeforeClose = true);

        void markClean();
        void markDirty();

        // ---- 增量操作 (内存级别) ----
        bool addChild(const QString& parentId, const QString& childType, QString* newId = nullptr);
        bool addSheetsToNode(const QString& parentId, const QList<SheetNodeData>& sheets);
        bool createItemWithSheets(const QString& parentId, const QString& itemName, const QList<SheetNodeData>& selectedSheets);
        bool removeNode(const QString& nodeId);
        bool renameNode(const QString& nodeId, const QString& newName, RenameConflictPolicy policy = RenameConflictPolicy::Fail);
        bool setNodeAttr(const QString& nodeId, const QString& key, const QVariant& value);

        // ★ 新增：供拖拽移动使用
        bool moveNode(const QString& nodeId, const QString& newParentId, int newIndex = -1);

    signals:
        void projectCreated(const QString& filePath);
        void projectOpened(const QString& filePath);
        void projectSaved(const QString& filePath);
        void projectClosed();
        void dirtyChanged(bool dirty);
        void errorOccurred(const QString& message);
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

        // SQLite 底层操作
        bool writeDb(const QString& targetPath);
        bool readDb(const QString& dbPath);

        QString acquireLock(const QString& filePath);
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
    };

} // namespace doclife::core