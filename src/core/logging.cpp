#include "logging.h"

#include <QDebug>
#include <QString>
#include <QtGlobal>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <unistd.h>

namespace sc {

namespace {

std::deque<QString> g_ring;
std::mutex g_ringMutex;
constexpr int kRingCapacity = 400;

void pushRing(const QString& line)
{
    std::lock_guard<std::mutex> lock(g_ringMutex);
    g_ring.push_back(line);
    if (static_cast<int>(g_ring.size()) > kRingCapacity)
        g_ring.pop_front();
}

void handler(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    const char* level = "INFO";
    switch (type) {
    case QtDebugMsg: level = "DEBUG"; break;
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    }
    const QString line = QStringLiteral("[%1] %2").arg(QLatin1String(level), message);
    std::fprintf(stderr, "%s\n", line.toUtf8().constData());
    std::fflush(stderr);
    pushRing(line);
}

} // namespace

QtMessageHandler installConsoleLogging()
{
    return qInstallMessageHandler(handler);
}

QStringList recentLogLines(int n)
{
    std::lock_guard<std::mutex> lock(g_ringMutex);
    QStringList out;
    const int take = qMin(n, static_cast<int>(g_ring.size()));
    auto it = g_ring.end();
    for (int i = 0; i < take; ++i)
        --it;
    for (int i = 0; i < take; ++i, ++it)
        out.append(*it);
    return out;
}

} // namespace sc

/* 供崩溃处理器调用：把日志环形缓冲写到 fd（try_lock 避免死锁） */
extern "C" void scCrashLogDump(int fd)
{
    using namespace sc;
    std::unique_lock<std::mutex> lock(g_ringMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        const char msg[] = "(log ring locked, skipped)\n";
        ::write(fd, msg, sizeof(msg) - 1);
        return;
    }
    for (const QString& line : g_ring) {
        const QByteArray b = line.toUtf8() + "\n";
        ::write(fd, b.constData(), static_cast<size_t>(b.size()));
    }
}
