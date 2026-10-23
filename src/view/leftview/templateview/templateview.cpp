#include "templateview.h"

#include <QVBoxLayout>

#include <components/layout/Divider.h>
#include <components/navigation/SelectorBar.h>
#include <components/navigation/StackContentHost.h>

namespace doclife::ui {
    TemplateView::TemplateView(QWidget* parent)
        : QWidget{ parent }
    {
        init();
    }
    void TemplateView::init()
    {
        auto* selectorBar = new fluent::navigation::SelectorBar(this);
        selectorBar->addItem({ "模板", Typography::Icons::glyph(QStringLiteral("ic_fluent_building_desktop_24_regular")) });
        selectorBar->addItem({ "搜索", Typography::Icons::glyph(QStringLiteral("ic_fluent_document_search_24_regular")) });
        selectorBar->setOverflowBehavior(fluent::navigation::SelectorBar::OverflowBehavior::MoreButton);

        auto* host = new fluent::navigation::StackContentHost(this);
        auto* wid1 = new QWidget();
        wid1->setStyleSheet("background-color: #a1a1Ba;");
        auto* wid2 = new QWidget();
        wid2->setStyleSheet("background-color: #F1F1Ba;");
        host->insertPage(0, wid1);
        host->insertPage(1, wid2);
        host->setCurrentIndex(0, 0, false);
        connect(selectorBar, &fluent::navigation::SelectorBar::currentChanged,
            host, [host](int index) { host->setCurrentIndex(index, 0, true); });
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(selectorBar);
        layout->addWidget(new fluent::layout::Divider(this));
        layout->addWidget(host, 1);
    }
}