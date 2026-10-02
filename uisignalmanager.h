#ifndef UISIGNALMANAGER_H
#define UISIGNALMANAGER_H

#include <QObject>
#include <QString>

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
    // ============ 示例信号：按需增删 ============

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

#endif // UISIGNALMANAGER_H