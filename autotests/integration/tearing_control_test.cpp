/*
    SPDX-FileCopyrightText: 2026 Hongfei Shang <shanghongfei@kylinos.cn>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "pointer_input.h"
#include "scene/surfaceitem.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/surface.h>

using namespace std::chrono_literals;

namespace KWin
{

class TearingControlTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testDefaultHint();
    void testSetPresentationHint();
    void testResetOnDestroy();
    void testRecreateAfterDestroy();
    void testDoubleConstructionError();
    void testInertAfterSurfaceDestroy();
};

void TearingControlTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(qAppName()));
    kwinApp()->start();
}

void TearingControlTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::TearingControlV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void TearingControlTest::cleanup()
{
    Test::destroyWaylandConnection();
}

// Without any tearing control object, the presentation hint defaults to vsync.
void TearingControlTest::testDefaultHint()
{
    Test::XdgToplevelWindow window;
    QVERIFY(window.show());

    QCOMPARE(window.m_window->surfaceItem()->presentationHint(), PresentationModeHint::VSync);
}

// Setting the presentation hint and committing propagates it to the scene item.
void TearingControlTest::testSetPresentationHint()
{
    Test::XdgToplevelWindow window;
    QVERIFY(window.show());
    KWayland::Client::Surface *surface = window.m_surface.get();
    SurfaceItem *item = window.m_window->surfaceItem();
    QVERIFY(item);

    auto control = std::make_unique<Test::TearingControlV1>(Test::tearingControl()->get_tearing_control(*surface));

    QSignalSpy committed(window.m_window->surface(), &SurfaceInterface::committed);

    control->set_presentation_hint(Test::TearingControlV1::presentation_hint_async);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committed.wait());
    QCOMPARE(item->presentationHint(), PresentationModeHint::Async);

    // Switching back to vsync works as well.
    control->set_presentation_hint(Test::TearingControlV1::presentation_hint_vsync);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committed.wait());
    QCOMPARE(item->presentationHint(), PresentationModeHint::VSync);
}

// Destroying the tearing control object reverts the hint to vsync on the next commit.
void TearingControlTest::testResetOnDestroy()
{
    Test::XdgToplevelWindow window;
    QVERIFY(window.show());
    KWayland::Client::Surface *surface = window.m_surface.get();
    SurfaceItem *item = window.m_window->surfaceItem();
    QVERIFY(item);

    auto control = std::make_unique<Test::TearingControlV1>(Test::tearingControl()->get_tearing_control(*surface));

    QSignalSpy committed(window.m_window->surface(), &SurfaceInterface::committed);

    control->set_presentation_hint(Test::TearingControlV1::presentation_hint_async);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committed.wait());
    QCOMPARE(item->presentationHint(), PresentationModeHint::Async);

    control.reset();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committed.wait());
    QCOMPARE(item->presentationHint(), PresentationModeHint::VSync);
}

// After the tearing control object is destroyed, a new one can be created for the same wl_surface.
// This guards the back-pointer being cleared on destruction (otherwise the next get_tearing_control
// would wrongly raise a tearing_control_exists protocol error).
void TearingControlTest::testRecreateAfterDestroy()
{
    Test::XdgToplevelWindow window;
    QVERIFY(window.show());
    KWayland::Client::Surface *surface = window.m_surface.get();
    SurfaceItem *item = window.m_window->surfaceItem();
    QVERIFY(item);

    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QSignalSpy committed(window.m_window->surface(), &SurfaceInterface::committed);

    auto control = std::make_unique<Test::TearingControlV1>(Test::tearingControl()->get_tearing_control(*surface));
    control->set_presentation_hint(Test::TearingControlV1::presentation_hint_async);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committed.wait());
    QCOMPARE(item->presentationHint(), PresentationModeHint::Async);

    control.reset();

    // A second tearing control object must be constructible now that the first one is gone.
    auto control2 = std::make_unique<Test::TearingControlV1>(Test::tearingControl()->get_tearing_control(*surface));
    control2->set_presentation_hint(Test::TearingControlV1::presentation_hint_async);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committed.wait());
    QCOMPARE(item->presentationHint(), PresentationModeHint::Async);

    QVERIFY(error.isEmpty());
}

// Requesting a second tearing control object for the same wl_surface is a protocol error.
void TearingControlTest::testDoubleConstructionError()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());

    auto first = std::make_unique<Test::TearingControlV1>(Test::tearingControl()->get_tearing_control(*surface));
    auto second = std::make_unique<Test::TearingControlV1>(Test::tearingControl()->get_tearing_control(*surface));

    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait(50ms));
}

// If the wl_surface is destroyed first, the tearing control object becomes inert: further requests
// are silently ignored, without a protocol error or a crash.
void TearingControlTest::testInertAfterSurfaceDestroy()
{
    // Use a role-less wl_surface so it can be destroyed freely while the tearing control still lives.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());

    auto control = std::make_unique<Test::TearingControlV1>(Test::tearingControl()->get_tearing_control(*surface));

    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);

    // Destroy the wl_surface while the tearing control object still references it. Requests are
    // processed in order, so the object is inert by the time set_presentation_hint is handled.
    surface.reset();
    control->set_presentation_hint(Test::TearingControlV1::presentation_hint_async);
    control.reset();

    QVERIFY(Test::waylandSync());
    QVERIFY(error.isEmpty());
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::TearingControlTest)
#include "tearing_control_test.moc"
