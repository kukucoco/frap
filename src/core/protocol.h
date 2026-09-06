/*
 * protocol.h —— 主程序 / 插件宿主之间的线协议定义（协议 v1）
 *
 * 传输：换行分隔的 JSON 消息（每行一个完整 JSON 对象），走 stdio。
 *       JSON 使用紧凑序列化，其中 base64 不包含换行符，因此按行分帧安全。
 * 语义：JSON-RPC 2.0 风格（request/response/notification）。
 *
 * 完整规范见 docs/protocol.md。
 */
#ifndef FRAP_CORE_PROTOCOL_H
#define FRAP_CORE_PROTOCOL_H

#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QImage>
#include <QBuffer>

#include "plugin_api.h"

namespace sc {

/* 当前协议版本 */
constexpr int kProtocolVersion = FRAP_PROTOCOL_VERSION;

/* 宿主（host）→ 主程序（core）：上报启动信息，紧接着主程序回 initialize.result */
const QString kMethodInitialize = QStringLiteral("initialize");

/* 主程序 → 宿主：回复启动信息（携带全部有效设置，供插件读取） */
const QString kMethodInitializeResult = QStringLiteral("initialize.result");

/* 宿主 → 主程序：上报已加载插件的 manifest */
const QString kMethodPluginManifest = QStringLiteral("plugin.manifest");

/* 宿主 → 主程序：日志 */
const QString kMethodLog = QStringLiteral("log");

/* 主程序 → 宿主：调用某插件 */
const QString kMethodPluginInvoke = QStringLiteral("plugin.invoke");

/* 主程序 → 宿主：优雅退出 */
const QString kMethodShutdown = QStringLiteral("host.shutdown");

/* JSON-RPC 错误码（自定义段） */
enum RpcErrorCode {
    kErrRequestFailed = -32000,
    kErrPluginNotFound = -32001,
    kErrPluginError = -32002,
    kErrProtocol = -32003,
};

/* ---- 消息构造 ---- */

inline QJsonObject makeNotification(const QString& method, const QJsonObject& params)
{
    return QJsonObject{
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("method"), method },
        { QStringLiteral("params"), params },
    };
}

inline QJsonObject makeRequest(int id, const QString& method, const QJsonObject& params)
{
    return QJsonObject{
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("method"), method },
        { QStringLiteral("params"), params },
    };
}

inline QJsonObject makeResponse(int id, const QJsonObject& result)
{
    return QJsonObject{
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("result"), result },
    };
}

inline QJsonObject makeError(int id, int code, const QString& message)
{
    return QJsonObject{
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("error"), QJsonObject{
            { QStringLiteral("code"), code },
            { QStringLiteral("message"), message },
        } },
    };
}

/* 图像以 PNG 编码，JSON 中传递 base64。 */
inline QByteArray imageToBase64(const QImage& image, const QByteArray& format = "PNG")
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format.constData());
    return data.toBase64();
}

} // namespace sc

#endif /* FRAP_CORE_PROTOCOL_H */
