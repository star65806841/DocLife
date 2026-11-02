#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>

class QStandardItemModel;
class QStandardItem;
class QSqlDatabase;

namespace doclife::ui {

    // 树中每个叶子节点对应的数据对象
    struct TemplateFileInfo {
        // 来自 json 的字段
        QString sheetTitle;
        QString sheetName;
        int     sheetPage = -1;

        QString dbPath;
        int     sheetId = -1;

        QString collectionCode;
        QString folderKey;
        QString folderDisplay;

        QString displayName() const {
            return sheetTitle.isEmpty() ? sheetName : sheetTitle;
        }
    };
    // 工序的数据结构
    struct ProcessRuleInfo {
        QString processName;    // 工序名，如："明洞浇筑"
        QString sheetCode;      // 表号，如："ZJ10.3.2"
        QString sheetTitle;     // 表名，如："明洞浇筑现场质量检验表"
        int sheetId = -1;       // 关联 catalog.db 里的真实 ID
    };

    class TemplateCatalog : public QObject
    {
        Q_OBJECT
    public:
        static TemplateCatalog* instance();

        void load();
        bool isLoaded() const { return m_loaded; }

        QStandardItemModel* treeModel() const { return m_treeModel; }
        const QList<TemplateFileInfo>& leafFiles() const { return m_leaves; }


        // ★ 工序相关接口
        QStringList getAllProcessNames() const;
        QList<ProcessRuleInfo> getRulesByProcess(const QString& processName) const;
        const TemplateFileInfo* findSheetByNameAndCode(const QString& title, const QString& code) const;


    signals:
        void reloaded();

    private:
        explicit TemplateCatalog(QObject* parent = nullptr);
        void loadProcessRules(QSqlDatabase& db);
        // 工序名 -> 规则列表
        QHash<QString, QList<ProcessRuleInfo>> m_processRules;
        // 从某个集合的 catalog.db 构建子树
        QStandardItem* buildCollectionFromDb(const QString& dbPath,
                                             const QString& collectionCode);

        QStandardItemModel* m_treeModel = nullptr;
        QList<TemplateFileInfo> m_leaves;
        bool                    m_loaded = false;
    };

} // namespace doclife::ui