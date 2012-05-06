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
#include "scriptingutils.h"

namespace KWin
{
bool validateParameters(QScriptContext *context, int min, int max)
{
    if (context->argumentCount() < min || context->argumentCount() > max) {
        context->throwError(QScriptContext::SyntaxError,
                            i18nc("syntax error in KWin script", "Invalid number of arguments"));
        return false;
    }
    return true;
}

template<>
bool validateArgumentType<QVariant>(QScriptContext *context, int argument)
{
    const bool result =context->argument(argument).toVariant().isValid();
    if (!result) {
        context->throwError(QScriptContext::TypeError,
            i18nc("KWin Scripting function received incorrect value for an expected type",
                  "%1 is not a variant type", context->argument(argument).toString()));
    }
    return result;
}

}
