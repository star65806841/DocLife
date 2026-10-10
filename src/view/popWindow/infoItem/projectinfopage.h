#ifndef PROJECTINFOPAGE_H
#define PROJECTINFOPAGE_H

#include <QWidget>

namespace doclife::ui {
class ProjectInfoPage : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectInfoPage(const QString& title, QWidget *parent = nullptr);

signals:
    QString sign_text(const QString& text);
private:
    QString m_content = QString{};
private:
    void createPage(const QString& title);
};
}
#endif // PROJECTINFOPAGE_H
