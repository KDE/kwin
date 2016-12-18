/*
 * Copyright 2016  Martin Graesslin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "osd.h"
#include "onscreennotification.h"
#include "main.h"
#include "workspace.h"
#include "scripting/scripting.h"

#include <QQmlEngine>

namespace KWin
{
namespace OSD
{

static OnScreenNotification *create()
{
    auto osd = new OnScreenNotification(workspace());
    osd->setConfig(kwinApp()->config());
    osd->setEngine(Scripting::self()->qmlEngine());
    return osd;
}

static OnScreenNotification *osd()
{
    static OnScreenNotification *s_osd = create();
    return s_osd;
}

void show(const QString &message, const QString &iconName, int timeout)
{
    if (!kwinApp()->shouldUseWaylandForCompositing()) {
        // FIXME: only supported on Wayland
        return;
    }
    auto notification = osd();
    notification->setIconName(iconName);
    notification->setMessage(message);
    notification->setTimeout(timeout);
    notification->setVisible(true);
}

void show(const QString &message, int timeout)
{
    show(message, QString(), timeout);
}

void show(const QString &message, const QString &iconName)
{
    show(message, iconName, 0);
}

void hide()
{
    if (!kwinApp()->shouldUseWaylandForCompositing()) {
        // FIXME: only supported on Wayland
        return;
    }
    osd()->setVisible(false);
}

}
}
