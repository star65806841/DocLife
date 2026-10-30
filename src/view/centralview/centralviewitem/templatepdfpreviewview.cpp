#include "templatepdfpreviewview.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QSpinBox>
#include <QFileInfo>
#include <QFile>
#include <QTemporaryFile>

#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfPageNavigator>
#include <QtPdfWidgets/QPdfView>

namespace {

    // 显式指定 3 参数重载，避免与 jump(QPdfLink) 歧义
    using Jump3Fn = void (QPdfPageNavigator::*)(int, const QPointF&, qreal);

    inline void jumpToPage0(QPdfPageNavigator* nav, int page0,
        const QPointF& loc = {}, qreal zoom = 0)
    {
        static const Jump3Fn kFn = static_cast<Jump3Fn>(&QPdfPageNavigator::jump);
        (nav->*kFn)(page0, loc, zoom);
    }

} // namespace

namespace doclife::ui {

    // ---------------------------------------------------------------------------
    // 构造 / 析构
    // ---------------------------------------------------------------------------

    TemplatePdfPreviewView::TemplatePdfPreviewView(QWidget* parent)
        : QWidget(parent)
        , m_doc(new QPdfDocument(this))
        , m_view(new QPdfView(this))
    {
        m_view->setPageMode(QPdfView::PageMode::SinglePage);
        m_view->setZoomMode(QPdfView::ZoomMode::FitInView);

        m_pathLabel = new QLabel(this);
        m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        m_pageSpin = new QSpinBox(this);
        m_pageSpin->setMinimum(1);
        m_pageSpin->setMaximum(1);

        m_pageCount = new QLabel(QStringLiteral("/ 0"), this);

        m_prevBtn = new QToolButton(this);
        m_prevBtn->setText(QStringLiteral("上一页"));
        m_nextBtn = new QToolButton(this);
        m_nextBtn->setText(QStringLiteral("下一页"));

        auto* bar = new QHBoxLayout;
        bar->setContentsMargins(8, 4, 8, 4);
        bar->setSpacing(8);
        bar->addWidget(m_prevBtn);
        bar->addWidget(m_pageSpin);
        bar->addWidget(m_pageCount);
        bar->addWidget(m_nextBtn);
        bar->addStretch(1);
        bar->addWidget(m_pathLabel);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addLayout(bar);
        layout->addWidget(m_view, 1);

        // 按钮翻页
        connect(m_prevBtn, &QToolButton::clicked, this, [this] {
            m_pageSpin->setValue(m_pageSpin->value() - 1);
            });
        connect(m_nextBtn, &QToolButton::clicked, this, [this] {
            m_pageSpin->setValue(m_pageSpin->value() + 1);
            });

        // spinbox(1-based) → navigator(0-based)
        connect(m_pageSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TemplatePdfPreviewView::goToPage);

        // 用户用滚轮/方向键翻页时，同步回 spinbox
        if (auto* nav = m_view->pageNavigator()) {
            connect(nav, &QPdfPageNavigator::currentPageChanged,
                this, &TemplatePdfPreviewView::syncPageFromNavigator);
        }

        // 文档就绪后刷新 UI（兼容异步加载）
        connect(m_doc, &QPdfDocument::statusChanged,
            this, [this](QPdfDocument::Status s) {
                if (s == QPdfDocument::Status::Ready)
                    refreshFromDoc();
                else if (s == QPdfDocument::Status::Error)
                    resetToErrorState(QStringLiteral("PDF 加载失败"));
            });
    }

    TemplatePdfPreviewView::~TemplatePdfPreviewView() = default;

    // ---------------------------------------------------------------------------
    // 槽函数
    // ---------------------------------------------------------------------------

    void TemplatePdfPreviewView::goToPage(int page1Based)
    {
        if (m_syncing)
            return;
        if (!m_doc || m_doc->pageCount() <= 0)
            return;

        auto* nav = m_view->pageNavigator();
        if (!nav)
            return;

        const int page0 = qBound(0, page1Based - 1, m_doc->pageCount() - 1);
        jumpToPage0(nav, page0);   // zoom = 0 → 保持当前缩放
    }

    void TemplatePdfPreviewView::syncPageFromNavigator(int page0Based)
    {
        m_syncing = true;
        m_pageSpin->setValue(page0Based + 1);
        m_syncing = false;
    }

    // ---------------------------------------------------------------------------
    // 对外接口
    // ---------------------------------------------------------------------------

    void TemplatePdfPreviewView::loadPdf(const QString& pdfPath, int page)
    {
        // 1) 解绑旧文档，防止 view 访问已释放对象
        m_view->setDocument(nullptr);
        m_doc->close();
        m_tmpPdf.reset();

        m_pathLabel->setText(QStringLiteral("正在加载…"));

        // 2) 直接加载（Qt 6 返回 void，状态通过 status() 查询）
        m_doc->load(pdfPath);

        // 3) qrc 路径兜底：拷到临时文件后 load(QIODevice*)
        //    Qt 6 会接管 device 指针，device 必须长期存活 → 成员 m_tmpPdf
        if (m_doc->status() == QPdfDocument::Status::Error &&
            pdfPath.startsWith(QLatin1Char(':'))) {
            QFile src(pdfPath);
            if (src.open(QIODevice::ReadOnly)) {
                auto tmp = std::make_unique<QTemporaryFile>();
                if (tmp->open()) {
                    tmp->write(src.readAll());
                    tmp->flush();
                    m_doc->load(tmp.get());
                    if (m_doc->status() != QPdfDocument::Status::Error)
                        m_tmpPdf = std::move(tmp);   // 保活
                }
            }
        }

        // 4) 立即失败的话给出错误 UI
        if (m_doc->status() == QPdfDocument::Status::Error) {
            resetToErrorState(QStringLiteral("无法加载 PDF：%1")
                .arg(QFileInfo(pdfPath).fileName()));
            return;
        }

        // 5) 绑定到 view
        m_view->setDocument(m_doc);

        // 6) 本地文件通常同步 ready；异步场景由 statusChanged 兜底
        if (m_doc->status() == QPdfDocument::Status::Ready)
            refreshFromDoc();

        m_pathLabel->setText(QFileInfo(pdfPath).fileName());
    }

    // ---------------------------------------------------------------------------
    // 内部辅助
    // ---------------------------------------------------------------------------

    void TemplatePdfPreviewView::refreshFromDoc()
    {
        if (!m_doc || m_doc->status() != QPdfDocument::Status::Ready)
            return;

        const int pages = m_doc->pageCount();
        if (pages <= 0)
            return;

        m_pageSpin->setMinimum(1);
        m_pageSpin->setMaximum(pages);
        m_pageCount->setText(QStringLiteral("/ %1").arg(pages));
        m_prevBtn->setEnabled(pages > 1);
        m_nextBtn->setEnabled(pages > 1);

        if (auto* nav = m_view->pageNavigator())
            jumpToPage0(nav, 0);

        // 同步 spinbox，不触发 goToPage
        m_syncing = true;
        m_pageSpin->setValue(1);
        m_syncing = false;
    }

    void TemplatePdfPreviewView::resetToErrorState(const QString& msg)
    {
        m_view->setDocument(nullptr);

        m_pathLabel->setText(msg);
        m_pageSpin->setMinimum(1);
        m_pageSpin->setMaximum(1);
        m_pageSpin->setValue(1);
        m_pageCount->setText(QStringLiteral("/ 0"));
        m_prevBtn->setEnabled(false);
        m_nextBtn->setEnabled(false);
    }

} // namespace doclife::ui