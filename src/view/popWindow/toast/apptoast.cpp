#include "apptoast.h"

namespace doclife::ui {

    namespace {
        AppToast* createToast(QWidget* anchor) {
            if (!anchor) return nullptr;
            auto* toast = new AppToast(anchor->window());
            QObject::connect(toast, &fluent::status_info::Toast::dismissed,
                toast, &QObject::deleteLater);
            return toast;
        }
    } // namespace

    AppToast::AppToast(QWidget* parent)
        : fluent::status_info::Toast(parent)
    {
    }

    bool AppToast::presentWithMessage(QWidget* anchor, const QString& message,
        Severity severity, int durationMs)
    {
        setMessage(message);
        setSeverity(severity);
        setDuration(durationMs);
        return present(anchor);
    }

    void AppToast::showInfo(QWidget* anchor, const QString& message, int durationMs)
    {
        auto* toast = createToast(anchor);
        if (!toast) return;
        toast->setMessage(message);
        toast->setSeverity(Informational);
        toast->setDuration(durationMs);
        if (!toast->present(anchor)) {
            delete toast;
        }
    }

    void AppToast::showSuccess(QWidget* anchor, const QString& message, int durationMs)
    {
        auto* toast = createToast(anchor);
        if (!toast) return;
        toast->setMessage(message);
        toast->setSeverity(Success);
        toast->setDuration(durationMs);
        if (!toast->present(anchor)) {
            delete toast;
        }
    }

    void AppToast::showWarning(QWidget* anchor, const QString& message, int durationMs)
    {
        auto* toast = createToast(anchor);
        if (!toast) return;
        toast->setMessage(message);
        toast->setSeverity(Warning);
        toast->setDuration(durationMs);
        if (!toast->present(anchor)) {
            delete toast;
        }
    }

    void AppToast::showError(QWidget* anchor, const QString& message, int durationMs)
    {
        auto* toast = createToast(anchor);
        if (!toast) return;
        toast->setMessage(message);
        toast->setSeverity(Error);
        toast->setDuration(durationMs);
        if (!toast->present(anchor)) {
            delete toast;
        }
    }

    void AppToast::showWithAction(QWidget* anchor, const QString& message,
        Severity severity,
        const QString& actionText,
        std::function<void()> onAction,
        int durationMs)
    {
        auto* toast = createToast(anchor);
        if (!toast) return;

        toast->setMessage(message);
        toast->setSeverity(severity);
        toast->setDuration(durationMs);

        auto* action = new QAction(actionText, toast);
        QObject::connect(action, &QAction::triggered, toast, [toast, onAction]() {
            if (onAction) {
                onAction();
            }
            toast->dismiss();
            });
        toast->setAction(action);

        if (!toast->present(anchor)) {
            delete toast;
        }
    }

} // namespace your_namespace
