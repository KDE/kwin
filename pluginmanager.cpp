/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pluginmanager.h"
#include "dbusinterface.h"
#include "main.h"
#include "plugin.h"
#include "utils.h"

#include <KConfigGroup>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KPluginMetaData>

namespace KWin
{

KWIN_SINGLETON_FACTORY(PluginManager)

static const QString s_pluginDirectory = QStringLiteral("kwin/plugins");

static QJsonValue readPluginInfo(const QJsonObject &metadata, const QString &key)
{
    return metadata.value(QLatin1String("KPlugin")).toObject().value(key);
}

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
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

    const QVector<KPluginMetaData> plugins = KPluginLoader::findPlugins(s_pluginDirectory);
    for (const KPluginMetaData &metadata : plugins) {
        if (m_plugins.contains(metadata.pluginId())) {
            qCWarning(KWIN_CORE) << "Conflicting plugin id" << metadata.pluginId();
            continue;
        }
        if (checkEnabled(metadata.pluginId(), metadata.rawData())) {
            loadDynamicPlugin(metadata);
        }
    }

    new PluginManagerDBusInterface(this);
}

PluginManager::~PluginManager()
{
    s_self = nullptr;
}

QStringList PluginManager::loadedPlugins() const
{
    return m_plugins.keys();
}

QStringList PluginManager::availablePlugins() const
{
    QStringList ret = m_staticPlugins.keys();

    const QVector<KPluginMetaData> plugins = KPluginLoader::findPlugins(s_pluginDirectory);
    for (const KPluginMetaData &metadata : plugins) {
        ret.append(metadata.pluginId());
    }

    return ret;
}

bool PluginManager::loadPlugin(const QString &pluginId)
{
    if (m_plugins.contains(pluginId)) {
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

    QScopedPointer<PluginFactory> factory(qobject_cast<PluginFactory *>(staticIt->instance()));
    if (!factory) {
        qCWarning(KWIN_CORE) << "Failed to get plugin factory for" << pluginId;
        return false;
    }

    return instantiatePlugin(pluginId, factory.data());
}

bool PluginManager::loadDynamicPlugin(const QString &pluginId)
{
    const auto offers = KPluginLoader::findPluginsById(s_pluginDirectory, pluginId);
    for (const KPluginMetaData &metadata : offers) {
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
    KPluginLoader pluginLoader(metadata.fileName());
    if (pluginLoader.pluginVersion() != KWIN_PLUGIN_API_VERSION) {
        qCWarning(KWIN_CORE) << pluginId << "has mismatching plugin version";
        return false;
    }

    QScopedPointer<PluginFactory> factory(qobject_cast<PluginFactory *>(pluginLoader.instance()));
    if (!factory) {
        qCWarning(KWIN_CORE) << "Failed to get plugin factory for" << pluginId;
        return false;
    }

    return instantiatePlugin(pluginId, factory.data());
}

bool PluginManager::instantiatePlugin(const QString &pluginId, PluginFactory *factory)
{
    Plugin *plugin = factory->create();
    if (!plugin) {
        return false;
    }

    m_plugins.insert(pluginId, plugin);
    plugin->setParent(this);

    connect(plugin, &QObject::destroyed, this, [this, pluginId]() {
        m_plugins.remove(pluginId);
    });

    return true;
}

void PluginManager::unloadPlugin(const QString &pluginId)
{
    Plugin *plugin = m_plugins.take(pluginId);
    if (!plugin) {
        qCWarning(KWIN_CORE) << "No plugin with the specified id:" << pluginId;
    }
    delete plugin;
}

} // namespace KWin
