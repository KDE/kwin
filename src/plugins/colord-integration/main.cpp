/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "main.h"
#include "colordintegration.h"

#include <KPluginFactory>

using namespace KWin;

class KWIN_EXPORT ColordIntegrationFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    explicit ColordIntegrationFactory() = default;

    std::unique_ptr<Plugin> create() const override;
};

std::unique_ptr<Plugin> ColordIntegrationFactory::create() const
{
    switch (kwinApp()->operationMode()) {
    case Application::OperationModeX11:
        return nullptr;
    case Application::OperationModeXwayland:
    case Application::OperationModeWaylandOnly:
        return std::make_unique<ColordIntegration>();
    default:
        return nullptr;
    }
}

#include "main.moc"
