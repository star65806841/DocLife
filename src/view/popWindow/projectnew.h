#ifndef PROJECTNEW_H
#define PROJECTNEW_H

#include <QWidget>


namespace fluent {

namespace textfields { class LineEdit; }
namespace dialogs_flyouts { class ContentDialog; } }
namespace doclife::ui {

class ProjectNew : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectNew(QWidget *parent = nullptr);
    void show(QWidget *parent);
signals:

private slots:
    void sendNewProjectName();
private:
    fluent::dialogs_flyouts::ContentDialog* m_dialog = nullptr;
    fluent::textfields::LineEdit* m_newProjectName = nullptr;
private:
    void init();
};
}

#endif // PROJECTNEW_H
