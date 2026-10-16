#ifndef PROJECTINFOPAGE_H
#define PROJECTINFOPAGE_H

#include <QWidget>

namespace doclife::ui {
class ProjectInfoPage : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectInfoPage(const QString& title, const QString& content = QString{}, QWidget *parent = nullptr);

signals:
    QString sign_text(const QString& text);

private:
    void createPage(const QString& title, const QString& content);
};
}
#endif // PROJECTINFOPAGE_H
