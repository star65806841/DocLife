#include "singleinstanceguard.h"

#include "uisignalmanager.h"

#include <QDir>
#include <QLockFile>
#include <QLocalSocket>

namespace doclife::core {

SingleInstanceGuard::SingleInstanceGuard(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection,
            this, &SingleInstanceGuard::handleNewConnection);
}

SingleInstanceGuard::~SingleInstanceGuard() = default;

bool SingleInstanceGuard::enforce(const QString &key, int staleLockMs)
{
    m_key = key;

    const QString lockPath = QDir::tempPath() + "/" + key + ".lock";
    m_lockFile = std::make_unique<QLockFile>(lockPath);
    m_lockFile->setStaleLockTime(staleLockMs);

    // 抢锁失败 → 已有实例，通知它并放弃
    if (!m_lockFile->tryLock(100)) {
        m_lockFile.reset();
        notifyExisting(m_key);
        return false;
    }

    // 拿到锁了，肯定是唯一实例，可以放心清理残留 socket
    QLocalServer::removeServer(m_key);
    if (!m_server.listen(m_key)) {
        m_lockFile->unlock();
        m_lockFile.reset();
        return false;
    }

    return true;
}

void SingleInstanceGuard::notifyExisting(const QString &key,
                                         const QString &message,
                                         int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(key);
    if (!socket.waitForConnected(timeoutMs)) {
        return; // 旧进程可能正在退出，静默失败
    }
    socket.write(message.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(timeoutMs);
    socket.disconnectFromServer();
}

void SingleInstanceGuard::handleNewConnection()
{
    while (QLocalSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            const QByteArray data = socket->readAll();
            if (!data.isEmpty()) {
                qDebug()<< "emit duplicateInstanceDetected";
                emit doclife::ui::UISignalManager::instance()->duplicateInstanceDetected();
            }
        });
        connect(socket, &QLocalSocket::disconnected,
                socket, &QLocalSocket::deleteLater);
    }
}

} // namespace doclife::core