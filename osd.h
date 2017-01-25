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

#ifndef KWIN_OSD_H
#define KWIN_OSD_H

#include <QFlags>
#include <QString>

namespace KWin
{
namespace OSD
{

void show(const QString &message, const QString &iconName = QString());
void show(const QString &message, int timeout);
void show(const QString &message, const QString &iconName, int timeout);
enum class HideFlag {
    SkipCloseAnimation = 1
};
Q_DECLARE_FLAGS(HideFlags, HideFlag)
void hide(HideFlags flags = HideFlags());

}
}

#endif
