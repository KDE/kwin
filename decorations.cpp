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
#include "client.h"
#include "workspace.h"
#include <kdecorationfactory.h>

#include <KDE/KLocalizedString>
#include <stdlib.h>
#include <QPixmap>

namespace KWin
{

KWIN_SINGLETON_FACTORY(DecorationPlugin)

DecorationPlugin::DecorationPlugin(QObject *parent)
    : QObject(parent)
    , KDecorationPlugins(KSharedConfig::openConfig())
    , m_disabled(false)
{
    defaultPlugin = QStringLiteral("Oxygen");
#ifndef KWIN_BUILD_OXYGEN
    defaultPlugin = QStringLiteral("aurorae");
#endif
#ifdef KWIN_BUILD_DECORATIONS
    loadPlugin(QString());   // load the plugin specified in cfg file
    connect(factory(), &KDecorationFactory::recreateDecorations, this, &DecorationPlugin::recreateDecorations);
    connect(this, &DecorationPlugin::compositingToggled, options, &KDecorationOptions::compositingChanged);
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

QString DecorationPlugin::supportInformation()
{
    if (m_disabled) {
        return QStringLiteral("Decoration Plugin disabled\n");
    }
    QString support;
    support.append(QStringLiteral("Current Plugin: "));
    support.append(currentPlugin());
    support.append(QStringLiteral("\n"));

    support.append(QStringLiteral("Shadows: "));
    support.append(hasShadows() ? QStringLiteral("yes\n") : QStringLiteral("no\n"));

    support.append(QStringLiteral("Alpha: "));
    support.append(hasAlpha() ? QStringLiteral("yes\n") : QStringLiteral("no\n"));

    support.append(QStringLiteral("Announces Alpha: "));
    support.append(supportsAnnounceAlpha() ? QStringLiteral("yes\n") : QStringLiteral("no\n"));

    support.append(QStringLiteral("Tabbing: "));
    support.append(supportsTabbing() ? QStringLiteral("yes\n") : QStringLiteral("no\n"));

    support.append(QStringLiteral("Frame Overlap: "));
    support.append(supportsFrameOverlap() ? QStringLiteral("yes\n") : QStringLiteral("no\n"));

    support.append(QStringLiteral("Blur Behind: "));
    support.append(supportsBlurBehind() ? QStringLiteral("yes\n") : QStringLiteral("no\n"));
    // TODO: Qt5 - read support information from Factory
    return support;
}

void DecorationPlugin::recreateDecorations()
{
    if (m_disabled) {
        return;
    }
    // Decorations need to be recreated
    workspace()->forEachClient([](Client *c) {
        c->updateDecoration(true, true);
    });
    // If the new decoration doesn't supports tabs then ungroup clients
    if (!supportsTabbing()) {
        workspace()->forEachClient([](Client *c) {
            c->untab();
        });
    }
}

} // namespace
