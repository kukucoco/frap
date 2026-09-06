#include "qml_backend.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QDateTime>

#include "render_ops.h"
#include "protocol.h"

namespace sc {

namespace {

QJsonObject rectToJson(const QRect& r)
{
    return QJsonObject{
        { QStringLiteral("x"), r.x() }, { QStringLiteral("y"), r.y() },
        { QStringLiteral("w"), r.width() }, { QStringLiteral("h"), r.height() },
    };
}

QJsonObject pointToJson(const QPoint& p)
{
    return QJsonObject{ { QStringLiteral("x"), p.x() }, { QStringLiteral("y"), p.y() } };
}

const QList<QColor> kPalette = {
    QColor("#e11d48"), QColor("#f59e0b"), QColor("#10b981"), QColor("#3b82f6"),
    QColor("#8b5cf6"), QColor("#ec4899"), QColor("#ffffff"), QColor("#111111"),
};

} // namespace

FrapBackend::FrapBackend(CaptureSession* session, Settings* settings,
                                     PluginManager* manager, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_settings(settings)
    , m_manager(manager)
{
    connect(m_session, &CaptureSession::operationsChanged, this, [this]() {
        rebuildFrame();
        emit historyChanged();
        emit toolbarChanged();
    });
    connect(m_manager, &PluginManager::pluginLoaded, this, [this](const PluginManifest&) {
        rebuildToolbar();
        rebuildOptions();
    });
    connect(m_manager, &PluginManager::ready, this, [this]() {
        rebuildToolbar();
        rebuildOptions();
    });
    if (m_manager->isReady()) {
        rebuildToolbar();
        rebuildOptions();
    }
}

/* ---------------- 帧渲染 ---------------- */

void FrapBackend::rebuildFrame()
{
    QList<QJsonObject> ops = m_session->operations();
    if (!m_previewOp.isEmpty())
        ops.append(m_previewOp);
    m_frame = applyOperations(m_session->image(), ops);
    emit frameChanged();
}

void FrapBackend::setToolPreview(const QString& toolId, const QVariant& geometry,
                                       const QVariant& params)
{
    m_previewOp = buildPreviewOp(toolId, geometry, params);
    rebuildFrame();
}

void FrapBackend::clearPreview()
{
    if (m_previewOp.isEmpty())
        return;
    m_previewOp = QJsonObject();
    rebuildFrame();
}

/* ---------------- 操作原语 ---------------- */

QString FrapBackend::opIdAt(qreal x, qreal y)
{
    return m_session->selectedOpIdAt(m_session->logicalToPixel(QPoint(qRound(x), qRound(y))));
}

QVariant FrapBackend::opBounds(const QString& id)
{
    const QJsonObject op = m_session->operationById(id);
    if (op.isEmpty())
        return QVariant();
    const QRect px = operationBounds(op);
    if (px.isEmpty())
        return QVariant();
    const QRect lr = QRect(m_session->pixelToLogical(px.topLeft()),
                           m_session->pixelToLogical(px.bottomRight()))
                         .normalized();
    QVariantMap m;
    m.insert(QStringLiteral("x"), lr.x());
    m.insert(QStringLiteral("y"), lr.y());
    m.insert(QStringLiteral("width"), lr.width());
    m.insert(QStringLiteral("height"), lr.height());
    return m;
}

void FrapBackend::moveOp(const QString& id, int dx, int dy)
{
    const QPointF s = m_session->scale();
    m_session->moveOperation(id, qRound(dx * s.x()), qRound(dy * s.y()));
}

void FrapBackend::deleteOp(const QString& id)
{
    m_session->deleteOperation(id);
}

void FrapBackend::undo()
{
    m_session->undo();
}

void FrapBackend::redo()
{
    m_session->redo();
}

void FrapBackend::clearOps()
{
    m_session->clearOperations();
}

/* ---------------- 裁剪 ---------------- */

void FrapBackend::setCrop(int x, int y, int w, int h)
{
    m_session->setCrop(m_session->logicalToPixel(QRect(x, y, w, h)));
    emit cropChanged();
}

void FrapBackend::clearCrop()
{
    m_session->clearCrop();
    emit cropChanged();
}

/* ---------------- 工具 ---------------- */

FrapBackend::Interaction FrapBackend::interactionFor(const QString& toolId) const
{
    const QJsonObject d = contributionData(toolId);
    const QString it = d.value(QStringLiteral("interaction")).toString();
    if (it == QStringLiteral("drag-rect"))
        return Interaction::DragRect;
    if (it == QStringLiteral("drag-line"))
        return Interaction::DragLine;
    if (it == QStringLiteral("drag-region"))
        return Interaction::DragRegion;
    if (it == QStringLiteral("click-point"))
        return Interaction::ClickPoint;
    return Interaction::DragRect;
}

QJsonObject FrapBackend::contributionData(const QString& toolId) const
{
    for (const PluginManager::ContributionEntry& e :
         m_manager->contributions(QStringLiteral("tool")))
        if (e.plugin.id == toolId)
            return e.contribution.data;
    return QJsonObject();
}

QString FrapBackend::toolSetting(const QString& toolId, const QString& key) const
{
    const QVariant v = m_settings->value(toolId + QLatin1Char('.') + key);
    if (v.isValid() && !v.toString().isEmpty())
        return v.toString();
    const PluginManifest manifest = m_manager->manifest(toolId);
    for (const SettingSpec& spec : manifest.configSchema)
        if (spec.key == toolId + QLatin1Char('.') + key)
            return spec.defaultValue.toString();
    return QString();
}

QJsonObject FrapBackend::geometryToPixel(const QVariant& geometry) const
{
    QJsonObject out;
    const QVariantMap g = geometry.toMap();
    const auto toPixel = [&](int x, int y) {
        return m_session->logicalToPixel(QPoint(x, y));
    };

    /* 扁平数字键（QML JS 对象直传，规避 QRect/QPoint 转换歧义） */
    if (g.contains(QStringLiteral("rectX")) && g.contains(QStringLiteral("rectY"))) {
        const int x = g.value(QStringLiteral("rectX")).toInt();
        const int y = g.value(QStringLiteral("rectY")).toInt();
        const int w = g.value(QStringLiteral("rectW")).toInt();
        const int h = g.value(QStringLiteral("rectH")).toInt();
        const QRect px = QRect(toPixel(x, y), toPixel(x + w, y + h)).normalized();
        out.insert(QStringLiteral("rect"), rectToJson(px));
    }
    if (g.contains(QStringLiteral("startX")))
        out.insert(QStringLiteral("start"),
                   pointToJson(toPixel(g.value("startX").toInt(), g.value("startY").toInt())));
    if (g.contains(QStringLiteral("endX")))
        out.insert(QStringLiteral("end"),
                   pointToJson(toPixel(g.value("endX").toInt(), g.value("endY").toInt())));
    if (g.contains(QStringLiteral("pointX")))
        out.insert(QStringLiteral("point"),
                   pointToJson(toPixel(g.value("pointX").toInt(), g.value("pointY").toInt())));
    if (g.contains(QStringLiteral("text")))
        out.insert(QStringLiteral("text"), g.value(QStringLiteral("text")).toString());
    return out;
}

QJsonObject FrapBackend::opFor(const QString& type, const QVariant& geometry) const
{
    QJsonObject op;
    const QJsonObject px = geometryToPixel(geometry);
    if (type == QStringLiteral("text")) {
        op = QJsonObject{
            { QStringLiteral("type"), QStringLiteral("text") },
            { QStringLiteral("point"), px.value(QStringLiteral("point")).toObject() },
            { QStringLiteral("text"), px.value(QStringLiteral("text")).toString() },
            { QStringLiteral("color"), toolSetting(m_activeTool, QStringLiteral("color")) },
            { QStringLiteral("size"), m_settings->value(m_activeTool + QStringLiteral(".size")).toInt() },
        };
    }
    return op;
}

QJsonObject FrapBackend::buildPreviewOp(const QString& toolId, const QVariant& geometry,
                                              const QVariant& params) const
{
    Q_UNUSED(params)
    if (toolId != m_activeTool && toolId != QStringLiteral("tool.text"))
        return QJsonObject();

    const QVariantMap g = geometry.toMap();
    const QString color = toolSetting(toolId, QStringLiteral("color"));
    const int width = m_settings->value(toolId + QStringLiteral(".width")).toInt();
    const QJsonObject px = geometryToPixel(geometry);

    QJsonObject op;
    switch (interactionFor(toolId)) {
    case Interaction::DragRect:
        op = QJsonObject{
            { QStringLiteral("type"), QStringLiteral("rect") },
            { QStringLiteral("rect"), px.value(QStringLiteral("rect")).toObject() },
            { QStringLiteral("color"), color },
            { QStringLiteral("width"), width },
        };
        break;
    case Interaction::DragLine:
        op = QJsonObject{
            { QStringLiteral("type"), QStringLiteral("arrow") },
            { QStringLiteral("start"), px.value(QStringLiteral("start")).toObject() },
            { QStringLiteral("end"), px.value(QStringLiteral("end")).toObject() },
            { QStringLiteral("color"), color },
            { QStringLiteral("width"), width },
        };
        break;
    case Interaction::DragRegion:
        op = QJsonObject{
            { QStringLiteral("type"), QStringLiteral("highlight") },
            { QStringLiteral("rect"), px.value(QStringLiteral("rect")).toObject() },
            { QStringLiteral("color"), color },
            { QStringLiteral("alpha"), 70 },
        };
        break;
    case Interaction::ClickPoint:
        /* 文本预览 */
        op = opFor(QStringLiteral("text"), geometry);
        break;
    default:
        break;
    }
    return op;
}

void FrapBackend::applyTool(const QString& toolId, const QVariant& geometry,
                                  const QVariant& params)
{
    const QJsonObject px = geometryToPixel(geometry);
    QJsonObject paramsObj = QJsonObject::fromVariantMap(params.toMap());
    /* 文本走瞬态参数（不落盘）；预览时在 geometry 里透传 */
    if (px.contains(QStringLiteral("text")))
        paramsObj.insert(QStringLiteral("text"), px.value(QStringLiteral("text")));

    QJsonObject payload{
        { QStringLiteral("imageSize"), QJsonObject{
            { QStringLiteral("w"), m_session->image().width() },
            { QStringLiteral("h"), m_session->image().height() },
        } },
        { QStringLiteral("params"), paramsObj },
    };
    for (auto it = px.constBegin(); it != px.constEnd(); ++it) {
        if (it.key() != QStringLiteral("text"))
            payload.insert(it.key(), it.value());
    }

    /* 编号工具自动编号 */
    const QString autoType = contributionData(toolId).value(QStringLiteral("auto")).toString();
    if (autoType == QStringLiteral("number")) {
        QJsonObject p = payload.value(QStringLiteral("params")).toObject();
        p.insert(QStringLiteral("number"),
                 m_session->countOperationType(QStringLiteral("number")) + 1);
        payload.insert(QStringLiteral("params"), p);
    }

    m_manager->invoke(toolId, QStringLiteral("apply"), payload,
                      [this, toolId](const QJsonObject& result, const QString& error) {
                          if (!error.isEmpty()) {
                              emit message(QStringLiteral("error"),
                                           QStringLiteral("工具调用失败: %1").arg(error));
                              return;
                          }
                          for (const QJsonValue& v :
                               result.value(QStringLiteral("operations")).toArray())
                              m_session->addOperation(v.toObject());
                          executeActions(result, toolId);
                      });
}

void FrapBackend::setActiveTool(const QString& id)
{
    if (m_activeTool == id) {
        m_activeTool.clear();
    } else {
        m_activeTool = id;
        m_cropToolActive = false;
    }
    rebuildToolbar();
    rebuildOptions();
    emit activeToolChanged();
}

void FrapBackend::toggleCropTool()
{
    m_cropToolActive = !m_cropToolActive;
    m_activeTool.clear();
    rebuildToolbar();
    rebuildOptions();
    emit activeToolChanged();
}

void FrapBackend::deactivateTool()
{
    m_activeTool.clear();
    m_cropToolActive = false;
    rebuildToolbar();
    rebuildOptions();
    emit activeToolChanged();
}

void FrapBackend::setSetting(const QString& key, const QVariant& value)
{
    m_settings->setValue(key, value);
    m_settings->save(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                     + QStringLiteral("/frap/settings.json"));
    rebuildFrame(); /* 预览用当前配置重渲 */
    rebuildOptions();
}

/* ---------------- 工具条 / 选项模型 ---------------- */

QString FrapBackend::iconFor(const QString& id) const
{
    if (id == QStringLiteral("tool.crop"))
        return QStringLiteral("transform-crop");
    if (id.startsWith(QStringLiteral("tool.")))
        return id.mid(5); /* tool.rect -> rect，由 IconProvider 映射主题名 */
    if (id.startsWith(QStringLiteral("export.")))
        return id.mid(7);
    if (id.startsWith(QStringLiteral("service.")))
        return id.mid(8); /* service.translate -> translate */
    if (id.startsWith(QStringLiteral("capture.")))
        return id.mid(8); /* capture.longshot -> longshot */
    return id.mid(7); /* action.undo -> undo */
}

void FrapBackend::rebuildToolbar()
{
    m_toolbar.clear();

    auto add = [this](const QVariantMap& e) { m_toolbar.append(e); };
    auto sep = [&]() { add({ { QStringLiteral("type"), QStringLiteral("sep") } }); };
    auto toolBtn = [&](const QString& id, const QString& label, const QString& icon,
                       bool toggled, const QString& shortcut) {
        add({
            { QStringLiteral("type"), QStringLiteral("tool") },
            { QStringLiteral("id"), id },
            { QStringLiteral("label"), label },
            { QStringLiteral("icon"), icon },
            { QStringLiteral("toggled"), toggled },
            { QStringLiteral("shortcut"), shortcut },
        });
    };

    /* 裁剪 */
    toolBtn(QStringLiteral("tool.crop"), QStringLiteral("裁剪"),
            QStringLiteral("transform-crop"), m_cropToolActive, QStringLiteral("C"));

    /* 工具插件 */
    auto tools = m_manager->contributions(QStringLiteral("tool"));
    std::sort(tools.begin(), tools.end(),
              [](const PluginManager::ContributionEntry& a,
                 const PluginManager::ContributionEntry& b) {
                  return a.contribution.data.value(QStringLiteral("order")).toInt()
                      < b.contribution.data.value(QStringLiteral("order")).toInt();
              });
    for (const auto& e : tools) {
        if (!m_settings->isPluginEnabled(e.plugin.id))
            continue;
        const QJsonObject d = e.contribution.data;
        const QString icon = d.value(QStringLiteral("icon")).toString(iconFor(e.plugin.id));
        toolBtn(e.plugin.id,
                d.value(QStringLiteral("label")).toString(e.plugin.name),
                icon, e.plugin.id == m_activeTool,
                d.value(QStringLiteral("shortcut")).toString());
    }

    sep();
    for (const QString& action : { QStringLiteral("undo"), QStringLiteral("redo") })
        add({
            { QStringLiteral("type"), QStringLiteral("action") },
            { QStringLiteral("id"), action },
            { QStringLiteral("icon"), iconFor(action) },
            { QStringLiteral("label"), action == QStringLiteral("undo") ? QStringLiteral("撤销")
                                                                        : QStringLiteral("重做") },
            { QStringLiteral("enabled"),
              action == QStringLiteral("undo") ? canUndo() : canRedo() },
        });

    sep();
    auto exports = m_manager->contributions(QStringLiteral("export"));
    std::sort(exports.begin(), exports.end(),
              [](const PluginManager::ContributionEntry& a,
                 const PluginManager::ContributionEntry& b) {
                  return a.contribution.data.value(QStringLiteral("order")).toInt()
                      < b.contribution.data.value(QStringLiteral("order")).toInt();
              });
    for (const auto& e : exports) {
        if (!m_settings->isPluginEnabled(e.plugin.id))
            continue;
        add({
            { QStringLiteral("type"), QStringLiteral("export") },
            { QStringLiteral("id"), e.plugin.id },
            { QStringLiteral("icon"), iconFor(e.plugin.id) },
            { QStringLiteral("label"),
              e.contribution.data.value(QStringLiteral("label")).toString(e.plugin.name) },
        });
    }

    /* 服务插件（翻译 / 长截图…） */
    auto services = m_manager->contributions(QStringLiteral("service"));
    std::sort(services.begin(), services.end(),
              [](const PluginManager::ContributionEntry& a,
                 const PluginManager::ContributionEntry& b) {
                  return a.contribution.data.value(QStringLiteral("order")).toInt()
                      < b.contribution.data.value(QStringLiteral("order")).toInt();
              });
    if (!services.isEmpty()) {
        sep();
        for (const auto& e : services) {
            if (!m_settings->isPluginEnabled(e.plugin.id))
                continue;
            const QJsonObject d = e.contribution.data;
            add({
                { QStringLiteral("type"), QStringLiteral("service") },
                { QStringLiteral("id"), e.plugin.id },
                { QStringLiteral("icon"),
                  d.value(QStringLiteral("icon")).toString(iconFor(e.plugin.id)) },
                { QStringLiteral("label"),
                  d.value(QStringLiteral("label")).toString(e.plugin.name) },
                { QStringLiteral("shortcut"), d.value(QStringLiteral("shortcut")).toString() },
            });
        }
    }

    sep();
    add({
        { QStringLiteral("type"), QStringLiteral("action") },
        { QStringLiteral("id"), QStringLiteral("cancel") },
        { QStringLiteral("icon"), QStringLiteral("dialog-cancel") },
        { QStringLiteral("label"), QStringLiteral("取消") },
        { QStringLiteral("enabled"), true },
    });
    add({
        { QStringLiteral("type"), QStringLiteral("action") },
        { QStringLiteral("id"), QStringLiteral("done") },
        { QStringLiteral("icon"), QStringLiteral("dialog-ok-apply") },
        { QStringLiteral("label"), QStringLiteral("完成") },
        { QStringLiteral("accent"), true },
        { QStringLiteral("enabled"), true },
    });

    emit toolbarChanged();
}

void FrapBackend::rebuildOptions()
{
    m_activeOptions.clear();
    if (m_cropToolActive) {
        if (m_session->hasCrop())
            m_activeOptions.append(QVariantMap{
                { QStringLiteral("type"), QStringLiteral("clear-crop") },
                { QStringLiteral("label"), QStringLiteral("清除裁剪") },
            });
        emit optionsChanged();
        return;
    }
    if (m_activeTool.isEmpty()) {
        emit optionsChanged();
        return;
    }
    const PluginManifest manifest = m_manager->manifest(m_activeTool);
    for (const SettingSpec& spec : manifest.configSchema) {
        if (!spec.key.startsWith(m_activeTool + QLatin1Char('.')))
            continue;
        QVariantMap o;
        o.insert(QStringLiteral("key"), spec.key);
        if (spec.type == QStringLiteral("color") || spec.key.endsWith(QStringLiteral(".color"))) {
            o.insert(QStringLiteral("type"), QStringLiteral("color"));
            o.insert(QStringLiteral("value"), m_settings->value(spec.key).toString());
            QStringList values = spec.enumValues;
            if (values.isEmpty())
                for (const QColor& c : kPalette)
                    values << c.name();
            o.insert(QStringLiteral("enum"), values);
        } else if (spec.type == QStringLiteral("int")) {
            o.insert(QStringLiteral("type"), QStringLiteral("int"));
            o.insert(QStringLiteral("value"), m_settings->value(spec.key).toInt());
            o.insert(QStringLiteral("min"), spec.minValue.isValid() ? spec.minValue.toInt() : 1);
            o.insert(QStringLiteral("max"), spec.maxValue.isValid() ? spec.maxValue.toInt() : 64);
        } else if (spec.type == QStringLiteral("string")
                   && spec.key.endsWith(QStringLiteral(".text"))) {
            o.insert(QStringLiteral("type"), QStringLiteral("text"));
        }
        if (!o.isEmpty())
            m_activeOptions.append(o);
    }
    emit optionsChanged();
}

/* ---------------- 导出 / 收尾 ---------------- */

void FrapBackend::executeActions(const QJsonObject& result, const QString& pluginId)
{
    const QJsonArray actions = result.value(QStringLiteral("actions")).toArray();
    for (const QJsonValue& v : actions) {
        const QJsonObject action = v.toObject();
        const QString method = action.value(QStringLiteral("method")).toString();
        const QJsonObject params = action.value(QStringLiteral("params")).toObject();
        if (method == QStringLiteral("app.clipboard.set")) {
            const QByteArray data = QByteArray::fromBase64(
                params.value(QStringLiteral("data")).toString().toLatin1());
            QImage img;
            if (img.loadFromData(data, "PNG") && !img.isNull())
                QGuiApplication::clipboard()->setImage(img);
        } else if (method == QStringLiteral("app.open.file")) {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(params.value(QStringLiteral("path")).toString()));
        } else if (method == QStringLiteral("app.notify")) {
            emit message(QStringLiteral("info"),
                         params.value(QStringLiteral("summary")).toString());
        } else {
            qWarning() << "未知插件动作:" << method;
        }
    }
}

void FrapBackend::runExport(const QString& exportId)
{
    ++m_pendingExports;
    const QString tempPath = m_session->exportToTempPng();
    if (tempPath.isEmpty()) {
        --m_pendingExports;
        if (m_pendingExports <= 0 && m_closing)
            emit sessionFinished();
        return;
    }
    QFile f(tempPath);
    QByteArray b64;
    if (f.open(QIODevice::ReadOnly))
        b64 = f.readAll().toBase64();

    const QImage output = m_session->renderedOutput();
    const QJsonObject payload{
        { QStringLiteral("request"), QStringLiteral("execute") },
        { QStringLiteral("imageRef"), tempPath },
        { QStringLiteral("imageSize"), QJsonObject{
            { QStringLiteral("w"), output.width() },
            { QStringLiteral("h"), output.height() },
        } },
        { QStringLiteral("imageBase64"), QString::fromLatin1(b64) },
        { QStringLiteral("params"), QJsonObject{} },
    };
    m_manager->invoke(exportId, QStringLiteral("execute"), payload,
                      [this, exportId](const QJsonObject& result, const QString& error) {
                          if (!error.isEmpty())
                              emit message(QStringLiteral("error"),
                                           QStringLiteral("导出失败: %1").arg(error));
                          else
                              emit message(QStringLiteral("info"),
                                           result.value(QStringLiteral("message")).toString());
                          executeActions(result, exportId);
                          if (--m_pendingExports <= 0 && m_closing)
                              emit sessionFinished();
                      });
}

void FrapBackend::exportAction(const QString& exportId)
{
    runExport(exportId);
}

/* ---------------- 服务动作（翻译 / 长截图） ---------------- */

void FrapBackend::serviceAction(const QString& pluginId)
{
    if (pluginId == QStringLiteral("service.translate")) {
        /* 翻译当前选区/全屏 */
        const QImage out = m_session->renderedOutput();
        if (out.isNull()) {
            emit message(QStringLiteral("error"), QStringLiteral("没有可翻译的图像"));
            return;
        }
        const QByteArray b64 = imageToBase64(out);
        m_manager->invoke("service.translate", "translate",
                          QJsonObject{
                              { QStringLiteral("imageBase64"), QString::fromLatin1(b64) },
                              { QStringLiteral("imageSize"), QJsonObject{
                                  { QStringLiteral("w"), out.width() },
                                  { QStringLiteral("h"), out.height() },
                              } },
                          },
                          [this](const QJsonObject& result, const QString& error) {
                              if (!error.isEmpty()
                                  || result.value(QStringLiteral("status")).toString()
                                         != QStringLiteral("ok")) {
                                  m_translationVisible = false;
                                  emit translationChanged();
                                  emit message(QStringLiteral("error"),
                                               result.value(QStringLiteral("message")).toString());
                                  return;
                              }
                              m_translationSrc = result.value(QStringLiteral("src")).toString();
                              m_translationDst = result.value(QStringLiteral("dst")).toString();
                              m_translationVisible = true;
                              if (QGuiApplication::clipboard())
                                  QGuiApplication::clipboard()->setText(m_translationDst);
                              emit translationChanged();
                              emit message(QStringLiteral("info"),
                                           QStringLiteral("翻译完成，译文已复制到剪贴板"));
                          });
    } else if (pluginId == QStringLiteral("capture.longshot")) {
        /* 长截图：将当前输出作为第 1 段，并请求再截一屏 */
        if (m_longshotSegments.isEmpty()) {
            const QImage frame = m_session->image();
            if (!frame.isNull())
                m_longshotSegments.append(frame);
            emit longshotChanged();
            emit needCapture();
            emit message(QStringLiteral("info"),
                         QStringLiteral("长截图：已截第 1 屏，请滚动页面后点击『长截图』继续，"
                                        "或直接点击『拼接』完成"));
        } else {
            /* 已有 >=1 段，再触发一屏 */
            emit needCapture();
            emit message(QStringLiteral("info"),
                         QStringLiteral("长截图：已截 %1 屏，继续滚动后点击，再点『完成拼接』")
                             .arg(m_longshotSegments.size()));
        }
    } else {
        emit message(QStringLiteral("warn"), QStringLiteral("未知服务: %1").arg(pluginId));
    }
}

void FrapBackend::longshotAdd(const QImage& image)
{
    if (image.isNull())
        return;
    m_longshotSegments.append(image);
    emit longshotChanged();

    /* 满 2 段即拼接 */
    if (m_longshotSegments.size() >= 2) {
        QJsonArray sections;
        for (const QImage& img : m_longshotSegments)
            sections.append(QString::fromLatin1(imageToBase64(img)));
        m_manager->invoke("capture.longshot", "stitch",
                          QJsonObject{ { QStringLiteral("sections"), sections } },
                          [this](const QJsonObject& result, const QString& error) {
                              if (!error.isEmpty()
                                  || result.value(QStringLiteral("status")).toString()
                                         != QStringLiteral("ok")) {
                                  emit message(QStringLiteral("error"),
                                               result.value(QStringLiteral("message")).toString());
                                  return;
                              }
                              QImage combined;
                              if (!combined.loadFromData(
                                      QByteArray::fromBase64(
                                          result.value(QStringLiteral("imageBase64"))
                                              .toString()
                                              .toLatin1()),
                                      "PNG")
                                  || combined.isNull()) {
                                  emit message(QStringLiteral("error"), QStringLiteral("拼接失败"));
                                  return;
                              }
                              m_session->setImage(combined);
                              m_longshotSegments.clear();
                              emit longshotChanged();
                              emit message(QStringLiteral("info"),
                                           QStringLiteral("长截图完成：%1 × %2")
                                               .arg(combined.width())
                                               .arg(combined.height()));
                          });
    }
}

void FrapBackend::longshotClear()
{
    m_longshotSegments.clear();
    emit longshotChanged();
}

void FrapBackend::finish()
{
    m_closing = true;
    m_pendingExports = 0;
    auto exports = m_manager->contributions(QStringLiteral("export"));
    std::sort(exports.begin(), exports.end(),
              [](const PluginManager::ContributionEntry& a,
                 const PluginManager::ContributionEntry& b) {
                  return a.contribution.data.value(QStringLiteral("order")).toInt()
                      < b.contribution.data.value(QStringLiteral("order")).toInt();
              });
    for (const auto& e : exports)
        if (m_settings->isPluginEnabled(e.plugin.id))
            runExport(e.plugin.id);
    if (m_pendingExports == 0)
        emit sessionFinished();
}

void FrapBackend::cancel()
{
    emit sessionFinished();
}

QString FrapBackend::readoutAt(qreal x, qreal y)
{
    if (m_frame.isNull())
        return QString();
    const QPoint px = m_session->logicalToPixel(QPoint(qRound(x), qRound(y)));
    const QColor c = m_frame.pixelColor(qBound(0, px.x(), m_frame.width() - 1),
                                        qBound(0, px.y(), m_frame.height() - 1));
    return QStringLiteral("(%1, %2)  #%3").arg(px.x()).arg(px.y()).arg(c.name().mid(1));
}

QStringList FrapBackend::pluginIds() const
{
    QStringList ids;
    for (const PluginManifest& m : m_manager->plugins())
        ids << m.id;
    return ids;
}

QString FrapBackend::pluginInfo(const QString& id) const
{
    const PluginManifest m = m_manager->manifest(id);
    if (m.id.isEmpty())
        return QStringLiteral("插件未加载");
    return QStringLiteral("%1 v%2 · 工具条项 %3 · 设置项 %4")
        .arg(m.name, m.version)
        .arg(m.contributions.size())
        .arg(m.configSchema.size());
}

QString FrapBackend::interactionOf(const QString& id) const
{
    switch (interactionFor(id)) {
    case Interaction::DragRect: return QStringLiteral("drag-rect");
    case Interaction::DragLine: return QStringLiteral("drag-line");
    case Interaction::DragRegion: return QStringLiteral("drag-region");
    case Interaction::ClickPoint: return QStringLiteral("click-point");
    default: return QStringLiteral("drag-rect");
    }
}


QString FrapBackend::autoType(const QString& id) const
{
    return contributionData(id).value(QStringLiteral("auto")).toString();
}

} // namespace sc
