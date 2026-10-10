#include "projectnew.h"

#include <components/textfields/Label.h>
#include <components/textfields/LineEdit.h>
#include <components/dialogs_flyouts/ContentDialog.h>
#include <uisignalmanager.h>

using namespace doclife::ui;

ProjectNew::ProjectNew(QWidget *parent)
    : QWidget{parent}
{
    init();
}

void ProjectNew::show(QWidget *parent)
{
    if(!m_dialog)
    {
        return;
    }
    m_dialog->setParent(parent);
    m_dialog->open();
}

void ProjectNew::sendNewProjectName()
{
    qDebug()<<"sendNewProjectName:"<< m_newProjectName->text();
    emit UISignalManager::instance()->popProjectNewName(m_newProjectName->text());
}

void ProjectNew::init()
{
    auto *contentWid = new QWidget();
    auto *h = new QHBoxLayout(contentWid);
    h->setSpacing(10);
    h->setContentsMargins(10,0,0,20);
    auto* label = new fluent::textfields::Label("工 程 名 称：");
    label->setMinimumWidth(100);
    label->setFluentTypography(Typography::FontRole::BodyLargeStrong);
    m_newProjectName = new fluent::textfields::LineEdit();
    m_newProjectName->setPlaceholderText("xx*xx高速公路项目G2标段");
    m_newProjectName->setContentMargins(QMargins(10, 4, 10, 4));
    m_newProjectName->setFocusedBorderWidth(3);
    m_newProjectName->setUnfocusedBorderWidth(2);

    h->addWidget(label);
    h->addWidget(m_newProjectName);

    m_dialog = new fluent::dialogs_flyouts::ContentDialog();
    m_dialog->setFixedSize(QSize(470,300));
    m_dialog->setTitle("新建工程");
    m_dialog->setContent(contentWid);
    m_dialog->setPrimaryButtonText("确定");
    m_dialog->setCloseButtonText("取消");
    m_dialog->setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Primary);

    connect(m_dialog, &QDialog::finished, m_dialog, &QObject::deleteLater);
    connect(m_dialog, &QDialog::destroyed, this, &QObject::deleteLater);
    connect(m_dialog, &fluent::dialogs_flyouts::ContentDialog::primaryButtonClicked, this, &ProjectNew::sendNewProjectName);
}
