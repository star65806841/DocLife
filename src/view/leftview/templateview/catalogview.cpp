#include "catalogview.h"
#include "templatecatalog.h"
#include "uisignalmanager.h"

#include <QVBoxLayout>
#include <QMessageBox>
#include <QFile>
#include <components/collections/TreeView.h>

namespace doclife::ui {

    // 保留：外部通过文件路径激活（PDF / Excel），供未来其他入口复用
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

        qDebug() << "[activateSheet] dbPath=" << info.dbPath
                 << "sheetId=" << info.sheetId
                 << "pdfPath=" << info.pdfPath
                 << "xlsxPath=" << info.xlsxPath;

        // ★ 首选 db
        if (!info.dbPath.isEmpty() && info.sheetId >= 0 && QFile::exists(info.dbPath)) {
            sig->templateSheetActivated(info.dbPath, info.sheetId, info.displayName());
            return;
        }
        if (!info.pdfPath.isEmpty() && QFile::exists(info.pdfPath)) {
            sig->pdfPreviewRequested(info.pdfPath, qMax(0, info.sheetPage));
            return;
        }
        if (!info.xlsxPath.isEmpty() && QFile::exists(info.xlsxPath)) {
            sig->excelSheetActivated(info.xlsxPath, info.sheetName);
            return;
        }
        QMessageBox::information(nullptr, QStringLiteral("提示"),
                                 QStringLiteral("该模板没有可用的数据源。"));
    }

    CatalogView::CatalogView(QWidget* parent)
        : QWidget(parent)
    {
        TemplateCatalog::instance()->load();

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_tree = new fluent::collections::TreeView(this);
        m_tree->setModel(TemplateCatalog::instance()->treeModel());
        m_tree->setHeaderHidden(true);
        m_tree->setUniformRowHeights(true);
        m_tree->setSelectionMode(fluent::collections::SelectionMode::Single);
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