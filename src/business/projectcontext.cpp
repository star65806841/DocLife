#include "ProjectContext.h"

#include <QDir>
#include <QUuid>
#include <QFile>
#include <QHash>
#include <QDebug>
#include <QVector>
#include <QSaveFile>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QJsonParseError>
#include <QCoreApplication>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <projectNode.h>
#  include <windows.h>
#else
#  include <signal.h>
#  include <errno.h>
#endif

#include "utils.h"
#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"
#include "model/custommodel/NodeTypeRegistry.h"

namespace doclife::core {

    namespace {

        const QString kLockSuffix = QStringLiteral(".lock");

        bool isProcessAlive(qint64 pid) {
            if (pid <= 0) return false;
#ifdef Q_OS_WIN
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
            if (!h) return false;
            DWORD code = 0;
            BOOL ok = GetExitCodeProcess(h, &code);
            CloseHandle(h);
            return ok && code == STILL_ACTIVE;
#else
            return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
        }

        QString tempDirPath() {
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                + QStringLiteral("/DocLife");
            QDir().mkpath(dir);
            return dir;
        }

        // 返回空=成功，否则=错误文本
        QString tryAcquireLockFile(const QString& filePath) {
            const QString lockPath = filePath + kLockSuffix;
            QFile lock(lockPath);
            if (lock.exists()) {
                if (lock.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const qint64 pid = QString::fromUtf8(lock.readAll()).trimmed().toLongLong();
                    lock.close();
                    if (pid > 0 && isProcessAlive(pid)) {
                        const QString err = QStringLiteral("文件正在被其他进程使用（PID %1）").arg(pid);
                        qWarning() << "[ProjectContext]" << err;
                        return err;
                    }
                    qDebug() << "[ProjectContext] 发现残留锁（进程已死），清理:" << lockPath;
                }
                QFile::remove(lockPath);
            }
            if (!lock.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                const QString err = QStringLiteral("无法创建锁文件: %1").arg(lockPath);
                qWarning() << "[ProjectContext]" << err;
                return err;
            }
            lock.write(QByteArray::number(QCoreApplication::applicationPid()));
            lock.close();
            qDebug() << "[ProjectContext] 已加锁:" << lockPath;
            return {};
        }

        void releaseLockFile(const QString& filePath) {
            if (filePath.isEmpty()) return;
            const QString lockPath = filePath + kLockSuffix;
            if (QFile::remove(lockPath))
                qDebug() << "[ProjectContext] 已解锁:" << lockPath;
        }

        int cleanupOrphanTempFiles() {
            const QString dir = tempDirPath();
            if (!QDir(dir).exists()) return 0;
            int removed = 0;
            QDir d(dir);
            const QStringList patterns{
                QStringLiteral("*.doclife"),
                QStringLiteral("*.doclife.json"),
                QStringLiteral("*.doclife.json.bak"),
                QStringLiteral("*.lock")
            };
            for (const auto& pat : patterns) {
                for (const QFileInfo& fi : d.entryInfoList({ pat }, QDir::Files)) {
                    const QString path = fi.absoluteFilePath();
                    if (path.endsWith(QStringLiteral(".lock"))) {
                        QFile f(path);
                        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                            const qint64 pid = QString::fromUtf8(f.readAll()).trimmed().toLongLong();
                            f.close();
                            if (pid > 0 && isProcessAlive(pid)) continue;
                        }
                    }
                    if (QFile::remove(path)) ++removed;
                }
            }
            return removed;
        }

    } // namespace

    // ============================================================
    ProjectContext* ProjectContext::instance() {
        static ProjectContext s;
        return &s;
    }

    ProjectContext::ProjectContext(QObject* parent) : QObject(parent) {
        qDebug() << "[ProjectContext] 构造, PID =" << QCoreApplication::applicationPid();
        m_root = new doclife::model::Node;
        const int n = cleanupOrphanTempFiles();
        if (n > 0) qDebug() << "[ProjectContext] 清理残留临时文件:" << n << "个";
        initConnect();
    }

    ProjectContext::~ProjectContext() {
        qDebug() << "[ProjectContext] 析构";
        releaseLock();
        cleanupTempFile();
        delete m_root;
        m_root = nullptr;
    }

    void ProjectContext::initConnect() {}

    // ============================================================
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

    const doclife::model::Node*
        ProjectContext::findNodeById(const QString& id) const
    {
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
        return const_cast<doclife::model::Node*>(
            static_cast<const ProjectContext*>(this)->findNodeById(id));
    }

    void ProjectContext::setProjectInfo(model::ProjectInfoModel& projectInfo) {
        qDebug() << "[ProjectContext] setProjectInfo:" << projectInfo.projectName;
        m_projectInfo.id = projectInfo.id;
        m_projectInfo.projectnodeId = projectInfo.projectnodeId;
        m_projectInfo.projectName = projectInfo.projectName.trimmed();
        m_projectInfo.ownerUnitName = projectInfo.ownerUnitName.trimmed();
        m_projectInfo.constructionUnitName = projectInfo.constructionUnitName.trimmed();
        m_projectInfo.designUnitName = projectInfo.designUnitName.trimmed();
        m_projectInfo.supervisionUnitName = projectInfo.supervisionUnitName.trimmed();
        markDirty();
    }

    // ============================================================
    // 重命名
    // ============================================================
    bool ProjectContext::renameNode(const QString& nodeId, const QString& newName,
        RenameConflictPolicy policy)
    {
        qDebug() << "[ProjectContext] renameNode:" << nodeId << "->" << newName;

        auto* node = findNodeById(nodeId);
        if (!node) {
            const QString err = QStringLiteral("找不到节点: %1").arg(nodeId);
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        const QString trimmed = newName.trimmed();
        if (trimmed.isEmpty()) {
            const QString err = QStringLiteral("名称不能为空");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }
        if (trimmed == node->data.name) return true;

        const bool isRoot = (m_root && nodeId == m_root->data.id);
        const bool needFileMove = isRoot && !isUntitled() && !m_filePath.isEmpty();
        const QString oldName = node->data.name;

        node->data.name = trimmed;

        if (needFileMove) {
            QString actualBase;
            const QString mvErr = doRenameProjectFile(trimmed, policy, &actualBase);
            if (!mvErr.isEmpty()) {
                node->data.name = oldName;
                qWarning() << "[ProjectContext] renameNode 失败，名称回滚为:" << oldName;
                emit errorOccurred(mvErr);
                return false;
            }
            if (!actualBase.isEmpty() && actualBase != trimmed)
                node->data.name = actualBase;
        }
        else {
            markDirty();
        }
        emit nodeUpdated(nodeId);
        qDebug() << "[ProjectContext] renameNode 成功:" << node->data.name;
        return true;
    }

    bool ProjectContext::renameProjectFile(const QString& newBaseName,
        RenameConflictPolicy policy,
        QString* actualBaseName)
    {
        qDebug() << "[ProjectContext] renameProjectFile:" << newBaseName;
        const QString err = doRenameProjectFile(newBaseName, policy, actualBaseName);
        if (!err.isEmpty()) {
            emit errorOccurred(err);
            return false;
        }
        return true;
    }

    QString ProjectContext::doRenameProjectFile(const QString& newBaseName,
        RenameConflictPolicy policy,
        QString* actualBaseName)
    {
        if (m_filePath.isEmpty() || isUntitled()) {
            const QString err = QStringLiteral("当前项目尚未保存到用户路径");
            qWarning() << "[ProjectContext]" << err;
            return err;
        }

        const QFileInfo fi(m_filePath);
        const QString dir = fi.absolutePath();
        const QString suffix = fi.suffix();
        const QString base = Utils::sanitizeFileName(newBaseName);
        if (base.isEmpty()) {
            const QString err = QStringLiteral("文件名包含非法字符或为空");
            qWarning() << "[ProjectContext]" << err;
            return err;
        }

        QString finalBase = base;
        QString newPath = dir + '/' + finalBase
            + (suffix.isEmpty() ? QString() : '.' + suffix);
        if (newPath == m_filePath) {
            if (actualBaseName) *actualBaseName = finalBase;
            return {};
        }

        auto buildPath = [&](const QString& b) {
            return dir + '/' + b + (suffix.isEmpty() ? QString() : '.' + suffix);
            };

        switch (policy) {
        case RenameConflictPolicy::Fail:
            if (QFile::exists(newPath)) {
                const QString err = QStringLiteral("同目录已存在文件: %1").arg(newPath);
                qWarning() << "[ProjectContext]" << err;
                return err;
            }
            break;
        case RenameConflictPolicy::AutoSuffix: {
            if (QFile::exists(newPath)) {
                int n = 2;
                do {
                    finalBase = QStringLiteral("%1 (%2)").arg(base).arg(n++);
                    newPath = buildPath(finalBase);
                } while (QFile::exists(newPath) && n < 1000);
                if (QFile::exists(newPath)) {
                    const QString err = QStringLiteral("无法找到可用的文件名");
                    qWarning() << "[ProjectContext]" << err;
                    return err;
                }
                qDebug() << "[ProjectContext] AutoSuffix 最终名称:" << finalBase;
            }
            break;
        }
        case RenameConflictPolicy::Overwrite:
            break;
        }

        const QString oldPath = m_filePath;
        qDebug() << "[ProjectContext] 重命名文件:" << oldPath << "->" << newPath;

        if (policy != RenameConflictPolicy::Overwrite) {
            const QString lockErr = tryAcquireLockFile(newPath);
            if (!lockErr.isEmpty()) return lockErr;
        }
        auto rollbackNewLock = [&]() {
            if (policy != RenameConflictPolicy::Overwrite) releaseLockFile(newPath);
            };

        // 1. 先把最新树结构写到旧路径的 sidecar JSON
        const QString writeErr = writeProjectJson(oldPath);
        if (!writeErr.isEmpty()) { rollbackNewLock(); return writeErr; }

        // 2. Overwrite 情况：清理目标（xlsx + json + bak）
        if (policy == RenameConflictPolicy::Overwrite && QFile::exists(newPath)) {
            if (!QFile::remove(newPath)) {
                rollbackNewLock();
                const QString err = QStringLiteral("无法覆盖: %1").arg(newPath);
                qWarning() << "[ProjectContext]" << err;
                return err;
            }
            QFile::remove(newPath + ".json");
            QFile::remove(newPath + ".json.bak");
            const QString lockErr = tryAcquireLockFile(newPath);
            if (!lockErr.isEmpty()) { rollbackNewLock(); return lockErr; }
        }

        // 3. 重命名主 xlsx
        if (!QFile::rename(oldPath, newPath)) {
            rollbackNewLock();
            const QString err = QStringLiteral("文件重命名失败: %1 -> %2").arg(oldPath, newPath);
            qWarning() << "[ProjectContext]" << err;
            return err;
        }

        // 4. 重命名 sidecar JSON（主文件已动，失败必须回滚主文件）
        const QString oldJson = oldPath + ".json";
        const QString newJson = newPath + ".json";
        if (QFile::exists(oldJson)) {
            if (!QFile::rename(oldJson, newJson)) {
                QFile::rename(newPath, oldPath);        // 回滚主文件
                rollbackNewLock();
                const QString err = QStringLiteral("JSON 重命名失败: %1 -> %2")
                    .arg(oldJson, newJson);
                qWarning() << "[ProjectContext]" << err;
                return err;
            }
        }

        // 5. .bak 属于旧路径的残留，直接清理
        QFile::remove(oldPath + ".json.bak");
        QFile::remove(newPath + ".json.bak");

        releaseLockFile(oldPath);
        m_lockedPath = newPath;
        m_filePath = newPath;

        if (actualBaseName) *actualBaseName = finalBase;
        markClean();
        emit projectFilePathChanged(oldPath, newPath);
        qDebug() << "[ProjectContext] 文件重命名成功:" << newPath;
        return {};
    }

    void ProjectContext::markClean() {
        if (m_dirty) {
            m_dirty = false;
            qDebug() << "[ProjectContext] dirty false";
            emit dirtyChanged(false);
        }
    }

    void ProjectContext::markDirty() {
        if (!m_dirty) {
            m_dirty = true;
            qDebug() << "[ProjectContext] dirty true";
            emit dirtyChanged(true);
        }
    }

    void ProjectContext::onNewProject() {
        qDebug() << "[ProjectContext] onNewProject";
        this->newProject();
    }

    void ProjectContext::resetState() {
        qDebug() << "[ProjectContext] resetState";
        delete m_root;
        m_root = nullptr;
        m_projectInfo = doclife::model::ProjectInfoModel{};
        m_filePath.clear();
        markClean();
    }

    // ============================================================
    // 临时文件 / 锁
    // ============================================================
    QString ProjectContext::makeTempFilePath() const {
        const QString path = tempDirPath() + "/untitled_"
            + QUuid::createUuid().toString(QUuid::WithoutBraces)
            + ".doclife";
        qDebug() << "[ProjectContext] 生成临时路径:" << path;
        return path;
    }

    void ProjectContext::cleanupTempFile() {
        if (m_tempFilePath.isEmpty()) return;
        qDebug() << "[ProjectContext] 清理临时文件:" << m_tempFilePath;
        QFile::remove(m_tempFilePath);
        QFile::remove(m_tempFilePath + ".json");
        QFile::remove(m_tempFilePath + ".json.bak");
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
        qDebug() << "[ProjectContext] releaseLock:" << m_lockedPath;
        releaseLockFile(m_lockedPath);
        m_lockedPath.clear();
    }

    // ============================================================
    // 新建
    // ============================================================
    bool ProjectContext::newProject()
    {
        qDebug() << "[ProjectContext] newProject";
        if (hasOpenProject()) {
            if (!closeProject(true)) return false;
        }

        const QString tempPath = makeTempFilePath();
        QFile::remove(tempPath);
        QFile::remove(tempPath + ".json");
        QFile::remove(tempPath + ".json.bak");

        delete m_root;
        m_root = new doclife::model::Node;
        m_root->data.type = QStringLiteral("project");
        m_root->data.name = QStringLiteral("未命名项目");
        m_root->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        m_projectInfo = doclife::model::ProjectInfoModel{};
        m_projectInfo.projectnodeId = m_root->data.id;

        // 创建空 xlsx（业务数据容器）
        {
            QXlsx::Document emptyDoc;
            emptyDoc.addSheet(QStringLiteral("Sheet1"));
            if (!emptyDoc.saveAs(tempPath)) {
                resetState();
                const QString err = QStringLiteral("创建空项目文件失败: %1").arg(tempPath);
                qWarning() << "[ProjectContext]" << err;
                emit errorOccurred(err);
                return false;
            }
        }

        // 写 sidecar JSON
        const QString err = writeProjectJson(tempPath);
        if (!err.isEmpty()) {
            QFile::remove(tempPath);
            resetState();
            emit errorOccurred(err);
            return false;
        }

        m_filePath = tempPath;
        m_tempFilePath = tempPath;
        markClean();
        emit projectCreated(tempPath);
        emit projectOpened(tempPath);
        qDebug() << "[ProjectContext] newProject 成功:" << tempPath;
        return true;
    }

    // ============================================================
    // 打开
    // ============================================================
    bool ProjectContext::openProject(const QString& filePath) {
        qDebug() << "[ProjectContext] openProject:" << filePath;
        const QString cleanPath = QDir::cleanPath(filePath);

        if (cleanPath.isEmpty()) {
            qDebug() << "[ProjectContext] openProject 路径为空（用户取消），忽略";
            return false;
        }

        if (!QFile::exists(cleanPath)) {
            const QString err = QStringLiteral("文件不存在: %1").arg(cleanPath);
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        // 检查 sidecar JSON 存在性
        const QString jsonPath = projectJsonPath(cleanPath);
        if (!QFile::exists(jsonPath) && !QFile::exists(jsonPath + ".bak")) {
            const QString err = QStringLiteral(
                "项目缺少元数据文件（%1），可能是旧版本格式，请用旧版程序打开或迁移")
                .arg(jsonPath);
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        if (hasOpenProject()) {
            if (!closeProject(true)) return false;
        }

        const QString lockErr = tryAcquireLockFile(cleanPath);
        if (!lockErr.isEmpty()) {
            emit errorOccurred(lockErr);
            return false;
        }

        const QString readErr = readProjectJson(cleanPath);
        if (!readErr.isEmpty()) {
            releaseLockFile(cleanPath);
            emit errorOccurred(readErr);
            return false;
        }

        m_lockedPath = cleanPath;
        m_filePath = cleanPath;
        m_tempFilePath.clear();
        markClean();
        emit projectOpened(cleanPath);
        qDebug() << "[ProjectContext] openProject 成功:" << cleanPath;
        return true;
    }

    // ============================================================
    // 保存
    // ============================================================
    bool ProjectContext::saveProject() {
        qDebug() << "[ProjectContext] saveProject:" << m_filePath;
        if (!hasOpenProject()) {
            const QString err = QStringLiteral("没有打开的项目");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }
        if (isUntitled()) {
            const QString err = QStringLiteral("项目尚未保存过，请使用「另存为」");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        const QString err = writeProjectJson(m_filePath);
        if (!err.isEmpty()) {
            emit errorOccurred(err);
            return false;
        }
        markClean();
        emit projectSaved(m_filePath);
        qDebug() << "[ProjectContext] saveProject 成功:" << m_filePath;
        return true;
    }

    bool ProjectContext::saveProjectAs(const QString& filePath) {
        qDebug() << "[ProjectContext] saveProjectAs:" << filePath;
        if (!hasOpenProject()) {
            const QString err = QStringLiteral("没有打开的项目");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        const QString cleanPath = QDir::cleanPath(filePath);
        if (cleanPath.isEmpty()) {
            qDebug() << "[ProjectContext] saveProjectAs 取消";
            emit errorOccurred(QStringLiteral("保存取消"));
            return false;
        }

        if (cleanPath == m_filePath) return saveProject();

        const QString lockErr = tryAcquireLockFile(cleanPath);
        if (!lockErr.isEmpty()) {
            emit errorOccurred(lockErr);
            return false;
        }
        auto rollbackLock = [&]() { releaseLockFile(cleanPath); };

        // 1. 先复制 xlsx（业务数据）
        QDir().mkpath(QFileInfo(cleanPath).absolutePath());
        if (QFile::exists(cleanPath) && !QFile::remove(cleanPath)) {
            rollbackLock();
            const QString err = QStringLiteral("无法覆盖目标文件: %1").arg(cleanPath);
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }
        if (!QFile::copy(m_filePath, cleanPath)) {
            rollbackLock();
            const QString err = QStringLiteral("拷贝到目标路径失败: %1").arg(cleanPath);
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        // 2. 再写 JSON 到新路径
        const QString writeErr = writeProjectJson(cleanPath);
        if (!writeErr.isEmpty()) {
            QFile::remove(cleanPath);
            rollbackLock();
            emit errorOccurred(writeErr);
            return false;
        }

        // 3. 清理旧路径 sidecar（项目已迁到新路径）
        if (!m_lockedPath.isEmpty() && m_lockedPath != cleanPath)
            releaseLockFile(m_lockedPath);

        if (isUntitled()) {
            QFile::remove(m_tempFilePath);
            QFile::remove(m_tempFilePath + ".json");
            QFile::remove(m_tempFilePath + ".json.bak");
            m_tempFilePath.clear();
        }
        else {
            QFile::remove(m_filePath + ".json");
            QFile::remove(m_filePath + ".json.bak");
        }

        m_lockedPath = cleanPath;
        m_filePath = cleanPath;
        markClean();
        emit projectSaved(cleanPath);
        qDebug() << "[ProjectContext] saveProjectAs 成功:" << cleanPath;
        return true;
    }

    // ============================================================
    // 关闭
    // ============================================================
    bool ProjectContext::closeProject(bool saveBeforeClose) {
        qDebug() << "[ProjectContext] closeProject, saveBeforeClose =" << saveBeforeClose
            << ", dirty =" << m_dirty;
        if (!hasOpenProject()) return true;

        if (saveBeforeClose && m_dirty) {
            if (isUntitled()) {
                const QString err = QStringLiteral("项目尚未保存，请先另存为");
                qWarning() << "[ProjectContext]" << err;
                emit errorOccurred(err);
                return false;
            }
            if (!saveProject()) return false;
        }

        emit projectClosed();
        releaseLock();
        cleanupTempFile();
        resetState();
        qDebug() << "[ProjectContext] closeProject 完成";
        return true;
    }

    // ============================================================
    // JSON sidecar 路径 / 树 <-> JSON
    // ============================================================
    QString ProjectContext::projectJsonPath(const QString& projectPath) const {
        // "MyProject.doclife" -> "MyProject.doclife.json"
        return projectPath + QStringLiteral(".json");
    }

    QJsonObject ProjectContext::nodeToJson(const doclife::model::Node& n) const {
        QJsonObject o;
        o["id"] = n.data.id;
        o["type"] = n.data.type;
        o["name"] = n.data.name;
        if (!n.data.attrs.isEmpty())
            o["attrs"] = n.data.attrs;

        if (!n.children.isEmpty()) {
            QJsonArray arr;
            for (const auto* c : n.children)
                arr.append(nodeToJson(*c));
            o["children"] = arr;
        }
        return o;
    }

    doclife::model::Node* ProjectContext::nodeFromJson(const QJsonObject& o) const {
        auto* n = new doclife::model::Node;
        n->data.id = o.value(QStringLiteral("id")).toString();
        n->data.type = o.value(QStringLiteral("type")).toString();
        n->data.name = o.value(QStringLiteral("name")).toString();
        n->data.attrs = o.value(QStringLiteral("attrs")).toObject();

        const auto arr = o.value(QStringLiteral("children")).toArray();
        for (const auto& v : arr) {
            auto* c = nodeFromJson(v.toObject());
            c->parent = n;
            n->children.append(c);
        }
        return n;
    }

    // ============================================================
    // 元数据读写：原子写 + 备份 + 容错
    // ============================================================
    QString ProjectContext::writeProjectJson(const QString& projectPath) {
        const QString jsonPath = projectJsonPath(projectPath);
        const QString bakPath = jsonPath + ".bak";

        qDebug() << "[ProjectContext] writeProjectJson:" << jsonPath;

        // 1. 序列化
        QJsonObject head;
        head["version"] = 2;
        head["info"] = m_projectInfo.toJson();
        head["modified"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        QJsonObject rootObj;
        if (m_root) rootObj = nodeToJson(*m_root);
        head["root"] = rootObj;

        const QByteArray bytes = QJsonDocument(head).toJson(QJsonDocument::Indented);

        // 2. 备份旧文件（只保留 1 份）
        if (QFile::exists(jsonPath)) {
            QFile::remove(bakPath);
            if (!QFile::copy(jsonPath, bakPath)) {
                qWarning() << "[ProjectContext] 备份失败（不阻断保存）:" << bakPath;
            }
            else {
                qDebug() << "[ProjectContext] 已备份:" << bakPath;
            }
        }

        // 3. 原子写
        QSaveFile f(jsonPath);
        if (!f.open(QIODevice::WriteOnly)) {
            const QString err = QStringLiteral("无法打开临时文件: %1 (%2)")
                .arg(jsonPath, f.errorString());
            qWarning() << "[ProjectContext]" << err;
            return err;
        }
        const qint64 written = f.write(bytes);
        if (written != bytes.size()) {
            const QString err = QStringLiteral("写入不完整: %1 (%2/%3)")
                .arg(jsonPath).arg(written).arg(bytes.size());
            qWarning() << "[ProjectContext]" << err;
            f.cancelWriting();
            return err;
        }
        if (!f.commit()) {
            const QString err = QStringLiteral("提交失败: %1 (%2)")
                .arg(jsonPath, f.errorString());
            qWarning() << "[ProjectContext]" << err;
            return err;
        }

        qDebug() << "[ProjectContext] writeProjectJson 成功:" << jsonPath;
        return {};
    }

    QString ProjectContext::readProjectJson(const QString& projectPath) {
        const QString jsonPath = projectJsonPath(projectPath);
        const QString bakPath = jsonPath + ".bak";

        qDebug() << "[ProjectContext] readProjectJson:" << jsonPath;

        // 内部：从某个文件读取并解析
        auto tryRead = [this](const QString& path, QString* outErr) -> bool {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) {
                *outErr = QStringLiteral("无法打开: %1 (%2)")
                    .arg(path, f.errorString());
                return false;
            }
            const QByteArray bytes = f.readAll();
            f.close();

            QJsonParseError err{};
            const auto doc = QJsonDocument::fromJson(bytes, &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                *outErr = QStringLiteral("JSON 解析失败: %1 (offset %2)")
                    .arg(err.errorString()).arg(err.offset);
                return false;
            }

            const auto head = doc.object();
            const int version = head.value(QStringLiteral("version")).toInt(0);
            if (version <= 0) {
                *outErr = QStringLiteral("缺少 version 字段");
                return false;
            }
            if (version > 2) {
                *outErr = QStringLiteral("文件版本过高（%1），请升级程序").arg(version);
                return false;
            }

            m_projectInfo = doclife::model::ProjectInfoModel::fromJson(
                head.value(QStringLiteral("info")).toObject());

            const auto rootObj = head.value(QStringLiteral("root")).toObject();
            if (rootObj.isEmpty()
                || rootObj.value(QStringLiteral("id")).toString().isEmpty()) {
                *outErr = QStringLiteral("root 节点缺失或无效");
                return false;
            }

            delete m_root;
            m_root = nodeFromJson(rootObj);
            return true;
            };

        // 1. 先读主文件
        QString mainErr;
        if (tryRead(jsonPath, &mainErr)) {
            qDebug() << "[ProjectContext] readProjectJson 成功（主文件）";
            return {};
        }
        qWarning() << "[ProjectContext] 主文件读取失败:" << mainErr;

        // 2. 主文件失败 → 试备份
        if (QFile::exists(bakPath)) {
            qWarning() << "[ProjectContext] 尝试从备份恢复:" << bakPath;
            QString bakErr;
            if (tryRead(bakPath, &bakErr)) {
                // 恢复成功：把 bak 内容回写成主文件，避免下次再走这条路径
                const QString saveErr = writeProjectJson(projectPath);
                if (!saveErr.isEmpty()) {
                    qWarning() << "[ProjectContext] 从备份恢复后回写失败:" << saveErr;
                }
                emit errorOccurred(QStringLiteral(
                    "上次保存可能异常，已从备份恢复。原始错误：%1").arg(mainErr));
                return {};
            }
            qWarning() << "[ProjectContext] 备份也读取失败:" << bakErr;
            return QStringLiteral("主文件与备份均损坏。主文件：%1；备份：%2")
                .arg(mainErr, bakErr);
        }

        // 3. 主文件失败且无备份
        return QStringLiteral("读取失败且无可用备份：%1").arg(mainErr);
    }

    // ============================================================
    // 打印
    // ============================================================
    bool ProjectContext::printAllSheets() {
        qDebug() << "[ProjectContext] printAllSheets";
        if (!hasOpenProject()) {
            const QString err = QStringLiteral("没有打开的项目");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        QXlsx::Document doc(m_filePath);
        const QStringList allSheets = doc.sheetNames();
        int printed = 0;
        for (const QString& sheetName : allSheets) {
            doc.selectSheet(sheetName);
            QXlsx::CellRange range = doc.dimension();
            if (!range.isValid()) { qDebug() << "[print] 空表:" << sheetName; continue; }
            qDebug() << "[print] 表:" << sheetName
                << "行:" << range.firstRow() << "~" << range.lastRow()
                << "列:" << range.firstColumn() << "~" << range.lastColumn();
            ++printed;
        }
        qDebug() << "[print] 共处理" << printed << "张表";
        return true;
    }

    // ============================================================
    // 增量
    // ============================================================
    bool ProjectContext::addChild(const QString& parentId, const QString& childType,
        QString* newId)
    {
        qDebug() << "[ProjectContext] addChild:" << parentId << "type =" << childType;
        using doclife::model::NodeTypeRegistry;

        if (!m_root) {
            const QString err = QStringLiteral("没有打开的项目");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }
        if (!NodeTypeRegistry::instance().spec(childType)) {
            const QString err = QStringLiteral("未知节点类型: %1").arg(childType);
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }
        auto* parent = findNodeById(parentId);
        if (!parent) {
            const QString err = QStringLiteral("找不到父节点: %1").arg(parentId);
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }
        if (NodeTypeRegistry::instance().isLeaf(parent->data.type)) {
            const QString err = QStringLiteral("叶子节点不能添加子节点");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        auto* child = new doclife::model::Node;
        child->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        child->data.type = childType;
        child->data.name = NodeTypeRegistry::instance().displayLabel(childType);
        child->parent = parent;
        parent->children.append(child);

        markDirty();
        emit nodeAdded(parentId, child->data.id);
        if (newId) *newId = child->data.id;
        qDebug() << "[ProjectContext] addChild 成功:" << child->data.id;
        return true;
    }

    bool ProjectContext::removeNode(const QString& nodeId) {
        qDebug() << "[ProjectContext] removeNode:" << nodeId;
        if (!m_root) {
            const QString err = QStringLiteral("没有打开的项目");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }
        if (nodeId == m_root->data.id) {
            const QString err = QStringLiteral("不能删除根节点");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        auto* target = findNodeById(nodeId);
        if (!target) {
            const QString err = QStringLiteral("找不到节点: %1").arg(nodeId);
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }
        auto* parent = target->parent;
        if (!parent) {
            const QString err = QStringLiteral("找不到父节点");
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }
        const QString parentId = parent->data.id;

        emit nodeRemoved(parentId, nodeId);

        if (target->parent) {
            parent->children.removeOne(target);
            target->parent = nullptr;
        }

        delete target;
        markDirty();
        qDebug() << "[ProjectContext] removeNode 成功:" << nodeId;
        return true;
    }

    bool ProjectContext::setNodeAttr(const QString& nodeId, const QString& key,
        const QVariant& value)
    {
        qDebug() << "[ProjectContext] setNodeAttr:" << nodeId << "key =" << key;
        auto* node = findNodeById(nodeId);
        if (!node) {
            const QString err = QStringLiteral("找不到节点: %1").arg(nodeId);
            qWarning() << "[ProjectContext]" << err;
            emit errorOccurred(err);
            return false;
        }

        QJsonValue jv;
        const int t = value.typeId();
        if (t == QMetaType::Bool) jv = QJsonValue(value.toBool());
        else if (t == QMetaType::Int || t == QMetaType::LongLong
            || t == QMetaType::UInt || t == QMetaType::ULongLong)
            jv = QJsonValue(value.toLongLong());
        else if (t == QMetaType::Double || t == QMetaType::Float)
            jv = QJsonValue(value.toDouble());
        else if (value.canConvert<QJsonObject>())
            jv = QJsonValue(value.toJsonObject());
        else if (value.canConvert<QJsonArray>())
            jv = QJsonValue(value.toJsonArray());
        else
            jv = QJsonValue(value.toString());

        node->data.attrs.insert(key, jv);
        markDirty();
        emit nodeUpdated(nodeId);
        qDebug() << "[ProjectContext] setNodeAttr 成功";
        return true;
    }

} // namespace doclife::core