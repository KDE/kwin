/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "main.h"
#include "gamepadinput.h"

#include <KPluginFactory>

using namespace KWin;

class KWIN_EXPORT GamePadInputFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    explicit GamePadInputFactory() = default;

    std::unique_ptr<Plugin> create() const override;
};

std::unique_ptr<Plugin> GamePadInputFactory::create() const
{
    switch (kwinApp()->operationMode()) {
    case Application::OperationModeXwayland:
    case Application::OperationModeWaylandOnly:
        return std::make_unique<GamePadInput>();
    case Application::OperationModeX11:
    default:
        return nullptr;
    }
}

#include "main.moc"
