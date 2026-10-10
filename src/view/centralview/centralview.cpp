#include "centralview.h"

#include <topMenu/topmenu.h>

#include <components/collections/SplitView.h>

#include <popWindow/projectnew.h>
#include <popWindow/projectsetting.h>

#include <uisignalmanager.h>

#include <popWindow/dialog/dialogbase.h>


using namespace doclife::ui;

CentralView::CentralView(QWidget *parent)
    : QWidget{parent}
{
    init();
}
void CentralView::init()
{
    // ---------- 顶部按钮区 -----------
    auto *topBar = new TopMenu();

    // ----------- 中部区域  -----------
    auto *splitter = new fluent::collections::SplitView();
    auto *left   = new QWidget();
    // left->setStyleSheet("background:#eAf;");

    auto *m_mainContent = new QWidget();

    splitter->addPane(left,{50,200,850,false});
    splitter->addPane(m_mainContent);
    splitter->setPaneFill(1, true);
    // ---------- 外层垂直布局：上按钮、下 splitter ----------
    // auto *central = new QWidget(this);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(topBar);       // 顶
    v->addWidget(splitter, 1);  // 余下空间全给 splitte

    initConnect();
}
void CentralView::showProjectsetting()
{
    ProjectSetting* pop = new ProjectSetting(this);
    pop->show();
}
void CentralView::showProjectNew()
{
    // ProjectNew* pop = new ProjectNew();
    // pop->open();
    DialogBase* pop = new DialogBase();
    pop->open();
}

void CentralView::initConnect()
{
    // connect(m_mainOpenBtn,&QPushButton::clicked,UISignalManager::instance(),&UISignalManager::openExcelRequested);
    connect(UISignalManager::instance(),&UISignalManager::popProjectNew,this,&CentralView::showProjectNew);
    connect(UISignalManager::instance(),&UISignalManager::popProjectsetting,this,&CentralView::showProjectsetting);
}
