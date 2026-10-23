#include "ProjectContext.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QDebug>
#include <QBuffer>
#include <QUuid>
#include <QVector>
#include <QSaveFile>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QJsonParseError>
#include <QCoreApplication>
#include <QRegularExpression>
#include <functional>
#ifdef Q_OS_WIN
#include <projectNode.h>
#  include <windows.h>
#else
#  include <signal.h>
#  include <errno.h>
#endif

// QUaZip（路径按你本地的安装方式调整）
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>
#include <quazip/quazipnewinfo.h>

#include "utils.h"
#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"
#include "model/custommodel/NodeTypeRegistry.h"

namespace doclife::core {

    namespace {

        // ZIP 内的固定条目名
        const QString kJsonEntry = QStringLiteral("project.json");
        const QString kJsonBakEntry = QStringLiteral("project.json.bak");
        const QString kBizEntry = QStringLiteral("business.xlsx");

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
            // 保留清理第一阶段残留（*.doclife.json*）以处理升级过渡
            const QStringList patterns{
                QStringLiteral("*.doclife"),
                QStringLiteral("*.doclife.tmp"),
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

        // 从 ZIP 里读一个条目。ok 返回条目是否存在且读取成功。
        QByteArray readZipEntry(const QString& zipPath,
            const QString& entryName,
            bool* ok)
        {
            if (ok) *ok = false;
            QuaZip zip(zipPath);
            if (!zip.open(QuaZip::mdUnzip)) {
                return {};
            }
            if (!zip.setCurrentFile(entryName)) {
                zip.close();
                return {};
            }
            QuaZipFile f(&zip);
            if (!f.open(QIODevice::ReadOnly)) {
                zip.close();
                return {};
            }
            const QByteArray data = f.readAll();
            f.close();
            zip.close();
            if (ok) *ok = true;
            return data;
        }

        // 把整个 ZIP 写到 device（必须已打开可写）
        QString writeZipToDevice(QIODevice* device,
            const QByteArray& jsonBytes,
            const QByteArray& jsonBakBytes,
            const QByteArray& bizBytes)
        {
            QuaZip zip(device);
            if (!zip.open(QuaZip::mdCreate)) {
                return QStringLiteral("无法创建 ZIP");
            }

            auto writeEntry = [&](const QString& name,
                const QByteArray& data) -> bool {
                    if (data.isEmpty()) return true;
                    QuaZipNewInfo info(name);
                    QuaZipFile f(&zip);
                    if (!f.open(QIODevice::WriteOnly, info)) {
                        qWarning() << "[ZIP] 打开条目失败:" << name << f.errorString();
                        return false;
                    }
                    if (f.write(data) != data.size()) {
                        qWarning() << "[ZIP] 写入不完整:" << name;
                        f.close();
                        return false;
                    }
                    f.close();
                    return true;
                };

            if (!writeEntry(kJsonEntry, jsonBytes))
            {
                zip.close(); return QStringLiteral("写入 project.json 失败");
            }
            if (!writeEntry(kJsonBakEntry, jsonBakBytes))
            {
                zip.close(); return QStringLiteral("写入 project.json.bak 失败");
            }
            if (!writeEntry(kBizEntry, bizBytes))
            {
                zip.close(); return QStringLiteral("写入 business.xlsx 失败");
            }

            zip.close();
            return {};
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
        releaseBusinessDocument();
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
    // 业务文档懒加载
    // ============================================================
    QXlsx::Document* ProjectContext::businessDocument()
    {
        if (m_businessDoc) return m_businessDoc;

        if (m_filePath.isEmpty()) {
            qWarning() << "[ProjectContext] businessDocument: 没有打开的项目";
            return nullptr;
        }

        bool ok = false;
        const QByteArray bytes = readZipEntry(m_filePath, kBizEntry, &ok);
        if (!ok) {
            qWarning() << "[ProjectContext] businessDocument: ZIP 内无 business.xlsx";
            return nullptr;
        }
        if (bytes.isEmpty()) {
            qWarning() << "[ProjectContext] businessDocument: business.xlsx 内容为空";
            return nullptr;
        }

        QBuffer buf;
        buf.setData(bytes);
        buf.open(QIODevice::ReadOnly);

        m_businessDoc = new QXlsx::Document(&buf);
        buf.close();

        qDebug() << "[ProjectContext] businessDocument 懒加载完成,"
            << bytes.size() << "bytes";
        return m_businessDoc;
    }

    void ProjectContext::markBusinessDirty()
    {
        if (!m_businessDirty) {
            m_businessDirty = true;
            markDirty();
        }
    }

    void ProjectContext::releaseBusinessDocument()
    {
        if (m_businessDoc) {
            delete m_businessDoc;
            m_businessDoc = nullptr;
            qDebug() << "[ProjectContext] businessDocument 已释放";
        }
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

        // 先把最新内存内容写回旧路径 ZIP
        const QString writeErr = writeZip(oldPath);
        if (!writeErr.isEmpty()) { rollbackNewLock(); return writeErr; }

        if (policy == RenameConflictPolicy::Overwrite && QFile::exists(newPath)) {
            if (!QFile::remove(newPath)) {
                rollbackNewLock();
                const QString err = QStringLiteral("无法覆盖: %1").arg(newPath);
                qWarning() << "[ProjectContext]" << err;
                return err;
            }
            const QString lockErr = tryAcquireLockFile(newPath);
            if (!lockErr.isEmpty()) { rollbackNewLock(); return lockErr; }
        }

        if (!QFile::rename(oldPath, newPath)) {
            rollbackNewLock();
            const QString err = QStringLiteral("文件重命名失败: %1 -> %2").arg(oldPath, newPath);
            qWarning() << "[ProjectContext]" << err;
            return err;
        }

        releaseLockFile(oldPath);
        m_lockedPath = newPath;
        m_filePath = newPath;

        if (actualBaseName) *actualBaseName = finalBase;
        markClean();
        emit projectFilePathChanged(oldPath, newPath);
        qDebug() << "[ProjectContext] 文件重命名成功:" << newPath;
        return {};
    }

    // ============================================================
    void ProjectContext::markClean() {
        if (m_dirty || m_businessDirty) {
            m_dirty = false;
            m_businessDirty = false;
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
        releaseBusinessDocument();
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
        QFile::remove(m_tempFilePath + ".tmp");
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
        QFile::remove(tempPath + ".tmp");

        delete m_root;
        m_root = new doclife::model::Node;
        m_root->data.type = QStringLiteral("project");
        m_root->data.name = QStringLiteral("未命名项目");
        m_root->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        m_projectInfo = doclife::model::ProjectInfoModel{};
        m_projectInfo.projectnodeId = m_root->data.id;

        releaseBusinessDocument();

        // writeZip 内部：旧 ZIP 不存在 → 自动创建空 business.xlsx
        const QString err = writeZip(tempPath);
        if (!err.isEmpty()) {
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

        // 快速校验：是有效 ZIP 且含 project.json
        {
            QuaZip zip(cleanPath);
            if (!zip.open(QuaZip::mdUnzip)) {
                const QString err = QStringLiteral(
                    "文件不是有效的 .doclife（ZIP）格式，可能是旧版本文件: %1")
                    .arg(cleanPath);
                qWarning() << "[ProjectContext]" << err;
                emit errorOccurred(err);
                return false;
            }
            const bool hasJson = zip.setCurrentFile(kJsonEntry);
            const bool hasJsonBak = hasJson ? true : zip.setCurrentFile(kJsonBakEntry);
            zip.close();
            if (!hasJson && !hasJsonBak) {
                const QString err = QStringLiteral(
                    "ZIP 内缺少 project.json 及备份: %1").arg(cleanPath);
                qWarning() << "[ProjectContext]" << err;
                emit errorOccurred(err);
                return false;
            }
        }

        if (hasOpenProject()) {
            if (!closeProject(true)) return false;
        }

        const QString lockErr = tryAcquireLockFile(cleanPath);
        if (!lockErr.isEmpty()) {
            emit errorOccurred(lockErr);
            return false;
        }

        const QString readErr = readZip(cleanPath);
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
            emit errorOccurred(QStringLiteral("没有打开的项目"));
            return false;
        }
        if (isUntitled()) {
            emit errorOccurred(QStringLiteral("项目尚未保存过，请使用「另存为」"));
            return false;
        }

        const QString err = writeZip(m_filePath);
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
            emit errorOccurred(QStringLiteral("没有打开的项目"));
            return false;
        }

        const QString cleanPath = QDir::cleanPath(filePath);
        if (cleanPath.isEmpty()) {
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

        // 1. 确保当前项目 ZIP 是最新的
        if (m_dirty || m_businessDirty) {
            const QString saveErr = writeZip(m_filePath);
            if (!saveErr.isEmpty()) {
                rollbackLock();
                emit errorOccurred(saveErr);
                return false;
            }
        }

        // 2. 拷贝到目标路径
        QDir().mkpath(QFileInfo(cleanPath).absolutePath());
        if (QFile::exists(cleanPath) && !QFile::remove(cleanPath)) {
            rollbackLock();
            emit errorOccurred(QStringLiteral("无法覆盖目标文件: %1").arg(cleanPath));
            return false;
        }
        if (!QFile::copy(m_filePath, cleanPath)) {
            rollbackLock();
            emit errorOccurred(QStringLiteral("拷贝到目标路径失败: %1").arg(cleanPath));
            return false;
        }

        // 3. 清理旧路径
        if (!m_lockedPath.isEmpty() && m_lockedPath != cleanPath)
            releaseLockFile(m_lockedPath);

        if (isUntitled()) {
            QFile::remove(m_tempFilePath);
            QFile::remove(m_tempFilePath + ".tmp");
            m_tempFilePath.clear();
        }
        else {
            QFile::remove(m_filePath);
            QFile::remove(m_filePath + ".tmp");
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
            << ", dirty =" << isDirty();
        if (!hasOpenProject()) return true;

        if (saveBeforeClose && isDirty()) {
            if (isUntitled()) {
                emit errorOccurred(QStringLiteral("项目尚未保存，请先另存为"));
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
    // 树 <-> JSON
    // ============================================================
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

        for (const auto& v : o.value(QStringLiteral("children")).toArray()) {
            auto* c = nodeFromJson(v.toObject());
            c->parent = n;
            n->children.append(c);
        }
        return n;
    }

    // ============================================================
    // ZIP 读写
    // ============================================================
    QString ProjectContext::writeZip(const QString& targetPath)
    {
        qDebug() << "[ProjectContext] writeZip:" << targetPath;

        // 1. 序列化树
        QJsonObject head;
        head["version"] = 2;
        head["info"] = m_projectInfo.toJson();
        head["modified"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (m_root) head["root"] = nodeToJson(*m_root);
        const QByteArray jsonBytes = QJsonDocument(head).toJson(QJsonDocument::Indented);

        // 2. 从旧 ZIP 读出：旧 json（做备份）+ 旧 business.xlsx（保留）
        QByteArray oldJsonBytes;
        QByteArray oldBizBytes;
        {
            QuaZip oldZip(targetPath);
            if (oldZip.open(QuaZip::mdUnzip)) {
                if (oldZip.setCurrentFile(kJsonEntry)) {
                    QuaZipFile f(&oldZip);
                    if (f.open(QIODevice::ReadOnly)) {
                        oldJsonBytes = f.readAll();
                        f.close();
                    }
                }
                if (oldZip.setCurrentFile(kBizEntry)) {
                    QuaZipFile f(&oldZip);
                    if (f.open(QIODevice::ReadOnly)) {
                        oldBizBytes = f.readAll();
                        f.close();
                    }
                }
                oldZip.close();
            }
        }

        // 3. 决定 business.xlsx 内容
        QByteArray bizBytes;
        if (m_businessDirty && m_businessDoc) {
            QBuffer buf(&bizBytes);
            buf.open(QIODevice::WriteOnly);
            if (!m_businessDoc->saveAs(&buf)) {
                return QStringLiteral("序列化业务文档失败");
            }
            buf.close();
            qDebug() << "[ProjectContext] 序列化内存业务文档" << bizBytes.size() << "bytes";
        }
        else if (!oldBizBytes.isEmpty()) {
            bizBytes = oldBizBytes;
            qDebug() << "[ProjectContext] 沿用旧 business.xlsx" << bizBytes.size() << "bytes";
        }
        else {
            QXlsx::Document emptyDoc;
            emptyDoc.addSheet(QStringLiteral("Sheet1"));
            QBuffer buf(&bizBytes);
            buf.open(QIODevice::WriteOnly);
            if (!emptyDoc.saveAs(&buf)) {
                return QStringLiteral("创建空业务文档失败");
            }
            buf.close();
            qDebug() << "[ProjectContext] 创建空 business.xlsx" << bizBytes.size() << "bytes";
        }

        // 4. 在内存里构建完整 ZIP
        QByteArray zipBytes;
        {
            QBuffer zipBuffer(&zipBytes);
            zipBuffer.open(QIODevice::WriteOnly);
            const QString zipErr = writeZipToDevice(&zipBuffer, jsonBytes,
                oldJsonBytes, bizBytes);
            zipBuffer.close();
            if (!zipErr.isEmpty()) return zipErr;
        }

        // 5. QSaveFile 原子写
        QSaveFile saveFile(targetPath);
        if (!saveFile.open(QIODevice::WriteOnly)) {
            return QStringLiteral("无法打开临时文件: %1 (%2)")
                .arg(targetPath, saveFile.errorString());
        }
        if (saveFile.write(zipBytes) != zipBytes.size()) {
            saveFile.cancelWriting();
            return QStringLiteral("写入不完整: %1").arg(targetPath);
        }
        if (!saveFile.commit()) {
            return QStringLiteral("提交失败: %1 (%2)")
                .arg(targetPath, saveFile.errorString());
        }

        qDebug() << "[ProjectContext] writeZip 成功" << zipBytes.size() << "bytes";
        return {};
    }

    QString ProjectContext::readZip(const QString& zipPath)
    {
        qDebug() << "[ProjectContext] readZip:" << zipPath;

        bool ok = false;
        QByteArray jsonBytes = readZipEntry(zipPath, kJsonEntry, &ok);

        if (!ok || jsonBytes.isEmpty()) {
            qWarning() << "[ProjectContext] 主 project.json 读取失败，尝试 .bak";
            jsonBytes = readZipEntry(zipPath, kJsonBakEntry, &ok);
            if (!ok || jsonBytes.isEmpty()) {
                return QStringLiteral("ZIP 内无可用的 project.json");
            }
            // 从 bak 恢复：标脏，下次 save 时正常写回主 JSON
            markDirty();
            emit errorOccurred(QStringLiteral("上次保存可能异常，已从备份恢复"));
        }

        QJsonParseError parseErr{};
        const auto doc = QJsonDocument::fromJson(jsonBytes, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
            return QStringLiteral("project.json 解析失败: %1").arg(parseErr.errorString());
        }

        const auto head = doc.object();
        const int version = head.value(QStringLiteral("version")).toInt(0);
        if (version <= 0) return QStringLiteral("project.json 缺少 version 字段");
        if (version > 2) return QStringLiteral("project.json 版本过高（%1）").arg(version);

        m_projectInfo = doclife::model::ProjectInfoModel::fromJson(
            head.value(QStringLiteral("info")).toObject());

        const auto rootObj = head.value(QStringLiteral("root")).toObject();
        if (rootObj.isEmpty()
            || rootObj.value(QStringLiteral("id")).toString().isEmpty()) {
            return QStringLiteral("project.json 缺少有效 root 节点");
        }

        delete m_root;
        m_root = nodeFromJson(rootObj);

        // 业务文档懒加载，不在这里读
        releaseBusinessDocument();

        qDebug() << "[ProjectContext] readZip 成功";
        return {};
    }

    // ============================================================
    // 打印
    // ============================================================
    bool ProjectContext::printAllSheets() {
        qDebug() << "[ProjectContext] printAllSheets";
        if (!hasOpenProject()) {
            emit errorOccurred(QStringLiteral("没有打开的项目"));
            return false;
        }

        QXlsx::Document* doc = businessDocument();
        if (!doc) {
            emit errorOccurred(QStringLiteral("无法加载业务文档"));
            return false;
        }

        const QStringList allSheets = doc->sheetNames();
        int printed = 0;
        for (const QString& sheetName : allSheets) {
            doc->selectSheet(sheetName);
            QXlsx::CellRange range = doc->dimension();
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
            emit errorOccurred(QStringLiteral("没有打开的项目"));
            return false;
        }
        if (!NodeTypeRegistry::instance().spec(childType)) {
            emit errorOccurred(QStringLiteral("未知节点类型: %1").arg(childType));
            return false;
        }
        auto* parent = findNodeById(parentId);
        if (!parent) {
            emit errorOccurred(QStringLiteral("找不到父节点: %1").arg(parentId));
            return false;
        }
        if (NodeTypeRegistry::instance().isLeaf(parent->data.type)) {
            emit errorOccurred(QStringLiteral("叶子节点不能添加子节点"));
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
            emit errorOccurred(QStringLiteral("没有打开的项目"));
            return false;
        }
        if (nodeId == m_root->data.id) {
            emit errorOccurred(QStringLiteral("不能删除根节点"));
            return false;
        }

        auto* target = findNodeById(nodeId);
        if (!target) {
            emit errorOccurred(QStringLiteral("找不到节点: %1").arg(nodeId));
            return false;
        }
        auto* parent = target->parent;
        if (!parent) {
            emit errorOccurred(QStringLiteral("找不到父节点"));
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
            emit errorOccurred(QStringLiteral("找不到节点: %1").arg(nodeId));
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