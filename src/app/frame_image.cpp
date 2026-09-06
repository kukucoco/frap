#include "frame_image.h"

#include <QPainter>
#include <QQuickWindow>

namespace sc {

FrameImage::FrameImage(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing);
}

void FrameImage::setSource(const QImage& source)
{
    if (source.isNull())
        return; /* 忽略空源，避免 QML 赋 null 警告 */
    if (m_source.size() == source.size()
        && m_source.constBits() == source.constBits())
        return;
    m_source = source;
    update();
    emit sourceChanged();
}

void FrameImage::paint(QPainter* painter)
{
    if (m_source.isNull())
        return;
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(boundingRect(), m_source, m_source.rect());
}

} // namespace sc
