/*
    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <input.h>

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
