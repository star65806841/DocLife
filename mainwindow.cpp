#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSplitter>

#include "mainContent.h"
#include "uisignalmanager.h"
#include "ExcelTreeWidget.h"

// MainWindow.cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ---------- 顶部按钮区 ----------
    m_topBar = new QWidget(this);

    auto *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(6, 6, 6, 6);
    topLayout->setSpacing(6);

    m_mainOpenBtn   = new QPushButton(tr("打开"), m_topBar);
    auto *btnSave   = new QPushButton(tr("保存"), m_topBar);
    auto *btnRun    = new QPushButton(tr("运行"), m_topBar);

    topLayout->addWidget(m_mainOpenBtn);
    topLayout->addWidget(btnSave);
    topLayout->addWidget(btnRun);
    topLayout->addStretch();          // 把按钮挤到左边，右边留空

    // ---------- 中间三分区（原来的 splitter） ----------
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    auto *left   = new QWidget();
    m_mainContent = new MainContent();
    auto *right  = new QWidget();

    // left->setMinimumWidth(50);
    // m_mainContent->setMinimumWidth(120);
    // right->setMinimumWidth(50);
    left  ->setStyleSheet("background:#eAf;");
    m_mainContent->setStyleSheet("background:#ef1;");
    right ->setStyleSheet("background:#aee;");
    splitter->addWidget(left);
    splitter->addWidget(m_mainContent);
    splitter->addWidget(right);
    splitter->setSizes({200, 700, 300});
    splitter->setChildrenCollapsible(false);
    // ---------- 外层垂直布局：上按钮、下 splitter ----------
    auto *central = new QWidget(this);
    auto *v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(m_topBar);       // 顶
    v->addWidget(splitter, 1);  // 余下空间全给 splitter
    m_topBar->setStyleSheet("background-color: rgba(0, 0, 119, 60);");
    setCentralWidget(central);

    initConnect();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initConnect()
{
    connect(m_mainOpenBtn,&QPushButton::clicked,UISignalManager::instance(),&UISignalManager::openExcelRequested);
}
