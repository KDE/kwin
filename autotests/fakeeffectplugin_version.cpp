/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <kwineffects.h>

namespace KWin
{

class FakeVersionEffect : public Effect
{
    Q_OBJECT
public:
    FakeVersionEffect() {}
    ~FakeVersionEffect() override {}
};

} // namespace

class FakeEffectPluginFactory : public KWin::EffectPluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID KPluginFactory_iid FILE "fakeeffectplugin_version.json")
    Q_INTERFACES(KPluginFactory)
public:
    FakeEffectPluginFactory() {}
    ~FakeEffectPluginFactory() override {}
    KWin::Effect *createEffect() const override {
        return new KWin::FakeVersionEffect();
    }
};
K_EXPORT_PLUGIN_VERSION(quint32(KWIN_EFFECT_API_VERSION) - 1)


#include "fakeeffectplugin_version.moc"
