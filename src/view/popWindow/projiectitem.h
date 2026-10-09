#ifndef MAINSECTION_H
#define MAINSECTION_H

#include <QWidget>
#include <QVector>
#include <functional>

#include "components/foundation/FluentElement.h"

class QBoxLayout;

namespace fluent::navigation { class StackContentHost; }

// 单条导航项（对应测试中的 NavigationItemRow）
class NavigationItemRow : public QWidget, public fluent::FluentElement
{
    Q_OBJECT
public:
    explicit NavigationItemRow(const QString& iconGlyph,
                               const QString& text,
                               bool selected = false,
                               QWidget* parent = nullptr);

    QSize sizeHint() const override;

    void setSelected(bool selected);
    // void setCompact(bool compact);
    bool selectedIndicatorVisible() const { return m_selected && !m_compact; }

    std::function<void()> onActivated;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QString m_iconGlyph;
    QString m_text;
    bool m_selected = false;
    bool m_compact = false;
    bool m_hovered = false;
    bool m_pressed = false;
};

// 导航项列表容器（对应测试中的 NavigationPaneSection）
class MainSection : public QWidget, public fluent::FluentElement
{
    Q_OBJECT
public:
    struct Entry {
        QString icon;
        QString text;
        bool selected = false;
    };

    explicit MainSection(const QVector<Entry>& entries,
                         QWidget* parent = nullptr);

    // QSize sizeHint() const override;
    // QSize minimumSizeHint() const override;

    // void setCompact(bool compact);
    void setSelectedIndex(int index);
    // void clearSelection();

    // void onThemeUpdated() override;

    // 点击某项时触发，参数为索引
    std::function<void(int)> onActivated;

private:
    QBoxLayout* m_layout = nullptr;
    QVector<NavigationItemRow*> m_rows;
    bool m_compact = false;
};

#endif // MAINSECTION_H