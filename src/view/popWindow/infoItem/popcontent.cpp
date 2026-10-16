#include "duplicateapp.h"

#include <components/textfields/Label.h>

DuplicateApp::DuplicateApp(QWidget *parent)
    : QWidget{parent}
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(40);

    auto* label1 = new fluent::textfields::Label("警告");
    label1->setFluentTypography(Typography::FontRole::Subtitle);
    auto* label2 = new fluent::textfields::Label("应用程序正在运行, 请先关闭后再启动!");
    label2->setFluentTypography(Typography::FontRole::Body);

    layout->addWidget(label1);
    layout->addWidget(label2);
    layout->addStretch();
}
