/*
 * icon_provider.h —— image://icons/<key> 主题图标提供器
 *
 * 语义键（tool.rect / undo / crop...）映射到 Qt/Breeze 主题图标名；
 * 主题缺失时回退为生成的彩色圆标（保证任何环境下不破相）。
 */
#ifndef FRAP_APP_ICON_PROVIDER_H
#define FRAP_APP_ICON_PROVIDER_H

#include <QQuickImageProvider>
#include <QString>
#include <QHash>

namespace sc {

class IconProvider : public QQuickImageProvider
{
public:
    explicit IconProvider();

    QPixmap requestPixmap(const QString& id, QSize* size,
                          const QSize& requestedSize) override;

private:
    QIcon themedIcon(const QString& key) const;

    QHash<QString, QString> m_themeMap;
};

} // namespace sc

#endif /* FRAP_APP_ICON_PROVIDER_H */
