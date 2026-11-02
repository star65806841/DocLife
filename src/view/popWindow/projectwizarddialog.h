#pragma once

#include <QString>
#include <QList>
#include <components/dialogs_flyouts/Dialog.h>
#include "business/projectcontext.h"

namespace fluent::textfields { class AutoSuggestBox; class Label; }
namespace fluent::basicinput { class Button; }
class QLineEdit;
class QStandardItemModel;

// 引入 Fluent 的列表视图
namespace fluent::collections { class ListView; }

namespace doclife::ui {

    class ProjectWizardDialog : public fluent::dialogs_flyouts::Dialog
    {
        Q_OBJECT
    public:
        explicit ProjectWizardDialog(const QString& parentNodeId, QWidget* parent = nullptr);
        ~ProjectWizardDialog() override = default;

        QString itemName() const;
        QList<doclife::core::SheetNodeData> selectedSheets() const;

    private:
        void onProcessChanged(const QString& processName);
        void updateOkButtonState();
        void updateSuggestions(const QString& text);

        QString m_parentNodeId;

        QLineEdit* m_nameEdit = nullptr;
        fluent::textfields::AutoSuggestBox* m_processBox = nullptr;

        fluent::collections::ListView* m_listView = nullptr;
        QStandardItemModel* m_listModel = nullptr;

        fluent::basicinput::Button* m_okBtn = nullptr;
    };

} // namespace doclife::ui