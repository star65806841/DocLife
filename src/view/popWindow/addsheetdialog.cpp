#include "addsheetdialog.h"
#include "templatecatalog.h"
#include "view/delegate/applistdelegate.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QLabel>
#include <QListView>
#include <QTabWidget>

#include <FluentQt/TextFields.h>
#include <components/basicinput/Button.h>
#include <components/collections/TreeView.h> // ★ 引入 Fluent 组件

#include "leftview/projecttreeview/fluenttreeitemdelegate.h"

using namespace fluent::textfields;

namespace doclife::ui {

    AddSheetDialog::AddSheetDialog(QWidget* parent)
        : fluent::dialogs_flyouts::Dialog(parent)
    {
        this->setMinimumSize(600, 700);
        this->setSmokeEnabled(true);
        this->setAnimationEnabled(true);
        this->setModal(true);
        this->setDim(true);
        this->setClosePolicy(Dialog::CloseOnEscape);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(55, 25, 45, 25);
        layout->setSpacing(18);

        auto* titleLabel = new QLabel("新建表格");
        titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #333;");
        layout->addWidget(titleLabel);

        m_tabWidget = new QTabWidget(this);
        m_tabWidget->setStyleSheet(R"(
        QTabWidget::pane { border: 1px solid #E0E0E0; border-radius: 4px; background: white; }
        QTabBar::tab { padding: 8px 16px; font-size: 14px; background: transparent; border: none; }
        QTabBar::tab:selected { border-bottom: 2px solid #0078D4; color: #0078D4; font-weight: bold; }
    )");

        // ==========================================
        // 页 1：智能搜索带出表格/工序
        // ==========================================
        auto* page1 = new QWidget();
        auto* p1Layout = new QVBoxLayout(page1);
        p1Layout->setContentsMargins(12, 12, 12, 12);
        p1Layout->setSpacing(12);

        m_allSuggestions.clear();
        for (const auto& f : TemplateCatalog::instance()->leafFiles()) {
            m_allSuggestions << QStringLiteral("[表格] %1 | %2").arg(f.sheetTitle, f.sheetName);
        }
        for (const QString& proc : TemplateCatalog::instance()->getAllProcessNames()) {
            m_allSuggestions << QStringLiteral("[工序] %1").arg(proc);
        }

        m_searchBox = new AutoSuggestBox(page1);
        m_searchBox->setPlaceholderText("输入表格名称或工序（如：明洞浇筑）...");
        m_searchBox->setSuggestions(m_allSuggestions);
        p1Layout->addWidget(m_searchBox);

        connect(m_searchBox, &AutoSuggestBox::textChangedWithReason, this,
            [this](const QString& text, AutoSuggestBox::TextChangeReason reason) {
                if (reason == AutoSuggestBox::TextChangeReason::UserInput) {
                    doFilter(text);
                }
            });

        connect(m_searchBox, &AutoSuggestBox::suggestionChosen, this, [this](const QVariant& item) {
            onSuggestionChosen(item);
            });

        m_processListView = new QListView(page1);
        m_processListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_processListView->setStyleSheet("QListView { background: transparent; border: none; outline: none; }");
        auto* listDelegate = new AppListDelegate(m_processListView);
        listDelegate->setCheckBoxVisible(true);
        m_processListView->setItemDelegate(listDelegate);

        m_processListModel = new QStandardItemModel(this);
        m_processListView->setModel(m_processListModel);

        connect(m_processListView, &QListView::clicked, this, [this](const QModelIndex& index) {
            auto* item = m_processListModel->itemFromIndex(index);
            if (item && item->isEnabled() && (item->flags() & Qt::ItemIsUserCheckable)) {
                item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            }
            });

        p1Layout->addWidget(m_processListView, 1);
        m_tabWidget->addTab(page1, "搜索并勾选");

        // ==========================================
        // 页 2：自由在库中选表（Fluent TreeView）
        // ==========================================
        auto* page2 = new QWidget();
        auto* p2Layout = new QVBoxLayout(page2);
        p2Layout->setContentsMargins(12, 12, 12, 12);

        // ★ 替换为 fluent::collections::TreeView
        m_treeView = new fluent::collections::TreeView(page2);
        m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_treeView->setHeaderHidden(true);
        m_treeView->setBackgroundVisible(false); // 透明背景
        m_treeView->setSelectionMode(fluent::collections::SelectionMode::Single);

        auto* treeDelegate = new FluentTreeItemDelegate(m_treeView);
        treeDelegate->setCheckBoxVisible(true);
        m_treeView->setItemDelegate(treeDelegate);

        // 设置模型
        auto* treeModel = TemplateCatalog::instance()->treeModel();
        m_treeView->setModel(treeModel);

        // ★ 只展开第一级
        for (int i = 0; i < treeModel->rowCount(); ++i) {
            m_treeView->expand(treeModel->index(i, 0));
        }

        p2Layout->addWidget(m_treeView, 1);
        m_tabWidget->addTab(page2, "按库浏览");

        layout->addWidget(m_tabWidget, 1);

        // 底部按钮
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

        // 监听数据变化刷新按钮
        connect(m_processListModel, &QStandardItemModel::itemChanged, this, &AddSheetDialog::updateOkButtonState);
        connect(treeModel, &QStandardItemModel::itemChanged, this, &AddSheetDialog::updateOkButtonState);

        // 切换页签时，重新校验按钮状态
        connect(m_tabWidget, &QTabWidget::currentChanged, this, &AddSheetDialog::updateOkButtonState);
        connect(this, &QDialog::finished, this, &QObject::deleteLater);
    }

    AddSheetDialog::~AddSheetDialog()
    {
        qDebug()<<"~AddSheetDialog";
    }

    void AddSheetDialog::doFilter(const QString& inputText)
    {
        QString input = inputText.trimmed();
        if (input.isEmpty()) {
            m_searchBox->setSuggestions(m_allSuggestions);
            return;
        }

        QStringList startsWithList;
        QStringList containsList;

        for (const QString& s : qAsConst(m_allSuggestions)) {
            QString realText = s.mid(s.indexOf(" ") + 1);
            if (realText.contains(input, Qt::CaseInsensitive)) {
                if (realText.startsWith(input, Qt::CaseInsensitive)) {
                    startsWithList << s;
                }
                else {
                    containsList << s;
                }
            }
        }

        startsWithList.append(containsList);
        if (startsWithList.size() > 50) startsWithList = startsWithList.mid(0, 50);
        m_searchBox->setSuggestions(startsWithList);
    }

    void AddSheetDialog::onSuggestionChosen(const QVariant& item)
    {
        const QString text = item.toString();
        if (text.startsWith("[表格] ")) {
            QStringList parts = text.mid(5).split(" | ");
            if (parts.size() == 2) {
                showSingleSheet(parts[0], parts[1]);
            }
        }
        else if (text.startsWith("[工序] ")) {
            showProcessList(text.mid(5));
        }
    }

    void AddSheetDialog::showSingleSheet(const QString& title, const QString& code)
    {
        m_processListModel->clear();

        const TemplateFileInfo* info = TemplateCatalog::instance()->findSheetByNameAndCode(title, code);
        if (!info) return;

        auto* item = new QStandardItem(QStringLiteral("%1 (%2)").arg(info->sheetTitle, info->sheetName));
        item->setCheckable(true);
        item->setEditable(false);
        item->setCheckState(Qt::Checked);

        QVariantMap data;
        data["displayName"] = info->sheetTitle;
        data["templateName"] = info->sheetTitle;
        data["sheetId"] = info->sheetId;
        data["dbPath"] = info->dbPath;
        item->setData(data, Qt::UserRole);

        m_processListModel->appendRow(item);
        updateOkButtonState();
    }

    void AddSheetDialog::showProcessList(const QString& processName)
    {
        m_processListModel->clear();

        const auto rules = TemplateCatalog::instance()->getRulesByProcess(processName);
        for (const auto& r : rules) {
            auto* item = new QStandardItem(QStringLiteral("%1 (%2)").arg(r.sheetTitle, r.sheetCode));
            item->setCheckable(true);
            item->setEditable(false);
            item->setCheckState(Qt::Checked);

            QVariantMap data;
            data["displayName"] = r.sheetTitle;
            data["templateName"] = r.sheetTitle;
            data["sheetId"] = r.sheetId;

            QString dbPath;
            if (r.sheetId >= 0) {
                for (const auto& f : TemplateCatalog::instance()->leafFiles()) {
                    if (f.sheetId == r.sheetId) {
                        dbPath = f.dbPath;
                        break;
                    }
                }
            }
            data["dbPath"] = dbPath;
            item->setData(data, Qt::UserRole);

            if (r.sheetId < 0 || dbPath.isEmpty()) {
                item->setEnabled(false);
                item->setText(item->text() + " [系统缺失]");
                item->setCheckState(Qt::Unchecked);
            }
            m_processListModel->appendRow(item);
        }
        updateOkButtonState();
    }

    void AddSheetDialog::updateOkButtonState()
    {
        bool hasChecked = false;

        if (m_tabWidget->currentIndex() == 0) {
            for (int i = 0; i < m_processListModel->rowCount(); ++i) {
                auto* item = m_processListModel->item(i);
                if (item->checkState() == Qt::Checked && !item->text().contains("[系统缺失]")) {
                    hasChecked = true;
                    break;
                }
            }
        }
        else {
            auto* model = TemplateCatalog::instance()->treeModel();
            std::function<bool(const QStandardItem*)> checkTree = [&](const QStandardItem* parent) -> bool {
                for (int i = 0; i < parent->rowCount(); ++i) {
                    auto* child = parent->child(i);
                    if (child->data(Qt::UserRole + 1).toString() == "sheet" && child->checkState() == Qt::Checked)
                        return true;
                    if (checkTree(child)) return true;
                }
                return false;
                };
            for (int i = 0; i < model->rowCount(); ++i) {
                if (model->item(i, 0)->checkState() == Qt::Checked) { hasChecked = true; break; }
                if (checkTree(model->item(i, 0))) { hasChecked = true; break; }
            }
        }

        m_okBtn->setEnabled(hasChecked);
    }

    QList<doclife::core::SheetNodeData> AddSheetDialog::selectedSheets() const
    {
        QList<doclife::core::SheetNodeData> result;

        if (m_tabWidget->currentIndex() == 0) {
            for (int i = 0; i < m_processListModel->rowCount(); ++i) {
                auto* item = m_processListModel->item(i);
                if (item->checkState() == Qt::Checked && !item->text().contains("[系统缺失]")) {
                    QVariantMap data = item->data(Qt::UserRole).toMap();
                    doclife::core::SheetNodeData s;
                    s.displayName = data["displayName"].toString();
                    s.templateName = data["templateName"].toString();
                    s.sheetId = data["sheetId"].toInt();
                    s.dbPath = data["dbPath"].toString();
                    result.append(s);
                }
            }
        }
        else {
            auto* model = TemplateCatalog::instance()->treeModel();
            std::function<void(const QStandardItem*)> collect = [&](const QStandardItem* parent) {
                for (int i = 0; i < parent->rowCount(); ++i) {
                    auto* child = parent->child(i);
                    if (child->data(Qt::UserRole + 1).toString() == "sheet" && child->checkState() == Qt::Checked) {
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
        }

        return result;
    }

} // namespace doclife::ui