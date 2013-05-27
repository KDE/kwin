/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#include "decorations.h"
#include "config-kwin.h"
#include <kdecorationfactory.h>

#include <kglobal.h>
#include <KDE/KLocalizedString>
#include <stdlib.h>
#include <QPixmap>

namespace KWin
{

KWIN_SINGLETON_FACTORY(DecorationPlugin)

DecorationPlugin::DecorationPlugin(QObject *parent)
    : QObject(parent)
    , KDecorationPlugins(KGlobal::config())
    , m_disabled(false)
{
    defaultPlugin = "kwin3_oxygen";
#ifndef KWIN_BUILD_OXYGEN
    defaultPlugin = "kwin3_aurorae";
#endif
#ifdef KWIN_BUILD_DECORATIONS
    loadPlugin("");   // load the plugin specified in cfg file
#else
    setDisabled(true);
#endif
}

DecorationPlugin::~DecorationPlugin()
{
    s_self = NULL;
}

void DecorationPlugin::error(const QString &error_msg)
{
    qWarning("%s", QString(i18n("KWin: ") + error_msg).toLocal8Bit().data());

    setDisabled(true);
}

bool DecorationPlugin::provides(Requirement)
{
    return false;
}

void DecorationPlugin::setDisabled(bool disabled)
{
    m_disabled = disabled;
}

bool DecorationPlugin::isDisabled() const
{
    return m_disabled;
}

bool DecorationPlugin::hasShadows() const
{
    if (m_disabled) {
        return false;
    }
    return factory()->supports(AbilityProvidesShadow);
}

bool DecorationPlugin::hasAlpha() const
{
    if (m_disabled) {
        return false;
    }
    return factory()->supports(AbilityUsesAlphaChannel);
}

bool DecorationPlugin::supportsAnnounceAlpha() const
{
    if (m_disabled) {
        return false;
    }
    return factory()->supports(AbilityAnnounceAlphaChannel);
}

bool DecorationPlugin::supportsTabbing() const
{
    if (m_disabled) {
        return false;
    }
    return factory()->supports(AbilityTabbing);
}

bool DecorationPlugin::supportsFrameOverlap() const
{
    if (m_disabled) {
        return false;
    }
    return factory()->supports(AbilityExtendIntoClientArea);
}

bool DecorationPlugin::supportsBlurBehind() const
{
    if (m_disabled) {
        return false;
    }
    return factory()->supports(AbilityUsesBlurBehind);
}

Qt::Corner DecorationPlugin::closeButtonCorner()
{
    if (m_disabled) {
        return Qt::TopRightCorner;
    }
    return factory()->closeButtonCorner();
}

QList< int > DecorationPlugin::supportedColors() const
{
    QList<int> ret;
    if (m_disabled) {
        return ret;
    }
    for (Ability ab = ABILITYCOLOR_FIRST;
            ab < ABILITYCOLOR_END;
            ab = static_cast<Ability>(ab + 1))
        if (factory()->supports(ab))
            ret << ab;
    return ret;
}

void DecorationPlugin::resetCompositing()
{
    if (m_disabled) {
        return;
    }
    factory()->reset(SettingCompositing);
}

QString DecorationPlugin::supportInformation()
{
    if (m_disabled) {
        return "Decoration Plugin disabled\n";
    }
    QString support;
    support.append("Current Plugin: ");
    support.append(currentPlugin());
    support.append('\n');

    support.append("Shadows: ");
    support.append(hasShadows() ? "yes\n" : "no\n");

    support.append("Alpha: ");
    support.append(hasAlpha() ? "yes\n" : "no\n");

    support.append("Announces Alpha: ");
    support.append(supportsAnnounceAlpha() ? "yes\n" : "no\n");

    support.append("Tabbing: ");
    support.append(supportsTabbing() ? "yes\n" : "no\n");

    support.append("Frame Overlap: ");
    support.append(supportsFrameOverlap() ? "yes\n" : "no\n");

    support.append("Blur Behind: ");
    support.append(supportsBlurBehind() ? "yes\n" : "no\n");
    // TODO: Qt5 - read support information from Factory
    return support;
}

} // namespace
