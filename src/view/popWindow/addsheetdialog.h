#pragma once

#include <QDialog>
#include <QList>
#include <components/dialogs_flyouts/Dialog.h>
#include "business/projectcontext.h"

namespace fluent::textfields { class Label; }
namespace fluent::basicinput { class Button; }
namespace fluent::collections { class TreeView; }

namespace doclife::ui {

    class AddSheetDialog : public fluent::dialogs_flyouts::Dialog
    {
        Q_OBJECT
    public:
        explicit AddSheetDialog(QWidget* parent = nullptr);
        ~AddSheetDialog() override = default;

        QList<doclife::core::SheetNodeData> selectedSheets() const;

    private:
        void updateOkButtonState();

        fluent::collections::TreeView* m_tree = nullptr;
        fluent::basicinput::Button* m_okBtn = nullptr;
    };

} // namespace doclife::ui