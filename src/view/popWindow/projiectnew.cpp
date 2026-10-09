#include "projiectnew.h"

#include <components/dialogs_flyouts/ContentDialog.h>

#include <components/textfields/LineEdit.h>

using namespace doclife::ui;

ProjiectNew::ProjiectNew(QWidget *parent)
    : QWidget{parent}
{
    init();
}

void ProjiectNew::show(QWidget *parent)
{
    if(!m_dialog)
    {
        return;
    }
    m_dialog->setParent(parent);
    m_dialog->open();
}

void ProjiectNew::init()
{
    auto* emphasizedEdit = new fluent::textfields::LineEdit();
    emphasizedEdit->setText("Border thickness demo");
    emphasizedEdit->setContentMargins(QMargins(10, 4, 10, 4));
    emphasizedEdit->setFocusedBorderWidth(3);
    emphasizedEdit->setUnfocusedBorderWidth(2);


    m_dialog = new fluent::dialogs_flyouts::ContentDialog();
    m_dialog->setTitle("Share draft?");
    m_dialog->setContent(emphasizedEdit);
    m_dialog->setPrimaryButtonText("Share");
    m_dialog->setCloseButtonText("Not now");
    m_dialog->setDefaultButton(fluent::dialogs_flyouts::ContentDialog::None);
    // connect(dialog, &ContentDialog::primaryButtonClicked, status,
    //         [status] { status->setText("Draft shared"); });
    connect(m_dialog, &QDialog::finished, m_dialog,
            &QObject::deleteLater);
}
