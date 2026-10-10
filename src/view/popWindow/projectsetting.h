#ifndef PROJECTSETTING_H
#define PROJECTSETTING_H

#include <QWidget>

#include <components/dialogs_flyouts/Dialog.h>

namespace fluent {
namespace navigation { class StackContentHost; }
}

namespace doclife::ui {

class ProjectSetting : public fluent::dialogs_flyouts::Dialog
{
    Q_OBJECT
public:
    explicit ProjectSetting(QWidget *parent = nullptr);

signals:

private slots:
    void slot_projectName(const QString &name);
private:
    void init();

private:
    fluent::dialogs_flyouts::Dialog* m_dialog = nullptr;
    bool editButtonEnabled = true;
};

}
#endif // PROJECTSETTING_H
