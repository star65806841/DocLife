#include "projectnew.h"

#include <components/textfields/Label.h>
#include <components/textfields/LineEdit.h>
#include <uisignalmanager.h>

using namespace doclife::ui;

ProjectNew::ProjectNew(QWidget *parent)
    : fluent::dialogs_flyouts::ContentDialog{parent}
{
    init();
}

void ProjectNew::sendNewProject()
{
    if (m_projectNameLine->text().isEmpty())
    {
        m_projectNameLine->setText("自定义文档");
    }
}

void ProjectNew::init()
{
    auto *contentWid = new QWidget();
    // auto *contenLayout = new QVBoxLayout(contentWid);
    // contenLayout->setSpacing(15);
    // =========================================================
    // 内容
    /**********************************************************/
    auto *h1 = new QHBoxLayout(contentWid);
    h1->setSpacing(10);
    h1->setContentsMargins(10,0,0,20);
    auto* label = new fluent::textfields::Label("文档名称：");
    label->setMinimumWidth(100);
    label->setFluentTypography(Typography::FontRole::BodyLargeStrong);
    m_projectNameLine = new fluent::textfields::LineEdit();
    m_projectNameLine->setPlaceholderText("单位工程或分部分项工程名称");
    m_projectNameLine->setContentMargins(QMargins(10, 4, 10, 4));
    m_projectNameLine->setFocusedBorderWidth(3);
    m_projectNameLine->setUnfocusedBorderWidth(2);
    h1->addWidget(label);
    h1->addWidget(m_projectNameLine);
    /**********************************************************/
    /**********************************************************/
    // auto *h2 = new QHBoxLayout(contenLayout);
    // h2->setSpacing(10);
    // h2->setContentsMargins(10,0,0,20);
    // auto* label = new fluent::textfields::Label("工程名称：");
    // label->setMinimumWidth(100);
    // label->setFluentTypography(Typography::FontRole::BodyLargeStrong);
    // m_newProjectName = new fluent::textfields::LineEdit();
    // m_newProjectName->setPlaceholderText("xx*xx高速公路项目G2标段");
    // m_newProjectName->setContentMargins(QMargins(10, 4, 10, 4));
    // m_newProjectName->setFocusedBorderWidth(3);
    // m_newProjectName->setUnfocusedBorderWidth(2);
    /**********************************************************/
    // =========================================================

    // h->addWidget(label);
    // h->addWidget(m_newProjectName);
    this->setFixedSize(QSize(470,300));
    // this->setFixedSize(QSize(580,430));
    this->setTitle("新建文档");
    this->setContent(contentWid);
    this->setPrimaryButtonText("确定");
    this->setCloseButtonText("取消");
    this->setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Primary);
    this->setAnimationEnabled(true);
    this->setSmokeEnabled(true);

    connect(this, &fluent::dialogs_flyouts::ContentDialog::primaryButtonClicked, this, &ProjectNew::sendNewProject);
    connect(this, &QDialog::finished, this, &QObject::deleteLater);
    connect(this, &QDialog::destroyed, []() {qDebug() << "ProjectNew dialog destroyed"; });
}
