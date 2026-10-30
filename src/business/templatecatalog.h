#pragma once

#include <QObject>
#include <QString>
#include <QList>

class QStandardItemModel;
class QStandardItem;

namespace doclife::ui {

    // 树中每个叶子节点对应的数据对象
    struct TemplateFileInfo {
        QString sheetTitle;     // 显示名（json 的 sheets[i].title）
        QString sheetName;      // 编号（json 的 sheets[i].name）
        int     sheetPage = -1; // 序号（json 的 sheets[i].page）

        QString jsonPath;       // 绝对路径
        QString pdfPath;        // 绝对路径，可能为空
        QString xlsxPath;       // 绝对路径，可能为空
        QString dbPath;         // 所属 collection 的 catalog.db
        int     sheetId = -1;   // template_sheets.id

        QString collectionCode;
        QString folderKey;
        QString folderDisplay;

        QString displayName() const {
            return sheetTitle.isEmpty() ? sheetName : sheetTitle;
        }
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

    signals:
        void reloaded();

    private:
        explicit TemplateCatalog(QObject* parent = nullptr);

        // 从某个集合的 catalog.db 构建子树
        QStandardItem* buildCollectionFromDb(const QString& collectionRoot,
            const QString& collectionCode);

        QStandardItemModel* m_treeModel = nullptr;
        QList<TemplateFileInfo> m_leaves;
        bool                    m_loaded = false;
    };

} // namespace doclife::ui