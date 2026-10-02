#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QWidget;
class MainContent;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void initConnect();

private:
    Ui::MainWindow *ui;

    QWidget *m_topBar = nullptr;
    MainContent *m_mainContent = nullptr;
    QPushButton *m_mainOpenBtn = nullptr;
};
#endif // MAINWINDOW_H
