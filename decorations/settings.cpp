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
#include "settings.h"
// KWin
#include "composite.h"
#include "virtualdesktops.h"

#include <KDecoration2/DecorationSettings>

namespace KWin
{
namespace Decoration
{
SettingsImpl::SettingsImpl(KDecoration2::DecorationSettings *parent)
    : QObject()
    , DecorationSettingsPrivate(parent)
{
    connect(Compositor::self(), &Compositor::compositingToggled,
            parent, &KDecoration2::DecorationSettings::alphaChannelSupportedChanged);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::countChanged, this,
        [parent](uint previous, uint current) {
            if (previous != 1 && current != 1) {
                return;
            }
            emit parent->onAllDesktopsAvailableChanged(current > 1);
        }
    );
}

SettingsImpl::~SettingsImpl() = default;

bool SettingsImpl::isAlphaChannelSupported() const
{
    return Compositor::self()->compositing();
}

bool SettingsImpl::isOnAllDesktopsAvailable() const
{
    return VirtualDesktopManager::self()->count() > 1;
}

QList< KDecoration2::DecorationButtonType > SettingsImpl::decorationButtonsLeft() const
{
    return QList<KDecoration2::DecorationButtonType >({
        KDecoration2::DecorationButtonType::Menu,
        KDecoration2::DecorationButtonType::OnAllDesktops
    });
}

QList< KDecoration2::DecorationButtonType > SettingsImpl::decorationButtonsRight() const
{
    return QList<KDecoration2::DecorationButtonType >({
        KDecoration2::DecorationButtonType::Minimize,
        KDecoration2::DecorationButtonType::Maximize,
        KDecoration2::DecorationButtonType::Close
    });
}

}
}
