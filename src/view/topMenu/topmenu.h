#ifndef TOPMENU_H
#define TOPMENU_H

#include <QWidget>

#include <components/basicinput/Button.h>

namespace doclife::ui {

class TopMenu : public QWidget
{
    Q_OBJECT
public:
    explicit TopMenu(QWidget *parent = nullptr);

signals:

private:
    fluent::basicinput::Button *m_mainNewBtn = nullptr;
    fluent::basicinput::Button *m_mainOpenBtn = nullptr;
    fluent::basicinput::Button *m_mainSaveBtn = nullptr;
    /*************************/
    void init();
    void initConnect();
};
}
#endif // TOPMENU_H
