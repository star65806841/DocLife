#include "projectwizarddialog.h"
#include "templatecatalog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QDebug>
#include <QDateTime>
#include <QListView>

#include <FluentQt/TextFields.h>
#include <FluentQt/Collections.h> 
#include <components/textfields/Label.h>
#include <components/basicinput/Button.h>
#include <components/dialogs_flyouts/Dialog.h>

// ★ 确保引入公共代理（根据你项目实际路径调整）
#include "src/view/delegate/applistdelegate.h" 

using namespace fluent::textfields;

namespace doclife::ui {

    ProjectWizardDialog::ProjectWizardDialog(const QString& parentNodeId, QWidget* parent)
        : fluent::dialogs_flyouts::Dialog(parent)
        , m_parentNodeId(parentNodeId)
    {
        this->setMinimumSize(550, 650);
        this->setSmokeEnabled(true);
        this->setAnimationEnabled(true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(55, 25, 45, 25);
        layout->setSpacing(18);

        auto* titleLabel = new fluent::textfields::Label("一键生成质检评定包");
        titleLabel->setFluentTypography(Typography::FontRole::Subtitle);
        layout->addWidget(titleLabel);

        auto* nameLabel = new fluent::textfields::Label("分项工程实例名称");
        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setPlaceholderText("例如：K12+300 明洞浇筑");
        m_nameEdit->setFixedHeight(32);
        m_nameEdit->setStyleSheet("QLineEdit { border: 1px solid #C0C0C0; border-bottom: 2px solid #0078D4; border-radius: 4px; padding: 0 8px; background: transparent; color: palette(text); }");
        layout->addWidget(nameLabel);
        layout->addWidget(m_nameEdit);

        auto* processLabel = new fluent::textfields::Label("选择工序（带出推荐资料包）");
        m_processBox = new fluent::textfields::AutoSuggestBox(this);
        m_processBox->setPlaceholderText("输入工序名，如：明洞浇筑");
        m_processBox->setSuggestions(TemplateCatalog::instance()->getAllProcessNames());
        layout->addWidget(processLabel);
        layout->addWidget(m_processBox);

        auto* listLabel = new fluent::textfields::Label("推荐表格清单（请勾选所需生成的表格）");
        layout->addWidget(listLabel);

        QListView* listView = new QListView(this);
        listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        listView->setStyleSheet("QListView { background-color: #FFFFFF; border: none; outline: none; selection-background-color: transparent; } QListView::item { background: transparent; }");

        auto* delegate = new AppListDelegate(listView);
        delegate->setCheckBoxVisible(true);
        listView->setItemDelegate(delegate);

        m_listModel = new QStandardItemModel(this);
        listView->setModel(m_listModel);

        layout->addWidget(listView, 1);

        auto* commandRow = new QHBoxLayout;
        commandRow->setContentsMargins(0, 0, 0, 0);
        commandRow->setSpacing(8);
        commandRow->addStretch(1);

        m_okBtn = new fluent::basicinput::Button("确 认");
        m_okBtn->setMinimumWidth(120);
        m_okBtn->setFluentStyle(fluent::basicinput::Button::Accent);
        m_okBtn->setEnabled(false);

        auto* cancelBtn = new fluent::basicinput::Button("取 消");
        cancelBtn->setMinimumWidth(120);
        cancelBtn->setFluentStyle(fluent::basicinput::Button::Standard);

        commandRow->addWidget(m_okBtn);
        commandRow->addWidget(cancelBtn);
        layout->addLayout(commandRow);

        connect(cancelBtn, &fluent::basicinput::Button::clicked, this, &QDialog::reject);
        connect(m_okBtn, &fluent::basicinput::Button::clicked, this, &QDialog::accept);
        connect(m_nameEdit, &QLineEdit::textChanged, this, &ProjectWizardDialog::updateOkButtonState);

        connect(m_processBox, &AutoSuggestBox::textChangedWithReason, this,
            [this](const QString& text, AutoSuggestBox::TextChangeReason reason) {
                if (reason == AutoSuggestBox::TextChangeReason::UserInput) {
                    updateSuggestions(text);
                }
            });

        connect(m_processBox, &AutoSuggestBox::suggestionChosen, this, [this](const QVariant& item) {
            onProcessChanged(item.toString());
            });

        connect(m_listModel, &QStandardItemModel::itemChanged, this, &ProjectWizardDialog::updateOkButtonState);
    }

    // ★ 缺失的核心函数 1：获取实例名称
    QString ProjectWizardDialog::itemName() const
    {
        return m_nameEdit->text().trimmed();
    }

    void ProjectWizardDialog::updateSuggestions(const QString& text)
    {
        QString input = text.trimmed();
        QStringList all = TemplateCatalog::instance()->getAllProcessNames();
        if (input.isEmpty()) {
            m_processBox->setSuggestions(all);
            return;
        }

        QStringList startsWith, contains;
        for (const QString& s : qAsConst(all)) {
            if (s.contains(input, Qt::CaseInsensitive)) {
                if (s.startsWith(input, Qt::CaseInsensitive)) startsWith << s;
                else contains << s;
            }
        }
        startsWith.append(contains);
        m_processBox->setSuggestions(startsWith);
    }

    void ProjectWizardDialog::onProcessChanged(const QString& processName)
    {
        m_listModel->clear();
        if (processName.isEmpty()) return;

        if (m_nameEdit->text().isEmpty()) {
            m_nameEdit->setText(processName + " " + QDateTime::currentDateTime().toString("MMdd_hhmm"));
        }

        const auto rules = TemplateCatalog::instance()->getRulesByProcess(processName);
        for (const auto& r : rules) {
            auto* item = new QStandardItem(QStringLiteral("%1 (%2)").arg(r.sheetTitle, r.sheetCode));
            item->setCheckable(true);
            item->setEditable(false);

            QVariantMap data;
            data["displayName"] = r.sheetTitle;
            data["templateName"] = r.sheetTitle;
            data["sheetId"] = r.sheetId;

            const TemplateFileInfo* tInfo = TemplateCatalog::instance()->findSheetByNameAndCode(r.sheetTitle, r.sheetCode);
            data["dbPath"] = (tInfo && r.sheetId >= 0) ? tInfo->dbPath : "";
            item->setData(data, Qt::UserRole);

            if (r.sheetId < 0 || !tInfo) {
                item->setEnabled(false);
                item->setText(item->text() + " [系统缺失]");
                item->setCheckState(Qt::Unchecked);
            }
            else {
                item->setCheckState(Qt::Checked); // 默认勾选
            }
            m_listModel->appendRow(item);
        }
        updateOkButtonState();
    }

    void ProjectWizardDialog::updateOkButtonState()
    {
        bool hasName = !m_nameEdit->text().trimmed().isEmpty();
        bool hasChecked = false;
        for (int i = 0; i < m_listModel->rowCount(); ++i) {
            auto* item = m_listModel->item(i);
            if (item->checkState() == Qt::Checked && !item->text().contains("[系统缺失]")) {
                hasChecked = true;
                break;
            }
        }
        m_okBtn->setEnabled(hasName && hasChecked);
    }

    // ★ 缺失的核心函数 2：收集用户勾选的表格
    QList<doclife::core::SheetNodeData> ProjectWizardDialog::selectedSheets() const
    {
        QList<doclife::core::SheetNodeData> result;
        for (int i = 0; i < m_listModel->rowCount(); ++i) {
            auto* item = m_listModel->item(i);
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
        return result;
    }

} // namespace doclife::ui