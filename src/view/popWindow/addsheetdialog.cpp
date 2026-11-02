#include "addsheetdialog.h"
#include "templatecatalog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QLabel>

#include <FluentQt/TextFields.h>
#include <FluentQt/Collections.h> 
#include <components/textfields/Label.h>
#include <components/basicinput/Button.h>
#include <components/dialogs_flyouts/Dialog.h>

#include "leftview/projecttreeview/fluenttreeitemdelegate.h"

namespace doclife::ui {

AddSheetDialog::AddSheetDialog(QWidget* parent)
    : fluent::dialogs_flyouts::Dialog(parent)
{
    this->setMinimumSize(600, 650);
    this->setSmokeEnabled(true);
    this->setAnimationEnabled(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(55, 25, 45, 25);
    layout->setSpacing(18);

    auto* titleLabel = new QLabel("自由添加表格资料");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #333;");
    layout->addWidget(titleLabel);

    auto* hintLabel = new QLabel("请在下方勾选需要添加到工程的表格：");
    layout->addWidget(hintLabel);

    // ★ 使用 Fluent 的 TreeView，并修复纯黑背景
    m_tree = new fluent::collections::TreeView(this);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setHeaderHidden(true);
    
    auto* delegate = new FluentTreeItemDelegate(m_tree);
    delegate->setCheckBoxVisible(true);
    m_tree->setItemDelegate(delegate);
    m_tree->setBackgroundVisible(false);
    m_tree->setModel(TemplateCatalog::instance()->treeModel());
    m_tree->expandAll();
    layout->addWidget(m_tree, 1);

    auto* commandRow = new QHBoxLayout;
    m_okBtn = new fluent::basicinput::Button("添加到工程");
    m_okBtn->setMinimumWidth(120);
    m_okBtn->setFluentStyle(fluent::basicinput::Button::Accent);
    m_okBtn->setEnabled(false);

    auto* cancelBtn = new fluent::basicinput::Button("取 消");
    cancelBtn->setMinimumWidth(120);
    cancelBtn->setFluentStyle(fluent::basicinput::Button::Standard);

    commandRow->addStretch(1);
    commandRow->addWidget(m_okBtn);
    commandRow->addWidget(cancelBtn);
    layout->addLayout(commandRow);

    connect(cancelBtn, &fluent::basicinput::Button::clicked, this, &QDialog::reject);
    connect(m_okBtn, &fluent::basicinput::Button::clicked, this, &QDialog::accept);

    connect(TemplateCatalog::instance()->treeModel(), &QStandardItemModel::itemChanged, 
            this, &AddSheetDialog::updateOkButtonState);
}

void AddSheetDialog::updateOkButtonState()
{
    bool hasChecked = false;
    auto* model = TemplateCatalog::instance()->treeModel();
    
    std::function<bool(const QStandardItem*)> check = [&](const QStandardItem* parentItem) -> bool {
        for (int i = 0; i < parentItem->rowCount(); ++i) {
            auto* child = parentItem->child(i);
            if (child->data(Qt::UserRole + 1).toString() == "sheet") {
                if (child->checkState() == Qt::Checked) return true;
            }
            if (check(child)) return true;
        }
        return false;
    };

    for (int i = 0; i < model->rowCount(); ++i) {
        if (model->item(i, 0)->checkState() == Qt::Checked) { hasChecked = true; break; }
        if (check(model->item(i, 0))) { hasChecked = true; break; }
    }
    
    m_okBtn->setEnabled(hasChecked);
}

QList<doclife::core::SheetNodeData> AddSheetDialog::selectedSheets() const
{
    QList<doclife::core::SheetNodeData> result;
    auto* model = TemplateCatalog::instance()->treeModel();

    std::function<void(const QStandardItem*)> collect = [&](const QStandardItem* parentItem) {
        for (int i = 0; i < parentItem->rowCount(); ++i) {
            auto* child = parentItem->child(i);
            if (child->data(Qt::UserRole + 1).toString() == "sheet" 
                && child->checkState() == Qt::Checked) {
                
                int leafIndex = child->data(Qt::UserRole + 5).toInt();
                const auto& leaves = TemplateCatalog::instance()->leafFiles();
                if (leafIndex >= 0 && leafIndex < leaves.size()) {
                    const auto& f = leaves[leafIndex];
                    doclife::core::SheetNodeData s;
                    s.displayName = f.displayName();
                    s.templateName = f.sheetTitle;
                    s.sheetId = f.sheetId;
                    s.dbPath = f.dbPath;
                    result.append(s);
                }
            }
            collect(child);
        }
    };

    for (int i = 0; i < model->rowCount(); ++i) {
        collect(model->item(i, 0));
    }
    return result;
}

} // namespace doclife::ui