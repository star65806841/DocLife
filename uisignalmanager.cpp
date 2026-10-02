#include "uisignalmanager.h"

#include <QDebug>

UISignalManager::UISignalManager(QObject *parent)
    : QObject{parent}
{
    qDebug() << "UISignalManager created";
}

UISignalManager::~UISignalManager()
{
    qDebug() << "~UISignalManager";
}

UISignalManager *UISignalManager::instance()
{
    // C++11 起，函数内 static 局部变量初始化是线程安全的
    static UISignalManager s_instance;
    return &s_instance;
}