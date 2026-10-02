#ifndef MAINCONTENT_H
#define MAINCONTENT_H

#include <QWidget>
class QTableWidget;
class QTabWidget;
class QPushButton;
class QLineEdit;
class QSplitter;
class ExcelFile;
class ExcelTreeWidget;

class MainContent : public QWidget
{
    Q_OBJECT
public:
    explicit MainContent(QWidget *parent = nullptr);
    ~MainContent() override;
    // 方便外部访问内部控件
    // QTableWidget *table() const { return m_table; }
    QTabWidget *tabs()  const { return m_tabWidget; }

signals:
    // 把按钮点击通过信号抛出去，MainWindow 里接
    void addRequested();
    void removeRequested();
    void editRequested();
    void searchChanged(const QString &text);

public slots:
    void onGetExcelFilePath();

private:
    void setupUi();

    // 上半部分：表格功能区
    QWidget      *m_topArea         = nullptr;
    QPushButton  *m_btnAdd          = nullptr;
    QPushButton  *m_btnRemove       = nullptr;
    QPushButton  *m_btnEdit         = nullptr;
    QLineEdit    *m_searchEdit      = nullptr;
    QTableWidget *m_crtExcelPreview = nullptr;

    // 下半部分：Tab
    QTabWidget   *m_tabWidget   = nullptr;

    // 上下分割
    QSplitter    *m_splitter    = nullptr;

    ExcelFile    *m_excelFile   = nullptr;

    QString m_filePath = QString{};
};

#endif // MAINCONTENT_H
