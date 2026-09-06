/*
 * crash_handler.h —— 崩溃信号处理 + 崩溃报告
 *
 * 为 app / host 安装 SIGSEGV/SIGABRT/SIGFPE/SIGBUS/SIGILL 处理器：
 * 崩溃时把 进程名/pid/信号/backtrace/最近日志 落盘为纯文本报告，
 * 然后恢复默认处理器并重新触发（保留 core dump）。
 *
 * 报告目录：~/.local/state/frap/crashes/crash_<pid>_<ts>.txt
 * 用 build/crash_viewer 查看。
 *
 * 注意：处理器内只使用 async-signal-safe 的最小操作（open/write/
 * backtrace_symbols_fd），并在崩溃线程执行。
 */
#ifndef FRAP_CORE_CRASH_HANDLER_H
#define FRAP_CORE_CRASH_HANDLER_H

#include <QString>

namespace sc {

/* 安装崩溃处理器（应用启动早期调用一次） */
void installCrashHandlers(const QString& appName);

/* 崩溃报告目录（不存在则创建） */
QString crashReportDir();

/* 手动写一份崩溃报告（用于测试/诊断注入），返回报告路径 */
QString writeCrashReport(const QString& appName, const QString& reason);

} // namespace sc

#endif /* FRAP_CORE_CRASH_HANDLER_H */
