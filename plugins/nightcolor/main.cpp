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
    explicit NightColorManagerFactory(QObject *parent = nullptr);

    Plugin *create() const override;
};

NightColorManagerFactory::NightColorManagerFactory(QObject *parent)
    : PluginFactory(parent)
{
}

Plugin *NightColorManagerFactory::create() const
{
    return new NightColorManager();
}

K_EXPORT_PLUGIN_VERSION(KWIN_PLUGIN_API_VERSION)

#include "main.moc"
