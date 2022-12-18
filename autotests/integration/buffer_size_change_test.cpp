/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "generic_scene_opengl_test.h"

#include "composite.h"
#include "scene/workspacescene.h"
#include "wayland_server.h"
#include "window.h"

#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_buffer_size_change-0");

class BufferSizeChangeTest : public GenericSceneOpenGLTest
{
    Q_OBJECT
public:
    BufferSizeChangeTest()
        : GenericSceneOpenGLTest(QByteArrayLiteral("O2"))
    {
    }
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

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // set buffer size
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // add a first repaint
    QSignalSpy frameRenderedSpy(Compositor::self()->scene(), &WorkspaceScene::frameRendered);
    Compositor::self()->scene()->addRepaintFull();
    QVERIFY(frameRenderedSpy.wait());

    // now change buffer size
    Test::render(surface.get(), QSize(30, 10), Qt::red);

    QSignalSpy damagedSpy(window, &Window::damaged);
    QVERIFY(damagedSpy.wait());
    KWin::Compositor::self()->scene()->addRepaintFull();
    QVERIFY(frameRenderedSpy.wait());
}

void BufferSizeChangeTest::testShmBufferSizeChangeOnSubSurface()
{
    // setup parent surface
    std::unique_ptr<KWayland::Client::Surface> parentSurface(Test::createSurface());
    QVERIFY(parentSurface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(parentSurface.get()));
    QVERIFY(shellSurface != nullptr);

    // setup sub surface
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(Test::createSubSurface(surface.get(), parentSurface.get()));
    QVERIFY(subSurface != nullptr);

    // set buffer sizes
    Test::render(surface.get(), QSize(30, 10), Qt::red);
    Window *parent = Test::renderAndWaitForShown(parentSurface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(parent);

    // add a first repaint
    QSignalSpy frameRenderedSpy(Compositor::self()->scene(), &WorkspaceScene::frameRendered);
    Compositor::self()->scene()->addRepaintFull();
    QVERIFY(frameRenderedSpy.wait());

    // change buffer size of sub surface
    QSignalSpy damagedParentSpy(parent, &Window::damaged);
    Test::render(surface.get(), QSize(20, 10), Qt::red);
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(damagedParentSpy.wait());

    // add a second repaint
    KWin::Compositor::self()->scene()->addRepaintFull();
    QVERIFY(frameRenderedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::BufferSizeChangeTest)
#include "buffer_size_change_test.moc"
