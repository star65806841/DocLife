#include "lefttop.h"

#include <QHBoxLayout>

#include <components/menus_toolbars/CommandBar.h>

#include <components/basicinput/Button.h>

namespace doclife::ui {
    LeftTop::LeftTop(QWidget* parent)
        : QWidget{ parent }
    {
        init();
    }
    void LeftTop::init()
    {
        auto *barLayout = new QHBoxLayout(this);
        barLayout->setContentsMargins(2, 0, 0, 2);
        barLayout->setSpacing(6);

        auto* bar = new fluent::menus_toolbars::CommandBar(this);

        bar->setAccessibleName("Document commands");
        bar->setLabelPosition(fluent::menus_toolbars::CommandBar::LabelPosition::Collapsed);
        bar->setBackgroundVisible(false);

        auto* actionFile = new QAction(QIcon(":/icons/menu/AddFile.svg"), "文件测试", bar);
        auto* actionAddBlank = new QAction(QIcon(":/icons/menu/AddFile.svg"), "新建空白", bar);
        auto* actionOpenProject = new QAction(QIcon(":/icons/menu/file_open.svg"), "打开", bar);
        auto* actionSaveProject = new QAction(QIcon(":/icons/menu/file_save.svg"), "保存", bar);

        auto* infoAction = new QAction(QIcon(":/icons/menu/InfoFile.svg"), "工程信息", bar);
        infoAction->setPriority(QAction::HighPriority);
        auto* linearMeterSetting = new QAction(QIcon(":/icons/logo/Flash.png"), "延米设置", bar);

        auto* separator = new QAction(bar);
        separator->setSeparator(true);

        auto* syncAction = new QAction(
            QIcon(":/icons/logo/Flash.png"), "Sync", bar);
        syncAction->setPriority(QAction::LowPriority);
        auto* pinAction = new QAction(
            QIcon(":/icons/logo/Flash.png"), "Pin", bar);
        pinAction->setCheckable(true);

        bar->addPrimaryAction(actionFile);
        bar->addPrimaryAction(actionAddBlank);
        bar->addPrimaryAction(actionOpenProject);
        bar->addPrimaryAction(actionSaveProject);
        bar->addPrimaryAction(infoAction);
        bar->addPrimaryAction(linearMeterSetting);

        for (auto *btn : bar->findChildren<fluent::basicinput::Button*>()) {
            btn->setFocusVisual(false);
        }

        barLayout->addWidget(bar);
        barLayout->addStretch();
    }
}