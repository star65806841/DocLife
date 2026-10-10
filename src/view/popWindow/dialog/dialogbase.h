#ifndef DIALOGBASE_H
#define DIALOGBASE_H

#include <components/dialogs_flyouts/Dialog.h>

namespace doclife::ui {
class DialogBase : public fluent::dialogs_flyouts::Dialog
{
    Q_OBJECT
public:
    explicit DialogBase(QWidget *parent = nullptr);

signals:
};
}
#endif // DIALOGBASE_H
