#include "ProjectContext.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QUuid>
#include <QSaveFile>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>
#include <algorithm>

#include "utils.h"
#include "model/custommodel/NodeTypeRegistry.h"

namespace doclife::core {

    namespace {
        const QString kLockSuffix = QStringLiteral(".lock");

        QString tempDirPath() {
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/DocLife");
            QDir().mkpath(dir);
            return dir;
        }

        QString tryAcquireLockFile(const QString& filePath) {
            // 简易文件锁实现
            const QString lockPath = filePath + kLockSuffix;
            QFile lock(lockPath);
            if (lock.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                lock.write(QByteArray::number(QCoreApplication::applicationPid()));
                lock.close();
                return {};
            }
            return QStringLiteral("无法创建锁文件: %1").arg(lockPath);
        }

        void releaseLockFile(const QString& filePath) {
            if (filePath.isEmpty()) return;
            QFile::remove(filePath + kLockSuffix);
        }
    }

    ProjectContext* ProjectContext::instance() {
        static ProjectContext s;
        return &s;
    }

    ProjectContext::ProjectContext(QObject* parent) : QObject(parent) {
        m_root = new doclife::model::Node;
    }

    ProjectContext::~ProjectContext() {
        releaseLock();
        cleanupTempFile();
        delete m_root;
    }

    void ProjectContext::initConnect() {}

    QString ProjectContext::projectName() const {
        if (!m_projectInfo.projectName.isEmpty()) return m_projectInfo.projectName;
        return QFileInfo(m_filePath).baseName();
    }

    QString ProjectContext::displayName() const {
        if (m_root && !m_root->data.name.isEmpty()) return m_root->data.name;
        if (isUntitled()) return QStringLiteral("未命名文档");
        return QFileInfo(m_filePath).baseName();
    }

    QString ProjectContext::getUuid() const {
        return m_root ? m_root->data.id : QString();
    }

    const doclife::model::Node* ProjectContext::findNodeById(const QString& id) const {
        if (!m_root || id.isEmpty()) return nullptr;
        QVector<const doclife::model::Node*> stack;
        stack.append(m_root);
        while (!stack.isEmpty()) {
            const auto* n = stack.takeLast();
            if (n->data.id == id) return n;
            for (const auto* c : n->children) stack.append(c);
        }
        return nullptr;
    }

    doclife::model::Node* ProjectContext::findNodeById(const QString& id) {
        return const_cast<doclife::model::Node*>(static_cast<const ProjectContext*>(this)->findNodeById(id));
    }

    void ProjectContext::setProjectInfo(model::ProjectInfoModel& projectInfo) {
        m_projectInfo = projectInfo;
        markDirty();
    }

    void ProjectContext::markClean() {
        if (m_dirty) { m_dirty = false; emit dirtyChanged(false); }
    }

    void ProjectContext::markDirty() {
        if (!m_dirty) { m_dirty = true; emit dirtyChanged(true); }
    }

    void ProjectContext::onNewProject() { newProject(); }

    void ProjectContext::resetState() {
        delete m_root; m_root = new doclife::model::Node;
        m_projectInfo = doclife::model::ProjectInfoModel{};
        m_filePath.clear(); markClean();
    }

    QString ProjectContext::makeTempFilePath() const {
        return tempDirPath() + "/untitled_" + QUuid::createUuid().toString(QUuid::WithoutBraces) + kFileSuffix;
    }

    void ProjectContext::cleanupTempFile() {
        if (m_tempFilePath.isEmpty()) return;
        QFile::remove(m_tempFilePath); QFile::remove(m_tempFilePath + ".tmp");
        m_tempFilePath.clear();
    }

    QString ProjectContext::acquireLock(const QString& filePath) {
        if (m_lockedPath == filePath) return {};
        const QString err = tryAcquireLockFile(filePath);
        if (!err.isEmpty()) return err;
        if (!m_lockedPath.isEmpty()) releaseLockFile(m_lockedPath);
        m_lockedPath = filePath;
        return {};
    }

    void ProjectContext::releaseLock() {
        if (m_lockedPath.isEmpty()) return;
        releaseLockFile(m_lockedPath);
        m_lockedPath.clear();
    }

    bool ProjectContext::newProject() {
        if (hasOpenProject()) { if (!closeProject(true)) return false; }
        const QString tempPath = makeTempFilePath();

        delete m_root; m_root = new doclife::model::Node;
        m_root->data.type = QStringLiteral("project");
        m_root->data.name = QStringLiteral("未命名项目");
        m_root->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        m_projectInfo = doclife::model::ProjectInfoModel{};
        m_projectInfo.projectnodeId = m_root->data.id;

        if (!writeDb(tempPath)) { resetState(); return false; }
        m_filePath = tempPath; m_tempFilePath = tempPath;
        markClean(); emit projectCreated(tempPath); emit projectOpened(tempPath);
        return true;
    }

    bool ProjectContext::openProject(const QString& filePath) {
        const QString cleanPath = QDir::cleanPath(filePath);
        if (!QFile::exists(cleanPath)) return false;
        if (hasOpenProject()) { if (!closeProject(true)) return false; }

        if (!readDb(cleanPath)) return false;

        m_lockedPath = cleanPath; m_filePath = cleanPath; m_tempFilePath.clear();
        markClean(); emit projectOpened(cleanPath);
        return true;
    }

    bool ProjectContext::saveProject() {
        if (isUntitled()) return false;
        if (!writeDb(m_filePath)) return false;
        markClean(); emit projectSaved(m_filePath);
        return true;
    }

    bool ProjectContext::saveProjectAs(const QString& filePath) {
        const QString cleanPath = QDir::cleanPath(filePath);
        if (cleanPath.isEmpty() || cleanPath == m_filePath) return saveProject();

        if (!writeDb(cleanPath)) return false;

        if (!m_lockedPath.isEmpty() && m_lockedPath != cleanPath) releaseLockFile(m_lockedPath);
        if (isUntitled()) { QFile::remove(m_tempFilePath); m_tempFilePath.clear(); }
        else { QFile::remove(m_filePath); }

        m_lockedPath = cleanPath; m_filePath = cleanPath;
        markClean(); emit projectSaved(cleanPath);
        return true;
    }

    bool ProjectContext::closeProject(bool saveBeforeClose) {
        if (!hasOpenProject()) return true;
        if (saveBeforeClose && isDirty()) { if (!saveProject()) return false; }
        emit projectClosed();
        releaseLock(); cleanupTempFile(); resetState();
        return true;
    }

    // ============================================================
    // 核心改造：基于统一树节点的 SQLite 读写
    // ============================================================

    bool ProjectContext::writeDb(const QString& targetPath) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "save_conn");
        db.setDatabaseName(targetPath);
        if (!db.open()) return false;

        {
            QSqlQuery q(db);
            // 建立适配未来拖拽的表结构
            q.exec("CREATE TABLE IF NOT EXISTS project_info (id TEXT PRIMARY KEY, name TEXT, owner_unit TEXT, construction_unit TEXT, design_unit TEXT, supervision_unit TEXT);");
            q.exec("CREATE TABLE IF NOT EXISTS tree_nodes (id TEXT PRIMARY KEY, parent_id TEXT, type TEXT, name TEXT, sort_order INTEGER, attrs TEXT);");
            q.exec("CREATE TABLE IF NOT EXISTS project_sheets (id INTEGER PRIMARY KEY AUTOINCREMENT, template_title TEXT, row_count INTEGER, col_count INTEGER);");
            q.exec("CREATE TABLE IF NOT EXISTS sheet_cells (sheet_id INTEGER, row INTEGER, col INTEGER, value TEXT, PRIMARY KEY(sheet_id, row, col));");
            q.exec("CREATE TABLE IF NOT EXISTS form_values (sheet_id INTEGER, row INTEGER, col INTEGER, value TEXT, PRIMARY KEY (sheet_id, row, col));");
            q.exec("CREATE TABLE IF NOT EXISTS sheet_row_heights (sheet_id INTEGER, row INTEGER, height REAL, PRIMARY KEY(sheet_id, row));");
            q.exec("CREATE TABLE IF NOT EXISTS sheet_col_widths (sheet_id INTEGER, col INTEGER, width REAL, PRIMARY KEY(sheet_id, col));");

            q.exec("BEGIN TRANSACTION;");
            q.exec("DELETE FROM project_info;");
            q.exec("DELETE FROM tree_nodes;");

            q.prepare("INSERT INTO project_info VALUES (?,?,?,?,?,?)");
            q.addBindValue(m_projectInfo.id);
            q.addBindValue(m_projectInfo.projectName);
            q.addBindValue(m_projectInfo.ownerUnitName);
            q.addBindValue(m_projectInfo.constructionUnitName);
            q.addBindValue(m_projectInfo.designUnitName);
            q.addBindValue(m_projectInfo.supervisionUnitName);
            q.exec();

            // 递归写入树节点，完美支持未来的拖拽和排序
            std::function<void(const doclife::model::Node&, const QString&)> saveNode =
                [&](const doclife::model::Node& n, const QString& parentId) {
                int sortOrder = 0;
                for (const auto* child : n.children) {
                    QSqlQuery nq(db);
                    nq.prepare("INSERT INTO tree_nodes (id, parent_id, type, name, sort_order, attrs) VALUES (?,?,?,?,?,?)");
                    nq.addBindValue(child->data.id);
                    nq.addBindValue(parentId);
                    nq.addBindValue(child->data.type);
                    nq.addBindValue(child->data.name);
                    nq.addBindValue(sortOrder++); // ★ 记录排序号

                    QJsonDocument doc(child->data.attrs);
                    nq.addBindValue(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
                    nq.exec();

                    saveNode(*child, child->data.id);
                }
                };
            if (m_root) saveNode(*m_root, "");

            q.exec("COMMIT;");
        }
        db.close();
        QSqlDatabase::removeDatabase("save_conn");
        return true;
    }

    bool ProjectContext::readDb(const QString& dbPath) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "load_conn");
        db.setDatabaseName(dbPath);
        if (!db.open()) return false;

        {
            QSqlQuery q(db);
            q.exec("SELECT id, name, owner_unit, construction_unit, design_unit, supervision_unit FROM project_info LIMIT 1;");
            if (q.next()) {
                m_projectInfo.id = q.value(0).toString();
                m_projectInfo.projectName = q.value(1).toString();
                m_projectInfo.ownerUnitName = q.value(2).toString();
                m_projectInfo.constructionUnitName = q.value(3).toString();
                m_projectInfo.designUnitName = q.value(4).toString();
                m_projectInfo.supervisionUnitName = q.value(5).toString();
            }

            delete m_root; m_root = nullptr;
            QHash<QString, doclife::model::Node*> nodeMap;
            QMultiHash<QString, doclife::model::Node*> childrenMap;

            // ★ 按 sort_order 读取，保证内存组装后的顺序与拖拽后一致
            q.exec("SELECT id, parent_id, type, name, attrs FROM tree_nodes ORDER BY parent_id, sort_order ASC;");
            while (q.next()) {
                auto* n = new doclife::model::Node;
                n->data.id = q.value(0).toString();
                n->data.type = q.value(2).toString();
                n->data.name = q.value(3).toString();

                QString attrsJson = q.value(4).toString();
                if (!attrsJson.isEmpty()) {
                    n->data.attrs = QJsonDocument::fromJson(attrsJson.toUtf8()).object();
                }

                nodeMap.insert(n->data.id, n);
                childrenMap.insert(q.value(1).toString(), n); // 以 parent_id 为键存入
            }

            // 组装树结构
            m_root = new doclife::model::Node;
            m_root->data.type = "project";
            m_root->data.id = m_projectInfo.projectnodeId;
            m_root->data.name = m_projectInfo.projectName.isEmpty() ? "未命名项目" : m_projectInfo.projectName;

            // 找出所有的顶层节点 (parent_id 为 "")
            QList<doclife::model::Node*> rootChildren = childrenMap.values("");
            for (auto* child : rootChildren) {
                child->parent = m_root;
                m_root->children.append(child);
            }

            // 递归组装子节点
            std::function<void(doclife::model::Node*)> buildTree = [&](doclife::model::Node* parent) {
                QList<doclife::model::Node*> children = childrenMap.values(parent->data.id);
                for (auto* child : children) {
                    child->parent = parent;
                    parent->children.append(child);
                    buildTree(child);
                }
                };
            buildTree(m_root);
        }
        db.close();
        QSqlDatabase::removeDatabase("load_conn");
        return true;
    }

    // ============================================================
    // 内存操作逻辑保持不变，保存时自动落盘
    // ============================================================

    bool ProjectContext::addChild(const QString& parentId, const QString& childType, QString* newId) {
        if (!m_root) return false;
        auto* parent = findNodeById(parentId);
        if (!parent) return false;

        auto* child = new doclife::model::Node;
        child->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        child->data.type = childType;
        child->data.name = doclife::model::NodeTypeRegistry::instance().displayLabel(childType);
        child->parent = parent;
        parent->children.append(child);

        markDirty();
        emit nodeAdded(parentId, child->data.id);
        if (newId) *newId = child->data.id;
        return true;
    }

    bool ProjectContext::addSheetsToNode(const QString& parentId, const QList<SheetNodeData>& sheets) {
        if (!m_root || sheets.isEmpty()) return false;
        auto* parent = findNodeById(parentId);
        if (!parent) return false;

        for (const auto& s : sheets) {
            auto* child = new doclife::model::Node;
            child->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            child->data.type = QStringLiteral("sheet");
            child->data.name = s.displayName;
            child->parent = parent;

            // ★ 为第二阶段的“复制数据”铺垫，先把模板线索存进 attrs
            child->data.attrs.insert(QStringLiteral("templateDbPath"), s.dbPath);
            child->data.attrs.insert(QStringLiteral("templateSheetId"), s.sheetId);
            child->data.attrs.insert(QStringLiteral("templateName"), s.templateName);

            parent->children.append(child);
            emit nodeAdded(parentId, child->data.id);
        }
        markDirty();
        return true;
    }

    bool ProjectContext::createItemWithSheets(const QString& parentId, const QString& itemName, const QList<SheetNodeData>& selectedSheets) {
        if (!m_root) return false;
        QString itemId;
        if (!addChild(parentId, QStringLiteral("item"), &itemId)) return false;
        renameNode(itemId, itemName);
        if (!selectedSheets.isEmpty()) addSheetsToNode(itemId, selectedSheets);
        emit nodeUpdated(parentId);
        return true;
    }

    bool ProjectContext::removeNode(const QString& nodeId) {
        if (!m_root || nodeId == m_root->data.id) return false;
        auto* target = findNodeById(nodeId);
        if (!target || !target->parent) return false;
        auto* parent = target->parent;

        emit nodeRemoved(parent->data.id, nodeId);
        parent->children.removeOne(target);
        delete target;
        markDirty();
        return true;
    }

    bool ProjectContext::renameNode(const QString& nodeId, const QString& newName, RenameConflictPolicy policy) {
        auto* node = findNodeById(nodeId);
        if (!node) return false;
        const QString trimmed = newName.trimmed();
        if (trimmed.isEmpty() || trimmed == node->data.name) return false;
        node->data.name = trimmed;
        markDirty();
        emit nodeUpdated(nodeId);
        return true;
    }

    bool ProjectContext::setNodeAttr(const QString& nodeId, const QString& key, const QVariant& value) {
        auto* node = findNodeById(nodeId);
        if (!node) return false;
        node->data.attrs.insert(key, QJsonValue::fromVariant(value));
        markDirty();
        emit nodeUpdated(nodeId);
        return true;
    }

    // ★ 新增：纯粹的移动逻辑，供未来接入拖拽
    bool ProjectContext::moveNode(const QString& nodeId, const QString& newParentId, int newIndex) {
        if (nodeId == newParentId) return false;
        auto* node = findNodeById(nodeId);
        auto* newParent = findNodeById(newParentId);
        if (!node || !newParent || node == m_root) return false;

        // 防止把父节点拖进自己的子孙节点
        auto* check = newParent;
        while (check) {
            if (check == node) return false;
            check = check->parent;
        }

        auto* oldParent = node->parent;
        if (!oldParent) return false;

        emit nodeRemoved(oldParent->data.id, nodeId);
        oldParent->children.removeOne(node);

        node->parent = newParent;
        if (newIndex < 0 || newIndex >= newParent->children.size()) {
            newParent->children.append(node);
        }
        else {
            newParent->children.insert(newIndex, node);
        }

        emit nodeAdded(newParentId, nodeId);
        markDirty();
        return true;
    }

} // namespace doclife::core