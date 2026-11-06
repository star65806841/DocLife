#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QVBoxLayout;

namespace doclife::ui {

    // 最近打开的工程（QSettings，最多 10 条）
    namespace backstage {
        QStringList recentFiles();
        void addRecentFile(const QString& path);
        void clearRecentFiles();
    }

    // 页面基类：切到/离开时回调，便于刷新数据
    class BackstagePageBase : public QWidget
    {
        Q_OBJECT
    public:
        explicit BackstagePageBase(QWidget* parent = nullptr) : QWidget(parent) {}
        virtual void onShown() {}
        virtual void onHidden() {}
    };

    // 新建：空白工程 / 打开已有 / 模板库（占位）+ 最近打开
    class BackstageNewPage : public BackstagePageBase
    {
        Q_OBJECT
    public:
        explicit BackstageNewPage(QWidget* parent = nullptr);
        void onShown() override;

    signals:
        void requestBlank();
        void requestOpen();
        void requestOpenPath(const QString& path);

    private:
        void rebuildRecent();
        QVBoxLayout* m_recentLayout = nullptr;
    };

    // 打开：历史列表 + 浏览
    class BackstageOpenPage : public BackstagePageBase
    {
        Q_OBJECT
    public:
        explicit BackstageOpenPage(QWidget* parent = nullptr);
        void onShown() override;

    signals:
        void requestBrowse();
        void requestOpenPath(const QString& path);

    private:
        void rebuildList();
        QVBoxLayout* m_listLayout = nullptr;
    };

    // 设置：入口按钮（复用已有的设置/工程信息/关于弹窗）
    class BackstageSettingsPage : public BackstagePageBase
    {
        Q_OBJECT
    public:
        explicit BackstageSettingsPage(QWidget* parent = nullptr);

    signals:
        void requestSettings();
        void requestProjectInfo();
        void requestAbout();
    };

    // 占位页：打印 / 用户（后续接入）
    class BackstagePlaceholderPage : public BackstagePageBase
    {
    public:
        BackstagePlaceholderPage(const QString& title, const QString& hint, QWidget* parent = nullptr);
    };

} // namespace doclife::ui
