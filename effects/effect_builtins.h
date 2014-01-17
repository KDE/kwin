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
#ifndef KWIN_EFFECT_BUILTINS_H
#define KWIN_EFFECT_BUILTINS_H
#include <kwineffects_export.h>

class QByteArray;

namespace KWin
{
class Effect;

namespace BuiltInEffects
{

KWINEFFECTS_EXPORT Effect *create(const QByteArray &name);
KWINEFFECTS_EXPORT bool available(const QByteArray &name);
KWINEFFECTS_EXPORT bool supported(const QByteArray &name);
KWINEFFECTS_EXPORT bool enabledByDefault(const QByteArray &name);
}

}

#endif
