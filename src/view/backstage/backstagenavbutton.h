#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QString>

namespace doclife::ui {

    /**
     * Backstage 左侧导航项：自绘图标 + 文字 + 悬停/选中底色 + 选中左条。
     * 不使用全局样式表，避免被 Fluent 的 QSS 污染。
     */
    class BackstageNavButton : public QAbstractButton
    {
        Q_OBJECT
    public:
        enum class Role { Page, Action, Separator };

        BackstageNavButton(const QString& glyphName,
                           const QString& text,
                           Role role = Role::Page,
                           QWidget* parent = nullptr);

        static BackstageNavButton* makeSeparator(QWidget* parent = nullptr);

        Role role() const { return m_role; }
        QSize sizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:
        QString m_glyph;
        QString m_text;
        Role    m_role = Role::Page;
        bool    m_hover = false;
    };

} // namespace doclife::ui
