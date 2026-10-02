#ifndef EXCELFILE_H
#define EXCELFILE_H

#include <QWidget>

class ExcelFile : public QWidget
{
    Q_OBJECT
public:
    explicit ExcelFile(QWidget *parent = nullptr);
    void openExcelFile(QString filePath);
signals:

private slots:

private:
    QString m_filePath;
};

#endif // EXCELFILE_H
