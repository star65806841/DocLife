#include "dialogbase.h"
using namespace doclife::ui;
DialogBase::DialogBase(QWidget *parent)
    : fluent::dialogs_flyouts::Dialog(parent)
{
    setMinimumSize(QSize(500,300));
}

