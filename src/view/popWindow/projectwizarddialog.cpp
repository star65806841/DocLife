#include "projectwizarddialog.h"
#include "templatecatalog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListView>
#include <QStandardItemModel>
#include <QDebug>
#include <QDateTime>
#include <QBrush>
#include <QColor>

// FluentQt 组件
#include <FluentQt/TextFields.h>
#include <components/textfields/Label.h>
#include <components/basicinput/Button.h>
#include <components/dialogs_flyouts/Dialog.h>

// 复用现有的精美树节点代理，用来画复选框
#include "leftview/projecttreeview/fluenttreeitemdelegate.h"

using namespace fluent::textfields;

namespace doclife::ui {

    ProjectWizardDialog::ProjectWizardDialog(const QString& parentNodeId, QWidget* parent)
        : fluent::dialogs_flyouts::Dialog(parent)
        , m_parentNodeId(parentNodeId)
    {
        // 1. 基础弹窗设置（参考 ProjectSetting）
        this->setMinimumSize(550, 600);
        this->setSmokeEnabled(true);
        this->setAnimationEnabled(true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(55, 25, 45, 25);
        layout->setSpacing(18);

        // 2. 标题
        auto* titleLabel = new fluent::textfields::Label("一键生成质检评定包");
        titleLabel->setFluentTypography(Typography::FontRole::Subtitle);
        layout->addWidget(titleLabel);

        // 3. 实例名称输入
        auto* nameLabel = new fluent::textfields::Label("分项工程实例名称");
        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setPlaceholderText("例如：K12+300 明洞浇筑");
        m_nameEdit->setFixedHeight(32);
        // 简单模拟 Fluent 的文本框样式（下划线+边框）
        m_nameEdit->setStyleSheet("QLineEdit { border: 1px solid #C0C0C0; border-bottom: 2px solid #0078D4; border-radius: 4px; padding: 0 8px; background: transparent; color: palette(text); }");

        layout->addWidget(nameLabel);
        layout->addWidget(m_nameEdit);

        // 4. 工序选择 (AutoSuggestBox)
        auto* processLabel = new fluent::textfields::Label("选择工序（带出推荐资料包）");
        m_processBox = new fluent::textfields::AutoSuggestBox(this);
        m_processBox->setPlaceholderText("输入工序名，如：明洞浇筑");
        m_processBox->setSuggestions(TemplateCatalog::instance()->getAllProcessNames());

        layout->addWidget(processLabel);
        layout->addWidget(m_processBox);

        // 5. 推荐表格列表
        auto* listLabel = new fluent::textfields::Label("推荐表格清单（评定表已锁定必选）");
        m_listView = new QListView(this);
        m_listView->setStyleSheet("QListView { border: 1px solid #E0E0E0; border-radius: 4px; padding: 4px; background-color: transparent; }");
        m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);

        // ★ 复用左侧树的精美代理来显示复选框
        auto* delegate = new FluentTreeItemDelegate(m_listView);
        delegate->setCheckBoxVisible(true);
        m_listView->setItemDelegate(delegate);

        m_listModel = new QStandardItemModel(this);
        m_listView->setModel(m_listModel);

        layout->addWidget(listLabel);
        layout->addWidget(m_listView, 1);

        // 6. 底部按钮
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

        // 7. 信号连接
        connect(cancelBtn, &fluent::basicinput::Button::clicked, this, &QDialog::reject);
        connect(m_okBtn, &fluent::basicinput::Button::clicked, this, &QDialog::accept);
        connect(m_nameEdit, &QLineEdit::textChanged, this, &ProjectWizardDialog::updateOkButtonState);

        connect(m_processBox, &AutoSuggestBox::suggestionChosen, this, [this](const QVariant& item) {
            onProcessChanged(item.toString());
            });

        connect(m_listModel, &QStandardItemModel::itemChanged, this, &ProjectWizardDialog::updateOkButtonState);
    }

    QString ProjectWizardDialog::itemName() const
    {
        return m_nameEdit->text().trimmed();
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

            bool isPD = r.sheetCode.startsWith("PD", Qt::CaseInsensitive);

            if (r.sheetId < 0 || !tInfo) {
                item->setEnabled(false);
                item->setText(item->text() + " [系统缺失]");
                item->setCheckState(Qt::Unchecked);
            }
            else if (isPD) {
                item->setCheckState(Qt::Checked);
                item->setEnabled(false); // 锁定，不可修改
            }
            else {
                item->setCheckState(Qt::Checked);
            }

            m_listModel->appendRow(item);
        }

        // ★ 关键：工序切换后，强制刷新一次按钮状态
        updateOkButtonState();
    }

    void ProjectWizardDialog::updateOkButtonState()
    {
        bool hasName = !m_nameEdit->text().trimmed().isEmpty();

        bool hasChecked = false;
        for (int i = 0; i < m_listModel->rowCount(); ++i) {
            auto* item = m_listModel->item(i);
            // 只要勾选了，且不是缺失的，就认为有效（忽略 isEnabled，因为 PD 表是禁用的但需要被算作勾选）
            if (item->checkState() == Qt::Checked && !item->text().contains("[系统缺失]")) {
                hasChecked = true;
                break;
            }
        }

        m_okBtn->setEnabled(hasName && hasChecked);
    }

    QList<doclife::core::SheetNodeData> ProjectWizardDialog::selectedSheets() const
    {
        QList<doclife::core::SheetNodeData> result;
        for (int i = 0; i < m_listModel->rowCount(); ++i) {
            auto* item = m_listModel->item(i);
            if (item->checkState() == Qt::Checked && item->isEnabled()) {
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