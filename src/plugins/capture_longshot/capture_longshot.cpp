/*
 * capture_longshot —— 长截图（滚动拼接）插件
 *
 * 负责"拼接"与"重叠检测"（纯图像处理，在宿主进程执行）：
 *   主程序分屏截取后，把多个分段图像（base64 PNG）传来，
 *   本插件自动检测相邻两图的垂直重叠，去掉重复后纵向拼成一张长图。
 *
 * request:
 *   "find_overlap"  payload { aBase64, bBase64 }      -> { overlap }
 *   "stitch"        payload { sections:[base64...] }  -> { imageBase64, height }
 */
#include "plugin_helper.h"
#include "json_min.h"

#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QString>
#include <algorithm>

namespace {

QImage fromBase64(const std::string& b64)
{
    QImage img;
    img.loadFromData(QByteArray::fromBase64(QByteArray::fromStdString(b64)), "PNG");
    return img;
}

std::string toBase64(const QImage& img)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return data.toBase64().toStdString();
}

/* 垂直重叠检测：bottom 的顶部 `o` 行 与 top 的底部 `o` 行 最匹配的偏移 */
int findOverlap(const QImage& top, const QImage& bottom, int maxOverlap)
{
    if (top.isNull() || bottom.isNull())
        return 0;
    const int w = qMin(top.width(), bottom.width());
    const int hTop = top.height(), hBot = bottom.height();
    int best = 0;
    double bestDiff = 1e18;

    const int range = qMin(maxOverlap, qMin(hTop, hBot) / 2);
    const int rows = qMin(12, hBot); /* 采样 bottom 顶部行数 */
    for (int o = 0; o <= range; ++o) {
        double diff = 0.0;
        int n = 0;
        for (int r = 0; r < rows; ++r) {
            const int ty = hTop - o + r;          /* top 的对应行（o 上方） */
            if (ty < 0 || ty >= hTop)
                break;
            const QRgb* bp = reinterpret_cast<const QRgb*>(bottom.constScanLine(r));
            const QRgb* tp = reinterpret_cast<const QRgb*>(top.constScanLine(ty));
            for (int x = 0; x < w; ++x) {
                const int br = qRed(bp[x]), bg = qGreen(bp[x]), bb = qBlue(bp[x]);
                const int tr = qRed(tp[x]), tg = qGreen(tp[x]), tb = qBlue(tp[x]);
                diff += qAbs(br - tr) + qAbs(bg - tg) + qAbs(bb - tb);
                ++n;
            }
        }
        if (n > 0) {
            diff /= n;
            if (diff < bestDiff) {
                bestDiff = diff;
                best = o;
            }
        }
    }
    return best;
}

/* 纵向拼接：top + bottom[overlap..] */
QImage stitch(const QImage& top, const QImage& bottom, int overlap)
{
    if (top.isNull())
        return bottom;
    if (bottom.isNull())
        return top;
    const int w = qMax(top.width(), bottom.width());
    const int newH = top.height() + bottom.height() - overlap;
    QImage out(w, newH, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawImage(0, 0, top);
    p.drawImage(0, top.height() - overlap, bottom);
    p.end();
    return out;
}

QString msg(const QString& m)
{
    QJsonObject o;
    o.insert(QStringLiteral("status"), QStringLiteral("error"));
    o.insert(QStringLiteral("message"), m);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace

class LongshotCapture : public sc::PluginBase
{
public:
    const std::string& id() const override { return m_id; }
    const std::string& name() const override { return m_name; }
    sc::PluginKind kind() const override { return sc::PluginKind::Capture; }

    std::string contributionsJson() const override
    {
        return R"([
            {"type":"service","data":{"id":"capture.longshot","label":"长截图","order":70,"shortcut":"g"}}
        ])";
    }

    std::string handle(const std::string& request) override
    {
        static QString lastResult;
        try {
            const jmin::Json req = jmin::parse(request);
            const std::string op = req.get("request").asString();
            const jmin::Json& payload = req.get("payload");

            if (op == "find_overlap") {
                const QImage a = fromBase64(payload.get("aBase64").asString());
                const QImage b = fromBase64(payload.get("bBase64").asString());
                const int overlap = findOverlap(a, b, 200);
                QJsonObject res;
                res.insert(QStringLiteral("status"), QStringLiteral("ok"));
                res.insert(QStringLiteral("overlap"), overlap);
                lastResult = QString::fromUtf8(
                    QJsonDocument(res).toJson(QJsonDocument::Compact));
                return lastResult.toStdString();
            }

            if (op == "stitch") {
                const jmin::Json& sections = payload.get("sections");
                if (sections.size() < 1)
                    return msg(QStringLiteral("至少需要一张截图")).toStdString();
                QImage result;
                for (size_t i = 0; i < sections.size(); ++i) {
                    const QImage img = fromBase64(sections.at(i).asString());
                    if (img.isNull())
                        return msg(QStringLiteral("分段图像无效")).toStdString();
                    if (result.isNull()) {
                        result = img;
                    } else {
                        const int overlap = findOverlap(result, img, 200);
                        result = stitch(result, img, overlap);
                    }
                }
                QJsonObject res;
                res.insert(QStringLiteral("status"), QStringLiteral("ok"));
                res.insert(QStringLiteral("imageBase64"),
                           QJsonValue(QString::fromStdString(toBase64(result))));
                res.insert(QStringLiteral("width"), result.width());
                res.insert(QStringLiteral("height"), result.height());
                lastResult = QString::fromUtf8(
                    QJsonDocument(res).toJson(QJsonDocument::Compact));
                return lastResult.toStdString();
            }

            return msg(QStringLiteral("未知请求: ") + QString::fromStdString(op)).toStdString();
        } catch (const std::exception& e) {
            return msg(QString::fromUtf8(e.what())).toStdString();
        }
    }

private:
    const std::string m_id = "capture.longshot";
    const std::string m_name = "长截图";
};

FRAP_PLUGIN_DEFINE(LongshotCapture)
