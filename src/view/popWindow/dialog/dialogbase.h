#ifndef DIALOGBASE_H
#define DIALOGBASE_H

#include <components/dialogs_flyouts/ContentDialog.h>
#include <functional>

namespace doclife::ui {

class DialogBase : public fluent::dialogs_flyouts::ContentDialog
{
    Q_OBJECT

public:
    using ConfirmCallback = std::function<void()>;
    using CancelCallback  = std::function<void()>;

    explicit DialogBase(const QString& title,
                        const QString& content,
                        QWidget* parent = nullptr);

    // —— 注册回调（推荐在构造完成后立刻调用） ——
    void setOnConfirmed(ConfirmCallback cb) { m_onConfirmed = std::move(cb); }
    void setOnCanceled(CancelCallback cb)   { m_onCanceled  = std::move(cb); }

    // —— 静态便捷方法：创建 → 注册回调 → 模态显示 → 关闭后自动释放 ——
    static DialogBase* show(QWidget* parent,
                            const QString& title,
                            const QString& content,
                            ConfirmCallback onConfirmed = nullptr,
                            CancelCallback  onCanceled  = nullptr,
                            bool autoDelete = true);

signals:
    // 除了回调，也提供 Qt 信号，方便用 connect 方式对接
    void confirmed();
    void canceled();

private:
    ConfirmCallback m_onConfirmed;
    CancelCallback  m_onCanceled;

    void initUi(const QString& title, const QString& content);
    void handleFinished(int result);
};

} // namespace doclife::ui

#endif // DIALOGBASE_H