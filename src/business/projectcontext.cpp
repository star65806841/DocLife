#include "projectcontext.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"

namespace doclife::core {

// 元数据工作表的名字（第一个工作表）
static const QString kMetaSheetName = QStringLiteral("_meta_");

// ============================================================
// 单例
// ============================================================
ProjectContext* ProjectContext::instance() {
    static ProjectContext s;
    return &s;
}

ProjectContext::ProjectContext(QObject* parent) : QObject(parent) {}
ProjectContext::~ProjectContext() {}

// ============================================================
// 状态
// ============================================================
QString ProjectContext::projectName() const {
    if (!m_projectInfo.projectName.isEmpty())
        return m_projectInfo.projectName;
    return QFileInfo(m_excelFilePath).baseName();
}

void ProjectContext::markDirty() {
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged(true);
    }
}

void ProjectContext::resetState() {
    m_root          = doclife::model::ProjectNode{};
    m_projectInfo   = doclife::model::ProjectInfoModel{};
    m_excelFilePath.clear();
    m_dirty = false;
    emit dirtyChanged(false);
}

// ============================================================
// 创建项目
// ============================================================
bool ProjectContext::createProject(const QString& filePath,
                                   const doclife::model::ProjectInfoModel& info,
                                   QString* errorMsg) {
    // 已有项目：先关闭
    if (hasOpenProject()) {
        if (!closeProject(true, errorMsg)) return false;
    }

    // 初始化树根
    m_root = doclife::model::ProjectNode{};
    m_root.type = QStringLiteral("project");
    m_root.id   = info.id.isEmpty()
                    ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                    : info.id;

    m_projectInfo   = info;
    if (m_projectInfo.id.isEmpty())
        m_projectInfo.id = m_root.id;
    m_projectInfo.projectnodeId = m_root.id;

    m_excelFilePath = QDir::cleanPath(filePath);

    // 确保目录存在
    QDir().mkpath(QFileInfo(m_excelFilePath).absolutePath());

    if (!writeMetadata(m_excelFilePath, errorMsg)) {
        resetState();
        return false;
    }

    m_dirty = false;
    emit dirtyChanged(false);
    emit projectCreated(m_excelFilePath);
    emit projectOpened(m_excelFilePath);
    return true;
}

// ============================================================
// 打开项目
// ============================================================
bool ProjectContext::openProject(const QString& filePath, QString* errorMsg) {
    const QString cleanPath = QDir::cleanPath(filePath);

    if (!QFile::exists(cleanPath)) {
        if (errorMsg) *errorMsg = QStringLiteral("文件不存在: %1").arg(cleanPath);
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    // 已有项目 → 保存并关闭
    if (hasOpenProject()) {
        if (!closeProject(true, errorMsg)) return false;
    }

    if (!readMetadata(cleanPath, errorMsg)) {
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }

    m_excelFilePath = cleanPath;
    m_dirty = false;
    emit dirtyChanged(false);
    emit projectOpened(m_excelFilePath);
    return true;
}

// ============================================================
// 保存
// ============================================================
bool ProjectContext::saveProject(QString* errorMsg) {
    if (!hasOpenProject()) {
        if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目");
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

bool ProjectContext::saveProjectAs(const QString& filePath, QString* errorMsg) {
    if (!hasOpenProject()) {
        if (errorMsg) *errorMsg = QStringLiteral("没有打开的项目");
        return false;
    }
    const QString cleanPath = QDir::cleanPath(filePath);
    QDir().mkpath(QFileInfo(cleanPath).absolutePath());

    if (!writeMetadata(cleanPath, errorMsg)) {
        emit errorOccurred(errorMsg ? *errorMsg : QString());
        return false;
    }
    m_excelFilePath = cleanPath;
    m_dirty = false;
    emit dirtyChanged(false);
    emit projectSaved(cleanPath);
    return true;
}

// ============================================================
// 关闭
// ============================================================
bool ProjectContext::closeProject(bool saveBeforeClose, QString* errorMsg) {
    if (!hasOpenProject()) return true;

    if (saveBeforeClose && m_dirty) {
        if (!saveProject(errorMsg)) return false;
    }

    resetState();
    emit projectClosed();
    return true;
}

// ============================================================
// 元数据读写
// ============================================================
bool ProjectContext::writeMetadata(const QString& filePath, QString* errorMsg) {
    // 原子保存：先写临时文件，再替换
    const QString tmpPath = filePath + QStringLiteral(".tmp");

    {
        // 若文件已存在，QXlsx 会加载它；否则创建新文档
        QXlsx::Document doc(filePath);

        // 确保元数据工作表存在
        if (!doc.sheetNames().contains(kMetaSheetName)) {
            doc.addSheet(kMetaSheetName);
        }
        doc.selectSheet(kMetaSheetName);

        // A1 = 整棵树的 JSON；B1 = ProjectInfoModel 的 JSON
        const QByteArray rootJson = QJsonDocument(
                                        doclife::model::ProjectNode::toJson(m_root)
                                        ).toJson(QJsonDocument::Compact);

        const QByteArray infoJson = QJsonDocument(
                                        m_projectInfo.toJson()
                                        ).toJson(QJsonDocument::Compact);

        doc.write(1, 1, QString::fromUtf8(rootJson));
        doc.write(1, 2, QString::fromUtf8(infoJson));

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

    // 优先用命名表；找不到就用第一个表兜底
    const QString metaName = sheets.contains(kMetaSheetName)
                                 ? kMetaSheetName : sheets.first();
    doc.selectSheet(metaName);

    const QString rootStr = doc.read(1, 1).toString();
    const QString infoStr = doc.read(1, 2).toString();

    QJsonParseError err{};
    const auto rootDoc = QJsonDocument::fromJson(rootStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !rootDoc.isObject()) {
        if (errorMsg) *errorMsg = QStringLiteral("根节点 JSON 解析失败: %1").arg(err.errorString());
        return false;
    }

    const auto infoDoc = QJsonDocument::fromJson(infoStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !infoDoc.isObject()) {
        if (errorMsg) *errorMsg = QStringLiteral("项目信息 JSON 解析失败: %1").arg(err.errorString());
        return false;
    }

    m_root        = doclife::model::ProjectNode::fromJson(rootDoc.object());
    m_projectInfo = doclife::model::ProjectInfoModel::fromJson(infoDoc.object());
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