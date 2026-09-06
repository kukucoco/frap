/*
 * qml_backend.h —— 暴露给 QML 的截图后端（C++ 原语层）
 *
 * 交互状态机在 QML（CaptureController），C++ 只提供原语与数据：
 *   - frame: 渲染帧（原图+操作+进行中预览），QML 直接显示
 *   - op 原语: opIdAt / opBounds / moveOp / deleteOp / undo / redo
 *   - 裁剪: setCrop / clearCrop / hasCrop / cropRect
 *   - 工具: setToolPreview(实时预览) / applyTool(松手提交->插件)
 *   - 模型: toolbar / activeOptions（schema 驱动，供 QML 渲染）
 *   - 收尾: exportAction / finish / cancel / message(toast)
 */
#ifndef FRAP_APP_QML_BACKEND_H
#define FRAP_APP_QML_BACKEND_H

#include <QObject>
#include <QImage>
#include <QRect>
#include <QString>
#include <QVariant>
#include <QJsonObject>

#include "settings.h"
#include "plugin_manager.h"
#include "session.h"

namespace sc {

class FrapBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QImage frame READ frame NOTIFY frameChanged)
    Q_PROPERTY(bool hasCrop READ hasCrop NOTIFY cropChanged)
    Q_PROPERTY(QRect cropRect READ cropRect NOTIFY cropChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(int opCount READ opCount NOTIFY historyChanged)
    Q_PROPERTY(QVariantList toolbar READ toolbar NOTIFY toolbarChanged)
    Q_PROPERTY(QVariantList activeOptions READ activeOptions NOTIFY optionsChanged)
    Q_PROPERTY(QString activeTool READ activeTool NOTIFY activeToolChanged)
    Q_PROPERTY(bool cropToolActive READ cropToolActive NOTIFY activeToolChanged)
    Q_PROPERTY(QString translationSrc READ translationSrc NOTIFY translationChanged)
    Q_PROPERTY(QString translationDst READ translationDst NOTIFY translationChanged)
    Q_PROPERTY(bool translationVisible READ translationVisible NOTIFY translationChanged)
    Q_PROPERTY(int longshotProgress READ longshotProgress NOTIFY longshotChanged)

public:
    explicit FrapBackend(CaptureSession* session, Settings* settings,
                               PluginManager* manager, QObject* parent = nullptr);

    QImage frame() const { return m_frame; }
    bool hasCrop() const { return m_session->hasCrop(); }
    QRect cropRect() const { return m_session->cropLogical(); }
    bool canUndo() const { return m_session->canUndo(); }
    bool canRedo() const { return m_session->canRedo(); }
    int opCount() const { return m_session->operationCount(); }
    QVariantList toolbar() const { return m_toolbar; }
    QVariantList activeOptions() const { return m_activeOptions; }
    QString activeTool() const { return m_activeTool; }
    bool cropToolActive() const { return m_cropToolActive; }
    QString translationSrc() const { return m_translationSrc; }
    QString translationDst() const { return m_translationDst; }
    bool translationVisible() const { return m_translationVisible; }
    int longshotProgress() const { return m_longshotSegments.size(); }

    void setImage(const QImage& image) { m_session->setImage(image); }

public slots:
    /* 输入原语（逻辑坐标，QML 侧） */
    QString opIdAt(qreal x, qreal y);
    QVariant opBounds(const QString& id);
    void moveOp(const QString& id, int dx, int dy);
    void deleteOp(const QString& id);
    void undo();
    void redo();
    void clearOps();

    void setCrop(int x, int y, int w, int h);
    void clearCrop();

    /* 工具 */
    void setToolPreview(const QString& toolId, const QVariant& geometry, const QVariant& params);
    void clearPreview();
    void applyTool(const QString& toolId, const QVariant& geometry, const QVariant& params);
    void setActiveTool(const QString& id);
    void toggleCropTool();
    void deactivateTool();

    /* 配置（选项条点击） */
    void setSetting(const QString& key, const QVariant& value);

    /* 服务动作（翻译 / 长截图） */
    void serviceAction(const QString& pluginId);
    /* 长截图：追加一段截图（主程序再调 portal 得到） */
    void longshotAdd(const QImage& image);
    void longshotClear();

    /* 导出 / 收尾 */
    void exportAction(const QString& exportId);
    void finish();
    void cancel();
    QString readoutAt(qreal x, qreal y);

    /* 插件面板（调试/演示） */
    QStringList pluginIds() const;
    QString pluginInfo(const QString& id) const;

    /* 工具元数据（供 QML 交互状态机） */
    Q_INVOKABLE QString interactionOf(const QString& id) const;
    Q_INVOKABLE QString autoType(const QString& id) const;

signals:
    void frameChanged();
    void cropChanged();
    void historyChanged();
    void toolbarChanged();
    void optionsChanged();
    void activeToolChanged();
    void sessionFinished();
    void message(const QString& level, const QString& text);
    void translationChanged();
    void longshotChanged();
    void needCapture(); /* 请求主程序再截一次屏（长截图） */

private:
    enum class Interaction { None, DragRect, DragLine, DragRegion, ClickPoint };

    void rebuildFrame();
    void rebuildToolbar();
    void rebuildOptions();
    void runExport(const QString& exportId);
    void executeActions(const QJsonObject& result, const QString& pluginId);

    QJsonObject contributionData(const QString& toolId) const;
    Interaction interactionFor(const QString& toolId) const;
    QJsonObject geometryToPixel(const QVariant& geometry) const;
    QJsonObject buildPreviewOp(const QString& toolId, const QVariant& geometry,
                               const QVariant& params) const;
    QJsonObject opFor(const QString& type, const QVariant& geometry) const;
    QString toolSetting(const QString& toolId, const QString& key) const;
    QString iconFor(const QString& id) const;

    CaptureSession* m_session;
    Settings* m_settings;
    PluginManager* m_manager;

    QImage m_frame;
    QJsonObject m_previewOp;
    QString m_activeTool;
    bool m_cropToolActive = false;

    QVariantList m_toolbar;
    QVariantList m_activeOptions;
    bool m_closing = false;
    int m_pendingExports = 0;

    QString m_translationSrc;
    QString m_translationDst;
    bool m_translationVisible = false;
    QList<QImage> m_longshotSegments;
    QString m_longshotPlugin = QStringLiteral("capture.longshot");
};

} // namespace sc

#endif /* FRAP_APP_QML_BACKEND_H */
