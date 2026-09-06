/*
 * plugin_manager.h —— 插件生命周期管理（主程序侧）
 *
 * 职责：
 *   - 启动插件宿主进程（frap-host）
 *   - 完成协议握手，收集插件 manifest
 *   - 向宿主转发 plugin.invoke 请求
 *   - 监视宿主进程，异常退出时自动重启（带退避）
 *   - 主程序退出时优雅关闭宿主
 *
 * 设计要点（与 VSCode extension host 对齐）：
 *   插件运行在独立进程中，主程序崩溃或插件崩溃互不影响；
 *   主程序与宿主的唯一契约是 JSON 协议，与 Qt/编译器 ABI 无关。
 */
#ifndef FRAP_CORE_PLUGIN_MANAGER_H
#define FRAP_CORE_PLUGIN_MANAGER_H

#include <QObject>
#include <QProcess>
#include <QList>
#include <QHash>
#include <QByteArray>

#include "rpc_peer.h"
#include "plugin_manifest.h"
#include "settings.h"

namespace sc {

class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(Settings* settings, QObject* parent = nullptr);
    ~PluginManager() override;

    /* 启动宿主并握手（异步，完成时发出 ready / failed） */
    void start();
    /* 停止宿主（发送 shutdown，等待后终止） */
    void stop();

    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }
    bool isReady() const { return m_ready; }
    qint64 hostPid() const { return m_process ? m_process->processId() : -1; }
    const QList<PluginManifest>& plugins() const { return m_manifests; }

    PluginManifest manifest(const QString& id) const;
    QList<PluginManifest> pluginsOfKind(int kind) const;

    /* 按贡献点类型聚合（tool/export/...），返回 Contribution + 所属插件 id */
    struct ContributionEntry {
        Contribution contribution;
        PluginManifest plugin;
    };
    QList<ContributionEntry> contributions(const QString& type) const;

    /*
     * 调用某插件。invoke 成功时 reply 收到 result（QJsonObject），
     * 失败时收到 errorMessage。结果中的 actions 由宿主直接回传，
     * 由调用方决定是否执行（见 session/应用层）。
     */
    using InvokeReplyFn = std::function<void(const QJsonObject& result, const QString& error)>;
    void invoke(const QString& pluginId, const QString& request,
                const QJsonObject& payload, InvokeReplyFn reply);
    void invoke(const QString& pluginId, const QString& request,
                const QJsonObject& payload,
                const QJsonObject& config,
                InvokeReplyFn reply);

signals:
    void ready();                              /* 握手完成、manifest 收集完毕 */
    void failed(const QString& message);       /* 宿主无法启动/握手失败 */
    void pluginLoaded(const sc::PluginManifest& manifest);
    void pluginLog(const QString& level, const QString& message);
    void hostRestarted(int attempt);

private:
    void spawn();
    void processExit(int exitCode, QProcess::ExitStatus status);
    void handleInitialize(const QJsonObject& params);
    void handleManifest(const QJsonObject& params);
    void handleLog(const QJsonObject& params);
    void onStdout();

    Settings* m_settings;
    QProcess* m_process = nullptr;
    RpcPeer m_peer;
    QList<PluginManifest> m_manifests;
    QHash<QString, int> m_manifestIndex;
    bool m_ready = false;
    bool m_stopping = false;
    int m_restartAttempt = 0;
};

} // namespace sc

#endif /* FRAP_CORE_PLUGIN_MANAGER_H */
