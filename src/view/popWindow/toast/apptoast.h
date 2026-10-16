#pragma once

#include <FluentQt/StatusInfo.h>
#include <QWidget>
#include <QString>
#include <QAction>
#include <functional>

// 请替换为你自己的命名空间
namespace doclife::ui {

    class AppToast : public fluent::status_info::Toast {
        Q_OBJECT

    public:
        explicit AppToast(QWidget* parent = nullptr);

        // —— 静态便捷方法：调用后自动管理生命周期 ——
        static void showInfo(QWidget* anchor, const QString& message,
            int durationMs = 3200);
        static void showSuccess(QWidget* anchor, const QString& message,
            int durationMs = 3200);
        static void showWarning(QWidget* anchor, const QString& message,
            int durationMs = 3200);
        static void showError(QWidget* anchor, const QString& message,
            int durationMs = 3200);

        // 带操作按钮的通知
        static void showWithAction(QWidget* anchor, const QString& message,
            Severity severity,
            const QString& actionText,
            std::function<void()> onAction,
            int durationMs = 2200);

        // —— 实例方法：手动配置后立即呈现 ——
        bool presentWithMessage(QWidget* anchor, const QString& message,
            Severity severity = Informational,
            int durationMs = 2200);
    };

} // namespace your_namespace