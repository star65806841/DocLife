#ifndef PROJECTSETTING_H
#define PROJECTSETTING_H

#include <QWidget>

namespace fluent {
namespace dialogs_flyouts { class Dialog; }
namespace navigation { class StackContentHost; }
}

namespace doclife::ui {

class ProjectSetting : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectSetting(QWidget *parent = nullptr);
    void show(QWidget *parent);
signals:

private:
    void init();
    void populateNavigationPages(fluent::navigation::StackContentHost *host);
    QWidget *buildNavigationDemo(QWidget *parent);
    QWidget *createDemoPage(const QString &title, const QString &description, QWidget *parent = nullptr);
    QWidget *createPage2(const QString &title1, const QString &description1,const QString &title2, const QString &description2, QWidget *parent = nullptr);

private:
    fluent::dialogs_flyouts::Dialog* m_dialog = nullptr;
};

}
#endif // PROJECTSETTING_H
