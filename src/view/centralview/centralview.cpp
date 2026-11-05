#include "centralview.h"

#include "maincontentview.h"

#include <topMenu/topmenu.h>

#include <components/collections/DrawerView.h>
#include <components/collections/SplitView.h>
#include <components/layout/Divider.h>
#include <components/date_time/CalendarView.h>
#include <components/dialogs_flyouts/ContentDialog.h>

#include <QVBoxLayout>
#include <QPointer>

#include <popWindow/addsheetdialog.h>
#include <popWindow/projectnew.h>
#include <popWindow/projectsetting.h>
#include <popWindow/settingsdialog.h>
#include <popWindow/aboutdialog.h>
#include <popWindow/dialog/PopupBase.h>
#include <popWindow/dialog/dialogbase.h>
#include <popWindow/dialog/popcontent.h>
#include <popWindow/toast/apptoast.h>

#include <leftview/templateview/templateview.h>
#include <leftview/projecttreeview/projectview.h>

#include <projectcontext.h>
#include <uisignalmanager.h>
#include <utils.h>

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
        const QString filepath = doclife::core::Utils::saveFileDialog(
            doclife::core::ProjectContext::instance()->displayName(), this);
        if (filepath.isEmpty()) return;   // 用户取消了另存为
        if (!doclife::core::ProjectContext::instance()->saveProjectAs(filepath)) {
            AppToast::showWarning(this, "保存失败，请检查目标位置是否可写");
        }
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
        showProjectDialog();
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
                this->showProjectDialog();
            }
        );
    }
    else {
        auto* popOpenedWarning = new PopupBase(this);
        auto* popConnect = new PopContent("打开提醒", "打开文档会关闭当前文档，关闭前会自动保存。", this);
        popOpenedWarning->setFixedSize(400, 250);
        popOpenedWarning->setContentWidget(popConnect);
        popOpenedWarning->open();
        connect(popOpenedWarning, &PopupBase::closed, [this]() {this->showProjectDialog(); });
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
    m_templateView = new TemplateView(this);
    m_leftView = new ProjectView(this);
    // ----------- 右侧区域  -----------
    m_mainContent = new MainContentView(this);

    auto* splitter = new fluent::collections::SplitView(this);
    splitter->addPane(m_templateView, { 50,230,850,false });
    m_templateView->setVisible(false);
    splitter->addPane(m_leftView, { 50,230,850,false });
    splitter->addPane(m_mainContent);
    splitter->setPaneFill(m_mainContent, true);
    // ---------- 外层垂直布局：上按钮、下 splitter ----------
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(topBar);       // 顶
    v->addWidget(new fluent::layout::Divider(this));
    v->addWidget(splitter, 1);  // 余下空间全给 splitte

    initConnect();
}
void CentralView::showProjectsetting()
{
    ProjectSetting* pop = new ProjectSetting(this);
    pop->open();
}

void CentralView::popOpenedWarning()
{
    AppToast::showInfo(this, "应用程序正在运行, 请先关闭后再启动。");
}

void CentralView::projectClosed()
{
    if (m_mainContent) {
        m_mainContent->clearView();
    }
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

void CentralView::onToastWarning(const QString& msg)
{
    AppToast::showWarning(this, msg);
}

void CentralView::onRequestAddSheet(const QString &parentNodeId)
{
    // 1. 创建弹窗
    auto* dlg = new AddSheetDialog(this);

    // 2. 设置弹窗关闭后自动销毁
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // 3. 监听弹窗完成（无论点确定还是取消，都会发 finished）
    connect(dlg, &QDialog::accepted, this, [this, dlg, parentNodeId]() {
        auto selectedSheets = dlg->selectedSheets();
        if (!selectedSheets.isEmpty()) {
            doclife::core::ProjectContext::instance()->addSheetsToNode(parentNodeId, selectedSheets);
        }
    });

    // 4. 打开弹窗（异步模态，不阻塞主界面，且带遮罩和动画）
    dlg->open();
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
    // 点击设置按钮
    connect(UISignalManager::instance(), &UISignalManager::popSettings, this, [this]() {
        auto* dialog = new SettingsDialog(this);
        dialog->open();
        });
    // 点击关于
    connect(UISignalManager::instance(), &UISignalManager::popAbout, this, [this]() {
        auto* dialog = new AboutDialog(this);
        dialog->open();
        });
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
    connect(doclife::core::ProjectContext::instance(), &doclife::core::ProjectContext::projectClosed, this, &CentralView::projectClosed);

    // 工程保存提醒
    connect(doclife::core::ProjectContext::instance(), &doclife::core::ProjectContext::projectSaved, this, &CentralView::popProjectSaved);
    // 工程创建提醒
    connect(doclife::core::ProjectContext::instance(), &doclife::core::ProjectContext::projectCreated, this, &CentralView::popProjectCreated);
    // 工程打开提醒
    connect(doclife::core::ProjectContext::instance(), &doclife::core::ProjectContext::projectOpened, this, &CentralView::popProjectOpened);
    /******************状态提醒*****************************************/
    connect(doclife::core::ProjectContext::instance(), &doclife::core::ProjectContext::errorOccurred, this, &CentralView::onToastWarning);
    /******************模板窗口*****************************************/
    connect(UISignalManager::instance(), &UISignalManager::showTemplateView, this,[this](bool show){m_templateView->setVisible(show);});
    // 新建表格
    connect(UISignalManager::instance(), &UISignalManager::requestAddSheet, this, &CentralView::onRequestAddSheet);
    // 时间区域双击：主界面统一弹日历窗
    connect(UISignalManager::instance(), &UISignalManager::datePickRequested, this, &CentralView::onDatePickRequested);

}

void CentralView::showProjectDialog()
{
    qDebug() << "start openProject-> open a fill diaog";
    QString filepath = doclife::core::Utils::openFileDialog();
    qDebug() << "file path is :" << filepath;
    doclife::core::ProjectContext::instance()->openProject(filepath);
}

void CentralView::onDatePickRequested(int sheetId, int row, int col, const QDate& current)
{
    // 日历内嵌在内容弹窗里：弹窗由本信号处理函数发起，脱离双击事件，
    // 不会被浮层的“点外部即关”逻辑误杀；确定/取消统一走 datePickConfirmed 回执。
    auto* dlg = new fluent::dialogs_flyouts::ContentDialog(this);
    dlg->setTitle("选择日期");
    dlg->setPrimaryButtonText("确定");
    dlg->setCloseButtonText("取消");
    dlg->setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Primary);
    dlg->setModal(true);
    dlg->setDim(true);
    dlg->setAnimationEnabled(true);

    auto* body = new QWidget(dlg);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(12, 10, 12, 10);

    auto* calendar = new fluent::date_time::CalendarView(body);
    calendar->setFrameVisible(false);
    calendar->setSelectedDate(current);
    calendar->setVisibleMonth(current);
    bodyLayout->addWidget(calendar);

    dlg->setContent(body);

    QPointer<fluent::date_time::CalendarView> calendarGuard(calendar);
    connect(dlg, &QDialog::finished, this,
        [calendarGuard, sheetId, row, col](int result) {
            const QDate picked = (result == QDialog::Accepted && calendarGuard)
                ? calendarGuard->selectedDate() : QDate();
            // 取消/关闭时 picked 无效，表格侧收到后保留单元格原值
            emit UISignalManager::instance()->datePickConfirmed(sheetId, row, col, picked);
        });

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
}
