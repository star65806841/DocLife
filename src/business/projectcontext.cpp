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

#include "utils.h"                                 // Utils::sanitizeFileName
#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"
#include "model/custommodel/NodeTypeRegistry.h"    // 按实际路径

namespace doclife::core {

namespace {

const QString kMetaSheetName = QStringLiteral("_meta_");
const QString kLockSuffix    = QStringLiteral(".lock");

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

bool tryAcquireLockFile(const QString& filePath, QString* errorMsg) {
    const QString lockPath = filePath + kLockSuffix;
    QFile lock(lockPath);
    if (lock.exists()) {
        if (lock.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const qint64 pid = QString::fromUtf8(lock.readAll()).trimmed().toLongLong();
            lock.close();
            if (pid > 0 && isProcessAlive(pid)) {
                if (errorMsg) *errorMsg = QStringLiteral("文件正在被其他进程使用（PID %1）").arg(pid);
                return false;
            }
        }
        QFile::remove(lockPath);
    }
    if (!lock.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMsg) *errorMsg = QStringLiteral("无法创建锁文件: %1").arg(lockPath);
        return false;
    }
    lock.write(QByteArray::number(QCoreApplication::applicationPid()));
    lock.close();
    return true;
}

void releaseLockFile(const QString& filePath) {
    if (filePath.isEmpty()) return;
    QFile::remove(filePath + kLockSuffix);
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
    if (!n.data.id.isEmpty())    o["id"]   = n.data.id;
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
    m_root = new doclife::model::Node;   // 空根，保证 findNodeById 不崩
    const int n = cleanupOrphanTempFiles();
    if (n > 0) qDebug() << "[ProjectContext] 清理残留临时文件:" << n << "个";
    initConnect();
}

ProjectContext::~ProjectContext() {
    releaseLock();
    cleanupTempFile();
    delete m_root;
    m_root = nullptr;
}

void ProjectContext::initConnect() { }

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
    m_projectInfo.id                   = projectInfo.id;
    m_projectInfo.projectnodeId        = projectInfo.projectnodeId;
    m_projectInfo.projectName          = projectInfo.projectName.trimmed();
    m_projectInfo.ownerUnitName        = projectInfo.ownerUnitName.trimmed();
    m_projectInfo.constructionUnitName = projectInfo.constructionUnitName.trimmed();
    m_projectInfo.designUnitName       = projectInfo.designUnitName.trimmed();
    m_projectInfo.supervisionUnitName  = projectInfo.supervisionUnitName.trimmed();
    markDirty();
}

// ============================================================
// 重命名
// ============================================================
bool ProjectContext::renameNode(const QString& nodeId, const QString& newName,
                                QString* errorMsg, RenameConflictPolicy policy)
{
    auto* node = findNodeById(nodeId);
    if (!node) { if (errorMsg) *errorMsg = QStringLiteral("找不到节点: %1").arg(nodeId); return false; }

    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty()) { if (errorMsg) *errorMsg = QStringLiteral("名称不能为空"); return false; }
    if (trimmed == node->data.name) return true;

    const bool isRoot = (m_root && nodeId == m_root->data.id);
    const bool needFileMove = isRoot && !isUntitled() && !m_filePath.isEmpty();
    const QString oldName = node->data.name;

    node->data.name = trimmed;

    if (needFileMove) {
        QString actualBase, localErr;
        if (!renameProjectFile(trimmed, policy, &actualBase, &localErr)) {
            node->data.name = oldName;
            if (errorMsg) *errorMsg = localErr;
            emit errorOccurred(localErr);
            return false;
        }
        if (!actualBase.isEmpty() && actualBase != trimmed)
            node->data.name = actualBase;
    } else {
        markDirty();
    }
    emit nodeUpdated(nodeId);
    return true;
}

bool ProjectContext::renameProjectFile(const QString& newBaseName,
                                       RenameConflictPolicy policy,
                                       QString* actualBaseName,
                                       QString* errorMsg)
{
    if (m_filePath.isEmpty() || isUntitled()) {
        if (errorMsg) *errorMsg = QStringLiteral("当前项目尚未保存到用户路径");
        return false;
    }

    const QFileInfo fi(m_filePath);
    const QString dir    = fi.absolutePath();
    const QString suffix = fi.suffix();
    const QString base   = Utils::sanitizeFileName(newBaseName);
    if (base.isEmpty()) {
        if (errorMsg) *errorMsg = QStringLiteral("文件名包含非法字符或为空");
        return false;
    }

    QString finalBase = base;
    QString newPath = dir + '/' + finalBase + (suffix.isEmpty() ? QString() : '.' + suffix);
    if (newPath == m_filePath) {
        if (actualBaseName) *actualBaseName = finalBase;
        return true;
    }

    auto buildPath = [&](const QString& b) {
        return dir + '/' + b + (suffix.isEmpty() ? QString() : '.' + suffix);
    };

    switch (policy) {
    case RenameConflictPolicy::Fail:
        if (QFile::exists(newPath)) {
            if (errorMsg) *errorMsg = QStringLiteral("同目录已存在文件: %1").arg(newPath);
            return false;
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
                if (errorMsg) *errorMsg = QStringLiteral("无法找到可用的文件名");
                return false;
            }
        }
        break;
    }
    case RenameConflictPolicy::Overwrite:
        break;
    }

    const QString oldPath = m_filePath;

    if (policy != RenameConflictPolicy::Overwrite) {
        if (!tryAcquireLockFile(newPath, errorMsg)) return false;
    }
    auto rollbackNewLock = [&]() {
        if (policy != RenameConflictPolicy::Overwrite) releaseLockFile(newPath);
    };

    if (!writeMetadata(oldPath, errorMsg)) { rollbackNewLock(); return false; }

    if (policy == RenameConflictPolicy::Overwrite && QFile::exists(newPath)) {
        if (!QFile::remove(newPath)) {
            if (errorMsg) *errorMsg = QStringLiteral("无法覆盖: %1").arg(newPath);
            rollbackNewLock();
            return false;
        }
        if (!tryAcquireLockFile(newPath, errorMsg)) { rollbackNewLock(); return false; }
    }

    if (!QFile::rename(oldPath, newPath)) {
        if (errorMsg) *errorMsg = QStringLiteral("文件重命名失败: %1 -> %2").arg(oldPath, newPath);
        rollbackNewLock();
        return false;
    }

    releaseLockFile(oldPath);
    m_lockedPath = newPath;
    m_filePath   = newPath;
    m_dirty      = false;

    if (actualBaseName) *actualBaseName = finalBase;
    emit dirtyChanged(false);
    emit projectFilePathChanged(oldPath, newPath);
    return true;
}

void ProjectContext::markDirty() {
    if (!m_dirty) { m_dirty = true; emit dirtyChanged(true); }
}

void ProjectContext::onNewProject() {
    qDebug() << "from a connect => onNewProject to newProject";
    this->newProject();
}

void ProjectContext::resetState() {
    delete m_root;
    m_root = nullptr;
    m_projectInfo = doclife::model::ProjectInfoModel{};
    m_filePath.clear();
    if (m_dirty) { m_dirty = false; emit dirtyChanged(false); }
}

// ============================================================
// 临时文件 / 锁
// ============================================================
QString ProjectContext::makeTempFilePath() const {
    return tempDirPath() + "/untitled_"
           + QUuid::createUuid().toString(QUuid::WithoutBraces)
           + ".doclife";
}

void ProjectContext::cleanupTempFile() {
    if (m_tempFilePath.isEmpty()) return;
    QFile::remove(m_tempFilePath);
    QFile::remove(m_tempFilePath + ".tmp");
    m_tempFilePath.clear();
}

bool ProjectContext::acquireLock(const QString& filePath, QString* errorMsg) {
    if (m_lockedPath == filePath) return true;
    if (!tryAcquireLockFile(filePath, errorMsg)) return false;
    if (!m_lockedPath.isEmpty()) releaseLockFile(m_lockedPath);
    m_lockedPath = filePath;
    return true;
}

void ProjectContext::releaseLock() {
    if (m_lockedPath.isEmpty()) return;
    releaseLockFile(m_lockedPath);
    m_lockedPath.clear();
}

// ============================================================
// 新建
// ============================================================
bool ProjectContext::newProject(QString* errorMsg)
{
    if (hasOpenProject()) {
        if (!closeProject(true, errorMsg)) return false;
    }

    const QString tempPath = makeTempFilePath();
    QFile::remove(tempPath);
    QFile::remove(tempPath + ".tmp");

    // 重建树
    delete m_root;
    m_root = new doclife::model::Node;
    m_root->data.type = QStringLiteral("project");
    m_root->data.name = QStringLiteral("未命名项目");
    m_root->data.id   = QUuid::createUuid().toString(QUuid::WithoutBraces);

    m_projectInfo = doclife::model::ProjectInfoModel{};
    m_projectInfo.projectnodeId = m_root->data.id;

    if (!writeMetadata(tempPath, errorMsg)) { resetState(); return false; }

    m_filePath     = tempPath;
    m_tempFilePath = tempPath;
    m_dirty        = false;

    emit dirtyChanged(false);
    emit projectCreated(tempPath);
    emit projectOpened(tempPath);
    return true;
}

// ============================================================
// 打开
// ============================================================
bool ProjectContext::openProject(const QString& filePath, QString* errorMsg) {
    const QString cleanPath = QDir::cleanPath(filePath);
    if (!QFile::exists(cleanPath)) {
        if (errorMsg) *errorMsg = QStringLiteral("文件不存在: %1").arg(cleanPath);
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    if (hasOpenProject()) {
        if (!closeProject(true, errorMsg)) return false;
    }

    if (!tryAcquireLockFile(cleanPath, errorMsg)) {
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    if (!readMetadata(cleanPath, errorMsg)) {
        releaseLockFile(cleanPath);
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    m_lockedPath = cleanPath;
    m_filePath   = cleanPath;
    m_tempFilePath.clear();
    m_dirty      = false;

    emit dirtyChanged(false);
    emit projectOpened(cleanPath);
    return true;
}

// ============================================================
// 保存
// ============================================================
bool ProjectContext::saveProject(QString* errorMsg) {
    if (!hasOpenProject()) { if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目"); return false; }
    if (isUntitled()) { if (errorMsg) *errorMsg = QStringLiteral("项目尚未保存过，请使用「另存为」"); return false; }

    if (!writeMetadata(m_filePath, errorMsg)) {
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }
    m_dirty = false;
    emit dirtyChanged(false);
    emit projectSaved(m_filePath);
    return true;
}

bool ProjectContext::saveProjectAs(const QString& filePath, QString* errorMsg) {
    if (!hasOpenProject()) { if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目"); return false; }

    const QString cleanPath = QDir::cleanPath(filePath);
    if (cleanPath == m_filePath) return saveProject(errorMsg);

    if (!tryAcquireLockFile(cleanPath, errorMsg)) {
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }
    auto rollbackLock = [&]() { releaseLockFile(cleanPath); };

    if (!writeMetadata(m_filePath, errorMsg)) {
        rollbackLock();
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    QDir().mkpath(QFileInfo(cleanPath).absolutePath());
    if (QFile::exists(cleanPath) && !QFile::remove(cleanPath)) {
        if (errorMsg) *errorMsg = QStringLiteral("无法覆盖目标文件: %1").arg(cleanPath);
        rollbackLock();
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }
    if (!QFile::copy(m_filePath, cleanPath)) {
        if (errorMsg) *errorMsg = QStringLiteral("拷贝到目标路径失败: %1").arg(cleanPath);
        rollbackLock();
        emit errorOccurred(errorMsg ? *errorMsg : QString());
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
    m_filePath   = cleanPath;
    m_dirty      = false;

    emit dirtyChanged(false);
    emit projectSaved(cleanPath);
    return true;
}

// ============================================================
// 关闭
// ============================================================
bool ProjectContext::closeProject(bool saveBeforeClose, QString* errorMsg) {
    if (!hasOpenProject()) return true;
    if (saveBeforeClose && m_dirty && !isUntitled()) {
        if (!saveProject(errorMsg)) return false;
    }

    // 先通知视图：模型立刻把 m_root 置空，之后才能安全 delete
    emit projectClosed();

    releaseLock();
    cleanupTempFile();
    resetState();
    return true;
}

// ============================================================
// 元数据读写
// ============================================================
bool ProjectContext::writeMetadata(const QString& filePath, QString* errorMsg) {
    const QString tmpPath = filePath + ".tmp";
    {
        QXlsx::Document doc(filePath);
        if (doc.sheetNames().contains(kMetaSheetName))
            doc.deleteSheet(kMetaSheetName);
        doc.addSheet(kMetaSheetName);
        doc.selectSheet(kMetaSheetName);

        QJsonObject head;
        head["version"] = 1;
        head["info"]    = m_projectInfo.toJson();
        doc.write(1, 1, QString::fromUtf8(
                            QJsonDocument(head).toJson(QJsonDocument::Compact)));

        int row = 2;
        if (m_root) writeNodeFlat(doc, row, *m_root, 0);

        if (!doc.saveAs(tmpPath)) {
            if (errorMsg) *errorMsg = QStringLiteral("写临时文件失败: %1").arg(tmpPath);
            return false;
        }
    }

    if (QFile::exists(filePath) && !QFile::remove(filePath)) {
        if (errorMsg) *errorMsg = QStringLiteral("无法删除原文件: %1").arg(filePath);
        return false;
    }
    if (!QFile::rename(tmpPath, filePath)) {
        if (errorMsg) *errorMsg = QStringLiteral("重命名临时文件失败: %1").arg(tmpPath);
        return false;
    }
    return true;
}

bool ProjectContext::readMetadata(const QString& filePath, QString* errorMsg) {
    QXlsx::Document doc(filePath);

    const QStringList sheets = doc.sheetNames();
    if (sheets.isEmpty()) { if (errorMsg) *errorMsg = QStringLiteral("Excel 文件为空"); return false; }
    if (!sheets.contains(kMetaSheetName)) {
        if (errorMsg) *errorMsg = QStringLiteral("缺少元数据表 %1").arg(kMetaSheetName);
        return false;
    }
    doc.selectSheet(kMetaSheetName);

    QJsonParseError err{};
    const auto headDoc = QJsonDocument::fromJson(doc.read(1, 1).toString().toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !headDoc.isObject()) {
        if (errorMsg) *errorMsg = QStringLiteral("头部 JSON 解析失败: %1").arg(err.errorString());
        return false;
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
            if (e.error != QJsonParseError::NoError || !d.isObject()) continue;
            const auto o = d.object();

            doclife::model::NodeAttributes a;
            a.id    = o.value(QStringLiteral("id")).toString();
            a.type  = o.value(QStringLiteral("type")).toString();
            a.name  = o.value(QStringLiteral("name")).toString();
            a.attrs = o.value(QStringLiteral("attrs")).toObject();

            attrsByRow.insert(r, a);
            const int p = o.value(QStringLiteral("parentRow")).toInt(0);
            if (p == 0) rootRow = r;
            else        childrenOf[p].append(r);
        }
    }

    if (rootRow < 0) { if (errorMsg) *errorMsg = QStringLiteral("元数据缺少根节点"); return false; }

    std::function<doclife::model::Node*(int)> build =
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
    return true;
}

// ============================================================
// 打印
// ============================================================
bool ProjectContext::printAllSheets(QString* errorMsg) {
    if (!hasOpenProject()) { if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目"); return false; }

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
                              QString* newId, QString* errorMsg)
{
    using doclife::model::NodeTypeRegistry;
    if (!m_root) { if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目"); return false; }
    if (!NodeTypeRegistry::instance().spec(childType)) {
        if (errorMsg) *errorMsg = QStringLiteral("未知节点类型: %1").arg(childType);
        return false;
    }
    auto* parent = findNodeById(parentId);
    if (!parent) { if (errorMsg) *errorMsg = QStringLiteral("找不到父节点: %1").arg(parentId); return false; }
    if (NodeTypeRegistry::instance().isLeaf(parent->data.type)) {
        if (errorMsg) *errorMsg = QStringLiteral("叶子节点不能添加子节点");
        return false;
    }

    auto* child = new doclife::model::Node;
    child->data.id   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    child->data.type = childType;
    child->data.name = NodeTypeRegistry::instance().displayLabel(childType);
    child->parent    = parent;
    parent->children.append(child);

    markDirty();
    emit nodeAdded(parentId, child->data.id);
    if (newId) *newId = child->data.id;
    return true;
}

bool ProjectContext::removeNode(const QString& nodeId, QString* errorMsg) {
    if (!m_root) { if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目"); return false; }
    if (nodeId == m_root->data.id) { if (errorMsg) *errorMsg = QStringLiteral("不能删除根节点"); return false; }

    auto* target = findNodeById(nodeId);
    if (!target) { if (errorMsg) *errorMsg = QStringLiteral("找不到节点: %1").arg(nodeId); return false; }
    auto* parent = target->parent;
    if (!parent) { if (errorMsg) *errorMsg = QStringLiteral("找不到父节点"); return false; }
    const QString parentId = parent->data.id;

    // 通知模型层：beginRemoveRows → removeAt → endRemoveRows
    emit nodeRemoved(parentId, nodeId);

    // 兜底：如果模型层没连接，业务层自己摘除
    if (target->parent) {
        parent->children.removeOne(target);
        target->parent = nullptr;
    }

    delete target;
    markDirty();
    return true;
}

bool ProjectContext::setNodeAttr(const QString& nodeId, const QString& key,
                                 const QVariant& value, QString* errorMsg)
{
    auto* node = findNodeById(nodeId);
    if (!node) { if (errorMsg) *errorMsg = QStringLiteral("找不到节点: %1").arg(nodeId); return false; }

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
    return true;
}

} // namespace doclife::core