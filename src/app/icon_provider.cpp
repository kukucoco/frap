#include "icon_provider.h"

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QFont>

namespace sc {

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
    m_themeMap = {
        { QStringLiteral("transform-crop"), QStringLiteral("transform-crop") },
        { QStringLiteral("draw-rectangle"), QStringLiteral("draw-rectangle") },
        { QStringLiteral("draw-arrow-up"), QStringLiteral("draw-arrow-up") },
        { QStringLiteral("draw-text"), QStringLiteral("draw-text") },
        { QStringLiteral("format-list-numbered"), QStringLiteral("format-list-numbered") },
        { QStringLiteral("preferences-graphics-blur"), QStringLiteral("preferences-graphics-blur") },
        { QStringLiteral("draw-ellipse"), QStringLiteral("draw-ellipse") },
        { QStringLiteral("format-text-highlight"), QStringLiteral("format-text-highlight") },
        { QStringLiteral("blur"), QStringLiteral("blur") },
        { QStringLiteral("edit-undo"), QStringLiteral("edit-undo") },
        { QStringLiteral("edit-redo"), QStringLiteral("edit-redo") },
        { QStringLiteral("edit-copy"), QStringLiteral("edit-copy") },
        { QStringLiteral("document-save"), QStringLiteral("document-save") },
        { QStringLiteral("dialog-cancel"), QStringLiteral("dialog-cancel") },
        { QStringLiteral("dialog-ok-apply"), QStringLiteral("dialog-ok-apply") },
    };
}

QIcon IconProvider::themedIcon(const QString& key) const
{
    const QString themeName = m_themeMap.value(key, key);
    QIcon icon = QIcon::fromTheme(themeName);
    if (!icon.isNull())
        return icon;
    return QIcon::fromTheme(key);
}

QPixmap IconProvider::requestPixmap(const QString& id, QSize* size,
                                    const QSize& requestedSize)
{
    const int s = requestedSize.isValid() ? requestedSize.width() : 32;
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);

    const QIcon icon = themedIcon(id);
    if (!icon.isNull()) {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        icon.paint(&p, pm.rect());
        p.end();
        if (size)
            *size = pm.size();
        return pm;
    }

    /* 回退：彩色圆 + 首字母 */
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor c(QColor::fromHsl((qHash(id) % 360), 60, 55));
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawEllipse(pm.rect().adjusted(3, 3, -3, -3));
    p.setPen(Qt::white);
    QFont f(QStringLiteral("Sans Serif"), s / 2);
    f.setBold(true);
    p.setFont(f);
    const QString ch = id.left(1).toUpper();
    p.drawText(pm.rect(), Qt::AlignCenter, ch);
    p.end();
    if (size)
        *size = pm.size();
    return pm;
}

} // namespace sc
