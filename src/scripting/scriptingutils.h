/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SCRIPTINGUTILS_H
#define KWIN_SCRIPTINGUTILS_H

#include <QVariant>

namespace KWin
{

QVariant dbusToVariant(const QVariant &variant);

} // namespace KWin

#endif // KWIN_SCRIPTINGUTILS_H
