/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
