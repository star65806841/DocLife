#ifndef POPUPBASE_H
#define POPUPBASE_H

#include <components/dialogs_flyouts/Popup.h>

#include <QPointer>
#include <QWidget>

namespace doclife::ui {

/**
 * @brief 项目通用 Popup 基类。
 *
 * 只补充 fluent-qt Popup 缺失的部分：
 *   - 内容控件托管（setContentWidget / contentWidget）
 *   - 统一的默认关闭策略、动画、模态、遮罩
 *   - showAt(anchor) 便捷定位
 *   - onPopupOpened / onPopupClosed 虚函数回调
 *
 * 其余接口（open/close/isOpen、setModal、setDim、setAnimationEnabled、
 * setExitAnimationEnabled、setThemeSource、setPosition、setLightDismissConsumesPress
 * 以及所有信号）全部直接复用基类 Popup，不再重复定义。
 */
class PopupBase : public fluent::dialogs_flyouts::Popup
{
    Q_OBJECT

public:
    // 直接复用基类类型，避免重复定义，避免 |= 报错
    using CloseFlag   = fluent::dialogs_flyouts::Popup::CloseFlag;
    using ClosePolicy = fluent::dialogs_flyouts::Popup::ClosePolicy;
    using CloseReason = fluent::dialogs_flyouts::Popup::CloseReason;

    explicit PopupBase(QWidget *parent = nullptr);
    ~PopupBase() override;

    // ─── 内容管理（基类没有，这里补上） ─────────────────────────

    /**
     * @brief 设置 Popup 承载的内容控件；Popup 接管其父子关系。
     *        旧的 contentWidget 会被 deleteLater 释放。
     */
    void setContentWidget(QWidget *widget);

    /**
     * @brief 返回当前内容控件，可能为 nullptr。
     */
    QWidget *contentWidget() const;

    // ─── 定位便捷方法 ──────────────────────────────────────────

    /**
     * @brief 相对 anchor 的 (0,0) 偏移 offset 定位并打开。
     *        内部调用基类 setPosition(anchor, offset) + open()。
     */
    void showAt(QWidget *anchor, const QPoint &offset = QPoint(0, 0));

    // ─── 项目默认值 ────────────────────────────────────────────

    /**
     * @brief 应用项目统一的默认选项。
     *        构造函数已自动调用一次；子类如需覆盖可在构造后再次调用。
     */
    void applyDefaultOptions();

protected:
    /**
     * @brief 打开动画完成后的回调，子类重写即可，无需连信号。
     */
    virtual void onPopupOpened() {}

    /**
     * @brief 关闭动画完成后的回调。
     */
    virtual void onPopupClosed() {}

private:
    QPointer<QWidget> m_contentWidget;

    Q_DISABLE_COPY_MOVE(PopupBase)
};

#endif // POPUPBASE_H
}