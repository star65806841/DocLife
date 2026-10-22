#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

#include "projectNode.h"
#include "projectInfomodel.h"

namespace doclife::core {

enum class RenameConflictPolicy { Fail, AutoSuffix, Overwrite };

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
    bool    isDirty()        const { return m_dirty; }
    QString filePath()       const { return m_filePath; }
    QString projectName()    const;
    QString displayName()    const;
    QString getUuid()        const;

    // ---- 数据访问（非拥有视图） ----
    const doclife::model::Node* rootNode() const { return m_root; }
    doclife::model::Node*       rootNode()       { return m_root; }

    const doclife::model::Node* findNodeById(const QString& id) const;
    doclife::model::Node*       findNodeById(const QString& id);

    const doclife::model::ProjectInfoModel& projectInfo() const { return m_projectInfo; }
    doclife::model::ProjectInfoModel&       projectInfo()       { return m_projectInfo; }
    void setProjectInfo(doclife::model::ProjectInfoModel& projectInfo);

    // ---- 生命周期 ----
    bool newProject(QString* errorMsg = nullptr);
    bool openProject(const QString& filePath, QString* errorMsg = nullptr);
    bool saveProject(QString* errorMsg = nullptr);
    bool saveProjectAs(const QString& filePath, QString* errorMsg = nullptr);
    bool closeProject(bool saveBeforeClose = true, QString* errorMsg = nullptr);
    bool printAllSheets(QString* errorMsg = nullptr);
    void markDirty();

    // ---- 增量操作 ----
    bool addChild(const QString& parentId, const QString& childType,
                  QString* newId = nullptr, QString* errorMsg = nullptr);
    bool removeNode(const QString& nodeId, QString* errorMsg = nullptr);
    bool renameNode(const QString& nodeId, const QString& newName,
                    QString* errorMsg = nullptr,
                    RenameConflictPolicy policy = RenameConflictPolicy::Fail);
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
    void nodeAdded(const QString& parentId, const QString& nodeId);
    void nodeRemoved(const QString& parentId, const QString& nodeId);
    void nodeUpdated(const QString& nodeId);

public slots:
    void onNewProject();

private:
    explicit ProjectContext(QObject* parent = nullptr);
    ~ProjectContext() override;
    void initConnect();

    bool writeMetadata(const QString& filePath, QString* errorMsg);
    bool readMetadata(const QString& filePath, QString* errorMsg);
    bool acquireLock(const QString& filePath, QString* errorMsg);
    void releaseLock();
    QString makeTempFilePath() const;
    void cleanupTempFile();
    void resetState();

private:
    doclife::model::Node*             m_root = nullptr;   // 拥有
    doclife::model::ProjectInfoModel  m_projectInfo;
    QString m_filePath;
    QString m_tempFilePath;
    QString m_lockedPath;
    bool    m_dirty = false;
};

} // namespace doclife::core