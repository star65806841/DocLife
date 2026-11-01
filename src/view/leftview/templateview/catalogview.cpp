#include "catalogview.h"
#include "templatecatalog.h"
#include "uisignalmanager.h"

#include <QVBoxLayout>
#include <QMessageBox>
#include <QFile>
#include <components/collections/TreeView.h>
#include <leftview/projecttreeview/fluenttreeitemdelegate.h>

namespace doclife::ui {

    void activateTemplateFile(const QString& filePath, const QString& suffix)
    {
        if (filePath.isEmpty()) return;
        auto* sig = UISignalManager::instance();
        if (suffix == QLatin1String("pdf")) {
            sig->pdfPreviewRequested(filePath, 0);
        }
        else if (suffix == QLatin1String("xlsx")
            || suffix == QLatin1String("xls")) {
            sig->excelSheetActivated(filePath, QString());
        }
    }

    static void activateSheet(const doclife::ui::TemplateFileInfo& info)
    {
        auto* sig = UISignalManager::instance();

        // ★ 唯一路径：只认 DB 快速渲染，废弃 PDF/Excel 兜底
        if (!info.dbPath.isEmpty() && info.sheetId >= 0 && QFile::exists(info.dbPath)) {
            sig->templateSheetActivated(info.dbPath, info.sheetId, info.displayName());
            return;
        }

        QMessageBox::warning(nullptr, QStringLiteral("提示"),
            QStringLiteral("该模板的数据库未找到或已损坏，无法预览。\n\n路径：%1")
            .arg(info.dbPath));
    }

    CatalogView::CatalogView(QWidget* parent)
        : QWidget(parent)
    {
        TemplateCatalog::instance()->load();

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_tree = new fluent::collections::TreeView(this);
        auto* delegate = new FluentTreeItemDelegate(this);
        m_tree->setItemDelegate(delegate);
        m_tree->setModel(TemplateCatalog::instance()->treeModel());
        m_tree->setHeaderHidden(true);
        m_tree->setUniformRowHeights(true);
        m_tree->setSelectionMode(fluent::collections::SelectionMode::Single);

        // 保留像素级滚动
        m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

        m_tree->expandAll();

        layout->addWidget(m_tree, 1);

        connect(m_tree, &QTreeView::clicked,
            this, &CatalogView::onItemActivated);
    }

    void CatalogView::onItemActivated(const QModelIndex& index)
    {
        if (!index.isValid()) return;
        if (index.data(Qt::UserRole + 1).toString() != QLatin1String("sheet"))
            return;

        const int leafIndex = index.data(Qt::UserRole + 5).toInt();
        const auto& leaves = TemplateCatalog::instance()->leafFiles();
        if (leafIndex < 0 || leafIndex >= leaves.size()) return;

        activateSheet(leaves[leafIndex]);
    }

} // namespace doclife::ui