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
            const QString root = TemplateInstaller::collectionRoot(code);
            auto* item = buildCollectionFromDb(root, code);
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
        const QString& collectionRoot, const QString& collectionCode)
    {
        const QString dbPath = collectionRoot + QStringLiteral("/catalog.db");
        if (!QFile::exists(dbPath)) {
            qWarning() << "[TemplateCatalog] catalog.db 不存在:" << dbPath;
            return nullptr;
        }

        const QString connName = QStringLiteral("catalog_") + collectionCode;

        // 获取或新建连接（不用 removeDatabase，连接长期保留复用）
        QSqlDatabase db = QSqlDatabase::contains(connName)
                              ? QSqlDatabase::database(connName)
                              : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);

        if (db.isOpen()) db.close();
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            qWarning() << "[TemplateCatalog] db 打开失败:" << db.lastError().text();
            return nullptr;
        }

        // ★ 整个查询阶段用 { } 包起来，保证所有 QSqlQuery 在 db.close 前析构
        QStandardItem* collectionItem = nullptr;
        {
            // ---- catalogs ----
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "SELECT code, display_name FROM catalogs LIMIT 1"));
            if (!q.exec() || !q.next()) {
                qWarning() << "[TemplateCatalog] catalogs 查询失败";
                db.close();
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
                    db.close();
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
            }  // folderQ 析构

            // ---- 每个 folder 的 templates + sheets ----
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
                    QString jsonPath;
                    QString pdfPath;
                    QString xlsxPath;
                };
                QList<TemplateRow> templates;

                {
                    QSqlQuery tq(db);
                    tq.prepare(QStringLiteral(
                        "SELECT id, code, title, json_path, pdf_path, xlsx_path"
                        " FROM templates WHERE folder_id = ?"
                        " ORDER BY sort_order, id"));
                    tq.addBindValue(fr.id);
                    if (!tq.exec()) {
                        qWarning() << "[TemplateCatalog] templates 查询失败:"
                                   << tq.lastError().text();
                        delete folderItem;
                        continue;
                    }

                    auto absPath = [&](const QVariant& v) -> QString {
                        const QString rel = v.toString();
                        return rel.isEmpty() ? QString()
                                             : collectionRoot + QLatin1Char('/') + rel;
                    };

                    while (tq.next()) {
                        TemplateRow tr;
                        tr.id       = tq.value(0).toInt();
                        tr.code     = tq.value(1).toString();
                        tr.title    = tq.value(2).toString();
                        tr.jsonPath = absPath(tq.value(3));
                        tr.pdfPath  = absPath(tq.value(4));
                        tr.xlsxPath = absPath(tq.value(5));
                        templates.append(tr);
                    }
                }  // tq 析构

                for (const auto& tr : templates) {
                    auto* jsonItem = new QStandardItem(
                        tr.title.isEmpty() ? tr.code : tr.title);
                    jsonItem->setEditable(false);
                    jsonItem->setData(tr.jsonPath, Qt::UserRole);
                    jsonItem->setData(QStringLiteral("json"), Qt::UserRole + 1);
                    jsonItem->setToolTip(tr.code);

                    // sheets
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
                                info.folderKey      = fr.key;
                                info.folderDisplay  = fr.display;
                                info.jsonPath       = tr.jsonPath;
                                info.pdfPath        = tr.pdfPath;
                                info.xlsxPath       = tr.xlsxPath;
                                info.dbPath         = dbPath;
                                info.sheetId        = sq.value(0).toInt();
                                info.sheetPage      = sq.value(1).toInt();
                                info.sheetName      = sq.value(2).toString();
                                info.sheetTitle     = sq.value(3).toString();

                                const int leafIndex = m_leaves.size();
                                m_leaves.append(info);

                                auto* sheetItem = new QStandardItem(info.displayName());
                                sheetItem->setEditable(false);
                                sheetItem->setData(info.jsonPath,          Qt::UserRole);
                                sheetItem->setData(QStringLiteral("sheet"), Qt::UserRole + 1);
                                sheetItem->setData(info.sheetPage,         Qt::UserRole + 2);
                                sheetItem->setData(info.sheetName,         Qt::UserRole + 3);
                                sheetItem->setData(info.sheetTitle,        Qt::UserRole + 4);
                                sheetItem->setData(leafIndex,              Qt::UserRole + 5);
                                sheetItem->setToolTip(
                                    QStringLiteral("编号：%1\n序号：%2\n名称：%3")
                                        .arg(info.sheetName)
                                        .arg(info.sheetPage)
                                        .arg(info.sheetTitle));
                                jsonItem->appendRow(sheetItem);
                            }
                        }
                    }  // sq 析构

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
        }  // ★ 所有 QSqlQuery 已析构

        db.close();
        // ★ 不调用 removeDatabase！连接长期保留，下次 load 直接复用
        return collectionItem;
    }

} // namespace doclife::ui