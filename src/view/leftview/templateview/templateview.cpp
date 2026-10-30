#include "templateview.h"

#include <QVBoxLayout>
#include <components/layout/Divider.h>
#include <components/navigation/SelectorBar.h>
#include <components/navigation/StackContentHost.h>

#include "catalogsearchview.h"
#include "catalogview.h"

namespace doclife::ui {
    TemplateView::TemplateView(QWidget* parent)
        : QWidget{ parent }
    {
        init();
    }
    void TemplateView::init()
    {
        auto* selectorBar = new fluent::navigation::SelectorBar(this);
        selectorBar->addItem({ "目录", Typography::Icons::glyph(QStringLiteral("ic_fluent_building_desktop_24_regular")) });
        selectorBar->addItem({ "搜索", Typography::Icons::glyph(QStringLiteral("ic_fluent_document_search_24_regular")) });
        selectorBar->setOverflowBehavior(fluent::navigation::SelectorBar::OverflowBehavior::MoreButton);

        auto* host = new fluent::navigation::StackContentHost(this);

        auto* dirPage    = new CatalogView(this);        // 目录页（TreeView）
        auto* searchPage = new CatalogSearchView(this);  // 搜索页（ListView）

        host->insertPage(0, dirPage);
        host->insertPage(1, searchPage);
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