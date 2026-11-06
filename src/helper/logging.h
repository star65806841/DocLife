#pragma once

#include <QString>

namespace doclife::logging {

    enum class Level { Debug, Info, Warning, Error };

    // 启动时调用一次（QApplication 之后、能用 QStandardPaths 之后）：
    // 建日志目录、初始化 glog、把 qDebug/qWarning/qCritical 接到同一份日志里。
    void init(const QString& appName);

    // 退出前调用：刷盘并关闭
    void shutdown();

    // 立刻把缓冲写盘（崩溃前、导出前调用）
    void flush();

    // 日志目录（%LOCALAPPDATA%\OnePoint\DocLife\logs）
    QString logDirectory();

    // 写一条日志（一般用下面的宏，自动带文件名和行号）
    void write(Level level, const char* file, int line, const QString& message);

    // 导出诊断包：环境信息 + 最近的日志，合成一个 utf-8 文本文件。
    // targetDir 为空时放到桌面。返回文件完整路径，失败返回空串。
    QString exportDiagnostics(const QString& targetDir = QString());

} // namespace doclife::logging

// 调用点统一用这几个宏
#define DOC_DEBUG(msg) ::doclife::logging::write(::doclife::logging::Level::Debug,   __FILE__, __LINE__, (msg))
#define DOC_INFO(msg)  ::doclife::logging::write(::doclife::logging::Level::Info,    __FILE__, __LINE__, (msg))
#define DOC_WARN(msg)  ::doclife::logging::write(::doclife::logging::Level::Warning, __FILE__, __LINE__, (msg))
#define DOC_ERROR(msg) ::doclife::logging::write(::doclife::logging::Level::Error,   __FILE__, __LINE__, (msg))
