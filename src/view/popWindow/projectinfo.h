#ifndef PROJECTINFO_H
#define PROJECTINFO_H

#include <QWidget>
namespace doclife::ui {
class ProjectInfo : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectInfo(QWidget *parent = nullptr);

signals:

private:
    void init();
};
}
#endif // PROJECTINFO_H
