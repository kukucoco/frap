/*
 * logging.h —— 控制台日志（显式安装消息处理器，兼容非 TTY/管道环境）
 */
#ifndef FRAP_CORE_LOGGING_H
#define FRAP_CORE_LOGGING_H

#include <QString>
#include <QStringList>

namespace sc {

/* 安装输出到 stderr 的 Qt 消息处理器；返回旧处理器（无需恢复可不理会） */
QtMessageHandler installConsoleLogging();

/* 最近 n 条日志（供崩溃报告采集） */
QStringList recentLogLines(int n = 200);

} // namespace sc

#endif /* FRAP_CORE_LOGGING_H */
