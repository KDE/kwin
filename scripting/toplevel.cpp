/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Rohan Prabhu <rohan@rohanprabhu.com>

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

#include "toplevel.h"

Q_DECLARE_METATYPE(SWrapper::Toplevel*)

SWrapper::Toplevel::Toplevel(KWin::Toplevel* tlevel)
    : QObject(tlevel)
{
    tl_centralObject = 0;
}

KWin::Toplevel* SWrapper::Toplevel::extractClient(const QScriptValue& value)
{
    SWrapper::Toplevel* tlevel = qscriptvalue_cast<SWrapper::Toplevel*>(value);

    if (tlevel != 0) {
        return tlevel->tl_centralObject;
    }

    return 0;
}

MAP_GET(int, x)
MAP_GET(int, y)
MAP_GET(QSize, size)
MAP_GET(int, width)
MAP_GET(int, height)
MAP_GET(QRect, geometry)
MAP_GET(double, opacity)
MAP_GET(bool, hasAlpha)

QScriptValue SWrapper::Toplevel::pos(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Toplevel* central = extractClient(ctx->thisObject());

    if (central == 0) {
        return QScriptValue();
    } else {
        return eng->toScriptValue(central->pos());
    }
}

QScriptValue SWrapper::Toplevel::setOpacity(QScriptContext* ctx, QScriptEngine* eng)
{
    KWin::Toplevel* central = extractClient(ctx->thisObject());

    if (central == 0) {
        return eng->toScriptValue<bool>(0);
    } else {
        QScriptValue opac = ctx->argument(0);
        if (opac.isUndefined() || !opac.isNumber()) {
            return eng->toScriptValue<bool>(0);
        } else {
            qsreal _opac = opac.toNumber();
            // For some access restrictions, setting any window
            // completely transperent is not allowed. As if, a window
            // with 0.001 is perceptible. huh
            if ((_opac > 1) || (_opac <= 0)) {
                return eng->toScriptValue<bool>(0);
            } else {
                central->setOpacity(_opac);
                return eng->toScriptValue<bool>(1);
            }
        }
    }
}

void SWrapper::Toplevel::tl_append(QScriptValue& value, QScriptEngine* eng)
{
    QScriptValue func = eng->newFunction(pos, 0);
    value.setProperty("pos", func, QScriptValue::Undeletable);
    value.setProperty("x", eng->newFunction(x, 0), QScriptValue::Undeletable);
    value.setProperty("y", eng->newFunction(y, 0), QScriptValue::Undeletable);
    value.setProperty("size", eng->newFunction(size, 0), QScriptValue::Undeletable);
    value.setProperty("width", eng->newFunction(width, 0), QScriptValue::Undeletable);
    value.setProperty("height", eng->newFunction(height, 0), QScriptValue::Undeletable);
    value.setProperty("geometry", eng->newFunction(geometry, 0), QScriptValue::Undeletable);
    value.setProperty("opacity", eng->newFunction(opacity, 0), QScriptValue::Undeletable);
    value.setProperty("hasAlpha", eng->newFunction(hasAlpha, 0), QScriptValue::Undeletable);
    value.setProperty("setOpacity", eng->newFunction(setOpacity, 1), QScriptValue::Undeletable);
}
