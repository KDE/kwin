/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "nightcolormanager.h"

#include <KPluginFactory>

using namespace KWin;

class KWIN_EXPORT NightColorManagerFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    explicit NightColorManagerFactory() = default;

    std::unique_ptr<Plugin> create() const override;
};

std::unique_ptr<Plugin> NightColorManagerFactory::create() const
{
    return std::make_unique<NightColorManager>();
}

#include "main.moc"
