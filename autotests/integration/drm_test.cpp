/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "compositor.h"
#include "core/drmdevice.h"
#include "core/graphicsbuffer.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "core/outputlayer.h"
#include "core/renderbackend.h"
#include "kwin_wayland_test.h"
#include "scene/surfaceitem.h"
#include "wayland-client/linuxdmabuf.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <ranges>

namespace KWin
{

static const QString s_socketName = QStringLiteral("test_drm");

class DrmTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void initTestCase();
    void cleanup();

    void testModesets();
    void testPresentation();
    void testCursorLayer();
    void testDirectScanout();
    void testOverlay();
};

// TODO move this stuff into XdgToplevelWindow instead?
class DmabufWindow : public QObject
{
    Q_OBJECT
public:
    explicit DmabufWindow(std::function<void(Test::XdgToplevel *toplevel)> setup = [](Test::XdgToplevel *toplevel) { })
        : m_surface(Test::createSurface())
        , m_shellSurface(Test::createXdgToplevelSurface(m_surface.get(), setup))
        , m_device(DrmDevice::open(Test::linuxDmabuf()->mainDevice()))
        , m_surfaceFeedback(Test::linuxDmabuf()->getSurfaceFeedback(*m_surface))
    {
        connect(m_surfaceFeedback.get(), &WaylandClient::LinuxDmabufFeedbackV1::changed, this, &DmabufWindow::reactToDmabufFeedback);
    }

    bool renderAndWaitForShown(const QSize &size)
    {
        const auto formats = m_surfaceFeedback->formats().empty() ? Test::linuxDmabuf()->formats() : m_surfaceFeedback->formats();
        m_buffer = m_device->allocator()->allocate(GraphicsBufferOptions{
            .size = size,
            .format = DRM_FORMAT_XRGB8888,
            .modifiers = formats[DRM_FORMAT_XRGB8888],
            .software = false,
        });
        auto wlbuffer = Test::linuxDmabuf()->importBuffer(m_buffer.buffer());
        m_surface->attachBuffer(wlbuffer);
        m_surface->damage(QRect(QPoint(0, 0), size));
        m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
        wl_buffer_destroy(wlbuffer);
        m_window = Test::waitForWaylandWindowShown();
        return m_window;
    }

    bool presentWait()
    {
        wl_buffer *wlbuffer = nullptr;
        if (m_needsRealloc) {
            const auto tranches = m_surfaceFeedback->tranches();
            for (const auto &tranche : tranches) {
                if (!tranche.formats.contains(m_buffer->dmabufAttributes()->format)) {
                    continue;
                }
                // scanout flag is currently implicit in GraphicsBufferOptions
                m_buffer = m_device->allocator()->allocate(GraphicsBufferOptions{
                    .size = m_buffer->size(),
                    .format = m_buffer->dmabufAttributes()->format,
                    .modifiers = tranche.formats[m_buffer->dmabufAttributes()->format],
                    .software = false,
                });
                auto wlbuffer = Test::linuxDmabuf()->importBuffer(m_buffer.buffer());
                m_surface->attachBuffer(wlbuffer);
                m_surface->damage(QRect(QPoint(0, 0), m_buffer->size()));
                m_needsRealloc = false;
                break;
            }
        }
        auto feedback = std::make_unique<Test::WpPresentationFeedback>(Test::presentationTime()->feedback(*m_surface));
        m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
        if (wlbuffer) {
            wl_buffer_destroy(wlbuffer);
        }
        QSignalSpy spy(feedback.get(), &Test::WpPresentationFeedback::presented);
        return spy.wait();
    }

    void reactToDmabufFeedback()
    {
        m_needsRealloc = bool(m_buffer);
    }

    std::unique_ptr<KWayland::Client::Surface> m_surface;
    std::unique_ptr<Test::XdgToplevel> m_shellSurface;
    std::unique_ptr<DrmDevice> m_device;
    std::unique_ptr<WaylandClient::LinuxDmabufFeedbackV1> m_surfaceFeedback;
    GraphicsBufferRef m_buffer;
    bool m_needsRealloc = false;
    Window *m_window = nullptr;
};

void DrmTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PresentationTime
                                         | Test::AdditionalWaylandInterface::Seat
                                         | Test::AdditionalWaylandInterface::CursorShapeV1
                                         | Test::AdditionalWaylandInterface::LinuxDmabuf));
}

void DrmTest::initTestCase()
{
    if (!Test::primaryNodeAvailable()) {
        QSKIP("no primary node available");
        return;
    }

#ifdef FORCE_DRM_LEGACY
    qputenv("KWIN_DRM_NO_AMS", "1");
#endif
    // make sure overlays are allowed
    qputenv("KWIN_USE_OVERLAYS", "1");

    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();

    const auto allOutputs = kwinApp()->outputBackend()->outputs();
    QVERIFY(!allOutputs.isEmpty());

    // Create a simple setup that's useful for most tests.
    OutputConfiguration cfg;
    cfg.changeSet(allOutputs.front())->enabled = true;
    cfg.changeSet(allOutputs.front())->scaleSetting = 1;
    for (BackendOutput *output : allOutputs | std::views::drop(1)) {
        cfg.changeSet(output)->enabled = false;
    }
    QCOMPARE(workspace()->applyOutputConfiguration(cfg), OutputConfigurationError::None);
}

void DrmTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void DrmTest::testModesets()
{
    // verify that modesetting works as expected
    // and doesn't cause crashes or issues with presentation time
    Test::XdgToplevelWindow window;
    QVERIFY(window.show());

    BackendOutput *output = kwinApp()->outputBackend()->outputs().front();
    window.m_window->move(output->position());

    // verify that we can cycle through modes
    for (const auto &mode : output->modes()) {
        OutputConfiguration cfg;
        cfg.changeSet(output)->mode = mode;
        QCOMPARE(workspace()->applyOutputConfiguration(cfg), OutputConfigurationError::None);
        QVERIFY(window.presentWait());
    }
}

void DrmTest::testPresentation()
{
    // verify that normal presentation works as expected
    Test::XdgToplevelWindow window;
    QVERIFY(window.show());

    BackendOutput *output = kwinApp()->outputBackend()->outputs().front();
    window.m_window->move(output->position());

    for (int i = 0; i < 5; i++) {
        QVERIFY(window.presentWait());
    }
}

void DrmTest::testCursorLayer()
{
    // verify that the cursor layer is used, if it is available
    uint32_t time = 0;
    BackendOutput *output = kwinApp()->outputBackend()->outputs().front();

    const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
    if (layers.size() == 1) {
        QSKIP("The driver only advertises a primary plane");
    }

    Test::XdgToplevelWindow window;
    QVERIFY(window.show());
    window.m_window->move(output->position());

    // if there's a visible cursor, a non-primary plane should be used to present it
    std::unique_ptr<KWayland::Client::Pointer> pointer{Test::waylandSeat()->createPointer()};
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    Test::pointerMotion(window.m_window->frameGeometry().center(), time++);
    QVERIFY(enteredSpy.wait());
    auto cursorShapeDevice = Test::createCursorShapeDeviceV1(pointer.get());
    cursorShapeDevice->set_shape(enteredSpy.last().at(0).value<quint32>(), Test::CursorShapeDeviceV1::shape_default);

    QVERIFY(window.presentWait());

    const int enabledLayers = std::ranges::count_if(layers, [](OutputLayer *layer) {
        return layer->isEnabled();
    });
    QCOMPARE(enabledLayers, 2);
}

void DrmTest::testDirectScanout()
{
    QVERIFY2(Test::linuxDmabuf(), "This test needs dmabuf support");
    uint32_t time = 0;
    BackendOutput *output = kwinApp()->outputBackend()->outputs().front();

    const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
    if (layers.size() == 1) {
        QSKIP("The driver only advertises a primary plane");
    }

    DmabufWindow window{[](Test::XdgToplevel *toplevel) {
        toplevel->set_fullscreen(nullptr);
    }};
    QVERIFY(window.renderAndWaitForShown(output->pixelSize()));
    window.m_window->move(output->position());

    // if there's a visible cursor, a non-primary plane should be used to present it
    std::unique_ptr<KWayland::Client::Pointer> pointer{Test::waylandSeat()->createPointer()};
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    Test::pointerMotion(window.m_window->frameGeometry().center(), time++);
    QVERIFY(enteredSpy.wait());
    auto cursorShapeDevice = Test::createCursorShapeDeviceV1(pointer.get());
    cursorShapeDevice->set_shape(enteredSpy.last().at(0).value<quint32>(), Test::CursorShapeDeviceV1::shape_default);

    QVERIFY(window.presentWait());

    const int enabledLayers = std::ranges::count_if(layers, [](OutputLayer *layer) {
        return layer->isEnabled();
    });
    QCOMPARE(enabledLayers, 2);
    // TODO check here that the client buffer is actually used on the plane
}

void DrmTest::testOverlay()
{
    QVERIFY2(Test::linuxDmabuf(), "This test needs dmabuf support");
    uint32_t time = 0;
    BackendOutput *output = kwinApp()->outputBackend()->outputs().front();

    const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
    if (layers.size() < 3) {
        QSKIP("The driver doesn't advertise an overlay plane");
    }

    DmabufWindow window;
    QVERIFY(window.renderAndWaitForShown(QSize(100, 100)));
    window.m_window->move(output->position());

    // if there's a visible cursor, a non-primary plane should be used to present it
    std::unique_ptr<KWayland::Client::Pointer> pointer{Test::waylandSeat()->createPointer()};
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    Test::pointerMotion(window.m_window->frameGeometry().center(), time++);
    QVERIFY(enteredSpy.wait());
    auto cursorShapeDevice = Test::createCursorShapeDeviceV1(pointer.get());
    cursorShapeDevice->set_shape(enteredSpy.last().at(0).value<quint32>(), Test::CursorShapeDeviceV1::shape_default);

    // emulate a quickly updating surface
    for (int i = 0; i < 100; i++) {
        window.m_surface->damageBuffer(QRect(QPoint(), QSize(100, 100)));
        QVERIFY(window.presentWait());
    }
    QCOMPARE_LE(window.m_window->surfaceItem()->frameTimeEstimation(), std::chrono::milliseconds(50));

    const int enabledLayers = std::ranges::count_if(layers, [](OutputLayer *layer) {
        return layer->isEnabled();
    });
    QCOMPARE(enabledLayers, 3);
}
}

WAYLAND_DRM_TEST_MAIN(KWin::DrmTest)

#include "drm_test.moc"
