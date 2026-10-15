#include "centralview.h"

#include <topMenu/topmenu.h>

#include <components/collections/SplitView.h>

#include <popWindow/projectnew.h>
#include <popWindow/projectsetting.h>
#include <popWindow/dialog/PopupBase.h>
#include <popWindow/infoItem/duplicateapp.h>

#include <projectcontext.h>
#include <uisignalmanager.h>

#include <leftview/leftview.h>

using namespace doclife::ui;

CentralView::CentralView(QWidget *parent)
    : QWidget{parent}
{
    init();
}

void CentralView::newProject(const QString &name)
{
    //doclife::core::ProjectContext::instance()->addNewProject(name);
}

void CentralView::init()
{
    // =========================================================
    // 顶部按钮区
    // =========================================================
    auto *topBar = new TopMenu(this);

    // =========================================================
    // 中部区域
    // =========================================================
    // ----------- 左侧区域  -----------
    m_leftView = new LeftView(this);
    // ----------- 右侧区域  -----------
    auto *m_mainContent = new QWidget(this);

    auto *splitter = new fluent::collections::SplitView(this);
    splitter->addPane(m_leftView,{50,230,850,false});
    splitter->addPane(m_mainContent);
    splitter->setPaneFill(1, true);
    // ---------- 外层垂直布局：上按钮、下 splitter ----------
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

void CentralView::popOpenedWarning()
{
    auto* popOpenedWarning = new PopupBase(this);
    auto* popConnect = new DuplicateApp(this);
    popOpenedWarning->setFixedSize(400,250);
    popOpenedWarning->setContentWidget(popConnect);
    popOpenedWarning->open();

}
void CentralView::showProjectNew()
{
     ProjectNew* pop = new ProjectNew(this);
     pop->open();
    //DialogBase* pop = new DialogBase(this);
    //pop->open();
}

void CentralView::initConnect()
{
    // connect(m_mainOpenBtn,&QPushButton::clicked,UISignalManager::instance(),&UISignalManager::openExcelRequested);
    connect(UISignalManager::instance(),&UISignalManager::popProjectsetting,this,&CentralView::showProjectsetting);
    connect(UISignalManager::instance(),&UISignalManager::projectNewByName,this,&CentralView::newProject);
    connect(UISignalManager::instance(),&UISignalManager::popProjectNew,this,&CentralView::showProjectNew);
    // 多开处理
    connect(UISignalManager::instance(),&UISignalManager::duplicateInstanceDetected,this, &CentralView::popOpenedWarning);
}
