/*
 * frame_image.h —— 绘制渲染帧的 QQuickPaintedItem
 */
#ifndef FRAP_APP_FRAME_IMAGE_H
#define FRAP_APP_FRAME_IMAGE_H

#include <QQuickPaintedItem>
#include <QImage>

namespace sc {

class FrameImage : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QImage source READ source WRITE setSource NOTIFY sourceChanged)
public:
    explicit FrameImage(QQuickItem* parent = nullptr);

    QImage source() const { return m_source; }
    void setSource(const QImage& source);

    void paint(QPainter* painter) override;

signals:
    void sourceChanged();

private:
    QImage m_source;
};

} // namespace sc

#endif /* FRAP_APP_FRAME_IMAGE_H */
