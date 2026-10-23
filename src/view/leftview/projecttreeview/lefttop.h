#ifndef LEFTTOP_H
#define LEFTTOP_H

#include <QWidget>
namespace doclife::ui {
    class LeftTop : public QWidget
    {
        Q_OBJECT
    public:
        explicit LeftTop(QWidget* parent = nullptr);

    signals:
    private:
        void init();
    };
}
#endif // LEFTTOP_H
