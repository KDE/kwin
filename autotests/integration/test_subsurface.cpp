
/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/region.h"
#include "kwin_wayland_test.h"
#include "wayland/subcompositor.h"
#include "wayland/surface.h"
#include "wayland_server.h"

#include <KWayland/Client/subsurface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_subsurface-0");

class SubsurfaceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void doubleCommitDamage();
};

void SubsurfaceTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    Test::setOutputConfig({
        Rect(0, 0, 1280, 1024),
    });
}

void SubsurfaceTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void SubsurfaceTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void SubsurfaceTest::doubleCommitDamage()
{
    // This test verifies that committing a synchronized subsurface twice
    // before committing the parent doesn't throw away damage requests
    Test::XdgToplevelWindow window;
    QVERIFY(window.show());

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    const auto subsurface = Test::createSubSurface(surface.get(), window.m_surface.get());
    Test::render(surface.get(), QSize(10, 10), Qt::red);
    QVERIFY(window.presentWait());

    SurfaceInterface *kwinSurface = window.m_window->surface()->above().front()->surface();
    QVERIFY(kwinSurface);

    QSignalSpy damaged(kwinSurface, &SurfaceInterface::damaged);

    // damage from a first commit must not be overwritten by a second commit
    surface->damageBuffer(QRect(0, 0, 10, 10));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    // commit it again, before committing the parent
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    // commit the parent
    QVERIFY(window.presentWait());
    QCOMPARE(damaged.count(), 1);
    QCOMPARE(damaged.first().last().value<Region>(), Region(0, 0, 10, 10));

    // damage from each commit should be added together
    damaged.clear();
    surface->damageBuffer(QRect(0, 0, 5, 5));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    surface->damageBuffer(QRect(5, 5, 5, 5));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(window.presentWait());
    QCOMPARE(damaged.count(), 1);
    QCOMPARE(damaged.first().last().value<Region>(), Region(0, 0, 5, 5) | Rect(5, 5, 5, 5));
}

}

WAYLANDTEST_MAIN(KWin::SubsurfaceTest)
#include "test_subsurface.moc"
