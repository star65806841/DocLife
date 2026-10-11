#include "leftview.h"

#include <components/collections/SplitView.h>

namespace doclife::ui {
LeftView::LeftView(QWidget *parent)
    : QWidget{parent}
{
    init();
}
void LeftView::init()
{
    // ----------- 上侧区域  -----------
    auto *leftTop = new QWidget();
    leftTop->setStyleSheet("background-color: #a1F1BF;");
    // ----------- 下侧区域  -----------
    auto *leftDown = new QWidget();
    leftDown->setStyleSheet("background-color: #a1a1Ba;");
    auto *splitter = new fluent::collections::SplitView();
    splitter->setOrientation(Qt::Orientation::Vertical);
    splitter->addPane(leftTop);
    splitter->addPane(leftDown,{100,300,650,false});
    splitter->setPaneFill(0, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(splitter);
}
}

