#include "session.h"

#include <QPainter>
#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

#include "render_ops.h"

namespace sc {

void CaptureSession::setImage(const QImage& image)
{
    m_image = image;
    m_ops.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_hasCrop = false;
    m_cropPixel = QRect();
    emit imageChanged();
    emit operationsChanged();
}

QPointF CaptureSession::scale() const
{
    if (m_image.isNull() || m_screenSize.isEmpty())
        return QPointF(1.0, 1.0);
    return QPointF(m_image.width() / m_screenSize.width(),
                   m_image.height() / m_screenSize.height());
}

QPoint CaptureSession::logicalToPixel(const QPoint& p) const
{
    const QPointF s = scale();
    return QPoint(qRound(p.x() * s.x()), qRound(p.y() * s.y()));
}

QRect CaptureSession::logicalToPixel(const QRect& r) const
{
    const QRect n = r.normalized();
    return QRect(logicalToPixel(n.topLeft()), logicalToPixel(n.bottomRight())).normalized();
}

QPoint CaptureSession::pixelToLogical(const QPoint& p) const
{
    const QPointF s = scale();
    if (s.x() <= 0.0 || s.y() <= 0.0)
        return p;
    return QPoint(qRound(p.x() / s.x()), qRound(p.y() / s.y()));
}

void CaptureSession::setCrop(const QRect& pixelRect)
{
    const QRect n = pixelRect.normalized();
    m_hasCrop = !n.isEmpty() && n.width() >= 3 && n.height() >= 3;
    m_cropPixel = m_hasCrop ? n.intersected(QRect(QPoint(0, 0), m_image.size())) : QRect();
    m_hasCrop = m_hasCrop && !m_cropPixel.isEmpty();
}

void CaptureSession::clearCrop()
{
    m_hasCrop = false;
    m_cropPixel = QRect();
}

QRect CaptureSession::cropLogical() const
{
    if (!m_hasCrop)
        return QRect();
    const QPointF s = scale();
    if (s.x() <= 0.0 || s.y() <= 0.0)
        return m_cropPixel;
    return QRect(qRound(m_cropPixel.left() / s.x()), qRound(m_cropPixel.top() / s.y()),
                 qRound(m_cropPixel.width() / s.x()), qRound(m_cropPixel.height() / s.y()));
}

/* ---------------- 编辑栈 ---------------- */

void CaptureSession::pushState()
{
    m_undoStack.append(m_ops);
    if (m_undoStack.size() > 200)
        m_undoStack.removeFirst();
    m_redoStack.clear();
}

void CaptureSession::addOperation(const QJsonObject& op)
{
    if (op.isEmpty())
        return;
    QJsonObject withId = op;
    if (!withId.contains(QStringLiteral("id")))
        withId.insert(QStringLiteral("id"),
                      QStringLiteral("op_%1").arg(m_nextOpId++));
    pushState();
    m_ops.append(withId);
    emit operationsChanged();
}

bool CaptureSession::moveOperation(const QString& id, int dx, int dy)
{
    int idx = -1;
    for (int i = 0; i < m_ops.size(); ++i)
        if (m_ops[i].value(QStringLiteral("id")).toString() == id) {
            idx = i;
            break;
        }
    if (idx < 0)
        return false;
    pushState();
    m_ops[idx] = translateOperation(m_ops[idx], dx, dy);
    emit operationsChanged();
    return true;
}

bool CaptureSession::deleteOperation(const QString& id)
{
    int idx = -1;
    for (int i = 0; i < m_ops.size(); ++i)
        if (m_ops[i].value(QStringLiteral("id")).toString() == id) {
            idx = i;
            break;
        }
    if (idx < 0)
        return false;
    pushState();
    m_ops.removeAt(idx);
    emit operationsChanged();
    return true;
}

QString CaptureSession::selectedOpIdAt(const QPoint& pixelPos) const
{
    /* 后画的在上层，倒序命中 */
    for (int i = m_ops.size() - 1; i >= 0; --i)
        if (operationHitTest(m_ops[i], pixelPos))
            return m_ops[i].value(QStringLiteral("id")).toString();
    return QString();
}

QJsonObject CaptureSession::operationById(const QString& id) const
{
    for (const QJsonObject& op : m_ops)
        if (op.value(QStringLiteral("id")).toString() == id)
            return op;
    return QJsonObject();
}

int CaptureSession::countOperationType(const QString& type) const
{
    int n = 0;
    for (const QJsonObject& op : m_ops)
        if (op.value(QStringLiteral("type")).toString() == type)
            ++n;
    return n;
}

void CaptureSession::undo()
{
    if (m_undoStack.isEmpty())
        return;
    m_redoStack.append(m_ops);
    m_ops = m_undoStack.takeLast();
    emit operationsChanged();
}

void CaptureSession::redo()
{
    if (m_redoStack.isEmpty())
        return;
    m_undoStack.append(m_ops);
    m_ops = m_redoStack.takeLast();
    emit operationsChanged();
}

void CaptureSession::clearOperations()
{
    m_ops.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    emit operationsChanged();
}

/* ---------------- 渲染 / 导出 ---------------- */

QImage CaptureSession::render() const
{
    return applyOperations(m_image, m_ops);
}

QImage CaptureSession::renderedOutput() const
{
    const QImage rendered = render();
    if (m_hasCrop && !m_cropPixel.isEmpty())
        return rendered.copy(m_cropPixel);
    return rendered;
}

QString CaptureSession::exportToTempPng() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString path = dir + QStringLiteral("/myscr_%1.png").arg(
        QDateTime::currentMSecsSinceEpoch());
    const QImage output = renderedOutput();
    if (output.save(path))
        return path;
    return QString();
}

} // namespace sc
