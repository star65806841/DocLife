#include "popcontent.h"

#include <components/textfields/Label.h>

PopContent::PopContent(const QString& title, const QString content, QWidget *parent)
    : QWidget{parent}
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(40);

    auto* label1 = new fluent::textfields::Label(title);
    label1->setFluentTypography(Typography::FontRole::Subtitle);
    auto* label2 = new fluent::textfields::Label(content);
    label2->setFluentTypography(Typography::FontRole::Body);

    layout->addWidget(label1);
    layout->addWidget(label2);
    layout->addStretch();
}
