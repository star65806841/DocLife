#include "logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>

#include <cstdio>
#include <vector>

#ifdef DOCLIFE_HAS_GLOG
#include <glog/logging.h>
#endif

namespace doclife::logging {

    namespace {
        QString s_logDir;
        bool s_inited = false;
        QMutex s_mutex;
        QString s_appName = QStringLiteral("DocLife");

        QString levelName(Level level)
        {
            switch (level) {
            case Level::Debug:   return QStringLiteral("DEBUG");
            case Level::Info:    return QStringLiteral("INFO");
            case Level::Warning: return QStringLiteral("WARN");
            case Level::Error:   return QStringLiteral("ERROR");
            }
            return QStringLiteral("INFO");
        }

        // 没有 glog 时的兜底：按天追加到同一个文本文件
        void writeFallback(Level level, const char* file, int line, const QString& message)
        {
            const QMutexLocker locker(&s_mutex);
            const QString path = s_logDir + QStringLiteral("/%1-%2.log")
                .arg(s_appName, QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));
            QFile f(path);
            if (!f.open(QIODevice::Append | QIODevice::Text)) return;
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            ts << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
               << QStringLiteral(" [") << levelName(level) << QStringLiteral("] ")
               << QFileInfo(QString::fromUtf8(file)).fileName()
               << QLatin1Char(':') << line << QStringLiteral("  ") << message << QLatin1Char('\n');
        }

        void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
        {
            // glog 自己不会再走 qDebug，这里加个简单的防重入保护
            static thread_local bool inHandler = false;
            if (inHandler) return;
            inHandler = true;

            Level level = Level::Info;
            switch (type) {
            case QtDebugMsg:    level = Level::Debug; break;
            case QtInfoMsg:     level = Level::Info; break;
            case QtWarningMsg:  level = Level::Warning; break;
            case QtCriticalMsg: level = Level::Error; break;
            case QtFatalMsg:    level = Level::Error; break;
            }

            QString message = msg;
            if (context.category && std::strcmp(context.category, "default") != 0) {
                message.prepend(QStringLiteral("[") + QString::fromUtf8(context.category) + QStringLiteral("] "));
            }
            write(level, context.file ? context.file : "", context.line, message);

            if (type == QtFatalMsg) {
                flush();   // 崩溃前必须落盘：glog 默认 1 秒缓冲，abort 不会帮你刷
                std::abort();
            }
            inHandler = false;
        }
    } // namespace

    QString logDirectory()
    {
        if (!s_logDir.isEmpty()) return s_logDir;

        QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (base.isEmpty()) base = QDir::tempPath() + QStringLiteral("/") + s_appName;
        s_logDir = base + QStringLiteral("/logs");
        QDir().mkpath(s_logDir);
        return s_logDir;
    }

    void init(const QString& appName)
    {
        if (s_inited) return;
        s_inited = true;
        s_appName = appName.isEmpty() ? QStringLiteral("DocLife") : appName;

        const QString dir = logDirectory();

        // 清理旧日志：保留最近 7 天，避免用户机器上的日志无限增长
        {
            const QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);
            const QFileInfoList old = QDir(dir).entryInfoList(QDir::Files, QDir::Time);
            for (const QFileInfo& info : old) {
                if (info.lastModified() < cutoff) QFile::remove(info.absoluteFilePath());
            }
        }

#ifdef DOCLIFE_HAS_GLOG
        // 日志文件：doclife.log.<yyyyMMdd-HHmmss>.<pid> 等
        google::InitGoogleLogging(qUtf8Printable(s_appName));
        FLAGS_logtostderr = false;          // 平时不往控制台刷
        FLAGS_alsologtostderr = false;
        FLAGS_colorlogtostderr = false;
        FLAGS_stderrthreshold = google::GLOG_FATAL;   // 只有 FATAL 同时打到 stderr
        FLAGS_logbufsecs = 1;               // 每秒刷盘，崩溃时最多丢 1 秒
        FLAGS_max_log_size = 20;            // 单个文件 20MB 后自动切分
        FLAGS_stop_logging_if_full_disk = true;
        // FLAGS_log_dir 存的是裸指针，glog 之后每次创建/轮转日志文件都会继续用它；
        // 局部 QByteArray 在 init() 返回后即销毁，指针会悬空。用 static 让路径活到进程结束。
        static QByteArray s_flagsLogDir;
        s_flagsLogDir = dir.toLocal8Bit();
        FLAGS_log_dir = s_flagsLogDir.data();

        // 显式指定各级别的目标文件：glog 默认用 ProgramInvocationShortName() 拼文件名，
        // 该函数在部分环境下返回空串，会直接导致 "Could not create logging file"。
        const QByteArray infoBase = (dir + QStringLiteral("/doclife.log")).toLocal8Bit();
        const QByteArray warnBase = (dir + QStringLiteral("/doclife-warning.log")).toLocal8Bit();
        const QByteArray errBase = (dir + QStringLiteral("/doclife-error.log")).toLocal8Bit();
        google::SetLogDestination(google::GLOG_INFO, infoBase.constData());
        google::SetLogDestination(google::GLOG_WARNING, warnBase.constData());
        google::SetLogDestination(google::GLOG_ERROR, errBase.constData());
        google::SetLogDestination(google::GLOG_FATAL, errBase.constData());
        google::FlushLogFiles(google::GLOG_INFO);
#else
        QDir().mkpath(dir);
#endif

        qInstallMessageHandler(messageHandler);

        DOC_INFO(QStringLiteral("日志已启动 dir=%1 Qt=%2 OS=%3")
            .arg(dir, QString::fromLatin1(qVersion()), QSysInfo::prettyProductName()));
    }

    void write(Level level, const char* file, int line, const QString& message)
    {
        if (!s_inited) {
            // 初始化之前的消息先落到 stderr，别丢
            std::fputs(qUtf8Printable(message + QLatin1Char('\n')), stderr);
            return;
        }

#ifdef DOCLIFE_HAS_GLOG
        const std::string text = message.toUtf8().toStdString();
        switch (level) {
        case Level::Debug:
        case Level::Info:    LOG(INFO) << text; break;
        case Level::Warning: LOG(WARNING) << text; break;
        case Level::Error:   LOG(ERROR) << text; break;
        }
        (void)file;
        (void)line;   // glog 自己会加文件名和行号
#else
        writeFallback(level, file, line, message);
#endif
    }

    void flush()
    {
#ifdef DOCLIFE_HAS_GLOG
        if (s_inited) google::FlushLogFiles(google::GLOG_INFO);
#endif
    }

    void shutdown()
    {
        if (!s_inited) return;
        flush();
#ifdef DOCLIFE_HAS_GLOG
        google::ShutdownGoogleLogging();
#endif
        qInstallMessageHandler(nullptr);
        s_inited = false;
    }

    QString exportDiagnostics(const QString& targetDir)
    {
        flush();

        QString outDir = targetDir;
        if (outDir.isEmpty()) {
            outDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        }
        if (outDir.isEmpty()) outDir = QDir::tempPath();
        QDir().mkpath(outDir);

        const QString fileName = QStringLiteral("DocLife-diagnostics-%1.txt")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
        QFile out(outDir + QLatin1Char('/') + fileName);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) return QString();

        QTextStream ts(&out);
        ts.setEncoding(QStringConverter::Utf8);

        // ---- 头部：排查问题需要的环境信息（不含用户文档内容）----
        ts << QStringLiteral("DocLife 诊断包\n");
        ts << QStringLiteral("生成时间: ") << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
        ts << QStringLiteral("程序版本: ") << QCoreApplication::applicationVersion() << '\n';
        ts << QStringLiteral("程序路径: ") << QCoreApplication::applicationFilePath() << '\n';
        ts << QStringLiteral("Qt 版本 : ") << QString::fromLatin1(qVersion()) << '\n';
        ts << QStringLiteral("系统    : ") << QSysInfo::prettyProductName() << '\n';
        ts << QStringLiteral("内核    : ") << QSysInfo::kernelType() << QLatin1Char(' ') << QSysInfo::kernelVersion() << '\n';
        ts << QStringLiteral("CPU     : ") << QSysInfo::currentCpuArchitecture() << '\n';
        ts << QStringLiteral("本地化  : ") << QLocale().name() << '\n';
        ts << QStringLiteral("日志目录: ") << logDirectory() << '\n';
        ts << QStringLiteral("============================== 日志 ==============================\n");

        // ---- 正文：按修改时间倒序取最近的几个日志文件，每个取尾部 ----
        QDir dir(logDirectory());
        QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Time);
        constexpr qint64 kPerFileBytes = 400 * 1024;
        constexpr qint64 kTotalBytes = 1200 * 1024;
        qint64 written = 0;
        int used = 0;

        for (const QFileInfo& info : files) {
            if (used >= 3 || written >= kTotalBytes) break;
            QFile f(info.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly)) continue;

            const qint64 size = f.size();
            const qint64 take = qMin(size, kPerFileBytes);
            ts << QStringLiteral("\n--- %1（共 %2 字节，显示最后 %3 字节）---\n")
                .arg(info.fileName()).arg(size).arg(take);
            if (size > take) f.seek(size - take);
            QByteArray tail = f.read(take);
            tail.replace("\r\n", "\n");   // 统一换行，避免导出文件里出现空行
            ts << QString::fromUtf8(tail);
            written += take;
            ++used;
        }

        if (used == 0) {
            ts << QStringLiteral("（没有找到日志文件）\n");
        }

        ts.flush();
        out.close();

        DOC_INFO(QStringLiteral("已导出诊断包 %1").arg(out.fileName()));
        return out.fileName();
    }

} // namespace doclife::logging
