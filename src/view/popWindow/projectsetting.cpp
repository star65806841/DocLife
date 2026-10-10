#include "projectsetting.h"

#include <components/textfields/Label.h>
#include <components/basicinput/Button.h>
// #include <components/dialogs_flyouts/Dialog.h>
#include <components/navigation/NavigationView.h>
#include <components/navigation/StackContentHost.h>

#include "popWindow/infoItem/projectinfopage.h"
#include "infoItem/projectitem.h"

using namespace doclife::ui;

ProjectSetting::ProjectSetting(QWidget *parent)
    : fluent::dialogs_flyouts::Dialog{parent}
{
    init();
}

void ProjectSetting::slot_projectName(const QString &name)
{
    qDebug()<< "projectName length ="<< name.length();
    // return !name.isEmpty();
}

void ProjectSetting::init()
{
    this->setMinimumSize(650, 500);
    this->setSmokeEnabled(true);
    this->setAnimationEnabled(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(55, 25, 45, 25);
    layout->setSpacing(18);

    auto* titleLabel = new fluent::textfields::Label("项目信息");
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
    // ---- 内容宿主：插入页面 ----
    fluent::navigation::StackContentHost* host = navView->contentHost();
    auto* projectNamePage = new ProjectInfoPage("工程名称",this);
    connect(projectNamePage,&ProjectInfoPage::sign_text,this,&ProjectSetting::slot_projectName,Qt::ConnectionType::DirectConnection);
    host->insertPage(0,projectNamePage,fluent::WidgetOwnership::Reparented);
    host->insertPage(1,new ProjectInfoPage("施工单位",this),fluent::WidgetOwnership::Reparented);
    host->insertPage(2,new ProjectInfoPage("监理单位",this),fluent::WidgetOwnership::Reparented);
    host->insertPage(3,new ProjectInfoPage("设计单位",this),fluent::WidgetOwnership::Reparented);
    host->insertPage(4,new ProjectInfoPage("建设单位",this),fluent::WidgetOwnership::Reparented);

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
    auto* editButton  = new fluent::basicinput::Button(QStringLiteral("确 认"));
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

    // connect(editButton, &fluent::basicinput::Button::clicked, this,&QDialog::close);
    // connect(cancelButton, &fluent::basicinput::Button::clicked, this,&QDialog::close);
    // connect(this, &QDialog::finished, this,&QObject::deleteLater);
    // connect(this, &QDialog::destroyed, [](){qDebug()<<"ProjectSetting destroyed";});
}