#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <projiectnew.h>
#include <FluentQt/FluentQt.h>

#include "topmenu.h"
#include "projectsetting.h"
#include "uisignalmanager.h"

using namespace doclife::ui;
// MainWindow.cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("_公路资料管理");
    // ---------- 顶部按钮区 -----------
    auto *topBar = new TopMenu();

    // ----------- 中部区域  -----------

    auto *splitter = new fluent::collections::SplitView(this);
    auto *left   = new QWidget();
    left->setStyleSheet("background:#eAf;");
    auto *m_mainContent = new QWidget();
    m_mainContent->setStyleSheet("background:#ef1;");
    // auto *right  = new QWidget();

    splitter->addPane(left,{15,200,550,false});
    splitter->addPane(m_mainContent);
    // splitter->addPane(m_mainContent,{350,450,650,true});
    // splitter->setPanePreferredSize(0, 150);
    splitter->setPaneFill(1, true);
    // left->setMinimumWidth(50);
    // m_mainContent->setMinimumWidth(120);
    // right->setMinimumWidth(50);
    // left  ->setStyleSheet("background:#eAf;");
    //
    // right ->setStyleSheet("background:#aee;");
    // splitter->addWidget(left);
    // splitter->addWidget(m_mainContent);
    // splitter->addWidget(right);
    // splitter->setSizes({200, 700, 300});
    // splitter->setChildrenCollapsible(false);
    // ---------- 外层垂直布局：上按钮、下 splitter ----------
    auto *central = new QWidget(this);
    auto *v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(topBar);       // 顶
    v->addWidget(splitter, 1);  // 余下空间全给 splitter
    // topBar->setStyleSheet("background-color: rgba(0, 0, 119, 60);");
    setCentralWidget(central);

    initConnect();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showProjectsetting()
{
    doclife::ui::ProjectSetting* pop = new doclife::ui::ProjectSetting();
    pop->show(this);
}
void MainWindow::showProjectNew()
{
    doclife::ui::ProjiectNew* pop = new doclife::ui::ProjiectNew();
    pop->show(this);
}

void MainWindow::initConnect()
{
    // connect(m_mainOpenBtn,&QPushButton::clicked,UISignalManager::instance(),&UISignalManager::openExcelRequested);
    connect(UISignalManager::instance(),&UISignalManager::popProjectNew,this,&MainWindow::showProjectNew);
    connect(UISignalManager::instance(),&UISignalManager::popProjectsetting,this,&MainWindow::showProjectsetting);
}
