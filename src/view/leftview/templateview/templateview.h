#ifndef TEMPLATEVIEW_H
#define TEMPLATEVIEW_H

#include <QWidget>

namespace doclife::ui {
    class TemplateView : public QWidget
    {
        Q_OBJECT
    public:
        TemplateView(QWidget* parent = nullptr);
    signals:

    private:
        void init();
    };
}
#endif // TEMPLATEVIEW_H
