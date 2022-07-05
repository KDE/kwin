/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "main.h"
#include "windowsrunnerinterface.h"

#include <KPluginFactory>

using namespace KWin;

class KWIN_EXPORT KRunnerIntegrationFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    explicit KRunnerIntegrationFactory() = default;

    std::unique_ptr<Plugin> create() const override;
};

std::unique_ptr<Plugin> KRunnerIntegrationFactory::create() const
{
    return std::make_unique<WindowsRunner>();
}

#include "main.moc"
