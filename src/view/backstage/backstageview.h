#pragma once

#include <QWidget>

#include <functional>

class QVBoxLayout;
class QStackedWidget;
class QScrollArea;

namespace doclife::ui {

    class BackstageNavButton;
    class BackstagePageBase;

    /**
     * Office 风格 Backstage（文件抽屉）：覆盖在内容区之上，
     * 左侧自绘导航 + 右侧页面栈，完全脱离 Fluent 全局样式。
     */
    class BackstageView : public QWidget
    {
        Q_OBJECT
    public:
        explicit BackstageView(QWidget* parent = nullptr);

        void toggle();
        void openBackstage();
        void closeBackstage();
        bool isOpen() const { return isVisible(); }

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void showEvent(QShowEvent* event) override;

    private:
        void buildUi();
        void addPageItem(const QString& glyph, const QString& text,
                         BackstagePageBase* page, bool checked = false);
        void addActionItem(const QString& glyph, const QString& text,
                           const std::function<void()>& action);
        void addSeparatorItem();
        void select(int navIndex);
        void moveFocus(int delta);

        QVBoxLayout*      m_leftLayout = nullptr;
        QScrollArea*      m_leftScroll = nullptr;
        QStackedWidget*   m_stack = nullptr;

        QList<BackstageNavButton*> m_navButtons;
        QList<int>                 m_navToPage;   // -1 = 动作项
        int m_current = -1;
    };

} // namespace doclife::ui
