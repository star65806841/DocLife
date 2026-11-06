#pragma once

#include <QWidget>

namespace fluent::navigation { class StackContentHost; }

namespace doclife::ui {

    class BackstagePageBase;

    /**
     * 文件抽屉的内容面板：顶部「← 文件」+ 仿工程信息设置页的左侧导航。
     * 自身不负责显示/隐藏，由 DrawerView 承载。
     */
    class BackstageView : public QWidget
    {
        Q_OBJECT
    public:
        explicit BackstageView(QWidget* parent = nullptr);

    signals:
        // 用户点了「← 文件」：由宿主关闭抽屉
        void requestClose();

    private:
        void buildUi();
        void showPage(int index);

        fluent::navigation::StackContentHost* m_host = nullptr;
        QList<BackstagePageBase*> m_pages;
    };

} // namespace doclife::ui
