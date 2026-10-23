#ifndef PROJECTVIEW_H
#define PROJECTVIEW_H

#include <QWidget>
namespace doclife::ui {
    class ProjectView : public QWidget
    {
        Q_OBJECT
    public:
        explicit ProjectView(QWidget* parent = nullptr);

    signals:

    private:
        void init();
    };
}
#endif // PROJECTVIEW_H
