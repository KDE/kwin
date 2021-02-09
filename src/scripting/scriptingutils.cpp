/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
