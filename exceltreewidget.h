#ifndef EXCELTREEWIDGET_H
#define EXCELTREEWIDGET_H

#include <QTreeWidget>
#include <QString>

// 树节点类型，存在 item 的 UserRole 里
enum class ExcelNodeType {
    Dir   = 0,
    File  = 1,
    Sheet = 2
};

class ExcelTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ExcelTreeWidget(QWidget *parent = nullptr);

    // 从根目录开始扫描并构建树
    void loadDirectory(const QString &rootPath);

Q_SIGNALS:
    // 用户点击了某个 sheet 节点
    void sheetSelected(const QString &filePath, const QString &sheetName);

private Q_SLOTS:
    // 点击节点时判断类型并抛信号
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    void buildTree(const QString &dirPath, QTreeWidgetItem *parentItem);
    void addExcelFile(const QString &filePath, QTreeWidgetItem *parentItem);
};

#endif // EXCELTREEWIDGET_H