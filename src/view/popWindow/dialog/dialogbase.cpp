#include "dialogbase.h"

#include <components/textfields/Label.h>
#include <QHBoxLayout>
#include <QWidget>
#include <QDebug>

namespace doclife::ui {

DialogBase::DialogBase(const QString& title,
                       const QString& content,
                       QWidget* parent)
    : fluent::dialogs_flyouts::ContentDialog(parent)
{
    initUi(title, content);

    // 无论是点“确定”、“取消”、按 Esc 还是点右上角关闭，
    // 最终都会走到 QDialog::finished，这里统一分发回调/信号
    connect(this, &QDialog::finished, this, &DialogBase::handleFinished);
}

DialogBase::~DialogBase()
{
    qDebug() << "~DialogBase";
}

void DialogBase::initUi(const QString& title, const QString& content)
{
    // ---------------- 内容区 ----------------
    auto* contentWid = new QWidget(this);
    auto* h1 = new QHBoxLayout(contentWid);
    h1->setSpacing(10);
    h1->setContentsMargins(10, 10, 10, 0);

    auto* label = new fluent::textfields::Label(content, contentWid);
    label->setMinimumWidth(100);
    label->setFluentTypography(Typography::FontRole::Body);
    label->setWordWrap(true);
    h1->addWidget(label, 1);   // 拉伸因子 1，让文字占满宽度

    setContent(contentWid);

    // ---------------- 基础配置 ----------------
    setTitle(title);
    setPrimaryButtonText(QStringLiteral("确定"));
    setCloseButtonText(QStringLiteral("取消"));
    setDefaultButton(fluent::dialogs_flyouts::ContentDialog::Primary);
    setAnimationEnabled(true);

    // 模态 + 变暗。注意：不要再用 setSmokeEnabled()，
    // 它是历史兼容包，语义模糊且与 setModal/setDim 重复。
    setModal(true);
    setDim(true);

    // 用最小尺寸代替固定尺寸，避免内容稍长就被截断
    setMinimumSize(QSize(410, 250));
}

void DialogBase::handleFinished(int result)
{
    if (result == QDialog::Accepted) {
        if (m_onConfirmed) m_onConfirmed();
        emit confirmed();
    } else {
        if (m_onCanceled) m_onCanceled();
        emit canceled();
    }
}

DialogBase* DialogBase::show(QWidget* parent,
                             const QString& title,
                             const QString& content,
                             ConfirmCallback onConfirmed,
                             CancelCallback  onCanceled,
                             bool autoDelete)
{
    auto* dlg = new DialogBase(title, content, parent);
    dlg->setOnConfirmed(std::move(onConfirmed));
    dlg->setOnCanceled(std::move(onCanceled));

    if (autoDelete) {
        QObject::connect(dlg, &QDialog::finished, dlg, &QObject::deleteLater);
    }

    dlg->open();   // 非阻塞式模态：不冻结当前线程，UI 依然响应
    return dlg;
}

} // namespace doclife::ui