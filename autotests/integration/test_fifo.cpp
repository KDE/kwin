/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/colorpipeline.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "outputconfigurationstore.h"
#include "pointer_input.h"
#include "tiles/tilemanager.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>
#include <format>

using namespace std::chrono_literals;

namespace KWin
{

class FifoTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testFifo_data();
    void testFifo();
    void testFifoInitiallyHidden();
    void testFifoBarriersOnly();
    void testFifoWaitOnly();
    void testFifoOnSubsurfaces();
    void testBarrierNotClearedByEmptyCommit();
    void testFifoOnUnmappedSurface();
    void testFifoOnSubsurfaceOfUnmappedToplevel_data();
    void testFifoOnSubsurfaceOfUnmappedToplevel();
    void testFifoOnSubsurfaceOfUnmappedSubsurface();
    void testFifoOnSubsurfaceOfUnmappedSubsurfaceInitiallyMapped();
    void testFifoWithExplicitReset_data();
    void testFifoWithExplicitReset();
};

class FifoV1Surface : public QObject, public QtWayland::wp_fifo_v1
{
    Q_OBJECT

public:
    explicit FifoV1Surface(::wp_fifo_v1 *obj)
        : QtWayland::wp_fifo_v1(obj)
    {
    }

    ~FifoV1Surface() override
    {
        wp_fifo_v1_destroy(object());
    }
};

void FifoTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(qAppName()));
    kwinApp()->start();
}

void FifoTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::FifoV1 | Test::AdditionalWaylandInterface::PresentationTime));
    Test::setupWaylandConnection();

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void FifoTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void FifoTest::testFifo_data()
{
    QTest::addColumn<uint32_t>("refreshRate");

    QTest::addRow("50Hz") << 50'000u;
    QTest::addRow("24Hz") << 24'000u;
}

void FifoTest::testFifo()
{
    QFETCH(uint32_t, refreshRate);
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = Rect(0, 0, 200, 200),
            .modes = {
                OutputModeline(QSize(200, 200), refreshRate, OutputModeline::Flag::Preferred),
            },
        },
    });

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));

    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 3> frames;

    // commit 3 frames in quick succession: without fifo, the first two should be discarded, and the last should be presented
    for (auto &frame : frames) {
        frame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }

    {
        QSignalSpy discarded1(frames[0].get(), &Test::WpPresentationFeedback::discarded);
        QSignalSpy discarded2(frames[1].get(), &Test::WpPresentationFeedback::discarded);
        QSignalSpy presented(frames[2].get(), &Test::WpPresentationFeedback::presented);

        QVERIFY(presented.wait(100));
        QVERIFY(discarded1.count());
        QVERIFY(discarded2.count());
    }

    // do it again; this time with fifo, all frames should be presented
    fifo->set_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    for (auto &frame : frames) {
        frame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->set_barrier();
        fifo->wait_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }

    {
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &Test::WpPresentationFeedback::presented),
            QSignalSpy(frames[1].get(), &Test::WpPresentationFeedback::presented),
            QSignalSpy(frames[2].get(), &Test::WpPresentationFeedback::presented),
        };
        for (size_t i = 0; i < frames.size(); i++) {
            QVERIFY(spies[i].wait(100));
            if (i > 0) {
                // each frame should be presented in the refresh cycle after the last one
                const auto thisTimestamp = spies[i].last().at(0).value<std::chrono::nanoseconds>();
                const auto lastTimestamp = spies[i - 1].last().at(0).value<std::chrono::nanoseconds>();
                const auto refreshDuration = spies[i].last().at(1).value<std::chrono::nanoseconds>();
                const auto diff = thisTimestamp - lastTimestamp;
                QCOMPARE_GT(diff, refreshDuration / 2);
                // sometimes, the compositor seems to drop a frame -> don't test for this
                // the only requirement for fifo is that the commit is shown for at least one frame
                // so this doesn't technically change the test result
                // QCOMPARE_LT(diff, refreshDuration * 3 / 2);
            }
        }
    }

    // even if the window is hidden, forward progress must be guaranteed
    window->setMinimized(true);

    for (auto &frame : frames) {
        frame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->set_barrier();
        fifo->wait_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    // another commit, so that the last one will be discarded as well
    fifo->set_barrier();
    fifo->wait_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    {
        const std::chrono::nanoseconds targetRefreshDuration = std::chrono::nanoseconds(1'000'000'000'000 / std::min(30'000u, refreshRate));
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &Test::WpPresentationFeedback::discarded),
        };
        for (size_t i = 0; i < frames.size(); i++) {
            const auto before = std::chrono::steady_clock::now();
            QVERIFY(spies[i].wait(100));
            const auto diff = std::chrono::steady_clock::now() - before;
            QCOMPARE_GT(diff, targetRefreshDuration * 2 / 3);
            QCOMPARE_LT(diff, targetRefreshDuration * 2);
        }
    }
}

void FifoTest::testFifoInitiallyHidden()
{
    // this test verifies that even when the window has never been presented once,
    // we still ensure forward progress

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    window->setMinimized(true);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));

    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 3> frames;

    for (auto &frame : frames) {
        frame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->set_barrier();
        fifo->wait_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    // another commit, so that the last one will be discarded as well
    fifo->set_barrier();
    fifo->wait_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    {
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &Test::WpPresentationFeedback::discarded),
        };
        for (size_t i = 0; i < frames.size(); i++) {
            const auto before = std::chrono::steady_clock::now();
            QVERIFY(spies[i].wait(100));
            const auto diff = std::chrono::steady_clock::now() - before;
            QCOMPARE_GT(diff, std::chrono::milliseconds(30));
            QCOMPARE_LT(diff, std::chrono::milliseconds(100));
        }
    }
}

void FifoTest::testFifoBarriersOnly()
{
    // this test verifies that if you only set barriers but don't wait on them,
    // commits still get applied immediately

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));

    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 3> frames;

    for (auto &frame : frames) {
        frame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->set_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    // another commit, so that the last one will be discarded as well
    fifo->set_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    {
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &Test::WpPresentationFeedback::discarded),
        };
        for (auto &spy : spies) {
            QVERIFY(spy.count() || spy.wait(100));
        }
    }
}

void FifoTest::testFifoWaitOnly()
{
    // this test verifies that if you only wait on barriers but don't set any,
    // commits still get applied immediately

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));

    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 3> frames;

    for (auto &frame : frames) {
        frame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->wait_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    // another commit, so that the last one will be discarded as well
    fifo->wait_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    {
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &Test::WpPresentationFeedback::discarded),
        };
        for (auto &spy : spies) {
            QVERIFY(spy.count() || spy.wait(100));
        }
    }
}

void FifoTest::testFifoOnSubsurfaces()
{
    // this test verifies that fifo works correctly with subsurfaces
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = Rect(0, 0, 200, 200),
            .modes = {
                OutputModeline(QSize(200, 200), 60'000, OutputModeline::Flag::Preferred),
            },
        },
    });

    std::unique_ptr<KWayland::Client::Surface> parentSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(parentSurface.get()));
    auto window = Test::renderAndWaitForShown(parentSurface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    auto subsurface = Test::createSubSurface(surface.get(), parentSurface.get());
    Test::render(surface.get(), QSize(100, 50), Qt::red);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));
    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 3> frames;

    // when the surface is desync, fifo should work like on a toplevel surface
    subsurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    for (auto &frame : frames) {
        frame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->set_barrier();
        fifo->wait_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    // another commit, so that the last one will be discarded as well
    fifo->set_barrier();
    fifo->wait_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    {
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &Test::WpPresentationFeedback::presented),
            QSignalSpy(frames[1].get(), &Test::WpPresentationFeedback::presented),
            QSignalSpy(frames[2].get(), &Test::WpPresentationFeedback::presented),
        };
        for (auto &spy : spies) {
            QVERIFY(spy.wait(100));
        }
    }

    // when the surface is synchronized though, fifo should be ignored
    subsurface->setMode(KWayland::Client::SubSurface::Mode::Synchronized);
    for (auto &frame : frames) {
        frame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->set_barrier();
        fifo->wait_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    // another commit, so that the last one will be discarded as well
    fifo->set_barrier();
    fifo->wait_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    {
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &Test::WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &Test::WpPresentationFeedback::discarded),
        };
        for (auto &spy : spies) {
            QVERIFY(spy.count() || spy.wait(100));
        }
    }
}

void FifoTest::testBarrierNotClearedByEmptyCommit()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));

    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 2> frames;

    fifo->set_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // this additional commit must not clear the barrier
    auto firstFrame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    auto secondFrame = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
    fifo->wait_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy firstSpy(firstFrame.get(), &Test::WpPresentationFeedback::presented);
    QVERIFY(firstSpy.wait(100));

    QSignalSpy secondSpy(secondFrame.get(), &Test::WpPresentationFeedback::presented);
    QVERIFY(secondSpy.wait(100));

    // the second frame should be presented in the refresh cycle after the first one
    const auto thisTimestamp = secondSpy.last().at(0).value<std::chrono::nanoseconds>();
    const auto lastTimestamp = firstSpy.last().at(0).value<std::chrono::nanoseconds>();
    const auto refreshDuration = secondSpy.last().at(1).value<std::chrono::nanoseconds>();
    const auto diff = thisTimestamp - lastTimestamp;
    QCOMPARE_GT(diff, refreshDuration / 2);
}

void FifoTest::testFifoOnUnmappedSurface()
{
    // This test verifies that transactions on a toplevel that is never mapped
    // don't get stuck because of fifo barriers

    Test::XdgToplevelWindow window;

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*window.m_surface));

    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 3> frames;
    for (size_t i = 0; i < frames.size(); i++) {
        fifo->set_barrier();
        fifo->wait_barrier();
        frames[i] = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*window.m_surface));
        window.m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    window.m_surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // all frames should be immediately discarded
    QSignalSpy spy0(frames[0].get(), &Test::WpPresentationFeedback::discarded);
    QSignalSpy spy1(frames[1].get(), &Test::WpPresentationFeedback::discarded);
    QSignalSpy spy2(frames[2].get(), &Test::WpPresentationFeedback::discarded);
    QVERIFY(spy0.wait());
    QVERIFY(spy1.count() || spy1.wait());
    QVERIFY(spy2.count() || spy2.wait());
}

void FifoTest::testFifoOnSubsurfaceOfUnmappedToplevel_data()
{
    QTest::addColumn<bool>("mapWindow");
    QTest::addColumn<bool>("destroyShell");

    QTest::addRow("never mapped") << false << false;
    QTest::addRow("initially mapped") << true << false;
    QTest::addRow("initially mapped, destroy shell") << true << true;
}

void FifoTest::testFifoOnSubsurfaceOfUnmappedToplevel()
{
    // This test verifies that transactions on subsurfaces of a toplevel
    // that isn't mapped don't get stuck because of fifo barriers
    QFETCH(bool, mapWindow);
    QFETCH(bool, destroyShell);

    Test::XdgToplevelWindow window;
    if (mapWindow) {
        QVERIFY(window.show());
    }

    auto surface = Test::createSurface();
    auto subsurface = Test::createSubSurface(surface.get(), window.m_surface.get());
    subsurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 3> frames;

    Test::render(surface.get(), QSize(100, 100), Qt::blue);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));
    for (size_t i = 0; i < frames.size(); i++) {
        fifo->set_barrier();
        fifo->wait_barrier();
        frames[i] = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    if (destroyShell) {
        window.m_toplevel.reset();
        window.m_window = nullptr;
    } else if (mapWindow) {
        window.unmap();
    }

    QSignalSpy spy0(frames[0].get(), &Test::WpPresentationFeedback::discarded);
    QSignalSpy spy1(frames[1].get(), &Test::WpPresentationFeedback::discarded);
    QSignalSpy spy2(frames[2].get(), &Test::WpPresentationFeedback::discarded);
    QVERIFY(spy0.wait());
    QVERIFY(spy1.count() || spy1.wait());
    QVERIFY(spy2.count() || spy2.wait());
}

void FifoTest::testFifoOnSubsurfaceOfUnmappedSubsurface()
{
    // This test verifies that transactions on subsurfaces of a subsurface
    // that is never mapped don't get stuck because of fifo barriers

    Test::XdgToplevelWindow window;
    QVERIFY(window.show());

    auto parentSurface = Test::createSurface();
    auto parentSubsurface = Test::createSubSurface(parentSurface.get(), window.m_surface.get());
    parentSubsurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);

    auto surface = Test::createSurface();
    auto subsurface = Test::createSubSurface(surface.get(), parentSurface.get());
    subsurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 3> frames;

    Test::render(surface.get(), QSize(100, 100), Qt::blue);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));
    for (size_t i = 0; i < frames.size(); i++) {
        fifo->set_barrier();
        fifo->wait_barrier();
        frames[i] = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy spy0(frames[0].get(), &Test::WpPresentationFeedback::discarded);
    QSignalSpy spy1(frames[1].get(), &Test::WpPresentationFeedback::discarded);
    QSignalSpy spy2(frames[2].get(), &Test::WpPresentationFeedback::discarded);
    QVERIFY(spy0.wait());
    QVERIFY(spy1.count() || spy1.wait());
    QVERIFY(spy2.count() || spy2.wait());
}

void FifoTest::testFifoOnSubsurfaceOfUnmappedSubsurfaceInitiallyMapped()
{
    // This test verifies that transactions on subsurfaces of a subsurface
    // that gets unmapped after being mapped don't get stuck because of fifo barriers

    Test::XdgToplevelWindow window;
    QVERIFY(window.show());

    auto parentSurface = Test::createSurface();
    auto parentSubsurface = Test::createSubSurface(parentSurface.get(), window.m_surface.get());
    parentSubsurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    Test::render(parentSurface.get(), QSize(100, 100), Qt::red);

    auto surface = Test::createSurface();
    auto subsurface = Test::createSubSurface(surface.get(), parentSurface.get());
    subsurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 3> frames;

    Test::render(surface.get(), QSize(100, 100), Qt::blue);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));
    // queue some commits
    for (size_t i = 0; i < frames.size(); i++) {
        fifo->set_barrier();
        fifo->wait_barrier();
        frames[i] = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    fifo->set_barrier();
    fifo->wait_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // and unmap the parent surface
    parentSurface->attachBuffer((wl_buffer *)nullptr);
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy spy0(frames[0].get(), &Test::WpPresentationFeedback::discarded);
    QSignalSpy spy1(frames[1].get(), &Test::WpPresentationFeedback::discarded);
    QSignalSpy spy2(frames[2].get(), &Test::WpPresentationFeedback::discarded);
    QVERIFY(spy0.wait());
    QVERIFY(spy1.count() || spy1.wait());
    QVERIFY(spy2.count() || spy2.wait());
}

void FifoTest::testFifoWithExplicitReset_data()
{
    QTest::addColumn<bool>("destroyShellFirst");

    QTest::addRow("destroy shell first") << true;
    QTest::addRow("unmap first") << false;
}

void FifoTest::testFifoWithExplicitReset()
{
    // This test verifies that pending transactions with fifo barriers
    // will not be stuck when the shell surface is destroyed
    QFETCH(bool, destroyShellFirst);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*surface));

    std::array<std::unique_ptr<Test::WpPresentationFeedback>, 2> frames;
    for (int i = 0; i < 2; i++) {
        frames[i] = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->set_barrier();
        fifo->wait_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    fifo->set_barrier();
    fifo->wait_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    if (destroyShellFirst) {
        shellSurface.reset();
    }
    surface->attachBuffer(static_cast<wl_buffer *>(nullptr), QPoint());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    shellSurface.reset();

    // the surface transactions must not be blocked anymore
    QSignalSpy first(frames[0].get(), &Test::WpPresentationFeedback::discarded);
    QSignalSpy second(frames[1].get(), &Test::WpPresentationFeedback::discarded);
    QVERIFY(first.wait());
    QVERIFY(second.count() || second.wait());

    shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window2 = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2 && window2->isShown());
}

}

WAYLANDTEST_MAIN(KWin::FifoTest)
#include "test_fifo.moc"
