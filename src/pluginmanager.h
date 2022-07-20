/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"

#include <QHash>
#include <QObject>
#include <QStaticPlugin>

#include <KPluginMetaData>

namespace KWin
{

class Plugin;
class PluginFactory;

/**
 * The PluginManager class loads and unloads binary compositor extensions.
 */
class KWIN_EXPORT PluginManager : public QObject
{
    Q_OBJECT

public:
    PluginManager();
    ~PluginManager() override;

    QStringList loadedPlugins() const;
    QStringList availablePlugins() const;

public Q_SLOTS:
    bool loadPlugin(const QString &pluginId);
    void unloadPlugin(const QString &pluginId);

private:
    bool loadStaticPlugin(const QString &pluginId);
    bool loadDynamicPlugin(const KPluginMetaData &metadata);
    bool loadDynamicPlugin(const QString &pluginId);
    bool instantiatePlugin(const QString &pluginId, PluginFactory *factory);

    std::map<QString, std::unique_ptr<Plugin>> m_plugins;
    QHash<QString, QStaticPlugin> m_staticPlugins;
};

} // namespace KWin
