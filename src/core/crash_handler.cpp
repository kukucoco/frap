#include "crash_handler.h"

#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

#include "logging.h"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <execinfo.h>

extern "C" void scCrashLogDump(int fd);

namespace sc {

namespace {

volatile sig_atomic_t g_crashed = 0;
char g_appName[64] = { 0 };

const char* signalName(int sig)
{
    switch (sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGABRT: return "SIGABRT";
    case SIGFPE: return "SIGFPE";
    case SIGBUS: return "SIGBUS";
    case SIGILL: return "SIGILL";
    default: return "UNKNOWN";
    }
}

/* 崩溃处理器：只使用 async-signal-safe 调用 */
void crashHandler(int sig)
{
    if (g_crashed) {
        /* 重复崩溃：直接恢复默认并退出，避免死循环 */
        signal(sig, SIG_DFL);
        raise(sig);
        _exit(128 + sig);
    }
    g_crashed = 1;

    char dir[512];
    char path[640];
    const QString dirStr = crashReportDir();
    std::snprintf(dir, sizeof(dir), "%s", dirStr.toUtf8().constData());

    const std::time_t now = std::time(nullptr);
    std::snprintf(path, sizeof(path), "%s/crash_%ld_%ld.txt", dir,
                  static_cast<long>(getpid()), static_cast<long>(now));

    const int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0644);
    if (fd >= 0) {
        dprintf(fd, "== frap crash report ==\n");
        dprintf(fd, "app:     %s\n", g_appName[0] ? g_appName : "?");
        dprintf(fd, "pid:     %ld\n", static_cast<long>(getpid()));
        dprintf(fd, "time:    %s", ctime(&now));
        dprintf(fd, "signal:  %s\n", signalName(sig));
        dprintf(fd, "backtrace:\n");
        void* frames[64];
        const int n = backtrace(frames, 64);
        backtrace_symbols_fd(frames, n, fd);
        dprintf(fd, "\nrecent log:\n");
        /* 环形缓冲在崩溃线程读：try_lock，避免持锁处崩溃导致死锁 */
        scCrashLogDump(fd);
        close(fd);
    }

    signal(sig, SIG_DFL);
    raise(sig);
    _exit(128 + sig);
}

} // namespace

/* 供 crash handler 使用的日志导出（定义在 logging.cpp，避免重复持锁） */

QString crashReportDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.local/share");
    const QString dir = base + QStringLiteral("/frap/crashes");
    QDir().mkpath(dir);
    return dir;
}

void installCrashHandlers(const QString& appName)
{
    std::strncpy(g_appName, appName.toUtf8().constData(), sizeof(g_appName) - 1);
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crashHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    for (int sig : { SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL })
        sigaction(sig, &sa, nullptr);
}

QString writeCrashReport(const QString& appName, const QString& reason)
{
    const QString dir = crashReportDir();
    const QString path = dir + QStringLiteral("/crash_%1_%2.txt")
                               .arg(QCoreApplication::applicationPid())
                               .arg(QDateTime::currentMSecsSinceEpoch());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    QTextStream ts(&f);
    ts << "== frap crash report ==\n";
    ts << "app:     " << appName << "\n";
    ts << "pid:     " << QCoreApplication::applicationPid() << "\n";
    ts << "time:    " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    ts << "reason:  " << reason << "\n";
    ts << "recent log:\n";
    for (const QString& line : recentLogLines(200))
        ts << line << "\n";
    f.close();
    return path;
}

} // namespace sc
