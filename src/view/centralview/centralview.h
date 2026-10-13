#ifndef CENTRALVIEW_H
#define CENTRALVIEW_H

#include <QWidget>

namespace doclife::core
{
    class ProjectContext ;
}
namespace doclife::ui {

    class LeftView;

    class CentralView : public QWidget
    {
        Q_OBJECT
    public:
        explicit CentralView(QWidget* parent = nullptr);

    signals:

    public slots:
        void newProject(const QString& name);
    private:
        //project context data
        doclife::core::ProjectContext * m_projectObject = nullptr;

        LeftView* m_leftView = nullptr;

    private:

        void init();
        void initConnect();
        void showProjectNew();
        void showProjectsetting();
    };
}
#endif // CENTRALVIEW_H
