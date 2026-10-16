#include "centralview.h"

#include <topMenu/topmenu.h>

#include <components/collections/SplitView.h>

#include <popWindow/projectnew.h>
#include <popWindow/projectsetting.h>
#include <popWindow/dialog/PopupBase.h>
#include <popWindow/dialog/dialogbase.h>
#include <popWindow/dialog/popcontent.h>
#include <popWindow/toast/apptoast.h>

#include <projectcontext.h>
#include <uisignalmanager.h>
#include <utils.h>

#include <leftview/leftview.h>

using namespace doclife::ui;

CentralView::CentralView(QWidget* parent)
    : QWidget{ parent }
{
    init();
}

void CentralView::newProject()
{
    if (!doclife::core::ProjectContext::instance()->hasOpenProject()) {
        // 没有打开的项目，直接新建
        doclife::core::ProjectContext::instance()->newProject();
        return;
    }
    else if (doclife::core::ProjectContext::instance()->isUntitled()) {
        // 项目尚未保存过
        doclife::ui::DialogBase::show(
            this,
            QStringLiteral("保存确认"),
            QStringLiteral("新建文档会关闭当前文档，点击确定保存否则点击取消！"),
            [this]() {
                qDebug() << "Save confirmation for newly created file that has not been saved. User clicks OK";
                this->saveProject();
                doclife::core::ProjectContext::instance()->newProject();
            },
            []() { qDebug() << "Save confirmation for newly created file that has not been saved. User clicks Cancel"; }
            );
    }
    else {
        // 不需要先保存 新建中有对 已经保存过的项目保存的操作
        auto* popOpenedWarning = new PopupBase(this);
        auto* popConnect = new PopContent("新建提醒","新建文档会关闭当前文档!",this);
        popOpenedWarning->setFixedSize(400, 250);
        popOpenedWarning->setContentWidget(popConnect);
        popOpenedWarning->open();
        doclife::core::ProjectContext::instance()->newProject();
    }
}

void CentralView::saveProject()
{
    if (doclife::core::ProjectContext::instance()->isUntitled())
    {
        qDebug() << "start saveProjectas-> open a fill diaog";
        QString filepath = doclife::core::Utils::saveFileDialog(doclife::core::ProjectContext::instance()->displayName(), this);
        qDebug() << "file path is :" << filepath;
        doclife::core::ProjectContext::instance()->saveProjectAs(filepath);
    }
    else {
        QString excelFilePath = doclife::core::ProjectContext::instance()->excelFilePath();
        qDebug() << "start saveProject-> the file path is:" << excelFilePath;
        doclife::core::ProjectContext::instance()->saveProject();
    }
}

void CentralView::openProject()
{
    qDebug() << "start openProject-> open a fill diaog";
    QString filepath = doclife::core::Utils::openFileDialog();
    qDebug() << "file path is :" << filepath;
    doclife::core::ProjectContext::instance()->openProject(filepath);
}

void CentralView::init()
{
    // =========================================================
    // 顶部按钮区
    // =========================================================
    auto* topBar = new TopMenu(this);

    // =========================================================
    // 中部区域
    // =========================================================
    // ----------- 左侧区域  -----------
    m_leftView = new LeftView(this);
    // ----------- 右侧区域  -----------
    auto* m_mainContent = new QWidget(this);

    auto* splitter = new fluent::collections::SplitView(this);
    splitter->addPane(m_leftView, { 50,230,850,false });
    splitter->addPane(m_mainContent);
    splitter->setPaneFill(1, true);
    // ---------- 外层垂直布局：上按钮、下 splitter ----------
    auto* v = new QVBoxLayout(this);
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
    // auto* popOpenedWarning = new PopupBase(this);
    // auto* popConnect = new DuplicateApp(this);
    // popOpenedWarning->setFixedSize(400,250);
    // popOpenedWarning->setContentWidget(popConnect);
    // popOpenedWarning->open();
    AppToast::showInfo(this, "应用程序正在运行, 请先关闭后再启动!");

}
void CentralView::showProjectNew()
{
    // ProjectNew* pop = new ProjectNew(this);
    // pop->open();
}

void CentralView::initConnect()
{
    // connect(m_mainOpenBtn,&QPushButton::clicked,UISignalManager::instance(),&UISignalManager::openExcelRequested);
    connect(UISignalManager::instance(), &UISignalManager::popProjectsetting, this, &CentralView::showProjectsetting);
    connect(UISignalManager::instance(), &UISignalManager::newProject, this, &CentralView::newProject);
    connect(UISignalManager::instance(), &UISignalManager::saveProject, this, &CentralView::saveProject);
    connect(UISignalManager::instance(), &UISignalManager::openProject, this, &CentralView::openProject);
    //connect(UISignalManager::instance(),&UISignalManager::popProjectNew,this,&CentralView::showProjectNew);
    // 多开处理
    connect(UISignalManager::instance(), &UISignalManager::duplicateInstanceDetected, this, &CentralView::popOpenedWarning);
}
