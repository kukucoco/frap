/*
 * plugin_manifest.h —— 插件描述信息（由宿主上报，主程序聚合展示）
 */
#ifndef FRAP_CORE_PLUGIN_MANIFEST_H
#define FRAP_CORE_PLUGIN_MANIFEST_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>

#include "settings.h"
#include "plugin_api.h"

namespace sc {

struct Contribution {
    QString type;       /* "tool" | "export" | "command" ... */
    QJsonObject data;   /* {id, label, order, ...} */
};

struct PluginManifest {
    QString id;
    QString name;
    QString version;
    int kind = 0;                        /* FrapPluginKind */
    QList<SettingSpec> configSchema;
    QList<Contribution> contributions;

    bool isTool() const { return kind == SC_PLUGIN_KIND_TOOL; }
    bool isExport() const { return kind == SC_PLUGIN_KIND_EXPORT; }
    bool isCapture() const { return kind == SC_PLUGIN_KIND_CAPTURE; }
    bool isService() const { return kind == SC_PLUGIN_KIND_SERVICE; }
};

/* 解析宿主上报的 manifest JSON（settings/contributions 数组） */
inline PluginManifest parseManifest(const QJsonObject& obj)
{
    PluginManifest m;
    m.id = obj.value(QStringLiteral("id")).toString();
    m.name = obj.value(QStringLiteral("name")).toString();
    m.version = obj.value(QStringLiteral("version")).toString();
    m.kind = obj.value(QStringLiteral("kind")).toInt();

    const QJsonArray schema = obj.value(QStringLiteral("configSchema")).toArray();
    for (const QJsonValue& v : schema) {
        const QJsonObject s = v.toObject();
        SettingSpec spec;
        spec.key = s.value(QStringLiteral("key")).toString();
        spec.type = s.value(QStringLiteral("type")).toString();
        spec.label = s.value(QStringLiteral("label")).toString();
        spec.description = s.value(QStringLiteral("description")).toString();
        spec.defaultValue = s.value(QStringLiteral("default")).toVariant();
        for (const QJsonValue& e : s.value(QStringLiteral("enum")).toArray())
            spec.enumValues << e.toString();
        spec.minValue = s.value(QStringLiteral("min")).toVariant();
        spec.maxValue = s.value(QStringLiteral("max")).toVariant();
        spec.hidden = s.value(QStringLiteral("hidden")).toBool();
        if (!spec.key.isEmpty())
            m.configSchema.append(spec);
    }

    const QJsonArray contrib = obj.value(QStringLiteral("contributions")).toArray();
    for (const QJsonValue& v : contrib) {
        const QJsonObject c = v.toObject();
        Contribution cc;
        cc.type = c.value(QStringLiteral("type")).toString();
        cc.data = c.value(QStringLiteral("data")).toObject();
        if (!cc.type.isEmpty())
            m.contributions.append(cc);
    }
    return m;
}

} // namespace sc

#endif /* FRAP_CORE_PLUGIN_MANIFEST_H */
