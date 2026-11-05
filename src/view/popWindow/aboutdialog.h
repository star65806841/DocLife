#pragma once

#include <components/dialogs_flyouts/Dialog.h>

namespace doclife::ui {

    /**
     * 关于弹窗：软件信息 + 第三方组件与许可声明。
     */
    class AboutDialog : public fluent::dialogs_flyouts::Dialog
    {
        Q_OBJECT
    public:
        explicit AboutDialog(QWidget* parent = nullptr);

    private:
        static QString buildContent();
    };

} // namespace doclife::ui
