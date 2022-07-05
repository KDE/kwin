/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "main.h"
#include "screencastmanager.h"

#include <KPluginFactory>

using namespace KWin;

class KWIN_EXPORT ScreencastManagerFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    explicit ScreencastManagerFactory() = default;

    std::unique_ptr<Plugin> create() const override;
};

std::unique_ptr<Plugin> ScreencastManagerFactory::create() const
{
    switch (kwinApp()->operationMode()) {
    case Application::OperationModeX11:
        return nullptr;
    case Application::OperationModeXwayland:
    case Application::OperationModeWaylandOnly:
        return std::make_unique<ScreencastManager>();
    default:
        return nullptr;
    }
}

#include "main.moc"
