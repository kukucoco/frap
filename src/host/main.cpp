/*
 * frap-host —— 插件宿主进程（extension host）
 *
 * 独立于主程序运行：主程序崩溃或插件崩溃互不影响。
 * 职责：
 *   1. 从 --plugin-dir 目录加载原生插件（.so/.dll/.dylib，稳定 C ABI）
 *   2. 通过 JSON-RPC(stdio) 上报插件 manifest 给主程序
 *   3. 接收主程序的 plugin.invoke 请求，分发给对应插件
 *
 * 插件 ABI 契约见 sdk/plugin_api.h；协议见 docs/protocol.md。
 */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QJsonDocument>
#include <QJsonArray>
#include <QThread>
#include <QDebug>

#include "rpc_peer.h"
#include "protocol.h"
#include "plugin_api.h"
#include "logging.h"
#include "crash_handler.h"

using namespace sc;

/* 从 stdin 按行读取（独立线程，避免阻塞 Qt 事件循环） */
class StdinReader : public QThread
{
    Q_OBJECT
public:
    explicit StdinReader(QObject* parent = nullptr) : QThread(parent) {}

    /* 关闭 stdin 解除 readLine 阻塞，等待线程退出 */
    void stop()
    {
        requestInterruption();
        m_stdin.close();
        wait(2000);
    }

signals:
    void lineRead(const QByteArray& line);

protected:
    void run() override
    {
        if (!m_stdin.open(0, QIODevice::ReadOnly)) {
            qWarning() << "host: 无法打开 stdin";
            return;
        }
        while (!isInterruptionRequested()) {
            const QByteArray line = m_stdin.readLine();
            if (line.isEmpty())
                break;
            emit lineRead(line);
        }
    }

private:
    QFile m_stdin;
};

/* 已加载的原生插件 */
struct LoadedPlugin {
    QString id;
    QString name;
    QString version;
    FrapPluginKind kind = SC_PLUGIN_KIND_SERVICE;
    QJsonArray configSchema;
    QJsonArray contributions;
    QLibrary* library = nullptr;
    FrapPluginVtable* vt = nullptr;

    bool valid() const { return vt && library && library->isLoaded(); }

    /* 调用插件；返回 JSON 结果对象（拷贝，插件缓冲下次调用会被覆盖） */
    QJsonObject handle(const QJsonObject& request)
    {
        const QByteArray req = QJsonDocument(request).toJson(QJsonDocument::Compact);
        const char* raw = vt->handle(req.constData());
        const QJsonDocument doc = QJsonDocument::fromJson(raw ? raw : "{}");
        return doc.isObject() ? doc.object() : QJsonObject{};
    }
};

namespace {

QJsonObject manifestOf(const LoadedPlugin& p)
{
    return QJsonObject{
        { QStringLiteral("id"), p.id },
        { QStringLiteral("name"), p.name },
        { QStringLiteral("version"), p.version },
        { QStringLiteral("kind"), static_cast<int>(p.kind) },
        { QStringLiteral("configSchema"), p.configSchema },
        { QStringLiteral("contributions"), p.contributions },
    };
}

bool isPluginFile(const QString& name)
{
    return name.endsWith(QStringLiteral(".so"))
        || name.endsWith(QStringLiteral(".dll"))
        || name.endsWith(QStringLiteral(".dylib"));
}

} // namespace

namespace {
QJsonObject s_settings; /* 主程序下发的有效设置（供插件 config 使用） */
QList<LoadedPlugin> m_plugins;
}

int main(int argc, char* argv[])
{
    sc::installConsoleLogging();
    sc::installCrashHandlers(QStringLiteral("frap-host"));
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("frap-host"));

    QStringList pluginDirs;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--plugin-dir") && i + 1 < argc)
            pluginDirs << QString::fromLocal8Bit(argv[++i]);
    }

    /* stdout：无缓冲直接写 fd 1 */
    QFile stdoutFile;
    if (!stdoutFile.open(1, QIODevice::WriteOnly | QIODevice::Unbuffered)) {
        qWarning() << "host: 无法打开 stdout";
        return 1;
    }

    RpcPeer peer;
    peer.setSendCallback([&stdoutFile](const QByteArray& line) {
        stdoutFile.write(line);
    });

    /* ---- 注册请求处理 ---- */

    peer.onRequest(kMethodPluginInvoke, [&](const QJsonObject& params) {
        const QString pluginId = params.value(QStringLiteral("pluginId")).toString();
        const QString request = params.value(QStringLiteral("request")).toString();
        const QJsonObject payload = params.value(QStringLiteral("payload")).toObject();
        const QJsonObject config = params.value(QStringLiteral("config")).toObject();

        for (LoadedPlugin& p : m_plugins) {
            if (p.id != pluginId)
                continue;
            if (!p.valid())
                return QJsonObject{
                    { QStringLiteral("status"), QStringLiteral("error") },
                    { QStringLiteral("message"), QStringLiteral("插件无效") },
                };
            const QJsonObject req{
                { QStringLiteral("request"), request },
                { QStringLiteral("payload"), payload },
                { QStringLiteral("config"), config },
            };
            QJsonObject result = p.handle(req);
            if (result.isEmpty())
                result = QJsonObject{
                    { QStringLiteral("status"), QStringLiteral("error") },
                    { QStringLiteral("message"), QStringLiteral("插件返回非法 JSON") },
                };
            return result;
        }
        return QJsonObject{
            { QStringLiteral("status"), QStringLiteral("error") },
            { QStringLiteral("message"), QStringLiteral("插件未找到: %1").arg(pluginId) },
        };
    });

    peer.onNotification(kMethodInitializeResult, [](const QJsonObject& params) {
        /* 记录主程序下发的有效设置（后续可注入插件 config） */
        s_settings = params.value(QStringLiteral("settings")).toObject();
    });

    peer.onNotification(kMethodShutdown, [&](const QJsonObject&) {
        app.quit();
    });

    /* ---- 加载插件 ---- */

    QList<LoadedPlugin> plugins;
    for (const QString& dirStr : pluginDirs) {
        const QDir dir(dirStr);
        if (!dir.exists())
            continue;
        const QStringList entries = dir.entryList(QDir::Files, QDir::Name);
        for (const QString& entry : entries) {
            if (!isPluginFile(entry))
                continue;
            const QString path = dir.filePath(entry);
            LoadedPlugin p;
            p.library = new QLibrary(path);
            if (!p.library->load()) {
                peer.sendNotification(kMethodLog, {
                    { QStringLiteral("level"), QStringLiteral("error") },
                    { QStringLiteral("message"), QStringLiteral("加载插件失败 %1: %2")
                        .arg(path, p.library->errorString()) },
                });
                continue;
            }
            auto entryFn = reinterpret_cast<FrapPluginEntry>(p.library->resolve("frap_plugin_entry"));
            if (!entryFn) {
                peer.sendNotification(kMethodLog, {
                    { QStringLiteral("level"), QStringLiteral("error") },
                    { QStringLiteral("message"), QStringLiteral("插件缺少入口符号 %1").arg(path) },
                });
                continue;
            }
            p.vt = entryFn();
            if (!p.vt || p.vt->apiVersion != FRAP_PLUGIN_API_VERSION) {
                peer.sendNotification(kMethodLog, {
                    { QStringLiteral("level"), QStringLiteral("error") },
                    { QStringLiteral("message"), QStringLiteral("插件 ABI 版本不匹配 %1").arg(path) },
                });
                continue;
            }
            p.id = QString::fromUtf8(p.vt->id());
            p.name = QString::fromUtf8(p.vt->name());
            p.version = QString::fromUtf8(p.vt->version());
            p.kind = p.vt->kind();
            p.configSchema = QJsonDocument::fromJson(p.vt->configSchemaJson()).array();
            p.contributions = QJsonDocument::fromJson(p.vt->contributionsJson()).array();
            plugins.append(p);
        }
    }

    /* ---- 握手：上报初始化信息 ---- */

    peer.sendNotification(kMethodInitialize, {
        { QStringLiteral("apiVersion"), FRAP_PLUGIN_API_VERSION },
        { QStringLiteral("protocolVersion"), FRAP_PROTOCOL_VERSION },
        { QStringLiteral("hostInfo"), QJsonObject{
            { QStringLiteral("pid"), QCoreApplication::applicationPid() },
            { QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()) },
            { QStringLiteral("hostVersion"), QStringLiteral("0.1.0") },
        } },
    });

    /* ---- 上报插件 manifest ---- */

    for (const LoadedPlugin& p : plugins)
        peer.sendNotification(kMethodPluginManifest,
                              { { QStringLiteral("manifest"), manifestOf(p) } });
    peer.sendNotification(kMethodPluginManifest, {
        { QStringLiteral("manifest"), QJsonObject{} },
        { QStringLiteral("complete"), true },
    });

    /* 保证 plugins 等对象生命周期到事件循环结束 */
    m_plugins = std::move(plugins);

    /* ---- stdin 事件进入主循环 ---- */

    StdinReader reader;
    QObject::connect(&reader, &StdinReader::lineRead, &app, [&](const QByteArray& line) {
        peer.handleLine(line);
    });
    reader.start();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        reader.stop();
        for (const LoadedPlugin& p : m_plugins)
            if (p.valid())
                p.vt->shutdown();
    });

    const int code = app.exec();
    /* 释放插件库 */
    for (LoadedPlugin& p : m_plugins)
        delete p.library;
    return code;
}

#include "main.moc"
