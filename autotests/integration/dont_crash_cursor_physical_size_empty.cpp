/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2018 Martin Flöser <mgraesslin@kde.org>

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
#include "kwin_wayland_test.h"
#include "composite.h"
#include "effectloader.h"
#include "client.h"
#include "cursor.h"
#include "effects.h"
#include "platform.h"
#include "shell_client.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>

#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/xdgshell.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>

using namespace KWin;
using namespace KWayland::Client;
static const QString s_socketName = QStringLiteral("wayland_test_kwin_crash_cursor_physical_size_empty-0");

class DontCrashCursorPhysicalSizeEmpty : public QObject
{
Q_OBJECT
private Q_SLOTS:
    void init();
    void initTestCase();
    void cleanup();
    void testMoveCursorOverDeco_data();
    void testMoveCursorOverDeco();
};

void DontCrashCursorPhysicalSizeEmpty::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration));

    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(640, 512));
}

void DontCrashCursorPhysicalSizeEmpty::cleanup()
{
    Test::destroyWaylandConnection();
}

void DontCrashCursorPhysicalSizeEmpty::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    if (!QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons/DMZ-White/index.theme")).isEmpty()) {
        qputenv("XCURSOR_THEME", QByteArrayLiteral("DMZ-White"));
    } else {
        // might be vanilla-dmz (e.g. Arch, FreeBSD)
        qputenv("XCURSOR_THEME", QByteArrayLiteral("Vanilla-DMZ"));
    }
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("0"));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
}

void DontCrashCursorPhysicalSizeEmpty::testMoveCursorOverDeco_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("xdgShellV6") << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("xdgWmBase") << Test::ShellSurfaceType::XdgShellStable;
}

void DontCrashCursorPhysicalSizeEmpty::testMoveCursorOverDeco()
{
    // This test ensures that there is no endless recursion if the cursor theme cannot be created
    // a reason for creation failure could be physical size not existing
    // see BUG: 390314
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    Test::waylandServerSideDecoration()->create(surface.data(), surface.data());
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isDecorated());

    // destroy physical size
    KWayland::Server::Display *display = waylandServer()->display();
    auto output = display->outputs().first();
    output->setPhysicalSize(QSize(0, 0));
    // and fake a cursor theme change, so that the theme gets recreated
    emit KWin::Cursor::self()->themeChanged();

    KWin::Cursor::setPos(QPoint(c->geometry().center().x(), c->clientPos().y() / 2));
}

WAYLANDTEST_MAIN(DontCrashCursorPhysicalSizeEmpty)
#include "dont_crash_cursor_physical_size_empty.moc"
