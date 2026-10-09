#ifndef PROJIECTNEW_H
#define PROJIECTNEW_H

#include <QWidget>


namespace fluent { namespace dialogs_flyouts { class ContentDialog; } }
namespace doclife::ui {

class ProjiectNew : public QWidget
{
    Q_OBJECT
public:
    explicit ProjiectNew(QWidget *parent = nullptr);
    void show(QWidget *parent);
signals:

private:
    fluent::dialogs_flyouts::ContentDialog* m_dialog = nullptr;
private:
    void init();
};
}

#endif // PROJIECTNEW_H
