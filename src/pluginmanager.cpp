/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pluginmanager.h"
#include "dbusinterface.h"
#include "main.h"
#include "plugin.h"
#include "utils/common.h"

#include <KConfigGroup>
#include <KPluginFactory>
#include <KPluginMetaData>

namespace KWin
{

static const QString s_pluginDirectory = QStringLiteral("kwin/plugins");

static QJsonValue readPluginInfo(const QJsonObject &metadata, const QString &key)
{
    return metadata.value(QLatin1String("KPlugin")).toObject().value(key);
}

PluginManager::PluginManager()
{
    const KConfigGroup config(kwinApp()->config(), QStringLiteral("Plugins"));

    auto checkEnabled = [&config](const QString &pluginId, const QJsonObject &metadata) {
        const QString configKey = pluginId + QLatin1String("Enabled");
        if (config.hasKey(configKey)) {
            return config.readEntry(configKey, false);
        }
        return readPluginInfo(metadata, QStringLiteral("EnabledByDefault")).toBool(false);
    };

    const QVector<QStaticPlugin> staticPlugins = QPluginLoader::staticPlugins();
    for (const QStaticPlugin &staticPlugin : staticPlugins) {
        const QJsonObject rootMetaData = staticPlugin.metaData();
        if (rootMetaData.value(QLatin1String("IID")) != QLatin1String(PluginFactory_iid)) {
            continue;
        }

        const QJsonObject pluginMetaData = rootMetaData.value(QLatin1String("MetaData")).toObject();
        const QString pluginId = readPluginInfo(pluginMetaData, QStringLiteral("Id")).toString();
        if (pluginId.isEmpty()) {
            continue;
        }
        if (m_staticPlugins.contains(pluginId)) {
            qCWarning(KWIN_CORE) << "Conflicting plugin id" << pluginId;
            continue;
        }

        m_staticPlugins.insert(pluginId, staticPlugin);

        if (checkEnabled(pluginId, pluginMetaData)) {
            loadStaticPlugin(pluginId);
        }
    }

    const QVector<KPluginMetaData> plugins = KPluginMetaData::findPlugins(s_pluginDirectory);
    for (const KPluginMetaData &metadata : plugins) {
        if (m_plugins.find(metadata.pluginId()) != m_plugins.end()) {
            qCWarning(KWIN_CORE) << "Conflicting plugin id" << metadata.pluginId();
            continue;
        }
        if (checkEnabled(metadata.pluginId(), metadata.rawData())) {
            loadDynamicPlugin(metadata);
        }
    }

    new PluginManagerDBusInterface(this);
}

PluginManager::~PluginManager() = default;

QStringList PluginManager::loadedPlugins() const
{
    QStringList ret;
    ret.reserve(m_plugins.size());
    for (const auto &[key, _] : m_plugins) {
        ret.push_back(key);
    }
    return ret;
}

QStringList PluginManager::availablePlugins() const
{
    QStringList ret = m_staticPlugins.keys();

    const QVector<KPluginMetaData> plugins = KPluginMetaData::findPlugins(s_pluginDirectory);
    for (const KPluginMetaData &metadata : plugins) {
        ret.append(metadata.pluginId());
    }

    return ret;
}

bool PluginManager::loadPlugin(const QString &pluginId)
{
    if (m_plugins.find(pluginId) != m_plugins.end()) {
        qCDebug(KWIN_CORE) << "Plugin with id" << pluginId << "is already loaded";
        return false;
    }
    return loadStaticPlugin(pluginId) || loadDynamicPlugin(pluginId);
}

bool PluginManager::loadStaticPlugin(const QString &pluginId)
{
    auto staticIt = m_staticPlugins.find(pluginId);
    if (staticIt == m_staticPlugins.end()) {
        return false;
    }

    std::unique_ptr<PluginFactory> factory(qobject_cast<PluginFactory *>(staticIt->instance()));
    if (!factory) {
        qCWarning(KWIN_CORE) << "Failed to get plugin factory for" << pluginId;
        return false;
    }

    return instantiatePlugin(pluginId, factory.get());
}

bool PluginManager::loadDynamicPlugin(const QString &pluginId)
{
    const KPluginMetaData metadata = KPluginMetaData::findPluginById(s_pluginDirectory, pluginId);
    if (metadata.isValid()) {
        if (loadDynamicPlugin(metadata)) {
            return true;
        }
    }
    return false;
}

bool PluginManager::loadDynamicPlugin(const KPluginMetaData &metadata)
{
    if (!metadata.isValid()) {
        qCDebug(KWIN_CORE) << "PluginManager::loadPlugin needs a valid plugin metadata";
        return false;
    }

    const QString pluginId = metadata.pluginId();
    QPluginLoader pluginLoader(metadata.fileName());
    if (pluginLoader.metaData().value("IID").toString() != PluginFactory_iid) {
        qCWarning(KWIN_CORE) << pluginId << "has mismatching plugin version";
        return false;
    }

    std::unique_ptr<PluginFactory> factory(qobject_cast<PluginFactory *>(pluginLoader.instance()));
    if (!factory) {
        qCWarning(KWIN_CORE) << "Failed to get plugin factory for" << pluginId;
        return false;
    }

    return instantiatePlugin(pluginId, factory.get());
}

bool PluginManager::instantiatePlugin(const QString &pluginId, PluginFactory *factory)
{
    if (std::unique_ptr<Plugin> plugin = factory->create()) {
        m_plugins[pluginId] = std::move(plugin);
        return true;
    } else {
        return false;
    }
}

void PluginManager::unloadPlugin(const QString &pluginId)
{
    auto it = m_plugins.find(pluginId);
    if (it != m_plugins.end()) {
        m_plugins.erase(it);
    } else {
        qCWarning(KWIN_CORE) << "No plugin with the specified id:" << pluginId;
    }
}

} // namespace KWin
