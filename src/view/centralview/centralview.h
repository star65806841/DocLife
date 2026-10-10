#ifndef CENTRALVIEW_H
#define CENTRALVIEW_H

#include <QWidget>
namespace doclife::ui {

class CentralView : public QWidget
{
    Q_OBJECT
public:
    explicit CentralView(QWidget *parent = nullptr);

signals:
private:
    void init();
    void initConnect();
    void showProjectNew();
    void showProjectsetting();
};
}
#endif // CENTRALVIEW_H
