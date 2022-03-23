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
    explicit KRunnerIntegrationFactory(QObject *parent = nullptr);

    Plugin *create() const override;
};

KRunnerIntegrationFactory::KRunnerIntegrationFactory(QObject *parent)
    : PluginFactory(parent)
{
}

Plugin *KRunnerIntegrationFactory::create() const
{
    return new WindowsRunner();
}

#include "main.moc"
