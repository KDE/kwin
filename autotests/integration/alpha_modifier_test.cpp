/*
    SPDX-FileCopyrightText: 2026 Hongfei Shang <shanghongfei@kylinos.cn>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "pointer_input.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/surface.h>

#include <limits>

namespace KWin
{

static constexpr uint32_t s_maxFactor = std::numeric_limits<uint32_t>::max();
static constexpr uint32_t s_halfFactor = s_maxFactor / 2;

class AlphaModifierTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testDefaultValue();
    void testSetMultiplier_data();
    void testSetMultiplier();
    void testResetOnDestroy();
    void testRecreateAfterDestroy();
    void testDoubleConstructionError();
    void testNoSurfaceError();
};

void AlphaModifierTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(qAppName()));
    kwinApp()->start();
}

void AlphaModifierTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::AlphaModifierV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void AlphaModifierTest::cleanup()
{
    Test::destroyWaylandConnection();
}

// Without any alpha modifier surface, the multiplier defaults to 1.0 (fully opaque).
void AlphaModifierTest::testDefaultValue()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    QCOMPARE(window->surface()->alphaMultiplier(), 1.0);
}

void AlphaModifierTest::testSetMultiplier_data()
{
    QTest::addColumn<uint32_t>("factor");
    QTest::addColumn<double>("expected");

    QTest::addRow("transparent") << 0u << 0.0;
    QTest::addRow("half") << s_halfFactor << s_halfFactor / double(s_maxFactor);
    QTest::addRow("opaque") << s_maxFactor << 1.0;
}

// Setting a multiplier and committing applies it to the surface's double-buffered state.
void AlphaModifierTest::testSetMultiplier()
{
    QFETCH(uint32_t, factor);
    QFETCH(double, expected);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    auto modifier = std::make_unique<Test::AlphaModifierSurfaceV1>(Test::alphaModifier()->get_surface(*surface));

    // The multiplier is double-buffered: it must not take effect before the next commit.
    modifier->set_multiplier(factor);
    QCOMPARE(window->surface()->alphaMultiplier(), 1.0);

    QSignalSpy committed(window->surface(), &SurfaceInterface::committed);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committed.wait());
    QCOMPARE(window->surface()->alphaMultiplier(), expected);
}

// Destroying the alpha modifier surface resets the multiplier back to 1.0 on the next commit.
void AlphaModifierTest::testResetOnDestroy()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    auto modifier = std::make_unique<Test::AlphaModifierSurfaceV1>(Test::alphaModifier()->get_surface(*surface));
    modifier->set_multiplier(s_halfFactor);
    {
        QSignalSpy committed(window->surface(), &SurfaceInterface::committed);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(committed.wait());
        QCOMPARE(window->surface()->alphaMultiplier(), s_halfFactor / double(s_maxFactor));
    }

    modifier.reset();

    // Destroy has the same double-buffered semantics as set_multiplier: the reset to opaque must
    // not take effect until the next commit.
    QCOMPARE(window->surface()->alphaMultiplier(), s_halfFactor / double(s_maxFactor));
    {
        QSignalSpy committed(window->surface(), &SurfaceInterface::committed);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(committed.wait());
        QCOMPARE(window->surface()->alphaMultiplier(), 1.0);
    }
}

// After the alpha modifier surface is destroyed, a new one can be created for the same wl_surface.
// This guards the back-pointer being cleared on destruction (otherwise the next get_surface would
// wrongly raise an already_constructed protocol error).
void AlphaModifierTest::testRecreateAfterDestroy()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);

    auto modifier = std::make_unique<Test::AlphaModifierSurfaceV1>(Test::alphaModifier()->get_surface(*surface));
    modifier->set_multiplier(0);
    {
        QSignalSpy committed(window->surface(), &SurfaceInterface::committed);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(committed.wait());
        QCOMPARE(window->surface()->alphaMultiplier(), 0.0);
    }

    modifier.reset();

    // A second alpha modifier surface must be constructible now that the first one is gone.
    auto modifier2 = std::make_unique<Test::AlphaModifierSurfaceV1>(Test::alphaModifier()->get_surface(*surface));
    modifier2->set_multiplier(s_halfFactor);
    {
        QSignalSpy committed(window->surface(), &SurfaceInterface::committed);
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(committed.wait());
        QCOMPARE(window->surface()->alphaMultiplier(), s_halfFactor / double(s_maxFactor));
    }

    QVERIFY(error.isEmpty());
}

// Requesting a second alpha modifier surface for the same wl_surface is a protocol error.
void AlphaModifierTest::testDoubleConstructionError()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());

    auto first = std::make_unique<Test::AlphaModifierSurfaceV1>(Test::alphaModifier()->get_surface(*surface));
    auto second = std::make_unique<Test::AlphaModifierSurfaceV1>(Test::alphaModifier()->get_surface(*surface));

    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait());
}

// Using set_multiplier after the wl_surface has been destroyed is a protocol error.
void AlphaModifierTest::testNoSurfaceError()
{
    // Use a role-less wl_surface so it can be destroyed freely while the alpha modifier still lives.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());

    auto modifier = std::make_unique<Test::AlphaModifierSurfaceV1>(Test::alphaModifier()->get_surface(*surface));

    // Destroy the wl_surface while the alpha modifier surface still references it. Requests are
    // processed in order, so the surface is gone by the time set_multiplier is handled.
    surface.reset();
    modifier->set_multiplier(s_halfFactor);

    QSignalSpy error(Test::waylandConnection(), &KWayland::Client::ConnectionThread::errorOccurred);
    QVERIFY(error.wait());
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::AlphaModifierTest)
#include "alpha_modifier_test.moc"
