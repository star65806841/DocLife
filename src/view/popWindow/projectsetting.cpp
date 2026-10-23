#include "projectsetting.h"

#include <components/textfields/Label.h>
#include <components/basicinput/Button.h>
// #include <components/dialogs_flyouts/Dialog.h>
#include <components/navigation/NavigationView.h>
#include <components/navigation/StackContentHost.h>

#include "popWindow/infoItem/projectinfopage.h"
#include "infoItem/projectitem.h"
#include "business/projectcontext.h"

using namespace doclife::ui;

ProjectSetting::ProjectSetting(QWidget* parent)
    : fluent::dialogs_flyouts::Dialog{ parent }
{
    init();
}

void ProjectSetting::slot_updateProjectInfo()
{
    // 写回 ProjectContext
    auto* ctx = doclife::core::ProjectContext::instance();
    auto info = ctx->projectInfo();          // 拿一份副本

    info.projectName = m_projectName.trimmed();
    info.constructionUnitName = m_constructionUnitName.trimmed();
    info.supervisionUnitName = m_supervisionUnitName.trimmed();
    info.designUnitName = m_designUnitName.trimmed();
    info.ownerUnitName = m_ownerUnitName.trimmed();

    ctx->setProjectInfo(info);               // 如果有 setter 就调用

    // 3. 关闭对话框并返回 Accepted
    accept();
}

void ProjectSetting::slot_updateProjectName(const QString& str)
{
    m_projectName = str;
}

void ProjectSetting::slot_updateConstructionUnitName(const QString& str)
{
    m_constructionUnitName = str;
}

void ProjectSetting::slot_updateSupervisionUnitName(const QString& str)
{
    m_supervisionUnitName = str;
}

void ProjectSetting::slot_updateDesignUnitName(const QString& str)
{
    m_designUnitName = str;
}

void ProjectSetting::slot_updateOwnerUnitName(const QString& str)
{
    m_ownerUnitName = str;
}

void ProjectSetting::init()
{
    this->setMinimumSize(650, 500);
    this->setSmokeEnabled(true);
    this->setAnimationEnabled(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(55, 25, 45, 25);
    layout->setSpacing(18);

    QString proName = "项目信息:";
    auto* titleLabel = new fluent::textfields::Label(proName);
    titleLabel->setFluentTypography(Typography::FontRole::Subtitle);

    // QWidget* contentWidget = new QWidget();
    // contentWidget->setMinimumWidth(220);
    // contentWidget->setMaximumWidth(620);
    // contentWidget->setFixedHeight(320);
    // contentWidget->setFixedSize(this->size());
    // contentWidget->setStyleSheet("background-color: #a1F1BF; border-radius: 8px;");
    // contentWidget->setContentsMargins(55, 25, 45, 25);

    auto* navView = new fluent::navigation::NavigationView();
    navView->setMinimumWidth(220);
    navView->setMaximumWidth(620);
    navView->setFixedHeight(320);
    navView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    navView->setAnimationEnabled(true);
    navView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Left);
    navView->setExpandedPaneWidth(180);
    // navView->setStyleSheet("background-color: #a1F1BF; border-radius: 8px;");
    // navView->setThemeOverrides({
    //     {"radius", QJsonObject{{"overlay", 0}}}
    // });

    // ---- 项目数据 ----
    auto const projectInfo = doclife::core::ProjectContext::instance()->projectInfo();
    // 用已有数据初始化成员，防止用户不改动就把空串写回去
    m_projectName = projectInfo.projectName;
    m_constructionUnitName = projectInfo.constructionUnitName;
    m_supervisionUnitName = projectInfo.supervisionUnitName;
    m_designUnitName = projectInfo.designUnitName;
    m_ownerUnitName = projectInfo.ownerUnitName;
    // ---- 内容宿主：插入页面 ----
    fluent::navigation::StackContentHost* host = navView->contentHost();
    auto* projectNamePage = new ProjectInfoPage("工程名称", projectInfo.projectName, this);
    auto* constructionUnitNamePage = new ProjectInfoPage("施工单位", projectInfo.constructionUnitName, this);
    auto* supervisionUnitNamePage = new ProjectInfoPage("监理单位", projectInfo.supervisionUnitName, this);
    auto* designUnitNamePage = new ProjectInfoPage("设计单位", projectInfo.designUnitName, this);
    auto* ownerUnitNamePage = new ProjectInfoPage("建设单位", projectInfo.ownerUnitName, this);
    host->insertPage(0, projectNamePage, fluent::WidgetOwnership::Reparented);
    host->insertPage(1, constructionUnitNamePage, fluent::WidgetOwnership::Reparented);
    host->insertPage(2, supervisionUnitNamePage, fluent::WidgetOwnership::Reparented);
    host->insertPage(3, designUnitNamePage, fluent::WidgetOwnership::Reparented);
    host->insertPage(4, ownerUnitNamePage, fluent::WidgetOwnership::Reparented);

    // 切换动画
    host->setTransitionEffect(fluent::navigation::StackContentHost::TransitionEffect::SlideFromBottom);

    // ---- 导航项列表作为 main chrome 接入 ----
    QVector<MainSection::Entry> entries =
    {
      {Typography::Icons::glyph(QStringLiteral("ic_fluent_vote_24_regular")), QStringLiteral("工程项目名称"), true},
      {Typography::Icons::glyph(QStringLiteral("ic_fluent_vehicle_tractor_24_regular")), QStringLiteral("施工单位"), false},
      {Typography::Icons::glyph(QStringLiteral("ic_fluent_receipt_cube_24_regular")), QStringLiteral("监理单位"), false},
      {Typography::Icons::glyph(QStringLiteral("ic_fluent_quiz_24_regular")), QStringLiteral("设计单位"), false},
      {Typography::Icons::glyph(QStringLiteral("ic_fluent_card_ui_portrait_flip_24_regular")), QStringLiteral("建设单位"), false},
    };

    auto* mainSection = new MainSection(entries, navView);
    navView->setMainChromeWidget(mainSection, fluent::WidgetOwnership::Borrowed);

    // 初始显示 Home
    host->setCurrentIndex(0, 0, false);

    // ---- 连接激活回调 ----
    mainSection->onActivated = [host](int pageIndex) {
        const int direction =
            pageIndex >= host->currentIndex() ? 1 : -1;
        host->setCurrentIndex(pageIndex, direction, true);
        };

    // // 可选：调试用背景色，确认 MainSection 区域
    // mainSection->setStyleSheet("background-color: #FF00FF;");

    // 底部命令按钮
    auto* editButton = new fluent::basicinput::Button(QStringLiteral("确 认"));
    editButton->setMinimumWidth(120);
    // editButton->setEnabled();

    auto* cancelButton = new fluent::basicinput::Button(QStringLiteral("取 消"));
    cancelButton->setMinimumWidth(120);

    editButton->setFluentStyle(fluent::basicinput::Button::Accent);
    cancelButton->setFluentStyle(fluent::basicinput::Button::Standard);

    // 底部按钮行
    auto* commandRow = new QHBoxLayout;
    commandRow->setContentsMargins(0, 0, 0, 0);
    commandRow->setSpacing(8);
    commandRow->addStretch(1);
    commandRow->addWidget(editButton);
    commandRow->addWidget(cancelButton);

    layout->addWidget(titleLabel);
    // layout->addWidget(contentWidget);
    layout->addWidget(navView);
    layout->addStretch(1);
    layout->addLayout(commandRow);

    connect(projectNamePage, &ProjectInfoPage::sign_text, this, &ProjectSetting::slot_updateProjectName);
    connect(constructionUnitNamePage, &ProjectInfoPage::sign_text, this, &ProjectSetting::slot_updateConstructionUnitName);
    connect(supervisionUnitNamePage, &ProjectInfoPage::sign_text, this, &ProjectSetting::slot_updateSupervisionUnitName);
    connect(designUnitNamePage, &ProjectInfoPage::sign_text, this, &ProjectSetting::slot_updateDesignUnitName);
    connect(ownerUnitNamePage, &ProjectInfoPage::sign_text, this, &ProjectSetting::slot_updateOwnerUnitName);

    connect(editButton, &fluent::basicinput::Button::clicked, this, &ProjectSetting::slot_updateProjectInfo);
    connect(cancelButton, &fluent::basicinput::Button::clicked, this, &QDialog::close);
    connect(this, &QDialog::finished, this, &QObject::deleteLater);
    // connect(this, &ProjectSetting::destroyed, []() {qDebug() << "ProjectSetting destroyed"; });// test ok 20261018
}
