
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

static const QString s_socketName = QStringLiteral("wayland_test_fifo-0");

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

class WpPresentationFeedback : public QObject, public QtWayland::wp_presentation_feedback
{
    Q_OBJECT
public:
    explicit WpPresentationFeedback(struct ::wp_presentation_feedback *obj)
        : QtWayland::wp_presentation_feedback(obj)
    {
    }

    ~WpPresentationFeedback() override
    {
        wp_presentation_feedback_destroy(object());
    }

Q_SIGNALS:
    void presented(std::chrono::nanoseconds timestamp, std::chrono::nanoseconds refreshDuration);
    void discarded();

private:
    void wp_presentation_feedback_presented(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec, uint32_t refresh, uint32_t seq_hi, uint32_t seq_lo, uint32_t flags) override
    {
        const std::chrono::nanoseconds timestamp = std::chrono::seconds((uint64_t(tv_sec_hi) << 32) | tv_sec_lo) + std::chrono::nanoseconds(tv_nsec);
        Q_EMIT presented(timestamp, std::chrono::nanoseconds(refresh));
    }

    void wp_presentation_feedback_discarded() override
    {
        Q_EMIT discarded();
    }
};

void FifoTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(s_socketName));
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
            .geometry = QRect(0, 0, 200, 200),
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

    std::array<std::unique_ptr<WpPresentationFeedback>, 3> frames;

    // commit 3 frames in quick succession: without fifo, the first two should be discarded, and the last should be presented
    for (auto &frame : frames) {
        frame = std::make_unique<WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }

    {
        QSignalSpy discarded1(frames[0].get(), &WpPresentationFeedback::discarded);
        QSignalSpy discarded2(frames[1].get(), &WpPresentationFeedback::discarded);
        QSignalSpy presented(frames[2].get(), &WpPresentationFeedback::presented);

        QVERIFY(presented.wait(100));
        QVERIFY(discarded1.count());
        QVERIFY(discarded2.count());
    }

    // do it again; this time with fifo, all frames should be presented
    fifo->set_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    for (auto &frame : frames) {
        frame = std::make_unique<WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->set_barrier();
        fifo->wait_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }

    {
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &WpPresentationFeedback::presented),
            QSignalSpy(frames[1].get(), &WpPresentationFeedback::presented),
            QSignalSpy(frames[2].get(), &WpPresentationFeedback::presented),
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
        frame = std::make_unique<WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
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
            QSignalSpy(frames[0].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &WpPresentationFeedback::discarded),
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

    std::array<std::unique_ptr<WpPresentationFeedback>, 3> frames;

    for (auto &frame : frames) {
        frame = std::make_unique<WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
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
            QSignalSpy(frames[0].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &WpPresentationFeedback::discarded),
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

    std::array<std::unique_ptr<WpPresentationFeedback>, 3> frames;

    for (auto &frame : frames) {
        frame = std::make_unique<WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->set_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    // another commit, so that the last one will be discarded as well
    fifo->set_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    {
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &WpPresentationFeedback::discarded),
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

    std::array<std::unique_ptr<WpPresentationFeedback>, 3> frames;

    for (auto &frame : frames) {
        frame = std::make_unique<WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
        fifo->wait_barrier();
        surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    // another commit, so that the last one will be discarded as well
    fifo->wait_barrier();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    {
        std::array<QSignalSpy, frames.size()> spies = {
            QSignalSpy(frames[0].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &WpPresentationFeedback::discarded),
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
            .geometry = QRect(0, 0, 200, 200),
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
    std::array<std::unique_ptr<WpPresentationFeedback>, 3> frames;

    // when the surface is desync, fifo should work like on a toplevel surface
    subsurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    for (auto &frame : frames) {
        frame = std::make_unique<WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
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
            QSignalSpy(frames[0].get(), &WpPresentationFeedback::presented),
            QSignalSpy(frames[1].get(), &WpPresentationFeedback::presented),
            QSignalSpy(frames[2].get(), &WpPresentationFeedback::presented),
        };
        for (auto &spy : spies) {
            QVERIFY(spy.wait(100));
        }
    }

    // when the surface is synchronized though, fifo should be ignored
    subsurface->setMode(KWayland::Client::SubSurface::Mode::Synchronized);
    for (auto &frame : frames) {
        frame = std::make_unique<WpPresentationFeedback>(Test::presentationTime()->feedback(*surface));
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
            QSignalSpy(frames[0].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[1].get(), &WpPresentationFeedback::discarded),
            QSignalSpy(frames[2].get(), &WpPresentationFeedback::discarded),
        };
        for (auto &spy : spies) {
            QVERIFY(spy.count() || spy.wait(100));
        }
    }
}
}

WAYLANDTEST_MAIN(KWin::FifoTest)
#include "test_fifo.moc"
