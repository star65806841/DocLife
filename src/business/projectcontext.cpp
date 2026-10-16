#include "projectcontext.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QHash>
#include <QVector>
#include <QDebug>
#include <QStandardPaths>
#include <QCoreApplication>
#include <functional>

#ifdef Q_OS_WIN
#  include <windows.h>
#else
#  include <signal.h>
#  include <errno.h>
#endif

#include <uisignalmanager.h>

#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"

namespace doclife::core {

// ============================================================
// 内部工具
// ============================================================
namespace {

// 元数据工作表名字
const QString kMetaSheetName = QStringLiteral("_meta_");
// 锁文件后缀
const QString kLockSuffix    = QStringLiteral(".lock");

// 当前进程是否存活（用于判断陈旧锁）
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

// 临时目录（统一入口）
QString tempDirPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
    + QStringLiteral("/DocLife");
    QDir().mkpath(dir);
    return dir;
}

// 只在锁文件层面操作，不改动 m_lockedPath
bool tryAcquireLockFile(const QString& filePath, QString* errorMsg) {
    const QString lockPath = filePath + kLockSuffix;

    // 已有锁：检查持有者是否还活着
    QFile lock(lockPath);
    if (lock.exists()) {
        if (lock.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const qint64 pid = QString::fromUtf8(lock.readAll()).trimmed().toLongLong();
            lock.close();
            if (pid > 0 && isProcessAlive(pid)) {
                if (errorMsg)
                    *errorMsg = QStringLiteral("文件正在被其他进程使用（PID %1）").arg(pid);
                return false;
            }
        }
        // 陈旧锁：持有进程已死
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

// 启动清理：临时文件 + 死锁
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
                // 只删除持有进程已死的锁
                QFile f(path);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const qint64 pid =
                        QString::fromUtf8(f.readAll()).trimmed().toLongLong();
                    f.close();
                    if (pid > 0 && isProcessAlive(pid)) continue;
                }
            }

            if (QFile::remove(path)) ++removed;
        }
    }
    return removed;
}

// 把整棵树按前序写入 A2 起的连续行
void writeNodeFlat(QXlsx::Document& doc, int& row,
                   const doclife::model::ProjectNode& n, int parentRow)
{
    const int myRow = row;

    QJsonObject o;
    o["parentRow"] = parentRow;
    if (!n.id.isEmpty())    o["id"]    = n.id;
    o["type"] = n.type;
    o["name"] = n.name;
    if (!n.attrs.isEmpty()) o["attrs"] = n.attrs;

    doc.write(myRow, 1,
              QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    ++row;

    for (const auto& c : n.children)
        writeNodeFlat(doc, row, c, myRow);
}

} // namespace

// ============================================================
// 单例 / 生命周期
// ============================================================
ProjectContext* ProjectContext::instance() {
    static ProjectContext s;
    return &s;
}

ProjectContext::ProjectContext(QObject* parent) : QObject(parent) {
    // 启动清理：清掉上次异常退出留下的临时文件和死锁
    const int n = cleanupOrphanTempFiles();
    if (n > 0)
        qDebug() << "[ProjectContext] 清理残留临时文件:" << n << "个";

    initConnect();
}

ProjectContext::~ProjectContext()
{
    releaseLock();
    cleanupTempFile();
}

void ProjectContext::initConnect()
{
    // connect(doclife::ui::UISignalManager::instance(),
    //         &doclife::ui::UISignalManager::newProject,
    //         this, &ProjectContext::slot_newProject);
}

// ============================================================
// 状态
// ============================================================
QString ProjectContext::projectName() const {
    if (!m_projectInfo.projectName.isEmpty())
        return m_projectInfo.projectName;
    return QFileInfo(m_excelFilePath).baseName();
}

QString ProjectContext::displayName() const {
    const QString name = m_root.name;
    if (!name.isEmpty()) return name;
    if (isUntitled())    return QStringLiteral("未命名文档");
    return QFileInfo(m_excelFilePath).baseName();
}

QString ProjectContext::getUuid() const
{
    return m_root.id;
}

void ProjectContext::setProjectInfo(model::ProjectInfoModel &projectInfo)
{
    m_projectInfo.projectName            = projectInfo.projectName.trimmed();
    m_projectInfo.constructionUnitName   = projectInfo.constructionUnitName.trimmed();
    m_projectInfo.supervisionUnitName    = projectInfo.supervisionUnitName.trimmed();
    m_projectInfo.designUnitName         = projectInfo.designUnitName.trimmed();
    m_projectInfo.ownerUnitName          = projectInfo.ownerUnitName.trimmed();
    // set 完成以后尝试保存一下
    if (!hasOpenProject()) {
        // 没有打开的项目
        qDebug() << "[ProjectContext] setProjectInfo: There are no open projects.Attempting to save failed.";
        return;
    }
    if (isUntitled()) {
        // 项目尚未保存过
        qDebug() << "[ProjectContext] setProjectInfo: The project has not been saved yet. The attempt to save failed.";
        return;
    }
    saveProject();
}

void ProjectContext::markDirty() {
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged(true);
    }
}

void ProjectContext::resetState() {
    m_root        = doclife::model::ProjectNode{};
    m_projectInfo = doclife::model::ProjectInfoModel{};
    m_excelFilePath.clear();
    // m_tempFilePath 由 cleanupTempFile 负责清
    // m_lockedPath   由 releaseLock   负责清
    if (m_dirty) {
        m_dirty = false;
        emit dirtyChanged(false);
    }
}

// ============================================================
// 临时文件
// ============================================================
QString ProjectContext::makeTempFilePath() const {
    // 用 UUID，多实例之间不会互相覆盖
    return tempDirPath()
           + QStringLiteral("/untitled_")
           + QUuid::createUuid().toString(QUuid::WithoutBraces)
           + QStringLiteral(".doclife");
}

void ProjectContext::cleanupTempFile() {
    if (m_tempFilePath.isEmpty()) return;
    QFile::remove(m_tempFilePath);
    QFile::remove(m_tempFilePath + QStringLiteral(".tmp"));
    m_tempFilePath.clear();
}

// ============================================================
// 锁
// ============================================================
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
    // 已有项目 → 先保存/关闭
    if (hasOpenProject()) {
        if (!closeProject(true, errorMsg)) return false;
    }

    // 每次新建都造新临时文件（UUID 不同，不会撞）
    const QString tempPath = makeTempFilePath();
    QFile::remove(tempPath);
    QFile::remove(tempPath + QStringLiteral(".tmp"));

    // 初始化树 + 信息
    m_root = doclife::model::ProjectNode{};
    m_root.type = QStringLiteral("project");
    m_root.name = QStringLiteral("未命名项目");
    m_root.id   = QUuid::createUuid().toString(QUuid::WithoutBraces);

    m_projectInfo = doclife::model::ProjectInfoModel{};
    m_projectInfo.projectnodeId = m_root.id;

    // 写临时文件
    if (!writeMetadata(tempPath, errorMsg)) {
        resetState();
        return false;
    }

    // 临时文件不加锁（还没落盘到用户路径）
    m_excelFilePath = tempPath;
    m_tempFilePath  = tempPath;
    m_dirty         = false;

    emit dirtyChanged(false);
    emit projectCreated(tempPath);
    emit projectOpened (tempPath);
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

    // 若当前打开着东西（可能是临时文件），先保存/关闭
    if (hasOpenProject()) {
        if (!closeProject(true, errorMsg)) return false;
    }

    // 加锁：确认没有别人在开
    if (!tryAcquireLockFile(cleanPath, errorMsg)) {
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    if (!readMetadata(cleanPath, errorMsg)) {
        releaseLockFile(cleanPath);   // 读失败要解锁
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    m_lockedPath    = cleanPath;
    m_excelFilePath = cleanPath;
    m_tempFilePath.clear();
    m_dirty         = false;

    emit dirtyChanged(false);
    emit projectOpened(cleanPath);
    return true;
}

// ============================================================
// 保存（已保存过的文件）
// ============================================================
bool ProjectContext::saveProject(QString* errorMsg) {
    if (!hasOpenProject()) {
        if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目");
        return false;
    }
    if (isUntitled()) {
        if (errorMsg) *errorMsg = QStringLiteral("项目尚未保存过，请使用「另存为」");
        return false;
    }

    if (!writeMetadata(m_excelFilePath, errorMsg)) {
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }
    m_dirty = false;
    emit dirtyChanged(false);
    emit projectSaved(m_excelFilePath);
    return true;
}

// ============================================================
// 另存为 / 首次保存
// ============================================================
bool ProjectContext::saveProjectAs(const QString& filePath, QString* errorMsg) {
    if (!hasOpenProject()) {
        if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目");
        return false;
    }

    const QString cleanPath = QDir::cleanPath(filePath);
    if (cleanPath == m_excelFilePath) {
        return saveProject(errorMsg);
    }

    // ---------- 1. 给目标文件加锁（失败即返回，旧锁原封不动） ----------
    if (!tryAcquireLockFile(cleanPath, errorMsg)) {
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    // 从此处起，任何失败都要回滚目标锁
    auto rollbackLock = [&]() { releaseLockFile(cleanPath); };

    // ---------- 2. 先把内存数据刷到当前操作的文件 ----------
    if (!writeMetadata(m_excelFilePath, errorMsg)) {
        rollbackLock();
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    // ---------- 3. 拷贝到目标路径 ----------
    QDir().mkpath(QFileInfo(cleanPath).absolutePath());

    if (QFile::exists(cleanPath) && !QFile::remove(cleanPath)) {
        if (errorMsg) *errorMsg = QStringLiteral("无法覆盖目标文件: %1").arg(cleanPath);
        rollbackLock();
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }
    if (!QFile::copy(m_excelFilePath, cleanPath)) {
        if (errorMsg) *errorMsg = QStringLiteral("拷贝到目标路径失败: %1").arg(cleanPath);
        rollbackLock();
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    // ---------- 4. 释放旧文件锁（如果有） ----------
    if (!m_lockedPath.isEmpty() && m_lockedPath != cleanPath)
        releaseLockFile(m_lockedPath);

    // ---------- 5. 临时文件使命结束 → 删掉 ----------
    if (isUntitled()) {
        QFile::remove(m_tempFilePath);
        QFile::remove(m_tempFilePath + QStringLiteral(".tmp"));
        m_tempFilePath.clear();
    }

    // ---------- 6. 更新内存状态 ----------
    m_lockedPath    = cleanPath;
    m_excelFilePath = cleanPath;
    m_dirty         = false;

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
    // 若还是临时文件：不主动保存，直接丢掉

    releaseLock();
    cleanupTempFile();
    resetState();
    emit projectClosed();
    return true;
}

// ============================================================
// 元数据读写（扁平化：每节点一行）
// ============================================================
bool ProjectContext::writeMetadata(const QString& filePath, QString* errorMsg) {
    // 原子保存：先写临时文件，再替换
    const QString tmpPath = filePath + QStringLiteral(".tmp");

    {
        // 若文件已存在，QXlsx 会加载它；否则创建新文档
        QXlsx::Document doc(filePath);

        // 重建 _meta_：删旧 + 加新，保证干净
        if (doc.sheetNames().contains(kMetaSheetName))
            doc.deleteSheet(kMetaSheetName);
        doc.addSheet(kMetaSheetName);
        doc.selectSheet(kMetaSheetName);

        // ---- A1：头部（版本 + ProjectInfoModel） ----
        QJsonObject head;
        head["version"] = 1;
        head["info"]    = m_projectInfo.toJson();
        doc.write(1, 1, QString::fromUtf8(
                            QJsonDocument(head).toJson(QJsonDocument::Compact)));

        // ---- A2..：所有节点（前序遍历） ----
        int row = 2;
        writeNodeFlat(doc, row, m_root, 0);

        if (!doc.saveAs(tmpPath)) {
            if (errorMsg) *errorMsg = QStringLiteral("写临时文件失败: %1").arg(tmpPath);
            return false;
        }
    }   // doc 析构，释放文件句柄

    // 用临时文件替换原文件
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
    if (sheets.isEmpty()) {
        if (errorMsg) *errorMsg = QStringLiteral("Excel 文件为空");
        return false;
    }

    if (!sheets.contains(kMetaSheetName)) {
        if (errorMsg) *errorMsg = QStringLiteral("缺少元数据表 %1").arg(kMetaSheetName);
        return false;
    }
    doc.selectSheet(kMetaSheetName);

    // ---- A1：头部 ----
    QJsonParseError err{};
    const auto headDoc = QJsonDocument::fromJson(
        doc.read(1, 1).toString().toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !headDoc.isObject()) {
        if (errorMsg) *errorMsg = QStringLiteral("头部 JSON 解析失败: %1").arg(err.errorString());
        return false;
    }

    const auto headObj = headDoc.object();
    const auto infoObj = headObj.value(QStringLiteral("info")).toObject();
    m_projectInfo = doclife::model::ProjectInfoModel::fromJson(infoObj);

    // ---- A2..：节点 ----
    QHash<int, doclife::model::ProjectNode> nodeByRow;
    QHash<int, QVector<int>>                childrenOf;
    int                                     rootRow = -1;

    const QXlsx::CellRange range = doc.dimension();
    if (range.isValid()) {
        for (int r = 2; r <= range.lastRow(); ++r) {
            const QString s = doc.read(r, 1).toString();
            if (s.isEmpty()) continue;

            QJsonParseError e{};
            const auto d = QJsonDocument::fromJson(s.toUtf8(), &e);
            if (e.error != QJsonParseError::NoError || !d.isObject()) continue;

            const auto o = d.object();
            doclife::model::ProjectNode n;
            n.id    = o.value(QStringLiteral("id")).toString();
            n.type  = o.value(QStringLiteral("type")).toString();
            n.name  = o.value(QStringLiteral("name")).toString();
            n.attrs = o.value(QStringLiteral("attrs")).toObject();

            nodeByRow.insert(r, n);

            const int p = o.value(QStringLiteral("parentRow")).toInt(0);
            if (p == 0)   rootRow = r;
            else          childrenOf[p].append(r);
        }
    }

    if (rootRow < 0) {
        if (errorMsg) *errorMsg = QStringLiteral("元数据缺少根节点");
        return false;
    }

    // ---- 按 parentRow 重建树 ----
    std::function<doclife::model::ProjectNode(int)> build =
        [&](int row) -> doclife::model::ProjectNode {
        doclife::model::ProjectNode n = nodeByRow.value(row);
        const auto kids = childrenOf.value(row);
        for (int cr : kids)
            n.children.append(build(cr));
        return n;
    };

    m_root = build(rootRow);
    return true;
}

// ============================================================
// 打印：除 _meta_ 外的所有工作表
// ============================================================
bool ProjectContext::printAllSheets(QString* errorMsg) {
    if (!hasOpenProject()) {
        if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目");
        return false;
    }

    QXlsx::Document doc(m_excelFilePath);
    const QStringList allSheets = doc.sheetNames();

    int printed = 0;
    for (const QString& sheetName : allSheets) {
        if (sheetName == kMetaSheetName) continue;

        doc.selectSheet(sheetName);
        QXlsx::CellRange range = doc.dimension();
        if (!range.isValid()) {
            qDebug() << "[print] 空表:" << sheetName;
            continue;
        }

        // 这里可以做真正的打印：
        //   1) 用 QPrinter + QPainter 逐格绘制
        //   2) 转 HTML 后用 QTextDocument::print()
        //   3) 导出 PDF
        qDebug() << "[print] 表:" << sheetName
                 << "行:" << range.firstRow() << "~" << range.lastRow()
                 << "列:" << range.firstColumn() << "~" << range.lastColumn();

        ++printed;
    }

    qDebug() << "[print] 共处理" << printed << "张表";
    return true;
}

} // namespace doclife::core