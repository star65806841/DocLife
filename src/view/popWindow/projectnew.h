#ifndef PROJECTNEW_H
#define PROJECTNEW_H

#include <QWidget>
#include <components/dialogs_flyouts/ContentDialog.h>


namespace fluent { namespace textfields { class LineEdit; } }
namespace doclife::ui {

class ProjectNew : public fluent::dialogs_flyouts::ContentDialog
{
    Q_OBJECT
public:
    explicit ProjectNew(QWidget *parent = nullptr);
signals:

private slots:
    void sendNewProjectName();
private:
    fluent::textfields::LineEdit* m_newProjectName = nullptr;
private:
    void init();
};
}

#endif // PROJECTNEW_H
