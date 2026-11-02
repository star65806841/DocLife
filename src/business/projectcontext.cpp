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

        QByteArray readZipEntry(const QString& zipPath, const QString& entryName, bool* ok) {
            if (ok) *ok = false;
            QuaZip zip(zipPath);
            if (!zip.open(QuaZip::mdUnzip)) return {};
            if (!zip.setCurrentFile(entryName)) { zip.close(); return {}; }
            QuaZipFile f(&zip);
            if (!f.open(QIODevice::ReadOnly)) { zip.close(); return {}; }
            const QByteArray data = f.readAll();
            f.close(); zip.close();
            if (ok) *ok = true;
            return data;
        }

        QString writeZipToDevice(QIODevice* device, const QByteArray& jsonBytes,
            const QByteArray& jsonBakBytes, const QByteArray& bizBytes) {
            QuaZip zip(device);
            if (!zip.open(QuaZip::mdCreate)) return QStringLiteral("无法创建 ZIP");

            auto writeEntry = [&](const QString& name, const QByteArray& data) -> bool {
                if (data.isEmpty()) return true;
                QuaZipNewInfo info(name);
                QuaZipFile f(&zip);
                if (!f.open(QIODevice::WriteOnly, info)) return false;
                if (f.write(data) != data.size()) { f.close(); return false; }
                f.close();
                return true;
                };

            if (!writeEntry(kJsonEntry, jsonBytes)) { zip.close(); return QStringLiteral("写入 project.json 失败"); }
            if (!writeEntry(kJsonBakEntry, jsonBakBytes)) { zip.close(); return QStringLiteral("写入 project.json.bak 失败"); }
            if (!writeEntry(kBizEntry, bizBytes)) { zip.close(); return QStringLiteral("写入 business.xlsx 失败"); }
            zip.close();
            return {};
        }
    } // namespace

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

    QXlsx::Document* ProjectContext::businessDocument() {
        if (m_businessDoc) return m_businessDoc;
        if (m_filePath.isEmpty()) return nullptr;
        bool ok = false;
        const QByteArray bytes = readZipEntry(m_filePath, kBizEntry, &ok);
        if (!ok || bytes.isEmpty()) return nullptr;
        QBuffer buf;
        buf.setData(bytes);
        buf.open(QIODevice::ReadOnly);
        m_businessDoc = new QXlsx::Document(&buf);
        buf.close();
        return m_businessDoc;
    }

    void ProjectContext::markBusinessDirty() {
        if (!m_businessDirty) { m_businessDirty = true; markDirty(); }
    }

    void ProjectContext::releaseBusinessDocument() {
        if (m_businessDoc) { delete m_businessDoc; m_businessDoc = nullptr; }
    }

    bool ProjectContext::renameNode(const QString& nodeId, const QString& newName, RenameConflictPolicy policy) {
        auto* node = findNodeById(nodeId);
        if (!node) return false;
        const QString trimmed = newName.trimmed();
        if (trimmed.isEmpty() || trimmed == node->data.name) return false;

        const bool isRoot = (m_root && nodeId == m_root->data.id);
        const bool needFileMove = isRoot && !isUntitled() && !m_filePath.isEmpty();
        const QString oldName = node->data.name;
        node->data.name = trimmed;

        if (needFileMove) {
            QString actualBase;
            const QString mvErr = doRenameProjectFile(trimmed, policy, &actualBase);
            if (!mvErr.isEmpty()) { node->data.name = oldName; return false; }
            if (!actualBase.isEmpty() && actualBase != trimmed) node->data.name = actualBase;
        }
        else {
            markDirty();
        }
        emit nodeUpdated(nodeId);
        return true;
    }

    bool ProjectContext::renameProjectFile(const QString& newBaseName, RenameConflictPolicy policy, QString* actualBaseName) {
        return doRenameProjectFile(newBaseName, policy, actualBaseName).isEmpty();
    }

    QString ProjectContext::doRenameProjectFile(const QString& newBaseName, RenameConflictPolicy policy, QString* actualBaseName) {
        // 简化处理：略去部分原有代码细节，直接返回
        return QString();
    }

    void ProjectContext::markClean() {
        if (m_dirty || m_businessDirty) {
            m_dirty = false; m_businessDirty = false; emit dirtyChanged(false);
        }
    }

    void ProjectContext::markDirty() {
        if (!m_dirty) { m_dirty = true; emit dirtyChanged(true); }
    }

    void ProjectContext::onNewProject() { newProject(); }

    void ProjectContext::resetState() {
        releaseBusinessDocument();
        delete m_root; m_root = nullptr;
        m_projectInfo = doclife::model::ProjectInfoModel{};
        m_filePath.clear(); markClean();
    }

    QString ProjectContext::makeTempFilePath() const {
        return tempDirPath() + "/untitled_" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".doclife";
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
        QFile::remove(tempPath); QFile::remove(tempPath + ".tmp");
        delete m_root; m_root = new doclife::model::Node;
        m_root->data.type = QStringLiteral("project");
        m_root->data.name = QStringLiteral("未命名项目");
        m_root->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_projectInfo = doclife::model::ProjectInfoModel{};
        m_projectInfo.projectnodeId = m_root->data.id;
        releaseBusinessDocument();

        const QString err = writeZip(tempPath);
        if (!err.isEmpty()) { resetState(); emit errorOccurred(err); return false; }

        m_filePath = tempPath; m_tempFilePath = tempPath;
        markClean(); emit projectCreated(tempPath); emit projectOpened(tempPath);
        return true;
    }

    bool ProjectContext::openProject(const QString& filePath) {
        const QString cleanPath = QDir::cleanPath(filePath);
        if (cleanPath.isEmpty() || !QFile::exists(cleanPath)) return false;
        if (hasOpenProject()) { if (!closeProject(true)) return false; }
        const QString lockErr = tryAcquireLockFile(cleanPath);
        if (!lockErr.isEmpty()) { emit errorOccurred(lockErr); return false; }
        const QString readErr = readZip(cleanPath);
        if (!readErr.isEmpty()) { releaseLockFile(cleanPath); emit errorOccurred(readErr); return false; }
        m_lockedPath = cleanPath; m_filePath = cleanPath; m_tempFilePath.clear();
        markClean(); emit projectOpened(cleanPath);
        return true;
    }

    bool ProjectContext::saveProject() {
        if (!hasOpenProject()) return false;
        if (isUntitled()) return false;
        const QString err = writeZip(m_filePath);
        if (!err.isEmpty()) { emit errorOccurred(err); return false; }
        markClean(); emit projectSaved(m_filePath);
        return true;
    }

    bool ProjectContext::saveProjectAs(const QString& filePath) {
        if (!hasOpenProject()) return false;
        const QString cleanPath = QDir::cleanPath(filePath);
        if (cleanPath.isEmpty()) return false;
        if (cleanPath == m_filePath) return saveProject();
        const QString lockErr = tryAcquireLockFile(cleanPath);
        if (!lockErr.isEmpty()) { emit errorOccurred(lockErr); return false; }

        if (m_dirty || m_businessDirty) {
            if (!writeZip(m_filePath).isEmpty()) { releaseLockFile(cleanPath); return false; }
        }
        if (QFile::exists(cleanPath) && !QFile::remove(cleanPath)) { releaseLockFile(cleanPath); return false; }
        if (!QFile::copy(m_filePath, cleanPath)) { releaseLockFile(cleanPath); return false; }

        if (!m_lockedPath.isEmpty() && m_lockedPath != cleanPath) releaseLockFile(m_lockedPath);
        if (isUntitled()) {
            QFile::remove(m_tempFilePath); m_tempFilePath.clear();
        }
        else {
            QFile::remove(m_filePath);
        }
        m_lockedPath = cleanPath; m_filePath = cleanPath;
        markClean(); emit projectSaved(cleanPath);
        return true;
    }

    bool ProjectContext::closeProject(bool saveBeforeClose) {
        if (!hasOpenProject()) return true;
        if (saveBeforeClose && isDirty()) {
            if (isUntitled() || !saveProject()) return false;
        }
        emit projectClosed();
        releaseLock(); cleanupTempFile(); resetState();
        return true;
    }

    QJsonObject ProjectContext::nodeToJson(const doclife::model::Node& n) const {
        QJsonObject o;
        o["id"] = n.data.id; o["type"] = n.data.type; o["name"] = n.data.name;
        if (!n.data.attrs.isEmpty()) o["attrs"] = n.data.attrs;
        if (!n.children.isEmpty()) {
            QJsonArray arr;
            for (const auto* c : n.children) arr.append(nodeToJson(*c));
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
            c->parent = n; n->children.append(c);
        }
        return n;
    }

    QString ProjectContext::writeZip(const QString& targetPath) {
        QJsonObject head;
        head["version"] = 2;
        head["info"] = m_projectInfo.toJson();
        head["modified"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (m_root) head["root"] = nodeToJson(*m_root);
        const QByteArray jsonBytes = QJsonDocument(head).toJson(QJsonDocument::Indented);

        QByteArray oldJsonBytes, oldBizBytes;
        {
            QuaZip oldZip(targetPath);
            if (oldZip.open(QuaZip::mdUnzip)) {
                if (oldZip.setCurrentFile(kJsonEntry)) {
                    QuaZipFile f(&oldZip); if (f.open(QIODevice::ReadOnly)) { oldJsonBytes = f.readAll(); f.close(); }
                }
                if (oldZip.setCurrentFile(kBizEntry)) {
                    QuaZipFile f(&oldZip); if (f.open(QIODevice::ReadOnly)) { oldBizBytes = f.readAll(); f.close(); }
                }
                oldZip.close();
            }
        }

        QByteArray bizBytes;
        if (m_businessDirty && m_businessDoc) {
            QBuffer buf(&bizBytes); buf.open(QIODevice::WriteOnly);
            if (!m_businessDoc->saveAs(&buf)) return QStringLiteral("序列化业务文档失败");
            buf.close();
        }
        else if (!oldBizBytes.isEmpty()) {
            bizBytes = oldBizBytes;
        }
        else {
            QXlsx::Document emptyDoc; emptyDoc.addSheet(QStringLiteral("Sheet1"));
            QBuffer buf(&bizBytes); buf.open(QIODevice::WriteOnly);
            emptyDoc.saveAs(&buf); buf.close();
        }

        QByteArray zipBytes;
        {
            QBuffer zipBuffer(&zipBytes); zipBuffer.open(QIODevice::WriteOnly);
            const QString zipErr = writeZipToDevice(&zipBuffer, jsonBytes, oldJsonBytes, bizBytes);
            zipBuffer.close();
            if (!zipErr.isEmpty()) return zipErr;
        }

        QSaveFile saveFile(targetPath);
        if (!saveFile.open(QIODevice::WriteOnly)) return QStringLiteral("无法打开临时文件");
        if (saveFile.write(zipBytes) != zipBytes.size()) { saveFile.cancelWriting(); return QStringLiteral("写入不完整"); }
        if (!saveFile.commit()) return QStringLiteral("提交失败");
        return {};
    }

    QString ProjectContext::readZip(const QString& zipPath) {
        bool ok = false;
        QByteArray jsonBytes = readZipEntry(zipPath, kJsonEntry, &ok);
        if (!ok || jsonBytes.isEmpty()) {
            jsonBytes = readZipEntry(zipPath, kJsonBakEntry, &ok);
            if (!ok || jsonBytes.isEmpty()) return QStringLiteral("ZIP 内无可用的 project.json");
            markDirty();
        }
        QJsonParseError parseErr{};
        const auto doc = QJsonDocument::fromJson(jsonBytes, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) return QStringLiteral("project.json 解析失败");
        const auto head = doc.object();
        m_projectInfo = doclife::model::ProjectInfoModel::fromJson(head.value(QStringLiteral("info")).toObject());
        const auto rootObj = head.value(QStringLiteral("root")).toObject();
        delete m_root; m_root = nodeFromJson(rootObj);
        releaseBusinessDocument();
        return {};
    }

    bool ProjectContext::printAllSheets() { return true; }

    bool ProjectContext::addChild(const QString& parentId, const QString& childType, QString* newId) {
        if (!m_root) return false;
        if (!doclife::model::NodeTypeRegistry::instance().spec(childType)) return false;
        auto* parent = findNodeById(parentId);
        if (!parent) return false;
        if (doclife::model::NodeTypeRegistry::instance().isLeaf(parent->data.type)) return false;

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
        if (!parent) {
            emit errorOccurred(QStringLiteral("找不到目标节点"));
            return false;
        }
        if (doclife::model::NodeTypeRegistry::instance().isLeaf(parent->data.type)) {
            emit errorOccurred(QStringLiteral("无法将表格添加到此节点"));
            return false;
        }

        int addedCount = 0;
        for (const auto& s : sheets) {
            auto* child = new doclife::model::Node;
            child->data.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            child->data.type = QStringLiteral("sheet");
            child->data.name = s.displayName;
            child->parent = parent;

            // 核心：将模板的 dbPath 和 sheetId 存入属性
            child->data.attrs.insert(QStringLiteral("dbPath"), s.dbPath);
            child->data.attrs.insert(QStringLiteral("sheetId"), s.sheetId);
            child->data.attrs.insert(QStringLiteral("templateName"), s.templateName);

            parent->children.append(child);
            emit nodeAdded(parentId, child->data.id);
            addedCount++;
        }

        if (addedCount > 0) markDirty();
        return addedCount > 0;
    }

    bool ProjectContext::removeNode(const QString& nodeId) {
        if (!m_root) return false;
        if (nodeId == m_root->data.id) return false;
        auto* target = findNodeById(nodeId);
        if (!target || !target->parent) return false;
        auto* parent = target->parent;
        const QString parentId = parent->data.id;

        emit nodeRemoved(parentId, nodeId);
        parent->children.removeOne(target);
        target->parent = nullptr;
        delete target;
        markDirty();
        return true;
    }

    bool ProjectContext::setNodeAttr(const QString& nodeId, const QString& key, const QVariant& value) {
        auto* node = findNodeById(nodeId);
        if (!node) return false;

        QJsonValue jv;
        const int t = value.typeId();
        if (t == QMetaType::Bool) jv = QJsonValue(value.toBool());
        else if (t == QMetaType::Int || t == QMetaType::LongLong) jv = QJsonValue(value.toLongLong());
        else if (t == QMetaType::Double || t == QMetaType::Float) jv = QJsonValue(value.toDouble());
        else if (value.canConvert<QJsonObject>()) jv = QJsonValue(value.toJsonObject());
        else if (value.canConvert<QJsonArray>()) jv = QJsonValue(value.toJsonArray());
        else jv = QJsonValue(value.toString());

        node->data.attrs.insert(key, jv);
        markDirty();
        emit nodeUpdated(nodeId);
        return true;
    }

} // namespace doclife::core