/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <kwineffects.h>

namespace KWin
{

class FakeEffect : public Effect
{
    Q_OBJECT
public:
    FakeEffect() {}
    ~FakeEffect() override {}

    static bool supported() {
        return effects->isOpenGLCompositing();
    }

    static bool enabledByDefault() {
        return effects->property("testEnabledByDefault").toBool();
    }
};

} // namespace

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED( FakeEffectPluginFactory,
                                       KWin::FakeEffect,
                                       "fakeeffectplugin.json",
                                       return KWin::FakeEffect::supported();,
                                       return KWin::FakeEffect::enabledByDefault();)

#include "fakeeffectplugin.moc"
