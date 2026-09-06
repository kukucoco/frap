/*
 * service_translate —— 截图翻译插件（参照 screenshot-translator.py）
 *
 * 工作流（与参考脚本一致）：
 *   主程序把选区的图像以 base64 传入 -> 本插件调用百度图片翻译 API
 *   （~/.config/screenshot-translator/config.json 提供 appid/api_key）
 *   -> 返回 { src 原文, dst 译文, paste_img 译文图(base64) }
 *
 * 插件类型：Service（工具条"翻译"按钮）。使用 Qt Network 做同步 HTTP（
 * handle 内嵌套事件循环等待回包），并保持无状态。
 */
#include "plugin_helper.h"
#include "json_min.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>

namespace {

const QString kApiUrl = QStringLiteral("https://fanyi-api.baidu.com/ait/api/picture/translate");

struct Config {
    QString appid;
    QString apiKey;
};

Config loadConfig()
{
    Config c;
    const QString path = QDir::homePath()
        + QStringLiteral("/.config/screenshot-translator/config.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return c;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonObject o = doc.object();
    c.appid = o.value(QStringLiteral("appid")).toString();
    c.apiKey = o.value(QStringLiteral("api_key")).toString();
    return c;
}

QString pictureTranslate(const QString& imageBase64, const Config& cfg)
{
    QNetworkAccessManager nam;

    QNetworkRequest req{QUrl(kApiUrl)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", ("Bearer " + cfg.apiKey).toUtf8());

    QJsonObject body;
    body.insert(QStringLiteral("from"), QStringLiteral("auto"));
    body.insert(QStringLiteral("to"), QStringLiteral("zh"));
    body.insert(QStringLiteral("appid"), cfg.appid);
    body.insert(QStringLiteral("content"), imageBase64);
    body.insert(QStringLiteral("paste"), 1);
    body.insert(QStringLiteral("need_intervene"), 0);
    body.insert(QStringLiteral("view_type"), 1);
    body.insert(QStringLiteral("model_type"), QStringLiteral("nmt"));
    const QJsonDocument bodyDoc(body);
    const QByteArray payload = bodyDoc.toJson(QJsonDocument::Compact);

    QNetworkReply* reply = nam.post(req, payload);
    QByteArray data;
    bool ok = false;
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, [&]() { ok = !reply->error(); });
    loop.exec();
    if (reply->isFinished())
        data = reply->readAll();
    reply->deleteLater();

    QString result;
    if (!ok)
        return QStringLiteral("__ERR__网络请求失败");
    const QJsonDocument resp = QJsonDocument::fromJson(data);
    const QJsonObject o = resp.object();
    const QString errCode = o.value(QStringLiteral("error_code")).toString();
    if (!errCode.isEmpty() && errCode != QStringLiteral("0"))
        return QStringLiteral("__ERR__[%1] %2").arg(errCode,
                                                    o.value(QStringLiteral("error_msg")).toString());

    result = QString::fromUtf8(data);
    return result;
}

} // namespace

class TranslateService : public sc::PluginBase
{
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Service; }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"service","data":{"id":"service.translate","label":"翻译","order":60,"shortcut":"y"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        static QString lastResult;
        try {
            const jmin::Json req = jmin::parse(request);
            const std::string op = req.get("request").asString();
            const std::string imageBase64 = req.get("payload").get("imageBase64").asString();
            if (op != "translate")
                return jmin::Json::objectOf("message",
                                            jmin::Json::makeString("未知请求"))
                           .toString();

            const Config cfg = loadConfig();
            if (cfg.appid.isEmpty() || cfg.apiKey.isEmpty())
                return jmin::Json::objectOf(
                           "message",
                           jmin::Json::makeString(
                               "缺少配置：请在 ~/.config/screenshot-translator/config.json "
                               "填写 appid 与 api_key"))
                       .toString();

            const QString raw = pictureTranslate(QString::fromStdString(imageBase64), cfg);
            if (raw.startsWith(QStringLiteral("__ERR__")))
                return jmin::Json::objectOf("message",
                                            jmin::Json::makeString(raw.mid(7).toStdString()))
                       .toString();

            /* 回传原文/译文/译文图 */
            const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
            const QJsonObject o = doc.object();
            QJsonObject res;
            res.insert(QStringLiteral("status"), QStringLiteral("ok"));
            res.insert(QStringLiteral("src"), o.value(QStringLiteral("src")));
            res.insert(QStringLiteral("dst"), o.value(QStringLiteral("dst")));
            res.insert(QStringLiteral("paste_img"), o.value(QStringLiteral("paste_img")));
            lastResult = QString::fromUtf8(
                QJsonDocument(res).toJson(QJsonDocument::Compact));
            return lastResult.toStdString();
        } catch (const std::exception& e) {
            return jmin::Json::objectOf("message", jmin::Json::makeString(e.what())).toString();
        }
    }

private:
    const std::string m_id = "service.translate";
    const std::string m_name = "翻译";
};

FRAP_PLUGIN_DEFINE(TranslateService)
