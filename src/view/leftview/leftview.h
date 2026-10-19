#ifndef LEFTVIEW_H
#define LEFTVIEW_H

#include <QWidget>
namespace doclife::ui {
    class LeftView : public QWidget
    {
        Q_OBJECT
    public:
        explicit LeftView(QWidget* parent = nullptr);

    signals:

    private:
        void init();
    };
}
#endif // LEFTVIEW_H
