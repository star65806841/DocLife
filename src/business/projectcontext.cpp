#include "ProjectContext.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QUuid>
#include <QStandardPaths>
#include <QDate>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>
#include <QVector>
#include <functional>

#include "utils.h"
#include "model/custommodel/NodeTypeRegistry.h"
#include "model/cellrole.h"
#include "model/fieldtokens.h"

namespace doclife::core {

    namespace {
        const QString kLockSuffix = QStringLiteral(".lock");

        QString tempDirPath() {
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/DocLife");
            QDir().mkpath(dir);
            return dir;
        }

        QString tryAcquireLockFile(const QString& filePath) {
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

        // 纯净版：一步到位建立完整的表结构
        bool ensureSchema(QSqlDatabase& db) {
            QSqlQuery q(db);
            if (!q.exec("CREATE TABLE IF NOT EXISTS project_info ("
                "id TEXT PRIMARY KEY, projectnode_id TEXT, name TEXT, owner_unit TEXT, construction_unit TEXT, design_unit TEXT, supervision_unit TEXT);")) return false;

            if (!q.exec("CREATE TABLE IF NOT EXISTS tree_nodes ("
                "id TEXT PRIMARY KEY, parent_id TEXT, type TEXT, name TEXT, sort_order INTEGER, attrs TEXT);")) return false;

            if (!q.exec("CREATE TABLE IF NOT EXISTS project_sheets ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, template_title TEXT, row_count INTEGER, col_count INTEGER);")) return false;

            // 完整的 20 列单元格表（cell_role 见 CellRole）
            if (!q.exec("CREATE TABLE IF NOT EXISTS sheet_cells ("
                "sheet_id INTEGER, row INTEGER, col INTEGER, row_span INTEGER, col_span INTEGER, value TEXT, "
                "font_family TEXT, font_size REAL, font_bold INTEGER, font_italic INTEGER, font_underline INTEGER, "
                "font_color TEXT, bg_color TEXT, h_align INTEGER, v_align INTEGER, "
                "border_top INTEGER, border_bottom INTEGER, border_left INTEGER, border_right INTEGER, wrap_text INTEGER, "
                "cell_role TEXT, "
                "PRIMARY KEY(sheet_id, row, col));")) return false;
            // 老工程库补列（失败说明列已存在，忽略即可）
            q.exec("ALTER TABLE sheet_cells ADD COLUMN cell_role TEXT;");

            if (!q.exec("CREATE TABLE IF NOT EXISTS form_values ("
                "sheet_id INTEGER, row INTEGER, col INTEGER, value TEXT, "
                "PRIMARY KEY (sheet_id, row, col));")) return false;
            if (!q.exec("CREATE TABLE IF NOT EXISTS sheet_row_heights ("
                "sheet_id INTEGER, row INTEGER, height REAL, "
                "PRIMARY KEY(sheet_id, row));")) return false;
            if (!q.exec("CREATE TABLE IF NOT EXISTS sheet_col_widths ("
                "sheet_id INTEGER, col INTEGER, width REAL, "
                "PRIMARY KEY(sheet_id, col));")) return false;

            q.exec("CREATE INDEX IF NOT EXISTS idx_tree_nodes_parent ON tree_nodes(parent_id);");
            return true;
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
        // 单例的析构发生在 QApplication 之后，这时再操作 QSqlDatabase 只会报警告
        if (QCoreApplication::instance()) closeDbConnection();
        releaseLock();
        cleanupTempFile();
        delete m_root;
        m_root = nullptr;
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
        // 工程名以 project_info 为准，树里的根节点跟着同步，避免两处显示不一致
        if (m_root && !m_projectInfo.projectName.isEmpty()
            && m_root->data.name != m_projectInfo.projectName) {
            m_root->data.name = m_projectInfo.projectName;
            emit nodeUpdated(m_root->data.id);
        }
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
        closeDbConnection();
        cleanupTempFile();
        delete m_root; m_root = new doclife::model::Node;
        m_projectInfo = doclife::model::ProjectInfoModel{};
        m_filePath.clear();
        m_tempFilePath.clear();
        markClean();
    }

    QString ProjectContext::makeTempFilePath() const {
        return tempDirPath() + "/untitled_" + QUuid::createUuid().toString(QUuid::WithoutBraces) + kFileSuffix;
    }

    void ProjectContext::cleanupTempFile() {
        if (m_tempFilePath.isEmpty()) return;
        QFile::remove(m_tempFilePath);
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

    QSqlDatabase ProjectContext::getProjectDb() {
        if (m_dbConnName.isEmpty()) return {};
        return QSqlDatabase::database(m_dbConnName);
    }

    bool ProjectContext::openDbConnection(const QString& dbPath) {
        closeDbConnection();

        const QString connName = "project_live_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        bool ok = false;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
            db.setDatabaseName(dbPath);
            if (!db.open()) {
                qWarning() << "无法打开项目数据库:" << dbPath << db.lastError().text();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(connName);
                return false;
            }
            m_dbConnName = connName;
            ok = ensureSchema(db);
            if (!ok) {
                qWarning() << "创建数据库结构失败:" << db.lastError().text();
            }
        }
        if (!ok) {
            closeDbConnection();
            return false;
        }
        return true;
    }

    QHash<QString, QString> ProjectContext::fieldValues(const QString& nodeId) const
    {
        QHash<QString, QString> values;
        values.insert(QStringLiteral("projectName"),
            m_projectInfo.projectName.isEmpty() ? displayName() : m_projectInfo.projectName);
        values.insert(QStringLiteral("ownerUnit"), m_projectInfo.ownerUnitName);
        values.insert(QStringLiteral("constructionUnit"), m_projectInfo.constructionUnitName);
        values.insert(QStringLiteral("designUnit"), m_projectInfo.designUnitName);
        values.insert(QStringLiteral("supervisionUnit"), m_projectInfo.supervisionUnitName);
        values.insert(QStringLiteral("date"), QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));

        // 从当前节点往上找最近的一层，填单位/分部/分项/表格名
        for (const auto* node = findNodeById(nodeId); node; node = node->parent) {
            if (node->data.type == QLatin1String("unit") && !values.contains(QStringLiteral("unitName")))
                values.insert(QStringLiteral("unitName"), node->data.name);
            else if (node->data.type == QLatin1String("sub") && !values.contains(QStringLiteral("subName")))
                values.insert(QStringLiteral("subName"), node->data.name);
            else if (node->data.type == QLatin1String("item") && !values.contains(QStringLiteral("itemName")))
                values.insert(QStringLiteral("itemName"), node->data.name);
            else if (node->data.type == QLatin1String("sheet") && !values.contains(QStringLiteral("sheetName")))
                values.insert(QStringLiteral("sheetName"), node->data.name);
        }
        return values;
    }

    void ProjectContext::closeDbConnection() {
        if (m_dbConnName.isEmpty()) return;
        {
            QSqlDatabase db = QSqlDatabase::database(m_dbConnName, false);
            if (db.isValid() && db.isOpen()) db.close();
        }
        QSqlDatabase::removeDatabase(m_dbConnName);
        m_dbConnName.clear();
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

        if (!openDbConnection(tempPath)) { resetState(); return false; }
        if (!writeDb(tempPath)) { closeDbConnection(); resetState(); return false; }

        m_filePath = tempPath;
        m_tempFilePath = tempPath;

        markClean();
        emit projectCreated(tempPath);
        emit projectOpened(tempPath);
        return true;
    }

    bool ProjectContext::openProject(const QString& filePath) {
        const QString cleanPath = QDir::cleanPath(filePath);
        if (!QFile::exists(cleanPath)) return false;
        if (hasOpenProject()) { if (!closeProject(true)) return false; }

        const QString lockErr = acquireLock(cleanPath);
        if (!lockErr.isEmpty()) {
            qWarning() << lockErr;
            return false;
        }

        if (!openDbConnection(cleanPath)) {
            releaseLock();
            return false;
        }
        if (!readDb(cleanPath)) {
            closeDbConnection();
            releaseLock();
            return false;
        }

        m_lockedPath = cleanPath;
        m_filePath = cleanPath;
        m_tempFilePath.clear();

        markClean();
        emit projectOpened(cleanPath);
        return true;
    }

    bool ProjectContext::saveProject() {
        if (isUntitled()) return false;
        // 通知视图先把内存里的填报内容写入数据库，再回写工程树
        emit projectSaving();
        if (!writeDb(m_filePath)) return false;
        markClean();
        emit projectSaved(m_filePath);
        return true;
    }

    bool ProjectContext::saveProjectAs(const QString& filePath) {
        const QString cleanPath = QDir::cleanPath(filePath);
        if (cleanPath.isEmpty()) return false;
        if (cleanPath == QDir::cleanPath(m_filePath)) return saveProject();
        if (m_filePath.isEmpty()) return false;

        const bool    wasUntitled = isUntitled();
        const QString oldPath = m_filePath;

        // 1. 先把内存中的编辑内容与工程树写进当前数据库
        emit projectSaving();
        if (!writeDb(oldPath)) return false;

        // 2. 锁定目标文件，避免两个实例同时写入
        const QString lockErr = acquireLock(cleanPath);
        if (!lockErr.isEmpty()) {
            qWarning() << lockErr;
            return false;
        }

        // 失败时回到原来的打开状态，保证会话还能继续用
        auto restoreOld = [&]() {
            openDbConnection(oldPath);
            acquireLock(oldPath);
            };

        // 3. 关闭连接后整库复制：表格内容、填报数据、行高列宽一并带走
        closeDbConnection();
        if (QFile::exists(cleanPath) && !QFile::remove(cleanPath)) {
            qWarning() << "另存失败：无法覆盖目标文件" << cleanPath;
            restoreOld();
            return false;
        }
        if (!QFile::copy(oldPath, cleanPath)) {
            qWarning() << "另存失败：无法复制数据库" << oldPath << "->" << cleanPath;
            restoreOld();
            return false;
        }
        if (!openDbConnection(cleanPath)) {
            qWarning() << "另存失败：无法打开新数据库" << cleanPath;
            restoreOld();
            return false;
        }

        if (wasUntitled) {
            if (!m_tempFilePath.isEmpty()) {
                QFile::remove(m_tempFilePath);
                m_tempFilePath.clear();
            }
        }

        m_filePath = cleanPath;
        m_lockedPath = cleanPath;   // acquireLock 已把锁切到目标文件

        markClean();
        emit projectSaved(cleanPath);
        return true;
    }

    bool ProjectContext::closeProject(bool saveBeforeClose) {
        if (!hasOpenProject()) return true;
        // 未命名项目只是临时数据库，没有可回写的文件；是否另存由上层提示用户决定，
        // 这里不能因为 saveProject() 返回 false 就把关闭流程卡住。
        if (saveBeforeClose && isDirty() && !isUntitled()) {
            if (!saveProject()) return false;
        }

        closeDbConnection();
        emit projectClosed();
        releaseLock();
        cleanupTempFile();
        resetState();
        return true;
    }

    bool ProjectContext::writeDb(const QString& targetPath) {
        const QString cleanTarget = QDir::cleanPath(targetPath);
        if (cleanTarget.isEmpty()) return false;

        {
            QSqlDatabase cur = QSqlDatabase::database(m_dbConnName, false);
            if (!cur.isValid() || !cur.isOpen() || QDir::cleanPath(cur.databaseName()) != cleanTarget) {
                if (!openDbConnection(cleanTarget)) return false;
            }
        }

        QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
        if (!db.isOpen()) return false;

        if (m_projectInfo.projectnodeId.isEmpty() && m_root) {
            m_projectInfo.projectnodeId = m_root->data.id;
        }

        if (!db.transaction()) return false;

        auto fail = [&]() {
            qWarning() << "写入项目数据库失败:" << db.lastError().text();
            db.rollback();
            return false;
            };

        {
            QSqlQuery q(db);
            if (!q.exec("DELETE FROM tree_nodes;")) return fail();
            if (!q.exec("DELETE FROM project_info;")) return fail();

            q.prepare("INSERT INTO project_info (id, projectnode_id, name, owner_unit, construction_unit, design_unit, supervision_unit) VALUES (?,?,?,?,?,?,?)");
            q.addBindValue(m_projectInfo.id);
            q.addBindValue(m_projectInfo.projectnodeId);
            q.addBindValue(m_projectInfo.projectName);
            q.addBindValue(m_projectInfo.ownerUnitName);
            q.addBindValue(m_projectInfo.constructionUnitName);
            q.addBindValue(m_projectInfo.designUnitName);
            q.addBindValue(m_projectInfo.supervisionUnitName);
            if (!q.exec()) return fail();
        }

        if (m_root) {
            bool ok = true;
            std::function<void(const doclife::model::Node&, const QString&)> saveNode =
                [&](const doclife::model::Node& n, const QString& parentId) {
                if (!ok) return;
                int sortOrder = 0;
                for (const auto* child : n.children) {
                    QSqlQuery nq(db);
                    nq.prepare("INSERT INTO tree_nodes (id, parent_id, type, name, sort_order, attrs) VALUES (?,?,?,?,?,?)");
                    nq.addBindValue(child->data.id);
                    nq.addBindValue(parentId);
                    nq.addBindValue(child->data.type);
                    nq.addBindValue(child->data.name);
                    nq.addBindValue(sortOrder++);
                    QJsonDocument doc(child->data.attrs);
                    nq.addBindValue(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
                    if (!nq.exec()) { ok = false; return; }
                    saveNode(*child, child->data.id);
                }
                };
            saveNode(*m_root, "");
            if (!ok) return fail();
        }

        if (!db.commit()) return fail();
        return true;
    }

    bool ProjectContext::readDb(const QString& dbPath) {
        QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
        if (!db.isOpen()) return false;
        if (QDir::cleanPath(db.databaseName()) != QDir::cleanPath(dbPath)) return false;

        {
            QSqlQuery q(db);
            m_projectInfo = doclife::model::ProjectInfoModel{};
            if (q.exec("SELECT id, projectnode_id, name, owner_unit, construction_unit, design_unit, supervision_unit FROM project_info LIMIT 1;") && q.next()) {
                m_projectInfo.id = q.value(0).toString();
                m_projectInfo.projectnodeId = q.value(1).toString();
                m_projectInfo.projectName = q.value(2).toString();
                m_projectInfo.ownerUnitName = q.value(3).toString();
                m_projectInfo.constructionUnitName = q.value(4).toString();
                m_projectInfo.designUnitName = q.value(5).toString();
                m_projectInfo.supervisionUnitName = q.value(6).toString();
            }

            delete m_root;
            m_root = new doclife::model::Node;
            m_root->data.type = QStringLiteral("project");
            m_root->data.id = m_projectInfo.projectnodeId;
            if (m_root->data.id.isEmpty()) m_root->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            m_projectInfo.projectnodeId = m_root->data.id;
            m_root->data.name = m_projectInfo.projectName.isEmpty() ? QStringLiteral("未命名项目") : m_projectInfo.projectName;

            // 用 QList 保序：QMultiHash::values() 的返回顺序不受保证，
            // 直接用会让重开工程后同级节点的顺序被打乱。
            QHash<QString, QList<doclife::model::Node*>> childrenMap;
            if (q.exec("SELECT id, parent_id, type, name, attrs FROM tree_nodes ORDER BY parent_id, sort_order ASC;")) {
                while (q.next()) {
                    auto* n = new doclife::model::Node;
                    n->data.id = q.value(0).toString();
                    n->data.type = q.value(2).toString();
                    n->data.name = q.value(3).toString();
                    const QString attrsJson = q.value(4).toString();
                    if (!attrsJson.isEmpty()) n->data.attrs = QJsonDocument::fromJson(attrsJson.toUtf8()).object();
                    childrenMap[q.value(1).toString()].append(n);
                }
            }

            // 根节点的子节点在库里 parent_id 是空串，其余节点用父节点 id。
            // 递归时必须把「库里的父键」和「内存里的父节点」分开传，
            // 否则第一层之后的节点永远挂不回去。
            std::function<void(doclife::model::Node*, const QString&)> buildTree =
                [&](doclife::model::Node* parent, const QString& dbParentId) {
                const QList<doclife::model::Node*> children = childrenMap.value(dbParentId);
                for (auto* child : children) {
                    child->parent = parent;
                    parent->children.append(child);
                    buildTree(child, child->data.id);
                }
                };
            buildTree(m_root, QString());
        }
        return true;
    }

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

        QSqlDatabase projectDb = getProjectDb();
        if (!projectDb.isOpen()) return false;

        bool anyAdded = false;

        for (const auto& s : sheets) {
            int rowCount = 0, colCount = 0;
            QVector<QVariantList> cellRows;
            QVector<QVariantList> widthRows;
            QVector<QVariantList> heightRows;
            bool isTplOpen = false;

            {
                const QString tplConnName = "tpl_copy_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
                {
                    QSqlDatabase tplDb = QSqlDatabase::addDatabase("QSQLITE", tplConnName);
                    tplDb.setDatabaseName(s.dbPath);
                    isTplOpen = tplDb.open();
                    if (isTplOpen) {
                        QSqlQuery q(tplDb);
                        q.prepare("SELECT row_count, col_count FROM template_sheets WHERE id = ?");
                        q.addBindValue(s.sheetId);
                        if (q.exec() && q.next()) {
                            rowCount = q.value(0).toInt();
                            colCount = q.value(1).toInt();
                        }

                        QSqlQuery tq(tplDb);
                        tq.prepare("SELECT row, col, row_span, col_span, value, font_family, font_size, font_bold, font_italic, font_underline, font_color, bg_color, h_align, v_align, border_top, border_bottom, border_left, border_right, wrap_text FROM sheet_cells WHERE sheet_id = ?");
                        tq.addBindValue(s.sheetId);
                        if (tq.exec()) {
                            while (tq.next()) {
                                QVariantList row;
                                for (int i = 0; i < 19; ++i) row << tq.value(i);
                                cellRows.append(row);
                            }
                        }

                        QSqlQuery cq(tplDb);
                        cq.prepare("SELECT col, width FROM sheet_col_widths WHERE sheet_id = ?");
                        cq.addBindValue(s.sheetId);
                        if (cq.exec()) {
                            while (cq.next()) {
                                QVariantList row; row << cq.value(0) << cq.value(1);
                                widthRows.append(row);
                            }
                        }

                        QSqlQuery hq(tplDb);
                        hq.prepare("SELECT row, height FROM sheet_row_heights WHERE sheet_id = ?");
                        hq.addBindValue(s.sheetId);
                        if (hq.exec()) {
                            while (hq.next()) {
                                QVariantList row; row << hq.value(0) << hq.value(1);
                                heightRows.append(row);
                            }
                        }
                        tplDb.close();
                    }
                }
                QSqlDatabase::removeDatabase(tplConnName);
            }

            if (!isTplOpen) continue;

            if (!projectDb.transaction()) continue;

            int projectSheetId = -1;
            {
                QSqlQuery q(projectDb);
                q.prepare("INSERT INTO project_sheets (template_title, row_count, col_count) VALUES (?,?,?)");
                q.addBindValue(s.templateName);
                q.addBindValue(rowCount);
                q.addBindValue(colCount);
                if (!q.exec()) { projectDb.rollback(); continue; }
                projectSheetId = q.lastInsertId().toInt();
            }

            if (projectSheetId >= 0 && !cellRows.isEmpty()) {
                QSqlQuery iq(projectDb);
                iq.prepare("INSERT INTO sheet_cells (sheet_id, row, col, row_span, col_span, value, font_family, font_size, font_bold, font_italic, font_underline, font_color, bg_color, h_align, v_align, border_top, border_bottom, border_left, border_right, wrap_text, cell_role) "
                    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");

                // 颜色即区域：模板里用填充色标记的区域，在这里翻译成 cell_role。
                // 颜色保留在 bg_color 上，编辑时直接显示为区域底色。
                QVector<QString> roles;
                roles.reserve(cellRows.size());
                for (const auto& r : cellRows) {
                    const QString text = r.value(4).toString().trimmed();
                    const QString bg = r.value(11).toString().trimmed().toUpper();
                    QString role;

                    if (bg == QStringLiteral("#D9D9D9")) {
                        role = model::cellRoleKey(model::CellRole::ReadOnly);
                    }
                    else if (bg == QStringLiteral("#FFF2CC")) {
                        role = model::cellRoleKey(model::CellRole::Project);
                    }
                    else if (bg == QStringLiteral("#DDEBF7")) {
                        role = model::cellRoleKey(model::CellRole::Summary);
                    }
                    else if (bg == QStringLiteral("#E2EFDA")) {
                        role = model::cellRoleKey(model::CellRole::Date);
                    }
                    else if (text.contains(QStringLiteral("工程名称")) || text.contains(QStringLiteral("项目名称"))
                        || text.contains(QStringLiteral("建设单位")) || text.contains(QStringLiteral("施工单位"))
                        || text.contains(QStringLiteral("设计单位")) || text.contains(QStringLiteral("监理单位"))) {
                        role = model::cellRoleKey(model::CellRole::Project);
                    }
                    else if (model::containsFieldToken(text)) {
                        // #projectName / #unitName 这类占位符：由工程/节点信息填充
                        role = model::cellRoleKey(model::CellRole::Project);
                    }
                    else {
                        role = model::cellRoleKey(model::CellRole::Input);
                    }
                    roles.append(role);
                }

                // 工程信息的“值单元格”在标签右侧，一并标成 project
                QHash<quint64, int> indexOfCell;
                for (int i = 0; i < cellRows.size(); ++i) {
                    const quint64 key = (quint64(cellRows[i].value(0).toInt()) << 32)
                        | quint32(cellRows[i].value(1).toInt());
                    indexOfCell.insert(key, i);
                }
                for (int i = 0; i < cellRows.size(); ++i) {
                    if (roles[i] != model::cellRoleKey(model::CellRole::Project)) continue;
                    const int row = cellRows[i].value(0).toInt();
                    const int col = cellRows[i].value(1).toInt();
                    auto it = indexOfCell.constFind((quint64(row) << 32) | quint32(col + 1));
                    if (it != indexOfCell.constEnd()) {
                        roles[it.value()] = model::cellRoleKey(model::CellRole::Project);
                    }
                }

                bool ok = true;
                for (int i = 0; i < cellRows.size(); ++i) {
                    const auto& r = cellRows[i];
                    iq.bindValue(0, projectSheetId);
                    for (int c = 0; c < 19; ++c) iq.bindValue(c + 1, r.value(c));
                    iq.bindValue(20, roles[i]);
                    if (!iq.exec()) { ok = false; break; }
                }
                if (!ok) { projectDb.rollback(); continue; }
            }

            if (projectSheetId >= 0 && !widthRows.isEmpty()) {
                QSqlQuery iq(projectDb);
                iq.prepare("INSERT INTO sheet_col_widths (sheet_id, col, width) VALUES (?,?,?)");
                for (const auto& r : widthRows) {
                    iq.bindValue(0, projectSheetId); iq.bindValue(1, r.value(0)); iq.bindValue(2, r.value(1));
                    iq.exec();
                }
            }

            if (projectSheetId >= 0 && !heightRows.isEmpty()) {
                QSqlQuery iq(projectDb);
                iq.prepare("INSERT INTO sheet_row_heights (sheet_id, row, height) VALUES (?,?,?)");
                for (const auto& r : heightRows) {
                    iq.bindValue(0, projectSheetId); iq.bindValue(1, r.value(0)); iq.bindValue(2, r.value(1));
                    iq.exec();
                }
            }

            if (!projectDb.commit()) { projectDb.rollback(); continue; }

            auto* child = new doclife::model::Node;
            child->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            child->data.type = QStringLiteral("sheet");
            child->data.name = s.displayName;
            child->data.attrs.insert(QStringLiteral("projectSheetId"), projectSheetId);
            child->parent = parent;
            parent->children.append(child);

            emit nodeAdded(parentId, child->data.id);
            anyAdded = true;
        }

        if (anyAdded) markDirty();
        return anyAdded;
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

        QSqlDatabase db = getProjectDb();

        // 先收集整棵子树：Node 析构时会 delete 自己的 children，
        // 如果递归删除子节点后再 delete 父节点，就会发生二次释放。
        QList<doclife::model::Node*> subtree;
        std::function<void(doclife::model::Node*)> collect =
            [&](doclife::model::Node* n) {
            subtree.append(n);
            for (auto* c : n->children) collect(c);
            };
        collect(target);

        for (auto* n : subtree) {
            if (n->data.type != QStringLiteral("sheet")) continue;

            const int psid = n->data.attrs.value(QStringLiteral("projectSheetId")).toInt(-1);
            if (psid < 0 || !db.isOpen()) continue;

            QSqlQuery q(db);
            q.prepare("DELETE FROM project_sheets WHERE id = ?"); q.addBindValue(psid); q.exec();
            q.prepare("DELETE FROM sheet_cells WHERE sheet_id = ?"); q.addBindValue(psid); q.exec();
            q.prepare("DELETE FROM form_values WHERE sheet_id = ?"); q.addBindValue(psid); q.exec();
            q.prepare("DELETE FROM sheet_row_heights WHERE sheet_id = ?"); q.addBindValue(psid); q.exec();
            q.prepare("DELETE FROM sheet_col_widths WHERE sheet_id = ?"); q.addBindValue(psid); q.exec();
        }

        // 断开父子关系后统一释放，~Node 就不会再重复删除子节点
        for (auto* n : subtree) n->children.clear();
        qDeleteAll(subtree);

        markDirty();
        return true;
    }

    bool ProjectContext::renameNode(const QString& nodeId, const QString& newName, RenameConflictPolicy policy) {
        Q_UNUSED(policy);
        auto* node = findNodeById(nodeId);
        if (!node) return false;
        const QString trimmed = newName.trimmed();
        if (trimmed.isEmpty() || trimmed == node->data.name) return false;
        node->data.name = trimmed;
        // 根节点就是工程本身：工程名单独存在 project_info 里，不同步的话重开工程会退回默认名
        if (m_root && node == m_root) m_projectInfo.projectName = trimmed;
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

    bool ProjectContext::moveNode(const QString& nodeId, const QString& newParentId, int newIndex) {
        if (nodeId == newParentId) return false;
        auto* node = findNodeById(nodeId);
        auto* newParent = findNodeById(newParentId);
        if (!node || !newParent || node == m_root) return false;

        // 不能把节点拖进自己的子树
        auto* check = newParent;
        while (check) {
            if (check == node) return false;
            check = check->parent;
        }

        // 结构规则统一在这里校验：拖拽和以后任何移动入口都走同一套判断
        const auto& registry = doclife::model::NodeTypeRegistry::instance();
        if (!registry.canAcceptChild(newParent->data.type, node->data.type)) {
            emit errorOccurred(QStringLiteral("「%1」不能放到「%2」下面")
                .arg(registry.displayLabel(node->data.type),
                    registry.displayLabel(newParent->data.type)));
            return false;
        }

        auto* oldParent = node->parent;
        if (!oldParent) return false;

        const int oldIndex = oldParent->children.indexOf(node);

        // 同一个父节点内向下移动时，摘掉自己后目标索引会左移一位
        int insertIndex = newIndex;
        if (oldParent == newParent && insertIndex >= 0 && oldIndex >= 0 && oldIndex < insertIndex) {
            --insertIndex;
        }

        emit nodeRemoved(oldParent->data.id, nodeId);
        oldParent->children.removeOne(node);

        node->parent = newParent;
        if (insertIndex < 0 || insertIndex >= newParent->children.size()) newParent->children.append(node);
        else newParent->children.insert(insertIndex, node);

        emit nodeAdded(newParentId, nodeId);
        markDirty();
        return true;
    }

} // namespace doclife::core