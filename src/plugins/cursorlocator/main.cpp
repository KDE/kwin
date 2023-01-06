/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <main.h>
#include <plugin.h>

#include "cursorlocator.h"

class KWIN_EXPORT CursorLocatorFactory : public KWin::PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    std::unique_ptr<KWin::Plugin> create() const override
    {
        switch (KWin::kwinApp()->operationMode()) {
        case KWin::Application::OperationModeXwayland:
        case KWin::Application::OperationModeWaylandOnly:
            return std::make_unique<KWin::CursorLocator>();
        case KWin::Application::OperationModeX11:
        default:
            return nullptr;
        }
    }
};

#include "main.moc"
