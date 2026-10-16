#ifndef POPCONTENT_H
#define POPCONTENT_H

#include <QWidget>

class PopContent : public QWidget
{
    Q_OBJECT
public:
    explicit PopContent(const QString& title, const QString content, QWidget *parent = nullptr);

signals:
};

#endif // POPCONTENT_H
