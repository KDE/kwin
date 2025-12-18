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
#include <deque>
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
    void testFifoCommitTiming();
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

class CommitTimerV1 : public QtWayland::wp_commit_timer_v1
{
public:
    explicit CommitTimerV1(::wp_commit_timer_v1 *obj)
        : QtWayland::wp_commit_timer_v1(obj)
    {
    }

    ~CommitTimerV1() override
    {
        destroy();
    }

    void setTimestamp(std::chrono::nanoseconds time)
    {
        const uint64_t seconds = time / 1s;
        const uint64_t nanos = (time - std::chrono::seconds(seconds)).count();
        set_timestamp(seconds >> 32, seconds & 0xFFFFFFFF, nanos);
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
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::FifoV1
                                         | Test::AdditionalWaylandInterface::PresentationTime
                                         | Test::AdditionalWaylandInterface::CommitTiming));
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
                std::make_tuple(QSize(200, 200), refreshRate, OutputMode::Flag::Preferred),
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
                std::make_tuple(QSize(200, 200), 60'000, OutputMode::Flag::Preferred),
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

void FifoTest::testFifoCommitTiming()
{
    // this tests that using fifo + commit timing in combination
    // (like Mesa does) works as expected

    Test::XdgToplevelWindow window;
    QVERIFY(window.show());

    auto fifo = std::make_unique<FifoV1Surface>(Test::fifoManager()->get_fifo(*window.m_surface));
    auto timer = std::make_unique<CommitTimerV1>(Test::connection()->commitTiming->get_timer(*window.m_surface));

    std::deque<std::unique_ptr<Test::WpPresentationFeedback>> frames;

    auto lastTarget = std::chrono::steady_clock::now().time_since_epoch();
    auto refreshDuration = std::chrono::nanoseconds(1'000'000) / workspace()->outputs().front()->refreshRate();
    std::optional<std::chrono::nanoseconds> lastPresentation;

    for (size_t i = 0; i < 10; i++) {
        if (frames.size() >= 3) {
            QSignalSpy presented(frames[0].get(), &Test::WpPresentationFeedback::presented);
            QVERIFY(presented.wait());

            refreshDuration = presented.last()[1].value<std::chrono::nanoseconds>();
            const auto timestamp = presented.last()[0].value<std::chrono::nanoseconds>();
            if (lastPresentation) {
                const auto duration = timestamp - *lastPresentation;
                QCOMPARE_GE(duration, refreshDuration / 2);
                QCOMPARE_LE(duration, refreshDuration * 2);
            }
            lastPresentation = timestamp;

            frames.pop_front();
        }

        frames.push_back(std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*window.m_surface)));
        fifo->set_barrier();
        fifo->wait_barrier();
        timer->setTimestamp(lastTarget + refreshDuration - 500us);
        lastTarget += refreshDuration;
        window.commit();

        fifo->wait_barrier();
        window.commit();
    }
}

}

WAYLANDTEST_MAIN(KWin::FifoTest)
#include "test_fifo.moc"
