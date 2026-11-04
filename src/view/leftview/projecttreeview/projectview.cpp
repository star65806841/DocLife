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

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(leftTop);
        layout->addWidget(new fluent::layout::Divider(this));
        layout->addWidget(projectTree,1);
    }
}

