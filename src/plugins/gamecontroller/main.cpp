/*
    SPDX-FileCopyrightText: 2025 Yelsin Sepulveda <yelsin.sepulveda@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gamecontrollermanager.h"

#include <KPluginFactory>

namespace KWin
{

class KWIN_EXPORT GameControllerManagerFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)
public:
    explicit GameControllerManagerFactory() = default;

    std::unique_ptr<Plugin> create() const override;
};

std::unique_ptr<Plugin> GameControllerManagerFactory::create() const
{
    return std::make_unique<GameControllerManager>();
}

} // namespace KWin

#include "main.moc"
