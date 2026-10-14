#ifndef SINGLEINSTANCEGUARD_H
#define SINGLEINSTANCEGUARD_H

#include <QObject>
#include <QLocalServer>
#include <QString>
#include <memory>

class QLockFile;

namespace doclife::core {

class SingleInstanceGuard : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstanceGuard(QObject *parent = nullptr);
    ~SingleInstanceGuard() override;

    // 保证只有一个实例：
    //   true  = 当前是第一个实例，继续运行
    //   false = 已有实例，已通知它，当前进程应立即退出
    bool enforce(const QString &key, int staleLockMs = 30000);

    // 供第二个进程使用：通知已有实例
    static void notifyExisting(const QString &key,
                               const QString &message = QStringLiteral("SHOW_EXISTING"),
                               int timeoutMs = 500);

signals:


private slots:
    void handleNewConnection();

private:
    QString m_key;
    QLocalServer m_server;
    std::unique_ptr<QLockFile> m_lockFile;
};
}
#endif // SINGLEINSTANCEGUARD_H
