#include "projectsetting.h"

#include <components/textfields/Label.h>
#include <components/basicinput/Button.h>
#include <components/dialogs_flyouts/Dialog.h>
#include <components/navigation/NavigationView.h>
#include <components/navigation/StackContentHost.h>

#include "projiectitem.h"

using namespace doclife::ui;

ProjectSetting::ProjectSetting(QWidget *parent)
    : QWidget{parent}
{
    init();
}

void ProjectSetting::show(QWidget *parent)
{
    if(!m_dialog)
    {
        return;
    }
    m_dialog->setParent(parent);
    m_dialog->open();
}

void ProjectSetting::init()
{
    m_dialog = new fluent::dialogs_flyouts::Dialog();
    m_dialog->setMinimumSize(480, 280);
    m_dialog->setSmokeEnabled(true);
    m_dialog->setAnimationEnabled(true);

    auto* layout = new QVBoxLayout(m_dialog);
    layout->setContentsMargins(55, 25, 45, 25);
    layout->setSpacing(12);

    auto* titleLabel = new fluent::textfields::Label("项目信息");
    titleLabel->setFluentTypography(Typography::FontRole::Subtitle);

    // auto* nameEdit = new fluent::textfields::Label("工程概况");
    // nameEdit->setFluentTypography(Typography::FontRole::Subtitle);

    // ---- NavigationView ----
    // auto* navViewBox = new QWidget();
    // navViewBox->setMinimumWidth(440);
    // navViewBox->setMaximumWidth(620);
    // navViewBox->setFixedHeight(320);

    auto* navView = new fluent::navigation::NavigationView();
    navView->setMinimumWidth(220);
    navView->setMaximumWidth(620);
    navView->setFixedHeight(320);
    navView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    navView->setAnimationEnabled(true);
    navView->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Left);
    navView->setExpandedPaneWidth(180);
    // navView->setThemeOverrides({
    //     {"radius", QJsonObject{{"overlay", 0}}}
    // });
    // ---- 内容宿主：插入页面 ----
    fluent::navigation::StackContentHost* host = navView->contentHost();

    host->insertPage(
        0,
        createDemoPage(QStringLiteral("工程名称"),
                       QStringLiteral("Welcome to the FluentQt "
                                      "NavigationView demo.")),
        fluent::WidgetOwnership::Reparented);

    host->insertPage(
        1,
        createDemoPage(QStringLiteral("Library"),
                       QStringLiteral("Browse your media library here.")),
        fluent::WidgetOwnership::Reparented);

    host->insertPage(
        2,
        createDemoPage(QStringLiteral("Settings"),
                       QStringLiteral("Adjust application preferences.")),
        fluent::WidgetOwnership::Reparented);

    host->insertPage(
        3,
        createDemoPage(QStringLiteral("About"),
                       QStringLiteral("FluentQt NavigationView sample.")),
        fluent::WidgetOwnership::Reparented);

    host->insertPage(
        4,
        createDemoPage(QStringLiteral("工程名称"),
                       QStringLiteral("Welcome to the FluentQt "
                                      "NavigationView demo.")),
        fluent::WidgetOwnership::Reparented);

    host->insertPage(
        5,
        createDemoPage(QStringLiteral("工程名称"),
                       QStringLiteral("Welcome to the FluentQt "
                                      "NavigationView demo.")),
        fluent::WidgetOwnership::Reparented);

    host->setTransitionEffect(fluent::navigation::StackContentHost::TransitionEffect::SlideFromBottom);

    // ---- 导航项列表作为 main chrome 接入 ----
    QVector<MainSection::Entry> entries =
        {
          {Typography::Icons::glyph(QStringLiteral("ic_fluent_vote_24_regular")), QStringLiteral("工程类型"), true},
          {Typography::Icons::glyph(QStringLiteral("ic_fluent_card_ui_portrait_flip_24_regular")), QStringLiteral("工程项目名称"), false},
          {Typography::Icons::glyph(QStringLiteral("ic_fluent_vehicle_tractor_24_regular")), QStringLiteral("施工单位"), false},
          {Typography::Icons::glyph(QStringLiteral("ic_fluent_receipt_cube_24_regular")), QStringLiteral("监理单位"), false},
          {Typography::Icons::glyph(QStringLiteral("ic_fluent_quiz_24_regular")), QStringLiteral("设计单位"), false},
          {Typography::Icons::glyph(QStringLiteral("ic_fluent_window_text_24_regular")), QStringLiteral("建设单位"), false},
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

    // 可选：调试用背景色，确认 MainSection 区域
    // mainSection->setStyleSheet("background-color: #FF00FF;");

    // 底部命令按钮
    auto* completeButton  = new fluent::basicinput::Button(QStringLiteral("完成"));
    auto* cancelButton = new fluent::basicinput::Button(QStringLiteral("取消"));
    completeButton->setFluentStyle(fluent::basicinput::Button::Accent);
    cancelButton->setFluentStyle(fluent::basicinput::Button::Standard);

    // 底部按钮行
    auto* commandRow = new QHBoxLayout;
    commandRow->setContentsMargins(0, 0, 0, 0);
    commandRow->setSpacing(8);
    commandRow->addStretch(1);
    commandRow->addWidget(cancelButton);
    commandRow->addWidget(completeButton);

    layout->addWidget(titleLabel);
    // layout->addWidget(nameEdit);
    layout->addWidget(navView);
    layout->addStretch(1);
    layout->addLayout(commandRow);

    connect(completeButton, &fluent::basicinput::Button::clicked, m_dialog,&QDialog::close);
    connect(cancelButton, &fluent::basicinput::Button::clicked, m_dialog,&QDialog::close);
    connect(m_dialog, &QDialog::finished, m_dialog,&QObject::deleteLater);
    connect(m_dialog, &QDialog::destroyed, [](){qDebug()<<"m_dialog destroyed";});
    connect(m_dialog, &QDialog::destroyed, this,&QObject::deleteLater);
    connect(this, &QWidget::destroyed, [](){qDebug()<<"ProjectSetting destroyed";});
}

QWidget* ProjectSetting::createDemoPage(const QString& title,
                        const QString& description, QWidget* parent)
{
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(title, page);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSizeF(qMax(titleFont.pointSizeF() * 1.5, 15.0));
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* descLabel = new QLabel(description, page);
    descLabel->setWordWrap(true);

    layout->addWidget(titleLabel);
    layout->addWidget(descLabel);
    layout->addStretch();
    return page;
}