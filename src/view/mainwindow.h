#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QWidget;
class QPushButton;
namespace doclife::ui
{
    class CentralView;
    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
    protected:
        void closeEvent(QCloseEvent* event) override;
    private:
        CentralView* m_cenWidget = nullptr;
    };
}
#endif // MAINWINDOW_H
