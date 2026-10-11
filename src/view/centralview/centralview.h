#ifndef CENTRALVIEW_H
#define CENTRALVIEW_H

#include <QWidget>

namespace doclife::model
{
    class ProjectObject;
}
namespace doclife::ui {

    class LeftView;

    class CentralView : public QWidget
    {
        Q_OBJECT
    public:
        explicit CentralView(QWidget* parent = nullptr);

    signals:
    private:
        //project context data
        doclife::model::ProjectObject* m_projectObject = nullptr;

        LeftView* m_leftView = nullptr;

    private:

        void init();
        void initConnect();
        void showProjectNew();
        void showProjectsetting();
    };
}
#endif // CENTRALVIEW_H
