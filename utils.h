#pragma once
#include <QString>

class QWidget;

class Utils {
public:
    static int add(int a, int b);
    static QString formatName(const QString &name);
    static QString openExcelFile(QWidget *parent = nullptr);
    static QString selectDir(QWidget *parent = nullptr);
};