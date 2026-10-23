#include "projectview.h"

#include <components/collections/SplitView.h>
#include <components/layout/Divider.h>

#include "lefttop.h"
#include "leftview/projecttreeview/projecttreeview.h"

namespace doclife::ui {
    ProjectView::ProjectView(QWidget* parent)
        : QWidget{ parent }
    {
        init();
    }
    void ProjectView::init()
    {
        auto* leftTop = new LeftTop(this);
        auto* projectTree = new ProjectTreeView(this);
        // ----------- 上侧区域  -----------
        // leftTop->setStyleSheet("background-color: #a1F1BF;");
        // ----------- 下侧区域  -----------
        // auto* leftDown = new QWidget();
        // leftDown->setStyleSheet("background-color: #a1a1Ba;");
        // auto* splitter = new fluent::collections::SplitView();
        // splitter->setOrientation(Qt::Orientation::Vertical);
        // splitter->addPane(leftTop);
        // splitter->addPane(leftDown, { 100,300,650,false });
        // splitter->setPaneFill(0, true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(leftTop);
        layout->addWidget(new fluent::layout::Divider(this));
        layout->addWidget(projectTree,1);
    }
}

