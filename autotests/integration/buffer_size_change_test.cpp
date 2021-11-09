/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "generic_scene_opengl_test.h"

#include "abstract_client.h"
#include "composite.h"
#include "scene.h"
#include "wayland_server.h"

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

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // set buffer size
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);

    // add a first repaint
    QSignalSpy frameRenderedSpy(Compositor::self()->scene(), &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    Compositor::self()->scene()->addRepaintFull();
    QVERIFY(frameRenderedSpy.wait());

    // now change buffer size
    Test::render(surface.data(), QSize(30, 10), Qt::red);

    QSignalSpy damagedSpy(client, &AbstractClient::damaged);
    QVERIFY(damagedSpy.isValid());
    QVERIFY(damagedSpy.wait());
    KWin::Compositor::self()->scene()->addRepaintFull();
    QVERIFY(frameRenderedSpy.wait());
}

void BufferSizeChangeTest::testShmBufferSizeChangeOnSubSurface()
{
    using namespace KWayland::Client;

    // setup parent surface
    QScopedPointer<KWayland::Client::Surface> parentSurface(Test::createSurface());
    QVERIFY(!parentSurface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(parentSurface.data()));
    QVERIFY(!shellSurface.isNull());

    // setup sub surface
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<SubSurface> subSurface(Test::createSubSurface(surface.data(), parentSurface.data()));
    QVERIFY(!subSurface.isNull());

    // set buffer sizes
    Test::render(surface.data(), QSize(30, 10), Qt::red);
    AbstractClient *parent = Test::renderAndWaitForShown(parentSurface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(parent);

    // add a first repaint
    QSignalSpy frameRenderedSpy(Compositor::self()->scene(), &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    Compositor::self()->scene()->addRepaintFull();
    QVERIFY(frameRenderedSpy.wait());

    // change buffer size of sub surface
    QSignalSpy damagedParentSpy(parent, &AbstractClient::damaged);
    QVERIFY(damagedParentSpy.isValid());
    Test::render(surface.data(), QSize(20, 10), Qt::red);
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(damagedParentSpy.wait());

    // add a second repaint
    KWin::Compositor::self()->scene()->addRepaintFull();
    QVERIFY(frameRenderedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::BufferSizeChangeTest)
#include "buffer_size_change_test.moc"
