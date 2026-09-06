/*
 * settings.h —— 配置注册表（类 VSCode settings）
 *
 * 插件通过 manifest 的 configSchema 声明自己的设置项（类型/默认值/枚举/范围），
 * 主程序负责：合并默认值 + 用户覆盖、类型校验、持久化到 settings.json。
 *
 * settings.json 结构：
 *   {
 *     "settings": { "tool.rect.color": "#e11d48", ... },
 *     "plugins":  { "enabled": ["export.save", ...], "disabled": ["xxx"] }
 *   }
 *
 * enabled 缺省 = 全部启用；显式 disabled 优先。
 */
#ifndef FRAP_CORE_SETTINGS_H
#define FRAP_CORE_SETTINGS_H

#include <QString>
#include <QVariant>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QSet>
#include <QJsonObject>

namespace sc {

struct SettingSpec {
    QString key;            /* 点分键，如 "tool.rect.color" */
    QString type;           /* string | int | double | bool | enum | color */
    QString label;          /* 设置面板中的人类可读名 */
    QString description;    /* 帮助文本 */
    QVariant defaultValue;
    QStringList enumValues; /* type == enum 时的可选值 */
    QVariant minValue;      /* int/double 下限 */
    QVariant maxValue;      /* int/double 上限 */
    bool hidden = false;    /* true 表示仅内部使用，不出现在配置文件中 */
};

class Settings
{
public:
    /* 注册设置项 schema（可从插件 manifest 批量导入） */
    void registerSpecs(const QList<SettingSpec>& specs);
    void registerSpec(const SettingSpec& spec);

    /* 加载/保存 settings.json */
    bool load(const QString& filePath);
    bool save(const QString& filePath) const;
    void clear();

    /* 读取有效值（默认值 < 用户覆盖） */
    QVariant value(const QString& key) const;
    bool contains(const QString& key) const;

    /* 用户显式设置（写覆盖） */
    void setValue(const QString& key, const QVariant& value);
    /* 清空某键的用户覆盖，回到默认值 */
    void resetValue(const QString& key);

    /* 插件启停 */
    bool isPluginEnabled(const QString& pluginId) const;
    void setPluginEnabled(const QString& pluginId, bool enabled);

    /* 插件目录（按优先级：用户配置 > 环境变量 > 可执行文件旁） */
    QStringList pluginDirectories() const;
    void setPluginDirectories(const QStringList& dirs) { m_pluginDirs = dirs; }

    /* 所有有效设置（展平），用于下发给宿主注入插件 config */
    QJsonObject effectiveSettings() const;

    /* 将某一插件 ID 前缀的所有设置切出来，形成 {key: value} 对象 */
    QJsonObject pluginConfig(const QString& pluginId) const;

    /* 当前用户覆盖值（用于保存时落盘） */
    QHash<QString, QVariant> userValues() const { return m_userValues; }
    QStringList enabledPlugins() const { return m_enabledPlugins.values(); }
    QStringList disabledPlugins() const { return m_disabledPlugins.values(); }

private:
    QVariant coerce(const SettingSpec& spec, const QVariant& raw) const;
    const SettingSpec* findSpec(const QString& key) const;

    QHash<QString, SettingSpec> m_specs;
    QHash<QString, QVariant> m_userValues;
    QSet<QString> m_enabledPlugins;
    QSet<QString> m_disabledPlugins;
    QStringList m_pluginDirs;
};

} // namespace sc

#endif /* FRAP_CORE_SETTINGS_H */
