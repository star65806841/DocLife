#include "catalogsearchview.h"
#include "templatecatalog.h"
#include "uisignalmanager.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

namespace doclife::ui {

    CatalogSearchView::CatalogSearchView(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        m_edit = new QLineEdit(this);
        m_edit->setPlaceholderText(QStringLiteral("输入中文名称搜索模板…"));
        m_edit->setClearButtonEnabled(true);

        m_view = new QListView(this);
        m_model = new QStandardItemModel(this);
        m_proxy = new QSortFilterProxyModel(this);
        m_proxy->setSourceModel(m_model);
        m_proxy->setFilterRole(Qt::DisplayRole);
        m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

        m_view->setModel(m_proxy);
        m_view->setUniformItemSizes(true);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);

        layout->addWidget(m_edit);
        layout->addWidget(m_view, 1);

        rebuild();

        connect(m_edit, &QLineEdit::textChanged, this, [this](const QString& text) {
            m_proxy->setFilterFixedString(text.trimmed());
            });

        connect(m_view, &QListView::clicked, this, [this](const QModelIndex& idx) {
            onItemActivated(idx);
            });

        connect(TemplateCatalog::instance(), &TemplateCatalog::reloaded,
            this, &CatalogSearchView::rebuild);
    }

    void CatalogSearchView::rebuild()
    {
        m_model->removeRows(0, m_model->rowCount());

        const auto& leaves = TemplateCatalog::instance()->leafFiles();
        for (int i = 0; i < leaves.size(); ++i) {
            const auto& f = leaves[i];
            auto* item = new QStandardItem(f.displayName());
            item->setEditable(false);
            item->setData(f.jsonPath, Qt::UserRole);
            item->setData(f.sheetPage, Qt::UserRole + 2);
            item->setData(f.sheetName, Qt::UserRole + 3);
            item->setData(f.sheetTitle, Qt::UserRole + 4);
            item->setData(i, Qt::UserRole + 5);
            item->setToolTip(QStringLiteral("%1 · 编号：%2 · 序号：%3")
                .arg(f.folderDisplay, f.sheetName)
                .arg(f.sheetPage));
            m_model->appendRow(item);
        }
    }

    void CatalogSearchView::onItemActivated(const QModelIndex& proxyIndex)
    {
        if (!proxyIndex.isValid()) return;

        const int leafIndex = proxyIndex.data(Qt::UserRole + 5).toInt();
        const auto& leaves = TemplateCatalog::instance()->leafFiles();
        if (leafIndex < 0 || leafIndex >= leaves.size()) return;

        const auto& info = leaves[leafIndex];
        auto* sig = UISignalManager::instance();

        if (!info.pdfPath.isEmpty()) {
            sig->pdfPreviewRequested(info.pdfPath, qMax(0, info.sheetPage));
            return;
        }
        if (!info.xlsxPath.isEmpty()) {
            sig->excelSheetActivated(info.xlsxPath, info.sheetName);
            return;
        }
        QMessageBox::information(
            nullptr, QStringLiteral("提示"),
            QStringLiteral("该模板没有关联的 PDF 或 Excel 文件。"));
    }

} // namespace doclife::ui