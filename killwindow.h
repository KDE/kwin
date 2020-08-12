/*
    KWin - the KDE window manager

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_KILLWINDOW_H
#define KWIN_KILLWINDOW_H

namespace KWin
{

class KillWindow
{
public:

    KillWindow();
    ~KillWindow();

    void start();
};

} // namespace

#endif
