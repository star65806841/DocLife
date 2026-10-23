#include "topmenu.h"

#include <QHBoxLayout>

#include <components/menus_toolbars/CommandBar.h>
#include <components/menus_toolbars/Menu.h>
#include <components/menus_toolbars/MenuBar.h>
#include <components/foundation/FontIcon.h>
#include <components/basicinput/DropDownButton.h>
#include <components/basicinput/ToggleButton.h>

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
    auto *barLayout = new QHBoxLayout(this);
    barLayout->setContentsMargins(2, 0, 0, 2);
    barLayout->setSpacing(6);

    auto* bar = new fluent::menus_toolbars::CommandBar(this);

    bar->setAccessibleName("Document commands");
    bar->setLabelPosition(fluent::menus_toolbars::CommandBar::LabelPosition::Right);
    bar->setBackgroundVisible(false);

    auto* actionFile = new QAction(QIcon(":/icons/menu/AddFile.svg"), "文件测试", bar);
    auto* actionAddBlank = new QAction(QIcon(":/icons/menu/AddFile.svg"), "新建空白", bar);
    auto* actionOpenProject = new QAction(QIcon(":/icons/menu/file_open.svg"), "打开", bar);
    auto* actionSaveProject = new QAction(QIcon(":/icons/menu/file_save.svg"), "保存", bar);

    auto* infoAction = new QAction(QIcon(":/icons/menu/InfoFile.svg"), "工程信息", bar);
    infoAction->setPriority(QAction::HighPriority);
    auto* shareAction = new QAction(QIcon(":/icons/logo/Flash.png"), "Share", bar);

    auto* separator = new QAction(bar);
    separator->setSeparator(true);

    auto* syncAction = new QAction(
        QIcon(":/icons/logo/Flash.png"), "Sync", bar);
    syncAction->setPriority(QAction::LowPriority);
    m_templateAction = new QAction(QIcon(":/icons/menu/template-grey.svg"), "模板", bar);
    m_templateAction->setCheckable(true);

    bar->addPrimaryAction(actionFile);
    bar->addPrimaryAction(actionAddBlank);
    bar->addPrimaryAction(actionOpenProject);
    bar->addPrimaryAction(separator);//|
    bar->addPrimaryAction(actionSaveProject);
    bar->addPrimaryAction(infoAction);
    bar->addPrimaryAction(shareAction);
    bar->addPrimaryAction(separator);//|
    bar->addPrimaryAction(syncAction);
    bar->addPrimaryAction(m_templateAction);
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
    barLayout->addStretch();

    connect(actionFile,&QAction::triggered,UISignalManager::instance(),&UISignalManager::drawerFileSetting);
    connect(actionAddBlank,&QAction::triggered,UISignalManager::instance(),&UISignalManager::newProject);
    connect(actionOpenProject,&QAction::triggered,UISignalManager::instance(),&UISignalManager::openProject);
    connect(actionSaveProject,&QAction::triggered,UISignalManager::instance(),&UISignalManager::saveProject);
    connect(infoAction,&QAction::triggered,UISignalManager::instance(),&UISignalManager::popProjectsetting);

}

void TopMenu::initConnect()
{
    connect(m_templateAction,&QAction::triggered,this,&TopMenu::changIcon);
}

void TopMenu::changIcon(bool checked)
{
    if(checked){
        m_templateAction->setIcon(QIcon(":/icons/menu/template-blue.svg"));
    }
    else {
        m_templateAction->setIcon(QIcon(":/icons/menu/template-grey.svg"));
    }
}
