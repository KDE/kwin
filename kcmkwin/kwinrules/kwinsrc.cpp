/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// Include some code from kwin core in order to avoid
// double implementation.

#include "ruleslist.h"
#include "../../cursor.cpp"
#include "../../plugins/platforms/x11/standalone/x11cursor.cpp"
#include "../../rules.cpp"
#include "../../placement.cpp"
#include "../../options.cpp"
#include "../../utils.cpp"

KWin::InputRedirection *KWin::InputRedirection::s_self = nullptr;

Qt::MouseButtons KWin::InputRedirection::qtButtonStates() const
{
    return Qt::NoButton;
}

Qt::KeyboardModifiers KWin::InputRedirection::keyboardModifiers() const
{
    return Qt::NoModifier;
}

void KWin::InputRedirection::warpPointer(const QPointF&)
{
}

bool KWin::InputRedirection::supportsPointerWarping() const
{
    return false;
}

QPointF KWin::InputRedirection::globalPointer() const
{
    return QPointF();
}
