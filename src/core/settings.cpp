#include "settings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QStandardPaths>
#include <QCoreApplication>

namespace sc {

namespace {
QString specType(const SettingSpec& spec)
{
    return spec.type.isEmpty() ? QStringLiteral("string") : spec.type;
}
} // namespace

void Settings::registerSpec(const SettingSpec& spec)
{
    m_specs.insert(spec.key, spec);
}

void Settings::registerSpecs(const QList<SettingSpec>& specs)
{
    for (const SettingSpec& spec : specs)
        registerSpec(spec);
}

const SettingSpec* Settings::findSpec(const QString& key) const
{
    auto it = m_specs.constFind(key);
    return it == m_specs.constEnd() ? nullptr : &it.value();
}

QVariant Settings::coerce(const SettingSpec& spec, const QVariant& raw) const
{
    const QString type = specType(spec);
    if (type == QStringLiteral("int"))
        return QVariant(raw.toInt());
    if (type == QStringLiteral("double"))
        return QVariant(raw.toDouble());
    if (type == QStringLiteral("bool"))
        return QVariant(raw.toBool());
    if (type == QStringLiteral("enum")) {
        const QString v = raw.toString();
        return spec.enumValues.contains(v) ? v : spec.defaultValue;
    }
    if (type == QStringLiteral("color"))
        return QVariant(raw.toString());
    return QVariant(raw.toString());
}

bool Settings::load(const QString& filePath)
{
    QFile file(filePath);
    if (!file.exists())
        return true; /* 首次运行：无配置，全部用默认值 */
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Settings: 无法读取" << filePath << file.errorString();
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qWarning() << "Settings: 配置不是合法 JSON:" << filePath;
        return false;
    }
    const QJsonObject root = doc.object();
    const QJsonObject settings = root.value(QStringLiteral("settings")).toObject();
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it)
        m_userValues.insert(it.key(), it.value().toVariant());

    const QJsonObject plugins = root.value(QStringLiteral("plugins")).toObject();
    const QJsonArray enabled = plugins.value(QStringLiteral("enabled")).toArray();
    const QJsonArray disabled = plugins.value(QStringLiteral("disabled")).toArray();
    m_enabledPlugins.clear();
    m_disabledPlugins.clear();
    for (const QJsonValue& v : enabled)
        m_enabledPlugins.insert(v.toString());
    for (const QJsonValue& v : disabled)
        m_disabledPlugins.insert(v.toString());
    return true;
}

bool Settings::save(const QString& filePath) const
{
    QJsonObject settings;
    for (auto it = m_userValues.constBegin(); it != m_userValues.constEnd(); ++it)
        settings.insert(it.key(), QJsonValue::fromVariant(it.value()));

    QJsonObject plugins;
    QJsonArray enabled, disabled;
    for (const QString& id : m_enabledPlugins)
        enabled.append(id);
    for (const QString& id : m_disabledPlugins)
        disabled.append(id);
    plugins.insert(QStringLiteral("enabled"), enabled);
    plugins.insert(QStringLiteral("disabled"), disabled);

    QJsonObject root;
    root.insert(QStringLiteral("settings"), settings);
    root.insert(QStringLiteral("plugins"), plugins);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Settings: 无法写入" << filePath << file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void Settings::clear()
{
    m_userValues.clear();
    m_enabledPlugins.clear();
    m_disabledPlugins.clear();
}

QVariant Settings::value(const QString& key) const
{
    const auto uv = m_userValues.constFind(key);
    if (uv != m_userValues.constEnd())
        return uv.value();

    const SettingSpec* spec = findSpec(key);
    return spec ? spec->defaultValue : QVariant();
}

bool Settings::contains(const QString& key) const
{
    return m_userValues.contains(key) || m_specs.contains(key);
}

void Settings::setValue(const QString& key, const QVariant& value)
{
    const SettingSpec* spec = findSpec(key);
    m_userValues.insert(key, spec ? coerce(*spec, value) : value);
}

void Settings::resetValue(const QString& key)
{
    m_userValues.remove(key);
}

bool Settings::isPluginEnabled(const QString& pluginId) const
{
    if (m_disabledPlugins.contains(pluginId))
        return false;
    /* 显式启用列表非空时，仅列表中插件可用 */
    if (!m_enabledPlugins.isEmpty() && !m_enabledPlugins.contains(pluginId))
        return false;
    return true;
}

void Settings::setPluginEnabled(const QString& pluginId, bool enabled)
{
    if (enabled) {
        m_enabledPlugins.insert(pluginId);
        m_disabledPlugins.remove(pluginId);
    } else {
        m_disabledPlugins.insert(pluginId);
        m_enabledPlugins.remove(pluginId);
    }
}

QStringList Settings::pluginDirectories() const
{
    if (!m_pluginDirs.isEmpty())
        return m_pluginDirs;

    QStringList dirs;
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                   + QStringLiteral("/frap");
    dirs << configRoot + QStringLiteral("/plugins");

    const QByteArray env = qgetenv("MY_FRAP_PLUGIN_PATH");
    if (!env.isEmpty())
        dirs << QString::fromUtf8(env).split(':');

    if (!QCoreApplication::instance())
        return dirs;
    dirs << QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");
    return dirs;
}

QJsonObject Settings::effectiveSettings() const
{
    QJsonObject out;
    for (auto it = m_specs.constBegin(); it != m_specs.constEnd(); ++it) {
        if (!it.value().hidden)
            out.insert(it.key(), QJsonValue::fromVariant(value(it.key())));
    }
    for (auto it = m_userValues.constBegin(); it != m_userValues.constEnd(); ++it)
        if (!findSpec(it.key()))
            out.insert(it.key(), QJsonValue::fromVariant(it.value()));
    return out;
}

QJsonObject Settings::pluginConfig(const QString& pluginId) const
{
    QJsonObject out;
    const QString prefix = pluginId + QLatin1Char('.');
    for (auto it = m_specs.constBegin(); it != m_specs.constEnd(); ++it) {
        if (it.key().startsWith(prefix))
            out.insert(it.key(), QJsonValue::fromVariant(value(it.key())));
    }
    return out;
}

} // namespace sc
