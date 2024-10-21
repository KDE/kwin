/*
    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@kdab.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "main.h"
#include "plugin.h"

#include "keynotification.h"

class KWIN_EXPORT KeyNotificationFactory : public KWin::PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    std::unique_ptr<KWin::Plugin> create() const override
    {
        switch (KWin::kwinApp()->operationMode()) {
        case KWin::Application::OperationModeWayland:
            return std::make_unique<KWin::KeyNotificationPlugin>();
        case KWin::Application::OperationModeX11:
        default:
            return nullptr;
        }
    }
};

#include "main.moc"
