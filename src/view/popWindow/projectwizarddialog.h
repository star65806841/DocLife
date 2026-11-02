#pragma once

#include <QString>
#include <QList>
#include <components/dialogs_flyouts/Dialog.h>
#include "business/projectcontext.h"

// 前向声明
namespace fluent::textfields { class AutoSuggestBox; class Label; }
namespace fluent::basicinput { class Button; }
class QLineEdit;
class QListView;
class QStandardItemModel;

namespace doclife::ui {

    class ProjectWizardDialog : public fluent::dialogs_flyouts::Dialog
    {
        Q_OBJECT
    public:
        explicit ProjectWizardDialog(const QString& parentNodeId, QWidget* parent = nullptr);
        ~ProjectWizardDialog() override = default;

        // 获取用户填写的实例名称
        QString itemName() const;

        // 获取用户勾选的表格数据
        QList<doclife::core::SheetNodeData> selectedSheets() const;

    private:
        void onProcessChanged(const QString& processName);
        void updateOkButtonState();

        QString m_parentNodeId;

        QLineEdit* m_nameEdit = nullptr;
        fluent::textfields::AutoSuggestBox* m_processBox = nullptr;
        QListView* m_listView = nullptr;
        QStandardItemModel* m_listModel = nullptr;
        fluent::basicinput::Button* m_okBtn = nullptr;
    };

} // namespace doclife::ui