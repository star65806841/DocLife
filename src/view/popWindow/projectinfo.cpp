#include "projectinfo.h"

#include <components/navigation/NavigationView.h>
#include <components/navigation/StackContentHost.h>

#include <popWindow/infoItem/projectinfopage.h>

namespace doclife::ui {
ProjectInfo::ProjectInfo(QWidget *parent)
    : QWidget{parent}
{
    init();
}
void ProjectInfo::init()
{
    // 导航设置
    auto *navView = new fluent::navigation::NavigationView(this);
    navView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Left);  // 左侧模式
    navView->setMinimumWidth(220);
    navView->setMaximumWidth(620);
    navView->setFixedHeight(320);
    navView->setExpandedPaneWidth(180);                          // 展开宽度
    navView->setPaneOpen(true);                                  // 默认展开
    navView->setAnimationEnabled(true);                          // 开启动画
    navView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 内容区域
    auto* contentHost = new fluent::navigation::StackContentHost(this);
    contentHost->setTransitionEffect(fluent::navigation::StackContentHost::TransitionEffect::SlideFromBottom);

    auto* projectNamePage = new ProjectInfoPage("工程名称",this);
    auto* buildPage = new ProjectInfoPage("施工单位",this);
    auto* supervisionPage = new ProjectInfoPage("监理单位",this);
    auto* designingPage = new ProjectInfoPage("设计单位",this);
    auto* constructionPage = new ProjectInfoPage("建设单位",this);
    // =========================================================
    // 将页面注册到 StackContentHost
    // =========================================================
    contentHost->insertPage(0,projectNamePage,fluent::WidgetOwnership::Reparented);
    contentHost->insertPage(1,buildPage,fluent::WidgetOwnership::Reparented);
    contentHost->insertPage(2,supervisionPage,fluent::WidgetOwnership::Reparented);
    contentHost->insertPage(3,designingPage,fluent::WidgetOwnership::Reparented);
    contentHost->insertPage(4,constructionPage,fluent::WidgetOwnership::Reparented);
    // 初始显示 Home
    contentHost->setCurrentIndex(0, 0, false);
    // navView->;
}
}