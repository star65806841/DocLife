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

private slots:
    void showProjectsetting();
    void showProjectNew();
private:
    Ui::MainWindow *ui;
    // MainContent *m_mainContent = nullptr;
private:
    void initConnect();

};
#endif // MAINWINDOW_H
