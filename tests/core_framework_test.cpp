/*
 * core_framework_test —— 插件框架无头集成测试
 *
 * 验证：
 *   1. 宿主握手 + manifest 收集
 *   2. plugin.invoke 调用链（工具/导出）
 *   3. 非破坏渲染 + undo
 *   4. 宿主崩溃自动重启 + 重新握手
 *
 * 运行：build/tests/core_framework_test <插件目录>
 */
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QDir>
#include <QFile>

#include <csignal>
#include <unistd.h>

#include "settings.h"
#include "plugin_manager.h"
#include "session.h"
#include "logging.h"
#include "crash_handler.h"

using namespace sc;

static bool g_passed = false;

static QStringList crashFiles()
{
    QDir dir(sc::crashReportDir());
    return dir.entryList({ QStringLiteral("crash_*.txt") }, QDir::Files, QDir::Time);
}

static void finish(int code)
{
    if (code != 0)
        qCritical() << "FAILED";
    else
        qInfo() << "PASS";
    qApp->exit(code);
}

static void verifyPlugins(PluginManager& manager)
{
    const QList<PluginManifest> plugins = manager.plugins();
    qInfo() << "已加载插件数:" << plugins.size();
    for (const PluginManifest& m : plugins)
        qInfo() << "  " << m.id << "|" << m.name << "| kind=" << m.kind
                << "| schema=" << m.configSchema.size()
                << "| contribs=" << m.contributions.size();
    if (plugins.size() < 6)
        finish(2);
}

int main(int argc, char* argv[])
{
    sc::installConsoleLogging();
    QCoreApplication app(argc, argv);
    qInfo() << "[test] 启动";

    const QString pluginDir = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                       : QStringLiteral("plugins");

    Settings settings;
    settings.setPluginDirectories({ pluginDir });

    PluginManager manager(&settings);

    QObject::connect(&manager, &PluginManager::pluginLog,
                     [](const QString& level, const QString& msg) {
                         qInfo() << "  [host/" << level << "]" << msg;
                     });
    QObject::connect(&manager, &PluginManager::hostRestarted, [](int attempt) {
        qInfo() << "  [hostRestarted]" << attempt;
    });
    QObject::connect(&manager, &PluginManager::failed, [](const QString& msg) {
        qCritical() << "host failed:" << msg;
        finish(2);
    });

    QObject::connect(&manager, &PluginManager::ready, [&]() {
        if (g_passed)
            return; /* 崩溃重启后再次 ready */

        verifyPlugins(manager);

        /* 工具调用链 */
        const QJsonObject payload{
            { QStringLiteral("selection"), QJsonObject{
                { QStringLiteral("x"), 10 }, { QStringLiteral("y"), 20 },
                { QStringLiteral("w"), 100 }, { QStringLiteral("h"), 50 } } },
            { QStringLiteral("imageSize"), QJsonObject{
                { QStringLiteral("w"), 1920 }, { QStringLiteral("h"), 1080 } } },
            { QStringLiteral("params"), QJsonObject{} },
        };
        manager.invoke("tool.rect", "apply", payload,
                       [&](const QJsonObject& result, const QString& error) {
                           if (!error.isEmpty()) {
                               qCritical() << "工具调用失败" << error;
                               finish(2);
                               return;
                           }
                           const QJsonArray ops = result.value("operations").toArray();
                           qInfo() << "tool.rect 返回操作:" << ops.size();
                           if (ops.size() != 1
                               || ops[0].toObject().value("type").toString() != "rect") {
                               finish(2);
                               return;
                           }

                           /* 非破坏渲染 + undo */
                           QImage img(1920, 1080, QImage::Format_ARGB32);
                           img.fill(Qt::white);
                           CaptureSession session;
                           session.setImage(img);
                           session.addOperation(ops[0].toObject());
                           const QColor c = session.render().pixelColor(11, 21);
                           qInfo() << "渲染像素(期望 #e11d48):" << c.name();
                           if (c.name() != "#e11d48")
                               finish(2);
                           session.undo();
                           if (session.render().pixelColor(11, 21) != QColor(Qt::white)) {
                               qCritical() << "undo 未生效";
                               finish(2);
                               return;
                           }
                           qInfo() << "undo 生效 ✓";

                           /* 导出调用链（clipboard 应回传 actions） */
                           manager.invoke(
                               "export.clipboard", "execute",
                               QJsonObject{
                                   { QStringLiteral("imageRef"), QStringLiteral("/tmp/x.png") },
                                   { QStringLiteral("imageBase64"), QStringLiteral("AAAA") },
                                   { QStringLiteral("params"), QJsonObject{} } },
                               [&](const QJsonObject& result, const QString& error) {
                                   if (!error.isEmpty() || !result.contains("actions")) {
                                       qCritical() << "export.clipboard 失败";
                                       finish(2);
                                       return;
                                   }
                                   qInfo() << "export.clipboard 回传 actions ✓";

                                   /* 崩溃重启验证（SIGSEGV -> 触发崩溃报告） */
                                   g_passed = true;
                                   const QStringList before = crashFiles();
                                   const qint64 pid = manager.hostPid();
                                   qInfo() << "宿主 pid:" << pid << "，SIGSEGV 模拟崩溃";
                                   ::kill(static_cast<pid_t>(pid), SIGSEGV);
                                   Q_UNUSED(before)
                               });
                       });
    });

    /* 崩溃重启后的第二次 ready：验证可再次握手 + 崩溃报告已生成 */
    QObject::connect(&manager, &PluginManager::ready, [&]() {
        if (g_passed) {
            verifyPlugins(manager);
            qInfo() << "崩溃重启后重新握手成功 ✓";

            /* 验证崩溃报告 */
            const QStringList files = crashFiles();
            bool found = false;
            for (const QString& f : files) {
                QFile file(sc::crashReportDir() + QLatin1Char('/') + f);
                if (file.open(QIODevice::ReadOnly)) {
                    const QByteArray data = file.readAll();
                    if (data.contains("frap-host")) {
                        found = true;
                        qInfo() << "崩溃报告已生成:" << f;
                        qInfo() << "  (包含 backtrace 与日志，见"
                                << sc::crashReportDir().toUtf8().constData() << ")";
                        break;
                    }
                }
            }
            if (!found) {
                qCritical() << "FAILED: 未找到宿主崩溃报告";
                finish(2);
                return;
            }
            finish(0);
        }
    });

    QTimer::singleShot(15000, &app, []() {
        qCritical() << "超时";
        finish(2);
    });

    manager.start();
    return app.exec();
}
