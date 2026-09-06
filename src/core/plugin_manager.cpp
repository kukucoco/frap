#include "plugin_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QEventLoop>
#include <QFileInfo>

namespace sc {

PluginManager::PluginManager(Settings* settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_peer.setSendCallback([this](const QByteArray& line) {
        if (m_process && m_process->state() == QProcess::Running)
            m_process->write(line);
    });

    m_peer.onNotification(kMethodInitialize, [this](const QJsonObject& params) {
        handleInitialize(params);
    });
    m_peer.onNotification(kMethodPluginManifest, [this](const QJsonObject& params) {
        handleManifest(params);
    });
    m_peer.onNotification(kMethodLog, [this](const QJsonObject& params) {
        handleLog(params);
    });
}

PluginManager::~PluginManager()
{
    stop();
}

void PluginManager::start()
{
    m_restartAttempt = 0;
    spawn();
}

void PluginManager::stop()
{
    m_stopping = true;
    if (!m_process)
        return;

    if (m_process->state() == QProcess::Running) {
        m_peer.sendNotification(kMethodShutdown, {});
        /* 给宿主 1 秒优雅退出，超时强杀 */
        QEventLoop loop;
        QTimer::singleShot(1000, &loop, &QEventLoop::quit);
        connect(m_process, &QProcess::finished, &loop, &QEventLoop::quit);
        if (m_process->state() == QProcess::Running)
            loop.exec();
    }
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(500))
            m_process->kill();
    }
    m_process->deleteLater();
    m_process = nullptr;
    m_ready = false;
}

void PluginManager::spawn()
{
    if (m_stopping)
        return;

    m_manifests.clear();
    m_manifestIndex.clear();
    m_ready = false;

    if (!m_process) {
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::SeparateChannels);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &PluginManager::onStdout);
        connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
            const QByteArray err = m_process->readAllStandardError();
            if (!err.trimmed().isEmpty())
                emit pluginLog(QStringLiteral("host"), QString::fromUtf8(err).trimmed());
        });
        connect(m_process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,
                &PluginManager::processExit);
    }

    const QString hostPath = QCoreApplication::applicationDirPath()
                             + QStringLiteral("/frap-host");
    /* 候选宿主路径：编译期注入 > 应用目录 > 应用目录上一级 */
    QStringList candidates;
#ifndef FRAP_HOST_PATH
#define FRAP_HOST_PATH ""
#endif
    const QString configured = QStringLiteral(FRAP_HOST_PATH);
    if (!configured.isEmpty())
        candidates << configured;
    candidates << hostPath
               << QCoreApplication::applicationDirPath() + QStringLiteral("/../frap-host");

    QString resolved;
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) {
            resolved = c;
            break;
        }
    }
    if (resolved.isEmpty()) {
        emit failed(QStringLiteral("找不到插件宿主（已尝试: %1）")
                        .arg(candidates.join(QStringLiteral(", "))));
        return;
    }

    QStringList args;
    for (const QString& dir : m_settings->pluginDirectories())
        args << QStringLiteral("--plugin-dir") << dir;

    qInfo() << "PluginManager: 启动宿主" << resolved << args.join(' ');
    m_process->start(resolved, args);
    if (!m_process->waitForStarted(3000)) {
        emit failed(QStringLiteral("宿主启动失败: %1").arg(m_process->errorString()));
        return;
    }
}

void PluginManager::onStdout()
{
    static QByteArray buffer;
    buffer.append(m_process->readAllStandardOutput());
    int idx;
    while ((idx = buffer.indexOf('\n')) >= 0) {
        const QByteArray line = buffer.left(idx).trimmed();
        buffer.remove(0, idx + 1);
        if (!line.isEmpty())
            m_peer.handleLine(line);
    }
}

void PluginManager::handleInitialize(const QJsonObject& params)
{
    Q_UNUSED(params)
    if (m_ready)
        return;

    /* 回发有效设置，供宿主注入各插件 config */
    const QJsonObject settings = m_settings->effectiveSettings();
    m_peer.sendNotification(kMethodInitializeResult,
                            QJsonObject{ { QStringLiteral("settings"), settings } });
}

void PluginManager::handleManifest(const QJsonObject& params)
{
    /* 完成标记：manifest 为空 id + complete=true，代表全部插件上报完毕 */
    if (params.value(QStringLiteral("complete")).toBool()) {
        m_ready = true;
        emit ready();
        return;
    }

    const PluginManifest manifest = parseManifest(
        params.value(QStringLiteral("manifest")).toObject());
    if (manifest.id.isEmpty())
        return;
    if (!m_manifestIndex.contains(manifest.id)) {
        m_manifestIndex.insert(manifest.id, m_manifests.size());
        m_manifests.append(manifest);
        /* 注册插件声明设置项 -> 有效设置/配置下发/选项条渲染 */
        m_settings->registerSpecs(manifest.configSchema);
        emit pluginLoaded(manifest);
    }
}

void PluginManager::handleLog(const QJsonObject& params)
{
    emit pluginLog(params.value(QStringLiteral("level")).toString(),
                   params.value(QStringLiteral("message")).toString());
}

void PluginManager::processExit(int exitCode, QProcess::ExitStatus status)
{
    if (m_stopping)
        return;

    const bool crashed = status == QProcess::CrashExit || exitCode != 0;
    qWarning() << "PluginManager: 宿主退出，code=" << exitCode
               << "crashed=" << crashed << "restartAttempt=" << m_restartAttempt;

    m_ready = false;
    if (!crashed)
        return;

    /* 退避重试：300ms * 2^n，最多 6 次 */
    ++m_restartAttempt;
    if (m_restartAttempt > 6) {
        emit failed(QStringLiteral("插件宿主多次崩溃，已停止重启"));
        return;
    }
    emit hostRestarted(m_restartAttempt);
    const int delay = 300 * (1 << (m_restartAttempt - 1));
    QTimer::singleShot(delay, this, [this]() { spawn(); });
}

PluginManifest PluginManager::manifest(const QString& id) const
{
    const int idx = m_manifestIndex.value(id, -1);
    return idx >= 0 ? m_manifests.at(idx) : PluginManifest{};
}

QList<PluginManifest> PluginManager::pluginsOfKind(int kind) const
{
    QList<PluginManifest> out;
    for (const PluginManifest& m : m_manifests)
        if (m.kind == kind)
            out.append(m);
    return out;
}

QList<PluginManager::ContributionEntry> PluginManager::contributions(const QString& type) const
{
    QList<ContributionEntry> out;
    for (const PluginManifest& m : m_manifests) {
        for (const Contribution& c : m.contributions) {
            if (c.type == type)
                out.append({ c, m });
        }
    }
    return out;
}

void PluginManager::invoke(const QString& pluginId, const QString& request,
                           const QJsonObject& payload, InvokeReplyFn reply)
{
    invoke(pluginId, request, payload, m_settings->pluginConfig(pluginId), std::move(reply));
}

void PluginManager::invoke(const QString& pluginId, const QString& request,
                           const QJsonObject& payload, const QJsonObject& config,
                           InvokeReplyFn reply)
{
    if (!m_ready) {
        if (reply)
            reply(QJsonObject(), QStringLiteral("宿主尚未就绪"));
        return;
    }
    if (!m_manifestIndex.contains(pluginId)) {
        if (reply)
            reply(QJsonObject(), QStringLiteral("插件未加载: %1").arg(pluginId));
        return;
    }
    const QJsonObject params{
        { QStringLiteral("pluginId"), pluginId },
        { QStringLiteral("request"), request },
        { QStringLiteral("payload"), payload },
        { QStringLiteral("config"), config },
    };
    m_peer.sendRequest(kMethodPluginInvoke, params,
                       [reply](const QJsonObject& result, const QString& error) {
                           if (reply)
                               reply(result, error);
                       });
}

} // namespace sc
