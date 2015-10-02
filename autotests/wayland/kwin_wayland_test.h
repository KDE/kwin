/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_WAYLAND_TEST_H
#define KWIN_WAYLAND_TEST_H

#include "../../main.h"

// Qt
#include <QtTest/QtTest>

namespace KWin
{

class WaylandTestApplication : public Application
{
    Q_OBJECT
public:
    WaylandTestApplication(int &argc, char **argv);
    virtual ~WaylandTestApplication();

protected:
    void performStartup() override;

private:
    void createBackend();
    void createX11Connection();
    void continueStartupWithScreens();
    void continueStartupWithX();
    void startXwaylandServer();

    int m_xcbConnectionFd = -1;
    QProcess *m_xwaylandProcess = nullptr;
};

}

#define WAYLANTEST_MAIN(TestObject) \
int main(int argc, char *argv[]) \
{ \
    setenv("QT_QPA_PLATFORM", "wayland-org.kde.kwin.qpa", true); \
    setenv("QT_QPA_PLATFORM_PLUGIN_PATH", KWINQPAPATH, true); \
    setenv("KWIN_FOCRE_OWN_QPA", "1", true); \
    KWin::WaylandTestApplication app(argc, argv); \
    app.setAttribute(Qt::AA_Use96Dpi, true); \
    TestObject tc; \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}

#endif
