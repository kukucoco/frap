/*
 * rpc_peer.h —— JSON-RPC 线协议客户端/服务端通用实现
 *
 * 用法（两端相同）：
 *   RpcPeer peer;
 *   peer.setSendCallback([&](const QByteArray& line) { 写入对方 fd });
 *   peer.onRequest("method", [](const QJsonObject& params) { return 结果; });
 *   peer.onNotification("method", [](const QJsonObject& params) { ... });
 *
 *   // 收到一行数据时：
 *   peer.handleLine(line);
 */
#ifndef FRAP_CORE_RPC_PEER_H
#define FRAP_CORE_RPC_PEER_H

#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QHash>
#include <functional>

#include "protocol.h"

namespace sc {

class RpcPeer
{
public:
    using SendFn = std::function<void(const QByteArray& /*line*/)>;
    using HandlerFn = std::function<void(const QJsonObject& /*params*/)>;
    /* 处理一次 request，返回 JSON 结果对象；返回空对象表示出错 */
    using RequestHandlerFn = std::function<QJsonObject(const QJsonObject& /*params*/)>;
    using ReplyFn = std::function<void(const QJsonObject& /*result*/, const QString& /*errorMessage*/)>;

    void setSendCallback(SendFn fn) { m_send = std::move(fn); }

    /* 注册某方法的 notification 处理器 */
    void onNotification(const QString& method, HandlerFn fn)
    {
        m_notifications[method] = std::move(fn);
    }

    /* 注册某方法的 request 处理器 */
    void onRequest(const QString& method, RequestHandlerFn fn)
    {
        m_requests[method] = std::move(fn);
    }

    /* 发送一个 notification */
    void sendNotification(const QString& method, const QJsonObject& params)
    {
        sendLine(makeNotification(method, params));
    }

    /* 发送一个 request，并登记回复回调 */
    void sendRequest(const QString& method, const QJsonObject& params, ReplyFn reply)
    {
        const int id = m_nextId++;
        m_pending.insert(id, std::move(reply));
        sendLine(makeRequest(id, method, params));
    }

    /* 对某 request id 发送回复 */
    void sendResponse(int id, const QJsonObject& result)
    {
        sendLine(makeResponse(id, result));
    }

    void sendError(int id, int code, const QString& message)
    {
        sendLine(makeError(id, code, message));
    }

    /* 解析并分发一行 JSON 消息 */
    void handleLine(const QByteArray& line)
    {
        if (line.trimmed().isEmpty())
            return;
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject())
            return;
        const QJsonObject msg = doc.object();
        const QString method = msg.value(QStringLiteral("method")).toString();
        const QJsonValue idVal = msg.value(QStringLiteral("id"));

        if (!method.isEmpty()) {
            /* notification 或 request */
            const QJsonObject params = msg.value(QStringLiteral("params")).toObject();

            if (idVal.isUndefined()) {
                auto it = m_notifications.find(method);
                if (it != m_notifications.end())
                    it.value()(params);
                return;
            }

            auto rit = m_requests.find(method);
            if (rit == m_requests.end()) {
                sendError(idVal.toInt(), kErrProtocol,
                          QStringLiteral("unknown method: %1").arg(method));
                return;
            }
            QJsonObject result = rit.value()(params);
            if (result.isEmpty())
                sendError(idVal.toInt(), kErrRequestFailed, QStringLiteral("handler failed"));
            else
                sendResponse(idVal.toInt(), result);
            return;
        }

        /* response 或 error */
        if (idVal.isUndefined())
            return;
        const int id = idVal.toInt();
        auto it = m_pending.find(id);
        if (it == m_pending.end())
            return;
        ReplyFn reply = it.value();
        m_pending.erase(it);

        if (msg.contains(QStringLiteral("error"))) {
            const QString message = msg.value(QStringLiteral("error")).toObject()
                                        .value(QStringLiteral("message")).toString();
            reply(QJsonObject(), message);
        } else {
            reply(msg.value(QStringLiteral("result")).toObject(), QString());
        }
    }

private:
    void sendLine(const QJsonObject& obj)
    {
        if (!m_send)
            return;
        m_send(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
    }

    SendFn m_send;
    QHash<QString, HandlerFn> m_notifications;
    QHash<QString, RequestHandlerFn> m_requests;
    QHash<int, ReplyFn> m_pending;
    int m_nextId = 1;
};

} // namespace sc

#endif /* FRAP_CORE_RPC_PEER_H */
