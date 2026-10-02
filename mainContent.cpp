#include "MainContent.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QHeaderView>
#include <QLabel>
#include <QAbstractItemView>
#include <QTableView>
#include <QDebug>

#include "utils.h"
#include "uisignalmanager.h"
#include "excelfile.h"

MainContent::MainContent(QWidget *parent)
    : QWidget{parent}
{
    setAttribute(Qt::WA_StyledBackground, true);
    setupUi();
}

MainContent::~MainContent()
{
    qDebug() << "~MainContent";
}

void MainContent::onGetExcelFilePath()
{
    m_filePath = Utils::openExcelFile(this);
    if(m_filePath.isEmpty())
    {
        qDebug()<<"didn't get empty file path";
        return;
    }
    emit UISignalManager::instance()->fileOpened(m_filePath);
    qDebug()<<"get the filepath:"<<m_filePath;
    if(!m_excelFile)
    {
        m_excelFile = new ExcelFile(this);
    }
    m_excelFile->openExcelFile(m_filePath);
}

void MainContent::setupUi()
{
    // ============================================================
    // 一、上半部分：表格功能区
    // ============================================================
    m_topArea = new QWidget(this);

    // --- 1) 操作按钮条 ---
    auto *toolBar    = new QWidget(m_topArea);
    auto *toolLayout = new QHBoxLayout(toolBar);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(6);

    m_btnAdd    = new QPushButton(tr("新增"), toolBar);
    m_btnRemove = new QPushButton(tr("删除"), toolBar);
    m_btnEdit   = new QPushButton(tr("编辑"), toolBar);

    m_searchEdit = new QLineEdit(toolBar);
    m_searchEdit->setPlaceholderText(tr("搜索..."));
    m_searchEdit->setFixedWidth(200);
    m_searchEdit->setClearButtonEnabled(true);

    toolLayout->addWidget(m_btnAdd);
    toolLayout->addWidget(m_btnRemove);
    toolLayout->addWidget(m_btnEdit);
    toolLayout->addStretch();
    toolLayout->addWidget(m_searchEdit);

    // --- 2) 上半部分布局：按钮条在上、表格在下 ---
    auto *topLayout = new QVBoxLayout(m_topArea);
    topLayout->setContentsMargins(6, 6, 6, 0);
    topLayout->setSpacing(6);
    topLayout->addWidget(toolBar);

    // 如果 m_table 还没创建，需要在这里创建：
    // m_table = new QTableWidget(m_topArea);
    // topLayout->addWidget(m_table, 1);

    // ============================================================
    // 二、下半部分：QTabWidget
    // ============================================================
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);

    auto *tabDetail  = new QWidget;
    auto *tabLog     = new QWidget;
    auto *tabSetting = new QWidget;

    // 预览页：先建控件，再建布局
    {
        m_crtExcelPreview = new QTableWidget(tabDetail);

        auto *l = new QVBoxLayout(tabDetail);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(m_crtExcelPreview);
    }

    // 日志页
    {
        auto *l2 = new QVBoxLayout(tabLog);
        l2->addWidget(new QLabel(tr("这里放日志内容")));
    }

    // 设置页
    {
        auto *l3 = new QVBoxLayout(tabSetting);
        l3->addWidget(new QLabel(tr("这里放设置内容")));
    }

    m_tabWidget->addTab(tabDetail,  tr("预览"));
    m_tabWidget->addTab(tabLog,     tr("日志"));
    m_tabWidget->addTab(tabSetting, tr("设置"));

    // ============================================================
    // 三、主布局
    // ============================================================
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_topArea);
    mainLayout->addWidget(m_tabWidget);

    // ============================================================
    // 四、信号连接
    // ============================================================
    connect(m_btnAdd,     &QPushButton::clicked,   this, &MainContent::addRequested);
    connect(m_btnRemove,  &QPushButton::clicked,   this, &MainContent::removeRequested);
    connect(m_btnEdit,    &QPushButton::clicked,   this, &MainContent::editRequested);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainContent::searchChanged);
    connect(UISignalManager::instance(),&UISignalManager::openExcelRequested, this, &MainContent::onGetExcelFilePath);
}