#include "PopupBase.h"

#include <QVBoxLayout>

#include <components/basicinput/Button.h>

namespace doclife::ui {

PopupBase::PopupBase(QWidget *parent)
    : fluent::dialogs_flyouts::Popup(parent)
{
    // 内容区使用一个零边距的垂直布局，方便直接挂载子控件
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(35,30,20,15);
    layout->setSpacing(0);

    auto* btnClose = new fluent::basicinput::Button("关闭", this);
    btnClose->setFluentSize(fluent::basicinput::Button::Large);
    btnClose->setFixedSize(85,30);
    btnClose->setLayoutDirection(Qt::RightToLeft);
    btnClose->setFluentStyle(fluent::basicinput::Button::Standard);
    layout->addStretch();
    layout->addWidget(btnClose);

    applyDefaultOptions();

    // 基类已经有 opened() / closed() 信号，转发到虚函数回调。
    // 只触发一次，切到成员函数上即可；不会在构造期立刻触发。
    connect(this, &Popup::opened, this, [this] { onPopupOpened(); });
    connect(this, &Popup::closed, this, [this] { onPopupClosed(); });
    connect(btnClose, &fluent::basicinput::Button::clicked, this, &Popup::close);
}

PopupBase::~PopupBase()
{
    qDebug()<<"~PopupBase";
}

// ─── 内容管理 ──────────────────────────────────────────────────

void PopupBase::setContentWidget(QWidget *widget)
{
    if (m_contentWidget == widget) {
        return;
    }

    if (m_contentWidget) {
        m_contentWidget->deleteLater();
    }

    m_contentWidget = widget;
    if (widget && widget->parentWidget() != this) {
        widget->setParent(this);
    }

    if (widget) {
        if (auto *layout = qobject_cast<QVBoxLayout *>(this->layout())) {
            layout->insertWidget(0,widget);
        }
    }
}

QWidget *PopupBase::contentWidget() const
{
    return m_contentWidget;
}

// ─── 定位 ──────────────────────────────────────────────────────

void PopupBase::showAt(QWidget *anchor, const QPoint &offset)
{
    if (!anchor) {
        return;
    }
    setPosition(anchor, offset);
    open();
}

// ─── 默认值 ────────────────────────────────────────────────────

void PopupBase::applyDefaultOptions()
{
    // 关闭策略：点击外部 + ESC 都关闭
    // CloseOnPressOutside 与 CloseOnEscape 都是 CloseFlag，
    // 直接用 QFlags 的 operator| 组合，结果类型是 ClosePolicy。
    setClosePolicy(CloseOnPressOutside | CloseOnEscape);

    setAnimationEnabled(true);
    setExitAnimationEnabled(true);
    setModal(false);
    setDim(false);
    setLightDismissConsumesPress(false);
}

void PopupBase::onPopupClosed()
{
    this->deleteLater();
}

} // namespace doclife::ui