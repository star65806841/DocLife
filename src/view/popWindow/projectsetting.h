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
    void slot_updateProjectInfo();
    void slot_updateProjectName(const QString& str);
    void slot_updateConstructionUnitName(const QString& str);
    void slot_updateSupervisionUnitName(const QString& str);
    void slot_updateDesignUnitName(const QString& str);
    void slot_updateOwnerUnitName(const QString& str);
private:
    void init();

private:
    fluent::dialogs_flyouts::Dialog* m_dialog = nullptr;
    bool editButtonEnabled = true;
    QString m_projectName;
    QString m_constructionUnitName;
    QString m_supervisionUnitName;
    QString m_designUnitName;
    QString m_ownerUnitName;
};

}
#endif // PROJECTSETTING_H
