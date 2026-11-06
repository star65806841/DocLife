#ifndef CENTRALVIEW_H
#define CENTRALVIEW_H

#include <QWidget>
#include <QDate>

namespace doclife::core
{
    class ProjectContext;
}
namespace doclife::ui {
    class MainContentView;
    class ProjectView;
    class TemplateView;
    class BackstageView;

    class CentralView : public QWidget
    {
        Q_OBJECT
    public:
        explicit CentralView(QWidget* parent = nullptr);
    signals:

    public slots:

    private slots:
        void newProject();
        void saveProject();
        void openProject();
        void onDrawerFileSetting();

        void showProjectDialog();
        void showProjectNew();
        void showProjectsetting();

        void popOpenedWarning();

        void projectClosed();
        void popProjectSaved();
        void popProjectCreated();
        void popProjectOpened();
        void onToastWarning(const QString &msg);
        void onRequestAddSheet(const QString& parentNodeId);
        // 时间区域双击：统一由主界面弹日历窗，确认后发回执信号
        void onDatePickRequested(int sheetId, int row, int col, const QDate& current);
    private:
        //project context data
        doclife::core::ProjectContext* m_projectObject = nullptr;

        MainContentView* m_mainContent = nullptr;
        ProjectView* m_leftView = nullptr;
        TemplateView* m_templateView = nullptr;
        BackstageView* m_backstage = nullptr;

    private:

        void init();
        void initConnect();
    };
}
#endif // CENTRALVIEW_H
