#include "topmenu.h"

#include <QHBoxLayout>
#include <components/menus_toolbars/CommandBar.h>
#include <components/foundation/FontIcon.h>

#include "uisignalmanager.h"

using namespace doclife::ui;

doclife::ui::TopMenu::TopMenu(QWidget *parent)
    : QWidget{parent}
{
    init();
    initConnect();
}

void TopMenu::init()
{
    // auto* settingsIcon = new fluent::FontIcon("ic_fluent_settings_20_regular");

    auto *barLayout = new QHBoxLayout(this);
    barLayout->setContentsMargins(2, 0, 0, 2);
    barLayout->setSpacing(6);

    auto* bar = new fluent::menus_toolbars::CommandBar(this);
    bar->setAccessibleName("Document commands");
    bar->setLabelPosition(
        fluent::menus_toolbars::CommandBar::LabelPosition::Right);
    bar->setBackgroundVisible(true);


    auto* addAction = new QAction(
        QIcon(":/icons/menu/AddFile.svg"), "新建工程", bar);
    auto* infoAction = new QAction(
        QIcon(":/icons/menu/InfoFile.svg"), "工程信息", bar);
    infoAction->setPriority(QAction::HighPriority);
    auto* shareAction = new QAction(
        QIcon(":/icons/logo/Flash.png"), "Share", bar);

    auto* separator = new QAction(bar);
    separator->setSeparator(true);

    auto* syncAction = new QAction(
        QIcon(":/icons/logo/Flash.png"), "Sync", bar);
    syncAction->setPriority(QAction::LowPriority);
    auto* pinAction = new QAction(
        QIcon(":/icons/logo/Flash.png"), "Pin", bar);
    pinAction->setCheckable(true);

    bar->addPrimaryAction(addAction);
    bar->addPrimaryAction(infoAction);
    bar->addPrimaryAction(shareAction);
    bar->addPrimaryAction(separator);
    bar->addPrimaryAction(syncAction);
    bar->addPrimaryAction(pinAction);
    bar->addPrimaryAction(new QAction(
        QIcon(Typography::Icons::Document), "Settings", this));
    bar->addPrimaryAction(new QAction(
        QIcon(Typography::Icons::Document), "Help", this));

    // for (QWidget *w : bar->findChildren<QWidget*>()) {
    //     qDebug() << w->metaObject()->className() << w->objectName();
    // }
    for (auto *btn : bar->findChildren<fluent::basicinput::Button*>()) {
        btn->setFocusVisual(false);
    }

    barLayout->addWidget(bar);

    connect(addAction,&QAction::triggered,UISignalManager::instance(),&UISignalManager::popProjectNew);
    connect(infoAction,&QAction::triggered,UISignalManager::instance(),&UISignalManager::popProjectsetting);
}

void TopMenu::initConnect()
{

}
