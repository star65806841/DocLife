#include "centralview.h"

#include <topMenu/topmenu.h>

#include <components/collections/DrawerView.h>
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
            QStringLiteral("新建文档会关闭当前文档，点击确定保存，否则点击取消放弃保存。"),
            [this]() {
                qDebug() << "Save confirmation for newly created file that has not been saved. User clicks OK";
                this->saveProject();
            },
            []() {
                qDebug() << "Save confirmation for newly created file that has not been saved. User clicks Cancel";
                doclife::core::ProjectContext::instance()->newProject();
            }
        );
    }
    else {
        // 不需要先保存 新建中有对 已经保存过的项目保存的操作
        auto* popOpenedWarning = new PopupBase(this);
        auto* popConnect = new PopContent("新建提醒", "新建文档会关闭当前文档，关闭前会自动保存。", this);
        popOpenedWarning->setFixedSize(400, 250);
        popOpenedWarning->setContentWidget(popConnect);
        popOpenedWarning->open();
        connect(popOpenedWarning, &PopupBase::destroyed, []() {doclife::core::ProjectContext::instance()->newProject(); });
    }
}

void CentralView::saveProject()
{
    // go saveas
    if (doclife::core::ProjectContext::instance()->isUntitled())
    {
        qDebug() << "start saveProjectas-> open a fill diaog";
        QString filepath = doclife::core::Utils::saveFileDialog(doclife::core::ProjectContext::instance()->displayName(), this);
        qDebug() << "file path is :" << filepath;
        doclife::core::ProjectContext::instance()->saveProjectAs(filepath);
    }
    // go save
    else
    {
        QString filePath = doclife::core::ProjectContext::instance()->filePath();
        qDebug() << "start saveProject-> the file path is:" << filePath;
        doclife::core::ProjectContext::instance()->saveProject();
    }
}

void CentralView::openProject()
{
    if (!doclife::core::ProjectContext::instance()->hasOpenProject()) {
        openProjectDialog();
        return;
    }
    else if (doclife::core::ProjectContext::instance()->isUntitled()) {
        // 项目尚未保存过
        doclife::ui::DialogBase::show(
            this,
            QStringLiteral("保存确认"),
            QStringLiteral("打开文档会关闭当前文档，点击确定保存,否则点击取消放弃保存！"),
            [this]() {
                qDebug() << "Open confirmation for newly open file that has not been saved. User clicks OK";
                this->saveProject();
            },
            [this]() {
                qDebug() << "Open confirmation for newly open file that has not been saved. User clicks Cancel";
                this->openProjectDialog();
            }
        );
    }
    else {
        auto* popOpenedWarning = new PopupBase(this);
        auto* popConnect = new PopContent("打开提醒", "打开文档会关闭当前文档，关闭前会自动保存。", this);
        popOpenedWarning->setFixedSize(400, 250);
        popOpenedWarning->setContentWidget(popConnect);
        popOpenedWarning->open();
        connect(popOpenedWarning, &PopupBase::closed, [this]() {this->openProjectDialog(); });
    }
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
    AppToast::showInfo(this, "应用程序正在运行, 请先关闭后再启动。");

}

void CentralView::popProjectClosed()
{
    // QString s2 = u"你好"_qs;
    QString infoStr = QStringLiteral("文档已关闭");
    AppToast::showInfo(this, infoStr);
}
void CentralView::popProjectSaved()
{
    AppToast::showSuccess(this, "保存成功");
}
void CentralView::popProjectCreated()
{
    AppToast::showSuccess(this, "创建成功");
}
void CentralView::popProjectOpened()
{
    AppToast::showSuccess(this, "打开成功");
}

void CentralView::onDrawerFileSetting()
{
    auto* drawer = new fluent::collections::DrawerView(this);
    drawer->setEdge(fluent::collections::DrawerView::DrawerEdge::Right);
    drawer->setDrawerLength(260);
    drawer->setContentWidget(new QWidget(), fluent::WidgetOwnership::Owned);
    drawer->open();
}
void CentralView::showProjectNew()
{
    // ProjectNew* pop = new ProjectNew(this);
    // pop->open();
}

void CentralView::initConnect()
{
    // connect(m_mainOpenBtn,&QPushButton::clicked,UISignalManager::instance(),&UISignalManager::openExcelRequested);
    connect(UISignalManager::instance(), &UISignalManager::drawerFileSetting, this, &CentralView::onDrawerFileSetting);
    // 点击项目信息按钮
    connect(UISignalManager::instance(), &UISignalManager::popProjectsetting, this, &CentralView::showProjectsetting);
    // 点击新建
    connect(UISignalManager::instance(), &UISignalManager::newProject, this, &CentralView::newProject);
    // 点击保存
    connect(UISignalManager::instance(), &UISignalManager::saveProject, this, &CentralView::saveProject);
    // 点击打开
    connect(UISignalManager::instance(), &UISignalManager::openProject, this, &CentralView::openProject);
    //connect(UISignalManager::instance(),&UISignalManager::popProjectNew,this,&CentralView::showProjectNew);
    // 多开处理
    connect(UISignalManager::instance(), &UISignalManager::duplicateInstanceDetected, this, &CentralView::popOpenedWarning);
    /******************状态提醒*****************************************/
    // 工程关闭提醒
    connect(doclife::core::ProjectContext::instance(), &doclife::core::ProjectContext::projectClosed, this, &CentralView::popProjectClosed);
    // 工程保存提醒
    connect(doclife::core::ProjectContext::instance(), &doclife::core::ProjectContext::projectSaved, this, &CentralView::popProjectSaved);
    // 工程创建提醒
    connect(doclife::core::ProjectContext::instance(), &doclife::core::ProjectContext::projectCreated, this, &CentralView::popProjectCreated);
    // 工程打开提醒
    connect(doclife::core::ProjectContext::instance(), &doclife::core::ProjectContext::projectOpened, this, &CentralView::popProjectOpened);
    /******************状态提醒*****************************************/
}

void CentralView::openProjectDialog()
{
    qDebug() << "start openProject-> open a fill diaog";
    QString filepath = doclife::core::Utils::openFileDialog();
    qDebug() << "file path is :" << filepath;
    doclife::core::ProjectContext::instance()->openProject(filepath);
}
