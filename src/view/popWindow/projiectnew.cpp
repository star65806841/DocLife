#include "projiectnew.h"

#include <components/dialogs_flyouts/ContentDialog.h>

#include <components/textfields/Label.h>
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
    auto *contentWid = new QWidget();
    auto *h = new QHBoxLayout(contentWid);
    h->setSpacing(10);
    h->setContentsMargins(10,0,0,20);
    auto* label = new fluent::textfields::Label("工 程 名 称：");
    label->setMinimumWidth(100);
    label->setFluentTypography(Typography::FontRole::BodyLargeStrong);
    auto* emphasizedEdit = new fluent::textfields::LineEdit();
    emphasizedEdit->setPlaceholderText("xx*xx高速公路项目G2标段");
    emphasizedEdit->setContentMargins(QMargins(10, 4, 10, 4));
    emphasizedEdit->setFocusedBorderWidth(3);
    emphasizedEdit->setUnfocusedBorderWidth(2);

    h->addWidget(label);
    h->addWidget(emphasizedEdit);

    m_dialog = new fluent::dialogs_flyouts::ContentDialog();
    m_dialog->setFixedSize(QSize(470,300));
    // m_dialog->setContentsMargins(5,10,5,5);
    m_dialog->setTitle("新建工程");
    m_dialog->setContent(contentWid);
    m_dialog->setPrimaryButtonText("确定");
    m_dialog->setCloseButtonText("取消");
    m_dialog->setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Primary);

    connect(m_dialog, &QDialog::finished, m_dialog, &QObject::deleteLater);
    connect(m_dialog, &QDialog::destroyed, this, &QObject::deleteLater);
}
