#ifndef CENTRALVIEW_H
#define CENTRALVIEW_H

#include <QWidget>

namespace doclife::core
{
    class ProjectContext;
}
namespace doclife::ui {

    class ProjectView;
    class TemplateView;

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

        void popProjectClosed();
        void popProjectSaved();
        void popProjectCreated();
        void popProjectOpened();
        void onToastWarning(const QString &msg);
    private:
        //project context data
        doclife::core::ProjectContext* m_projectObject = nullptr;

        ProjectView* m_leftView = nullptr;
        TemplateView* m_templateView = nullptr;

    private:

        void init();
        void initConnect();
    };
}
#endif // CENTRALVIEW_H
