/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "killwindow.h"
#include "abstract_client.h"
#include "main.h"
#include "platform.h"
#include "osd.h"
#include "unmanaged.h"

#include <KLocalizedString>

namespace KWin
{

KillWindow::KillWindow()
{
}

KillWindow::~KillWindow()
{
}

void KillWindow::start()
{
    OSD::show(i18n("Select window to force close with left click or enter.\nEscape or right click to cancel."),
              QStringLiteral("window-close"));
    kwinApp()->platform()->startInteractiveWindowSelection(
        [] (KWin::AbstractClient *t) {
            OSD::hide();
            if (!t) {
                return;
            }
            t->killWindow();
        }, QByteArrayLiteral("pirate")
    );
}

} // namespace
