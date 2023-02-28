/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwinglobals.h"

#include <QHash>
#include <QObject>

#include <KPluginMetaData>

namespace KWin
{

class Plugin;

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
    bool loadPlugin(const KPluginMetaData &metadata);

    std::map<QString, std::unique_ptr<Plugin>> m_plugins;
};

} // namespace KWin
