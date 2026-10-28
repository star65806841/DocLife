#ifndef UISIGNALMANAGER_H
#define UISIGNALMANAGER_H

#include <QObject>
#include <QString>

namespace doclife::ui{

class UISignalManager : public QObject
{
    Q_OBJECT

public:
    // 全局唯一入口
    static UISignalManager *instance();

    // 禁止拷贝
    UISignalManager(const UISignalManager &)            = delete;
    UISignalManager &operator=(const UISignalManager &) = delete;

Q_SIGNALS:
    // ============---============

    void drawerFileSetting();
    // 新建项目
    void popProjectNew();
    void newProject();
    void saveProject();
    void openProject();
    void showTemplateView(bool);

    // 项目设置
    void popProjectsetting();
    // 项目多开
    void duplicateInstanceDetected();
    // 点击了某个xlsx文件
    void excelFileActivated(const QString &xlsxPath);
    // 异步解析完了，把 sheet 名字拿去建子节点
    void sheetNamesLoaded(const QString &xlsxPath, const QStringList &sheetNames);
    // 激活了某个xlsx的某个sheet
    void excelSheetActivated(const QString &xlsxPath, const QString &sheetName);
    // 日志
    void logMessage(const QString &msg);
    void logError(const QString &msg);

    // 状态栏
    void statusMessage(const QString &msg, int timeoutMs = 3000);

    // 文件相关
    void openExcelRequested();
    void fileOpened(const QString &path);

    // 数据/刷新
    void dataChanged();
    void refreshRequested();

    // 主题/UI
    void themeChanged(const QString &themeName);

    // 通用进度
    void progressChanged(int value);           // 0 ~ 100
    void busyChanged(bool busy);

private:
    explicit UISignalManager(QObject *parent = nullptr);
    ~UISignalManager() override;

    // 禁止外部 new
};
}
#endif // UISIGNALMANAGER_H