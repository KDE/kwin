/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "main.h"
#include "splitscreen.h"

#include <KPluginFactory>

using namespace KWin;

class KWIN_EXPORT SplitscreenFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    explicit SplitscreenFactory(QObject *parent = nullptr);

    Plugin *create() const override;
};

SplitscreenFactory::SplitscreenFactory(QObject *parent)
    : PluginFactory(parent)
{
}

Plugin *SplitscreenFactory::create() const
{
    return new Splitscreen();
}

#include "main.moc"
