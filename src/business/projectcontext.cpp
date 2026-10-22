#include "ProjectContext.h"

#include <QDir>
#include <QUuid>
#include <QFile>
#include <QHash>
#include <QDebug>
#include <QVector>
#include <QFileInfo>
#include <functional>
#include <QJsonObject>
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

        const QString kMetaSheetName = QStringLiteral("_meta_");
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
                QStringLiteral("*.doclife.tmp"),
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

        void writeNodeFlat(QXlsx::Document& doc, int& row,
            const doclife::model::Node& n, int parentRow)
        {
            const int myRow = row;
            QJsonObject o;
            o["parentRow"] = parentRow;
            if (!n.data.id.isEmpty())    o["id"] = n.data.id;
            o["type"] = n.data.type;
            o["name"] = n.data.name;
            if (!n.data.attrs.isEmpty()) o["attrs"] = n.data.attrs;

            doc.write(myRow, 1,
                QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
            ++row;

            for (const auto* c : n.children)
                writeNodeFlat(doc, row, *c, myRow);
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

        const QString writeErr = writeMetadata(oldPath);
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
            if (!closeProject(true)) return false;   // closeProject 已 emit
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

        const QString err = writeMetadata(tempPath);
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

        if (hasOpenProject()) {
            if (!closeProject(true)) return false;   // closeProject 已 emit
        }

        const QString lockErr = tryAcquireLockFile(cleanPath);
        if (!lockErr.isEmpty()) {
            emit errorOccurred(lockErr);
            return false;
        }

        const QString readErr = readMetadata(cleanPath);
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

        const QString err = writeMetadata(m_filePath);
        if (!err.isEmpty()) {
            emit errorOccurred(err);   // helper 已 qWarning
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

        const QString writeErr = writeMetadata(m_filePath);
        if (!writeErr.isEmpty()) {
            rollbackLock();
            emit errorOccurred(writeErr);
            return false;
        }

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

        if (!m_lockedPath.isEmpty() && m_lockedPath != cleanPath)
            releaseLockFile(m_lockedPath);

        if (isUntitled()) {
            QFile::remove(m_tempFilePath);
            QFile::remove(m_tempFilePath + ".tmp");
            m_tempFilePath.clear();
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
            if (!saveProject()) return false;   // saveProject 已 emit
        }

        emit projectClosed();
        releaseLock();
        cleanupTempFile();
        resetState();
        qDebug() << "[ProjectContext] closeProject 完成";
        return true;
    }

    // ============================================================
    // 元数据读写
    // ============================================================
    QString ProjectContext::writeMetadata(const QString& filePath) {
        qDebug() << "[ProjectContext] writeMetadata:" << filePath;
        const QString tmpPath = filePath + ".tmp";
        {
            QXlsx::Document doc(filePath);
            if (doc.sheetNames().contains(kMetaSheetName))
                doc.deleteSheet(kMetaSheetName);
            doc.addSheet(kMetaSheetName);
            doc.selectSheet(kMetaSheetName);

            QJsonObject head;
            head["version"] = 1;
            head["info"] = m_projectInfo.toJson();
            doc.write(1, 1, QString::fromUtf8(
                QJsonDocument(head).toJson(QJsonDocument::Compact)));

            int row = 2;
            if (m_root) writeNodeFlat(doc, row, *m_root, 0);

            if (!doc.saveAs(tmpPath)) {
                const QString err = QStringLiteral("写临时文件失败: %1").arg(tmpPath);
                qWarning() << "[ProjectContext]" << err;
                return err;
            }
        }

        if (QFile::exists(filePath) && !QFile::remove(filePath)) {
            const QString err = QStringLiteral("无法删除原文件: %1").arg(filePath);
            qWarning() << "[ProjectContext]" << err;
            return err;
        }
        if (!QFile::rename(tmpPath, filePath)) {
            const QString err = QStringLiteral("重命名临时文件失败: %1").arg(tmpPath);
            qWarning() << "[ProjectContext]" << err;
            return err;
        }
        qDebug() << "[ProjectContext] writeMetadata 成功";
        return {};
    }

    QString ProjectContext::readMetadata(const QString& filePath) {
        qDebug() << "[ProjectContext] readMetadata:" << filePath;
        QXlsx::Document doc(filePath);

        const QStringList sheets = doc.sheetNames();
        if (sheets.isEmpty()) {
            const QString err = QStringLiteral("Excel 文件为空");
            qWarning() << "[ProjectContext]" << err;
            return err;
        }
        if (!sheets.contains(kMetaSheetName)) {
            const QString err = QStringLiteral("缺少元数据表 %1").arg(kMetaSheetName);
            qWarning() << "[ProjectContext]" << err;
            return err;
        }
        doc.selectSheet(kMetaSheetName);

        QJsonParseError err{};
        const auto headDoc = QJsonDocument::fromJson(doc.read(1, 1).toString().toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !headDoc.isObject()) {
            const QString msg = QStringLiteral("头部 JSON 解析失败: %1").arg(err.errorString());
            qWarning() << "[ProjectContext]" << msg;
            return msg;
        }

        const auto headObj = headDoc.object();
        m_projectInfo = doclife::model::ProjectInfoModel::fromJson(
            headObj.value(QStringLiteral("info")).toObject());

        QHash<int, doclife::model::NodeAttributes> attrsByRow;
        QHash<int, QVector<int>>                   childrenOf;
        int                                        rootRow = -1;

        const QXlsx::CellRange range = doc.dimension();
        if (range.isValid()) {
            for (int r = 2; r <= range.lastRow(); ++r) {
                const QString s = doc.read(r, 1).toString();
                if (s.isEmpty()) continue;
                QJsonParseError e{};
                const auto d = QJsonDocument::fromJson(s.toUtf8(), &e);
                if (e.error != QJsonParseError::NoError || !d.isObject()) {
                    qWarning() << "[ProjectContext] 第" << r << "行 JSON 解析失败，跳过";
                    continue;
                }
                const auto o = d.object();

                doclife::model::NodeAttributes a;
                a.id = o.value(QStringLiteral("id")).toString();
                a.type = o.value(QStringLiteral("type")).toString();
                a.name = o.value(QStringLiteral("name")).toString();
                a.attrs = o.value(QStringLiteral("attrs")).toObject();

                attrsByRow.insert(r, a);
                const int p = o.value(QStringLiteral("parentRow")).toInt(0);
                if (p == 0) rootRow = r;
                else        childrenOf[p].append(r);
            }
        }

        if (rootRow < 0) {
            const QString msg = QStringLiteral("元数据缺少根节点");
            qWarning() << "[ProjectContext]" << msg;
            return msg;
        }

        std::function<doclife::model::Node* (int)> build =
            [&](int row) -> doclife::model::Node* {
            auto* n = new doclife::model::Node;
            n->data = attrsByRow.value(row);
            for (int cr : childrenOf.value(row)) {
                auto* c = build(cr);
                c->parent = n;
                n->children.append(c);
            }
            return n;
            };

        delete m_root;
        m_root = build(rootRow);
        qDebug() << "[ProjectContext] readMetadata 成功，节点数 =" << attrsByRow.size();
        return {};
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
            if (sheetName == kMetaSheetName) continue;
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