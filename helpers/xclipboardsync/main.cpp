/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "waylandclipboard.h"

#include <QGuiApplication>

#include <config-kwin.h>
#if HAVE_PR_SET_PDEATHSIG
#include <sys/prctl.h>
#include <signal.h>
#endif

int main(int argc, char *argv[])
{
#if HAVE_PR_SET_PDEATHSIG
    prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
    qputenv("QT_QPA_PLATFORM", "xcb");
    QGuiApplication app(argc, argv);
    // perform sanity checks
    if (app.platformName().toLower() != QStringLiteral("xcb")) {
        fprintf(stderr, "%s: FATAL ERROR expecting platform xcb but got platform %s\n",
                argv[0], qPrintable(app.platformName()));
        return 1;
    }
    new WaylandClipboard(&app);
    return app.exec();
}
