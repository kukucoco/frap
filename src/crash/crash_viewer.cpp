/*
 * crash_viewer —— 崩溃报告查看器（验收工具）
 *
 * 用法：
 *   crash_viewer               列出崩溃报告
 *   crash_viewer list          同上
 *   crash_viewer show <file>   查看某份报告的 backtrace 与日志
 *   crash_viewer clean         删除全部报告
 *
 * 报告目录：~/.local/state/frap/crashes/crash_*.txt
 */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>
#include <QDebug>

#include "crash_handler.h"
#include "logging.h"

using namespace sc;

namespace {

QString crashDir()
{
    return crashReportDir();
}

QStringList listReports()
{
    const QDir dir(crashDir());
    return dir.entryList({ QStringLiteral("crash_*.txt") }, QDir::Files, QDir::Time);
}

void showReport(const QString& fileName)
{
    const QString path = QDir(crashDir()).filePath(fileName);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "无法读取:" << path;
        return;
    }
    QTextStream in(&f);
    while (!in.atEnd())
        qInfo().noquote() << in.readLine();
    f.close();
}

} // namespace

int main(int argc, char* argv[])
{
    installConsoleLogging();
    QCoreApplication app(argc, argv);

    QString mode = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("list");

    if (mode == QStringLiteral("clean")) {
        const QStringList files = listReports();
        for (const QString& f : files)
            QFile::remove(QDir(crashDir()).filePath(f));
        qInfo() << "已删除" << files.size() << "份崩溃报告";
        return 0;
    }

    if (mode == QStringLiteral("show")) {
        if (argc < 3) {
            qWarning() << "用法: crash_viewer show <file>";
            return 2;
        }
        showReport(QString::fromLocal8Bit(argv[2]));
        return 0;
    }

    const QStringList files = listReports();
    qInfo() << "崩溃报告目录:" << crashDir();
    if (files.isEmpty()) {
        qInfo() << "（无崩溃报告）";
        return 0;
    }
    for (const QString& f : files) {
        const QString summary = [&]() {
            QFile file(QDir(crashDir()).filePath(f));
            if (!file.open(QIODevice::ReadOnly))
                return QString();
            QString summary;
            int n = 0;
            while (!file.atEnd() && n < 6) {
                const QByteArray line = file.readLine();
                if (line.startsWith("app:") || line.startsWith("pid:")
                    || line.startsWith("signal:") || line.startsWith("time:"))
                    summary += QString::fromUtf8(line).trimmed() + "  ";
                ++n;
            }
            return summary;
        }();
        qInfo().noquote() << f << " " << summary;
    }
    return 0;
}
