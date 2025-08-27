/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenshot.h"

#include <KPluginFactory>

namespace KWin
{

class KWIN_EXPORT ScreenshotManagerFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)
public:
    explicit ScreenshotManagerFactory() = default;

    std::unique_ptr<Plugin> create() const override;
};

std::unique_ptr<Plugin> ScreenshotManagerFactory::create() const
{
    return std::make_unique<ScreenShotManager>();
}

} // namespace KWin

#include "main.moc"
