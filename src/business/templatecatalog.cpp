#include "templatecatalog.h"
#include "templateinstaller.h"
#include "utils.h"

#include <QStandardItemModel>
#include <QStandardItem>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

using namespace doclife::core;

namespace doclife::ui {

    TemplateCatalog* TemplateCatalog::instance()
    {
        static TemplateCatalog s;
        return &s;
    }

    TemplateCatalog::TemplateCatalog(QObject* parent)
        : QObject(parent)
        , m_treeModel(new QStandardItemModel(this))
    {
        m_treeModel->setHorizontalHeaderLabels({ QStringLiteral("模板") });
    }

    void TemplateCatalog::load()
    {
        m_treeModel->removeRows(0, m_treeModel->rowCount());
        m_leaves.clear();
        m_processRules.clear();

        const QStringList codes = TemplateInstaller::installedCollections();
        qDebug() << "[TemplateCatalog] dataRoot =" << TemplateInstaller::dataRoot()
                 << "collections =" << codes;

        if (codes.isEmpty()) {
            qWarning() << "[TemplateCatalog] 未安装任何模板集合";
            m_loaded = true;
            emit reloaded();
            return;
        }

        for (const QString& code : codes) {
            // ★ 直接获取 .dlt 文件的绝对路径
            const QString dbPath = TemplateInstaller::collectionFilePath(code);
            auto* item = buildCollectionFromDb(dbPath, code);
            if (item && item->rowCount() > 0)
                m_treeModel->appendRow(item);
            else
                delete item;
        }

        m_loaded = true;
        qDebug() << "[TemplateCatalog] loaded sheets:" << m_leaves.size();
        emit reloaded();
    }

    QStandardItem* TemplateCatalog::buildCollectionFromDb(
        const QString& dbPath, const QString& collectionCode)
    {
        // ★ 直接检查传入的文件路径是否存在，不再拼接 "/catalog.db"
        if (!QFile::exists(dbPath)) {
            qWarning() << "[TemplateCatalog] 模板数据库不存在:" << dbPath;
            return nullptr;
        }

        const QString connName = QStringLiteral("catalog_") + collectionCode;
        // 重新加载时先把上一次的连接彻底移除，否则 addDatabase 会报同名连接警告
        if (QSqlDatabase::contains(connName)) {
            {
                QSqlDatabase old = QSqlDatabase::database(connName, false);
                if (old.isOpen()) old.close();
            }
            QSqlDatabase::removeDatabase(connName);
        }
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            qWarning() << "[TemplateCatalog] db 打开失败:" << db.lastError().text();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connName);
            return nullptr;
        }

        // 释放连接时必须先丢掉本地引用，否则 Qt 会警告“连接仍在使用”
        auto releaseConnection = [&]() {
            if (db.isOpen()) db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connName);
            };

        QStandardItem* collectionItem = nullptr;
        {
            // ---- catalogs ----
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "SELECT code, display_name FROM catalogs LIMIT 1"));
            if (!q.exec() || !q.next()) {
                qWarning() << "[TemplateCatalog] catalogs 查询失败";
                releaseConnection();
                return nullptr;
            }
            const QString displayName = q.value(1).toString();

            collectionItem = new QStandardItem(displayName);
            collectionItem->setEditable(false);
            collectionItem->setData(collectionCode, Qt::UserRole);
            collectionItem->setData(QStringLiteral("collection"), Qt::UserRole + 1);
            collectionItem->setToolTip(collectionCode);

            // ---- folders ----
            struct FolderRow { int id; QString key; QString display; };
            QList<FolderRow> folders;

            {
                QSqlQuery folderQ(db);
                folderQ.prepare(QStringLiteral(
                    "SELECT id, key, display_name FROM folders ORDER BY sort_order, id"));
                if (!folderQ.exec()) {
                    qWarning() << "[TemplateCatalog] folders 查询失败";
                    releaseConnection();
                    delete collectionItem;
                    return nullptr;
                }
                while (folderQ.next()) {
                    folders.append({
                        folderQ.value(0).toInt(),
                        folderQ.value(1).toString(),
                        folderQ.value(2).toString()
                    });
                }
            }

            for (const auto& fr : folders) {
                auto* folderItem = new QStandardItem(fr.display);
                folderItem->setEditable(false);
                folderItem->setData(fr.key, Qt::UserRole);
                folderItem->setData(QStringLiteral("folder"), Qt::UserRole + 1);
                folderItem->setToolTip(fr.key);

                struct TemplateRow {
                    int id;
                    QString code;
                    QString title;
                };
                QList<TemplateRow> templates;

                {
                    QSqlQuery tq(db);
                    tq.prepare(QStringLiteral(
                        "SELECT id, code, title"
                        " FROM templates WHERE folder_id = ?"
                        " ORDER BY sort_order, id"));
                    tq.addBindValue(fr.id);
                    if (!tq.exec()) {
                        qWarning() << "[TemplateCatalog] templates 查询失败:"
                                   << tq.lastError().text();
                        delete folderItem;
                        continue;
                    }

                    while (tq.next()) {
                        TemplateRow tr;
                        tr.id = tq.value(0).toInt();
                        tr.code = tq.value(1).toString();
                        tr.title = tq.value(2).toString();
                        templates.append(tr);
                    }
                }

                for (const auto& tr : templates) {
                    auto* jsonItem = new QStandardItem(
                        tr.title.isEmpty() ? tr.code : tr.title);
                    jsonItem->setEditable(false);
                    jsonItem->setData(QStringLiteral("json"), Qt::UserRole + 1);
                    jsonItem->setToolTip(tr.code);

                    {
                        QSqlQuery sq(db);
                        sq.prepare(QStringLiteral(
                            "SELECT id, page, name, title FROM template_sheets"
                            " WHERE template_id = ? ORDER BY sort_order, id"));
                        sq.addBindValue(tr.id);
                        if (sq.exec()) {
                            while (sq.next()) {
                                TemplateFileInfo info;
                                info.collectionCode = collectionCode;
                                info.folderKey = fr.key;
                                info.folderDisplay = fr.display;
                                info.dbPath = dbPath; // ★ 这里就是 .dlt 的文件路径
                                info.sheetId = sq.value(0).toInt();
                                info.sheetPage = sq.value(1).toInt();
                                info.sheetName = sq.value(2).toString();
                                info.sheetTitle = sq.value(3).toString();

                                const int leafIndex = m_leaves.size();
                                m_leaves.append(info);

                                auto* sheetItem = new QStandardItem(info.displayName());
                                sheetItem->setEditable(false);
                                sheetItem->setData(QStringLiteral("sheet"), Qt::UserRole + 1);
                                sheetItem->setData(info.sheetPage, Qt::UserRole + 2);
                                sheetItem->setData(info.sheetName, Qt::UserRole + 3);
                                sheetItem->setData(info.sheetTitle, Qt::UserRole + 4);
                                sheetItem->setData(leafIndex, Qt::UserRole + 5);
                                sheetItem->setToolTip(
                                    QStringLiteral("编号：%1\n序号：%2\n名称：%3")
                                        .arg(info.sheetName)
                                        .arg(info.sheetPage)
                                        .arg(info.sheetTitle));
                                jsonItem->appendRow(sheetItem);
                            }
                        }
                    }

                    if (jsonItem->rowCount() > 0)
                        folderItem->appendRow(jsonItem);
                    else
                        delete jsonItem;
                }

                if (folderItem->rowCount() > 0)
                    collectionItem->appendRow(folderItem);
                else
                    delete folderItem;
            }

            // ★★★ 关键：在这里调用工序加载 ★★★
            loadProcessRules(db);
        }

        releaseConnection();
        return collectionItem;
    }

    void TemplateCatalog::loadProcessRules(QSqlDatabase& db)
    {
        // 注意：这里不能清空，否则加载第二个集合时会把前面集合的工序规则全丢掉。
        // 整体清空由 load() 负责。
        QSqlQuery q(db);
        // 联表查询，利用 LEFT JOIN 把真实的 sheet_id 找出来
        q.prepare("SELECT DISTINCT p.process_name, p.sheet_code, p.sheet_title, t.id "
            "FROM process_rules p "
            "LEFT JOIN template_sheets t ON p.sheet_code = t.name");

        if (!q.exec()) {
            qWarning() << "[TemplateCatalog] process_rules 查询失败:" << q.lastError().text();
            return;
        }

        int count = 0;
        while (q.next()) {
            ProcessRuleInfo info;
            info.processName = q.value(0).toString();
            info.sheetCode = q.value(1).toString();
            info.sheetTitle = q.value(2).toString();
            info.sheetId = q.value(3).isNull() ? -1 : q.value(3).toInt();
            m_processRules[info.processName].append(info);
            count++;
        }
        qDebug() << "[TemplateCatalog] 成功加载工序规则条数:" << count
            << " 涉及的工序种类数:" << m_processRules.size();
    }

    QStringList TemplateCatalog::getAllProcessNames() const
    {
        return m_processRules.keys();
    }

    QList<ProcessRuleInfo> TemplateCatalog::getRulesByProcess(const QString& processName) const
    {
        return m_processRules.value(processName);
    }

    const TemplateFileInfo* TemplateCatalog::findSheetByNameAndCode(const QString& title, const QString& code) const
    {
        for (const auto& f : m_leaves) {
            if (f.sheetTitle == title && f.sheetName == code) {
                return &f;
            }
        }
        return nullptr;
    }

} // namespace doclife::ui