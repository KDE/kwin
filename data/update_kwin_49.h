/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef UPDATE_KWIN_49_H
#define UPDATE_KWIN_49_H
#include <KDE/KConfigGroup>

class KConfig;

void migratePresentWindowsTabBox(KConfig &config);
void migrateDesktopChangeOSD(KConfig &config);
void migrateTabBoxConfig(KConfigGroup tabbox);

#endif
