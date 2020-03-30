/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

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
#include "generic_scene_opengl_test.h"

#include "abstract_client.h"
#include "composite.h"
#include "wayland_server.h"

#include <KWayland/Client/xdgshell.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_buffer_size_change-0");

class BufferSizeChangeTest : public GenericSceneOpenGLTest
{
    Q_OBJECT
public:
    BufferSizeChangeTest() : GenericSceneOpenGLTest(QByteArrayLiteral("O2")) {}
private Q_SLOTS:
    void init();
    void testShmBufferSizeChange();
    void testShmBufferSizeChangeOnSubSurface();
};

void BufferSizeChangeTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void BufferSizeChangeTest::testShmBufferSizeChange()
{
    // This test verifies that an SHM buffer size change is handled correctly

    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // set buffer size
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);

    // add a first repaint
    QSignalSpy swapSpy(Compositor::self(), &Compositor::bufferSwapCompleted);
    QVERIFY(swapSpy.isValid());
    Compositor::self()->addRepaintFull();
    QVERIFY(swapSpy.wait());

    // now change buffer size
    Test::render(surface.data(), QSize(30, 10), Qt::red);

    QSignalSpy damagedSpy(client, &AbstractClient::damaged);
    QVERIFY(damagedSpy.isValid());
    QVERIFY(damagedSpy.wait());
    KWin::Compositor::self()->addRepaintFull();
    QVERIFY(swapSpy.wait());
}

void BufferSizeChangeTest::testShmBufferSizeChangeOnSubSurface()
{
    using namespace KWayland::Client;

    // setup parent surface
    QScopedPointer<Surface> parentSurface(Test::createSurface());
    QVERIFY(!parentSurface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(parentSurface.data()));
    QVERIFY(!shellSurface.isNull());

    // setup sub surface
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<SubSurface> subSurface(Test::createSubSurface(surface.data(), parentSurface.data()));
    QVERIFY(!subSurface.isNull());

    // set buffer sizes
    Test::render(surface.data(), QSize(30, 10), Qt::red);
    AbstractClient *parent = Test::renderAndWaitForShown(parentSurface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(parent);

    // add a first repaint
    QSignalSpy swapSpy(Compositor::self(), &Compositor::bufferSwapCompleted);
    QVERIFY(swapSpy.isValid());
    Compositor::self()->addRepaintFull();
    QVERIFY(swapSpy.wait());

    // change buffer size of sub surface
    QSignalSpy damagedParentSpy(parent, &AbstractClient::damaged);
    QVERIFY(damagedParentSpy.isValid());
    Test::render(surface.data(), QSize(20, 10), Qt::red);
    parentSurface->commit(Surface::CommitFlag::None);

    QVERIFY(damagedParentSpy.wait());

    // add a second repaint
    KWin::Compositor::self()->addRepaintFull();
    QVERIFY(swapSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::BufferSizeChangeTest)
#include "buffer_size_change_test.moc"
