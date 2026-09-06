#include "render_ops.h"

#include <QPainter>
#include <QFont>
#include <QColor>
#include <QJsonArray>
#include <QPolygonF>
#include <QLineF>
#include <QDebug>
#include <QtMath>

namespace sc {

namespace {

QRectF toRectF(const QJsonObject& r)
{
    return QRectF(r.value(QStringLiteral("x")).toDouble(),
                  r.value(QStringLiteral("y")).toDouble(),
                  r.value(QStringLiteral("w")).toDouble(),
                  r.value(QStringLiteral("h")).toDouble());
}

QPointF toPointF(const QJsonObject& p)
{
    return QPointF(p.value(QStringLiteral("x")).toDouble(),
                   p.value(QStringLiteral("y")).toDouble());
}

QColor toColor(const QJsonObject& obj, const QJsonObject& fallback)
{
    const QString name = obj.value(QStringLiteral("color")).toString();
    if (!name.isEmpty())
        return QColor(name);
    const QString fb = fallback.value(QStringLiteral("color")).toString();
    return QColor(fb.isEmpty() ? QStringLiteral("#e11d48") : fb);
}

void makePen(QPainter& painter, const QJsonObject& obj, const QJsonObject& fb,
             int defaultWidth = 3)
{
    const qreal width = obj.value(QStringLiteral("width")).toDouble(defaultWidth);
    QPen pen(toColor(obj, fb), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
}

/* 马赛克：按 cell 网格对 rect 内像素取均值填色 */
void drawMosaic(QImage& img, const QJsonObject& op)
{
    const QRectF rect = toRectF(op.value(QStringLiteral("rect")).toObject());
    const QRect area = rect.toAlignedRect().intersected(QRect(QPoint(0, 0), img.size()));
    if (area.isEmpty())
        return;
    const int cell = qMax(2, op.value(QStringLiteral("cell")).toInt(12));
    QPainter painter(&img);
    for (int y = area.top(); y <= area.bottom(); y += cell) {
        for (int x = area.left(); x <= area.right(); x += cell) {
            const int h = qMin(cell, area.bottom() - y + 1);
            const int w = qMin(cell, area.right() - x + 1);
            QColor avg;
            int r = 0, g = 0, b = 0, n = 0;
            for (int j = y; j < y + h && j < img.height(); ++j) {
                for (int i = x; i < x + w && i < img.width(); ++i) {
                    const QRgb px = img.pixel(i, j);
                    r += qRed(px); g += qGreen(px); b += qBlue(px); ++n;
                }
            }
            if (n == 0)
                continue;
            avg = QColor(r / n, g / n, b / n);
            painter.fillRect(x, y, w, h, avg);
        }
    }
}

/* 模糊：先缩小再放大（快速高斯近似） */
void drawBlur(QImage& img, const QJsonObject& op)
{
    const QRectF rect = toRectF(op.value(QStringLiteral("rect")).toObject());
    const QRect area = rect.toAlignedRect().intersected(QRect(QPoint(0, 0), img.size()));
    if (area.isEmpty())
        return;
    const int radius = qMax(1, op.value(QStringLiteral("radius")).toInt(6));
    const QImage sub = img.copy(area);
    const QSize small(qMax(1, sub.width() / radius), qMax(1, sub.height() / radius));
    QImage blurred = sub.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                         .scaled(sub.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QPainter painter(&img);
    painter.drawImage(area.topLeft(), blurred);
}

bool drawOperation(QImage& img, const QJsonObject& op)
{
    const QString type = op.value(QStringLiteral("type")).toString();
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);

    if (type == QStringLiteral("rect")) {
        const QRectF rect = toRectF(op.value(QStringLiteral("rect")).toObject());
        makePen(painter, op, op);
        if (op.value(QStringLiteral("fill")).isBool() && op.value(QStringLiteral("fill")).toBool()) {
            painter.setBrush(toColor(op, {}));
            painter.setPen(Qt::NoPen);
        }
        painter.drawRect(rect);
        return true;
    }
    if (type == QStringLiteral("ellipse")) {
        const QRectF rect = toRectF(op.value(QStringLiteral("rect")).toObject());
        makePen(painter, op, op);
        if (op.value(QStringLiteral("fill")).isBool() && op.value(QStringLiteral("fill")).toBool()) {
            painter.setBrush(toColor(op, {}));
            painter.setPen(Qt::NoPen);
        }
        painter.drawEllipse(rect);
        return true;
    }
    if (type == QStringLiteral("line")) {
        const QPointF start = toPointF(op.value(QStringLiteral("start")).toObject());
        const QPointF end = toPointF(op.value(QStringLiteral("end")).toObject());
        makePen(painter, op, op);
        painter.drawLine(start, end);
        return true;
    }
    if (type == QStringLiteral("arrow")) {
        const QPointF start = toPointF(op.value(QStringLiteral("start")).toObject());
        const QPointF end = toPointF(op.value(QStringLiteral("end")).toObject());
        const qreal headSize = op.value(QStringLiteral("headSize")).toDouble(14);
        makePen(painter, op, op);
        painter.drawLine(start, end);
        /* 箭头 */
        const QLineF line(start, end);
        const qreal angle = line.angle();
        const QPointF p1 = end - QLineF::fromPolar(headSize, angle - 25).p2();
        const QPointF p2 = end - QLineF::fromPolar(headSize, angle + 25).p2();
        const QPolygonF head{ end, p1, p2 };
        painter.setBrush(toColor(op, {}));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(head);
        return true;
    }
    if (type == QStringLiteral("pen")) {
        QPolygonF points;
        for (const QJsonValue& v : op.value(QStringLiteral("points")).toArray())
            points << toPointF(v.toObject());
        if (points.size() < 2)
            return false;
        makePen(painter, op, op, 4);
        painter.drawPolyline(points);
        return true;
    }
    if (type == QStringLiteral("text")) {
        const QPointF point = toPointF(op.value(QStringLiteral("point")).toObject());
        const QString text = op.value(QStringLiteral("text")).toString();
        QFont font(op.value(QStringLiteral("fontFamily")).toString(
                       QStringLiteral("Sans Serif")),
                   op.value(QStringLiteral("size")).toInt(24));
        if (op.value(QStringLiteral("bold")).toBool())
            font.setBold(true);
        painter.setFont(font);
        painter.setPen(toColor(op, {}));
        painter.drawText(point, text);
        return true;
    }
    if (type == QStringLiteral("number")) {
        const QPointF point = toPointF(op.value(QStringLiteral("point")).toObject());
        const QString text = op.value(QStringLiteral("text")).toString();
        const qreal size = op.value(QStringLiteral("size")).toInt(24);
        const qreal radius = size * 0.62;
        const QColor color = toColor(op, {});
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(point, radius, radius);
        QFont font(QStringLiteral("Sans Serif"), qRound(size * 0.62));
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(op.value(QStringLiteral("textColor")).toString().isEmpty()
                           ? QColor(Qt::white)
                           : QColor(op.value(QStringLiteral("textColor")).toString()));
        QRectF box(point.x() - radius, point.y() - radius, 2 * radius, 2 * radius);
        painter.drawText(box, Qt::AlignCenter, text);
        return true;
    }
    if (type == QStringLiteral("highlight")) {
        const QRectF rect = toRectF(op.value(QStringLiteral("rect")).toObject());
        QColor c = toColor(op, {});
        c.setAlpha(op.value(QStringLiteral("alpha")).toInt(120));
        painter.fillRect(rect, c);
        return true;
    }
    if (type == QStringLiteral("mosaic")) {
        painter.end();
        drawMosaic(img, op);
        return true;
    }
    if (type == QStringLiteral("blur")) {
        painter.end();
        drawBlur(img, op);
        return true;
    }
    return false;
}

} // namespace

QStringList supportedOperationTypes()
{
    return { QStringLiteral("rect"), QStringLiteral("ellipse"), QStringLiteral("line"),
             QStringLiteral("arrow"), QStringLiteral("pen"), QStringLiteral("text"),
             QStringLiteral("number"), QStringLiteral("highlight"), QStringLiteral("mosaic"),
             QStringLiteral("blur") };
}

bool isOperationSupported(const QJsonObject& op)
{
    return supportedOperationTypes().contains(op.value(QStringLiteral("type")).toString());
}

QImage applyOperations(const QImage& source, const QList<QJsonObject>& ops)
{
    QImage result = source;
    if (result.isNull())
        return result;
    result = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (const QJsonObject& op : ops) {
        if (isOperationSupported(op)) {
            if (!drawOperation(result, op))
                qWarning() << "render_ops: 操作渲染失败" << op;
        } else {
            qWarning() << "render_ops: 未知操作类型" << op.value(QStringLiteral("type")).toString();
        }
    }
    return result;
}

namespace {

QPointF opPoint(const QJsonObject& obj, const char* key)
{
    return toPointF(obj.value(QLatin1String(key)).toObject());
}

QRect opRectBounds(const QJsonObject& op)
{
    const QJsonObject rect = op.value(QStringLiteral("rect")).toObject();
    if (!rect.isEmpty())
        return toRectF(rect).toAlignedRect();
    const QPointF start = toPointF(op.value(QStringLiteral("start")).toObject());
    const QPointF end = toPointF(op.value(QStringLiteral("end")).toObject());
    if (!start.isNull() || !end.isNull())
        return QRectF(start, end).normalized().toAlignedRect();
    return QRect();
}

} // namespace

QRect operationBounds(const QJsonObject& op)
{
    const QString type = op.value(QStringLiteral("type")).toString();
    QRect bounds;

    if (type == QStringLiteral("rect") || type == QStringLiteral("ellipse")
        || type == QStringLiteral("highlight") || type == QStringLiteral("mosaic")
        || type == QStringLiteral("blur")) {
        bounds = opRectBounds(op);
    } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        bounds = opRectBounds(op);
        /* 线型：额外留出箭头/线宽空间 */
        bounds.adjust(-14, -14, 14, 14);
    } else if (type == QStringLiteral("pen")) {
        QPointF minPt, maxPt;
        bool first = true;
        for (const QJsonValue& v : op.value(QStringLiteral("points")).toArray()) {
            const QPointF p = toPointF(v.toObject());
            if (first) {
                minPt = maxPt = p;
                first = false;
            } else {
                minPt.setX(qMin(minPt.x(), p.x()));
                minPt.setY(qMin(minPt.y(), p.y()));
                maxPt.setX(qMax(maxPt.x(), p.x()));
                maxPt.setY(qMax(maxPt.y(), p.y()));
            }
        }
        if (!first)
            bounds = QRectF(minPt, maxPt).toAlignedRect().adjusted(-12, -12, 12, 12);
    } else if (type == QStringLiteral("text") || type == QStringLiteral("number")) {
        const QPointF p = opPoint(op, "point");
        const int size = op.value(QStringLiteral("size")).toInt(24);
        const int textLen = op.value(QStringLiteral("text")).toString().size();
        if (type == QStringLiteral("text")) {
            bounds = QRect(p.x(), p.y() - size, qMax(1, textLen) * size * 3 / 4 + 8, size + 8);
        } else {
            const int r = size;
            bounds = QRect(p.x() - r, p.y() - r, 2 * r, 2 * r);
        }
    }
    return bounds;
}

bool operationHitTest(const QJsonObject& op, const QPoint& pos, int tolerance)
{
    const QString type = op.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        const QPointF start = toPointF(op.value(QStringLiteral("start")).toObject());
        const QPointF end = toPointF(op.value(QStringLiteral("end")).toObject());
        const QPointF p(pos);
        const QPointF ab = end - start;
        const qreal len2 = QPointF::dotProduct(ab, ab);
        qreal t = 0.0;
        if (len2 > 0.0)
            t = qBound(0.0, QPointF::dotProduct(p - start, ab) / len2, 1.0);
        const QPointF proj = start + t * ab;
        const qreal dist = (p - proj).manhattanLength();
        return dist <= tolerance + 4;
    }

    const QRect bounds = operationBounds(op);
    if (bounds.isEmpty())
        return false;
    return bounds.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(pos);
}

QJsonObject translateOperation(const QJsonObject& op, int dx, int dy)
{
    if (dx == 0 && dy == 0)
        return op;
    QJsonObject out = op;
    auto shiftPoint = [dx, dy](QJsonObject& obj) {
        obj.insert(QStringLiteral("x"), obj.value(QStringLiteral("x")).toInt() + dx);
        obj.insert(QStringLiteral("y"), obj.value(QStringLiteral("y")).toInt() + dy);
    };
    auto shiftRect = [dx, dy](QJsonObject& obj) {
        obj.insert(QStringLiteral("x"), obj.value(QStringLiteral("x")).toInt() + dx);
        obj.insert(QStringLiteral("y"), obj.value(QStringLiteral("y")).toInt() + dy);
    };

    const QJsonValue rect = out.value(QStringLiteral("rect"));
    if (rect.isObject()) {
        QJsonObject r = rect.toObject();
        shiftRect(r);
        out.insert(QStringLiteral("rect"), r);
    }
    for (const char* key : { "start", "end", "point" }) {
        const QJsonValue v = out.value(QLatin1String(key));
        if (v.isObject()) {
            QJsonObject p = v.toObject();
            shiftPoint(p);
            out.insert(QLatin1String(key), p);
        }
    }
    const QJsonValue points = out.value(QStringLiteral("points"));
    if (points.isArray()) {
        QJsonArray arr = points.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject p = arr[i].toObject();
            shiftPoint(p);
            arr[i] = p;
        }
        out.insert(QStringLiteral("points"), arr);
    }
    return out;
}

} // namespace sc
