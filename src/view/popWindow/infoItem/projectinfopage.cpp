#include "projectinfopage.h"

#include <QVBoxLayout>

#include <components/textfields/Label.h>
#include <components/textfields/LineEdit.h>

using namespace doclife::ui;

ProjectInfoPage::ProjectInfoPage(const QString& title,const QString& content, QWidget *parent)
    : QWidget{parent}
{
    createPage(title,content);
}

void ProjectInfoPage::createPage(const QString& title,const QString& content)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(35, 40, 32, 30);
    layout->setSpacing(20);

    auto* titleLabel = new fluent::textfields::Label(title);
    titleLabel->setFluentTypography(Typography::FontRole::BodyLargeStrong);

    auto* descEdit = new fluent::textfields::LineEdit();
    descEdit->setContentMargins(QMargins(10, 4, 10, 4));
    descEdit->setFocusedBorderWidth(3);
    descEdit->setUnfocusedBorderWidth(2);
    descEdit->setFontRole(Typography::FontRole::BodyStrong);
    descEdit->setText(content);

    layout->addWidget(titleLabel);
    layout->addWidget(descEdit);
    layout->addStretch();

    connect(descEdit,&fluent::textfields::LineEdit::textChanged,this,&ProjectInfoPage::sign_text);
    // connect(this,&ProjectInfoPage::destroyed,[](){qDebug()<<"ProjectInfoPage destroyed";});// test ok
    // connect(descEdit,&fluent::textfields::LineEdit::textChanged,[](){qDebug()<<"descEdit textChanged";});
}
