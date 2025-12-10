/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "backends/drm/drm_gpu.h"
#include "backends/drm/drm_object.h"
#include "backends/drm/drm_output.h"
#include "backends/drm/drm_pointer.h"
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
#include <xf86drmMode.h>

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
    void testOverlay_data();
    void testOverlay();
    void testDpms();
};

struct DrmCrtcState
{
    bool active;
    QSize modeSize;
    uint32_t refreshRateMilliHertz;

    static std::optional<DrmCrtcState> read(int fd, uint32_t id);
};
std::optional<DrmCrtcState> DrmCrtcState::read(int fd, uint32_t id)
{
    DrmUniquePtr<drmModeCrtc> crtc(drmModeGetCrtc(fd, id));
    if (!crtc) {
        return std::nullopt;
    }
    auto props = DrmObject::queryProperties(fd, id, DRM_MODE_OBJECT_CRTC);
    const auto active = props.takeProperty(QByteArrayLiteral("ACTIVE"));
    if (!active.has_value()) {
        return std::nullopt;
    }
    return DrmCrtcState{
        .active = active->second == 1,
        .modeSize = crtc->mode_valid ? QSize(crtc->mode.hdisplay, crtc->mode.vdisplay) : QSize(),
        .refreshRateMilliHertz = crtc->mode_valid ? DrmConnector::refreshRateForMode(&crtc->mode) : 0,
    };
}

struct DrmPlaneState
{
    uint32_t crtcId;
    QRect destinationRect;
    uint32_t frambufferId;
    std::array<uint32_t, 4> framebufferGemNames;

    static std::optional<DrmPlaneState> read(int fd, uint32_t id);
};
std::optional<DrmPlaneState> DrmPlaneState::read(int fd, uint32_t id)
{
    auto props = DrmObject::queryProperties(fd, id, DRM_MODE_OBJECT_PLANE);
    const auto crtcId = props.takeProperty(QByteArrayLiteral("CRTC_ID"));
    const auto crtcX = props.takeProperty(QByteArrayLiteral("CRTC_X"));
    const auto crtcY = props.takeProperty(QByteArrayLiteral("CRTC_Y"));
    const auto crtcW = props.takeProperty(QByteArrayLiteral("CRTC_W"));
    const auto crtcH = props.takeProperty(QByteArrayLiteral("CRTC_H"));
    const auto fbId = props.takeProperty(QByteArrayLiteral("FB_ID"));
    if (!crtcId || !crtcX || !crtcY || !crtcW || !crtcH || !fbId) {
        return std::nullopt;
    }
    std::array<uint32_t, 4> framebufferGemNames = {0};
    if (auto ptr = drmModeGetFB2(fd, fbId->second)) {
        std::unordered_set<uint32_t> handles;
        const auto guard = qScopeGuard([&]() {
            // NOTE that drmModeGetFB2 always creates new handles
            for (uint32_t handle : handles) {
                drmCloseBufferHandle(fd, handle);
            }
            drmModeFreeFB2(ptr);
        });
        for (int i = 0; i < 4; i++) {
            if (ptr->handles[i] == 0) {
                break;
            }
            handles.insert(ptr->handles[i]);
            drm_gem_flink flink{
                .handle = ptr->handles[i],
                .name = 0,
            };
            if (drmIoctl(fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
                return std::nullopt;
            }
            framebufferGemNames[i] = flink.name;
        }
    }
    return DrmPlaneState{
        .crtcId = uint32_t(crtcId->second),
        .destinationRect = QRect(crtcX->second, crtcY->second, crtcW->second, crtcH->second),
        .frambufferId = uint32_t(fbId->second),
        .framebufferGemNames = framebufferGemNames,
    };
}

struct DrmConnectorState
{
    uint32_t crtcId;

    static std::optional<DrmConnectorState> read(int fd, uint32_t id);
};
std::optional<DrmConnectorState> DrmConnectorState::read(int fd, uint32_t id)
{
    auto props = DrmObject::queryProperties(fd, id, DRM_MODE_OBJECT_CONNECTOR);
    const auto crtcId = props.takeProperty(QByteArrayLiteral("CRTC_ID"));
    if (!crtcId.has_value()) {
        return std::nullopt;
    }
    return DrmConnectorState{
        .crtcId = uint32_t(crtcId->second),
    };
}

struct DrmState
{
    std::unordered_map<uint32_t, DrmCrtcState> crtcs;
    std::unordered_map<uint32_t, DrmPlaneState> planes;
    std::unordered_map<uint32_t, DrmConnectorState> connectors;

    static std::optional<DrmState> read(int fd);
};
std::optional<DrmState> DrmState::read(int fd)
{
    DrmUniquePtr<drmModeRes> resources(drmModeGetResources(fd));
    DrmUniquePtr<drmModePlaneRes> planeResources(drmModeGetPlaneResources(fd));
    if (!resources || !planeResources) {
        return std::nullopt;
    }
    DrmState ret;
    for (uint32_t id : std::span(resources->connectors, resources->count_connectors)) {
        const auto state = DrmConnectorState::read(fd, id);
        if (!state) {
            return std::nullopt;
        }
        ret.connectors[id] = *state;
    }
    for (uint32_t id : std::span(resources->crtcs, resources->count_crtcs)) {
        const auto state = DrmCrtcState::read(fd, id);
        if (!state) {
            return std::nullopt;
        }
        ret.crtcs[id] = *state;
    }
    for (uint32_t id : std::span(planeResources->planes, planeResources->count_planes)) {
        const auto state = DrmPlaneState::read(fd, id);
        if (!state) {
            return std::nullopt;
        }
        ret.planes[id] = *state;
    }
    return ret;
}

struct DrmOutputState
{
    DrmConnectorState connector;
    std::optional<DrmCrtcState> crtc;
    std::unordered_map<uint32_t, DrmPlaneState> planes;

    static std::optional<DrmOutputState> read(BackendOutput *output);
};
std::optional<DrmOutputState> DrmOutputState::read(BackendOutput *output)
{
    const auto drmOutput = static_cast<DrmOutput *>(output);
    const auto allState = DrmState::read(drmOutput->connector()->gpu()->fd());
    if (!allState) {
        return std::nullopt;
    }
    const auto connector = allState->connectors.find(drmOutput->connector()->id());
    if (connector == allState->connectors.end()) {
        return std::nullopt;
    }
    DrmOutputState ret;
    ret.connector = connector->second;
    if (connector->second.crtcId == 0) {
        return ret;
    }
    const auto crtc = allState->crtcs.find(connector->second.crtcId);
    if (crtc == allState->crtcs.end()) {
        return std::nullopt;
    }
    ret.crtc = crtc->second;
    ret.planes = allState->planes | std::views::filter([crtcId = connector->second.crtcId](const auto &state) {
        return state.second.crtcId == crtcId;
    }) | std::ranges::to<std::unordered_map>();
    return ret;
}

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

    const auto state = DrmOutputState::read(allOutputs.front());
    QVERIFY(state.has_value());
    QVERIFY(state->crtc.has_value());

    for (BackendOutput *output : allOutputs | std::views::drop(1)) {
        const auto state = DrmOutputState::read(output);
        QVERIFY(state.has_value());
        QVERIFY(!state->crtc.has_value());
    }
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

        const auto state = DrmOutputState::read(output);
        QVERIFY(state);
        QVERIFY(state->crtc.has_value());
        QCOMPARE(state->crtc->modeSize, mode->size());
        QCOMPARE(state->crtc->refreshRateMilliHertz, mode->refreshRate());
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

#ifndef FORCE_DRM_LEGACY
    // NOTE that this *should* in principle work, but in practice doesn't work
    // with all drivers in legacy modesetting. VKMS doesn't report the cursor
    // plane as enabled if it was enabled through the legacy interface
    const auto state = DrmOutputState::read(output);
    QVERIFY(state);
    QVERIFY(state->crtc.has_value());
    auto enabledPlanes = state->planes | std::views::values | std::views::filter([](const auto &state) {
        return state.crtcId != 0;
    });
    QCOMPARE(std::distance(enabledPlanes.begin(), enabledPlanes.end()), 2);
#endif
}

static std::array<uint32_t, 4> gemNames(int fd, const DmaBufAttributes *attributes)
{
    std::array<uint32_t, 4> ret = {0, 0, 0, 0};
    for (int i = 0; i < attributes->planeCount; i++) {
        uint32_t handle = 0;
        if (drmPrimeFDToHandle(fd, attributes->fd[i].get(), &handle) != 0) {
            return {0};
        }
        drm_gem_flink flink{
            .handle = handle,
            .name = 0,
        };
        if (drmIoctl(fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
            return {0};
        }
        ret[i] = flink.name;
        drmCloseBufferHandle(fd, handle);
    }
    return ret;
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

    // if there's a visible cursor, a non-primary plane should be used to present it
    std::unique_ptr<KWayland::Client::Pointer> pointer{Test::waylandSeat()->createPointer()};
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    Test::pointerMotion(window.m_window->frameGeometry().center(), time++);
    QVERIFY(enteredSpy.wait());
    auto cursorShapeDevice = Test::createCursorShapeDeviceV1(pointer.get());
    cursorShapeDevice->set_shape(enteredSpy.last().at(0).value<quint32>(), Test::CursorShapeDeviceV1::shape_default);

    for (int i = 0; i < 10; i++) {
        QVERIFY(window.presentWait());
    }

    const int enabledLayers = std::ranges::count_if(layers, [](OutputLayer *layer) {
        return layer->isEnabled();
    });
    QCOMPARE(enabledLayers, 2);

    const auto state = DrmOutputState::read(output);
    QVERIFY(state);
    QVERIFY(state->crtc.has_value());
#ifndef FORCE_DRM_LEGACY
    auto enabledPlanes = state->planes | std::views::values | std::views::filter([](const DrmPlaneState &state) {
        return state.crtcId != 0;
    });
    // NOTE that this *should* in principle work, but in practice doesn't work
    // with all drivers in legacy modesetting. VKMS doesn't report the cursor
    // plane as enabled if it was enabled through the legacy interface
    QCOMPARE(std::distance(enabledPlanes.begin(), enabledPlanes.end()), 2);

    // TODO this bit is supposed to work, find out why it doesn't
    const int gpuFd = static_cast<DrmOutput *>(output)->connector()->gpu()->fd();
    const auto framebufferGemNames = gemNames(gpuFd, window.m_buffer->dmabufAttributes());
    QVERIFY(std::ranges::any_of(enabledPlanes, [&framebufferGemNames](const DrmPlaneState &state) {
        return state.framebufferGemNames == framebufferGemNames;
    }));
#endif
}

void DrmTest::testOverlay_data()
{
    QTest::addColumn<bool>("occluded");

    QTest::addRow("overlay") << false;
    QTest::addRow("underlay") << true;
    // TODO also add a test case for occluded == false + SSD with rounded corners?
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

    Test::XdgToplevelWindow dummy;
    QVERIFY(dummy.show());
    dummy.m_window->move(output->position() + QPoint(50, 50));

    DmabufWindow window;
    QVERIFY(window.renderAndWaitForShown(QSize(100, 100)));
    window.m_window->move(output->position());

    QFETCH(bool, occluded);
    if (occluded) {
        workspace()->raiseWindow(dummy.m_window);
    }

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

    const auto state = DrmOutputState::read(output);
    QVERIFY(state);
    QVERIFY(state->crtc.has_value());
    auto enabledPlanes = state->planes | std::views::values | std::views::filter([](const DrmPlaneState &state) {
        return state.crtcId != 0;
    });
    QCOMPARE(std::distance(enabledPlanes.begin(), enabledPlanes.end()), 3);

    const auto sceneIt = std::ranges::find_if(enabledPlanes, [output](const DrmPlaneState &state) {
        return state.destinationRect == QRect(QPoint(), output->modeSize());
    });
    QVERIFY(sceneIt != enabledPlanes.end());
    const auto overlayIt = std::ranges::find_if(enabledPlanes, [&window](const DrmPlaneState &state) {
        return state.destinationRect == window.m_window->frameGeometry().toRect();
    });
    QVERIFY(overlayIt != enabledPlanes.end());
    // TODO uncomment this once vkms supports the zpos property
    // QCOMPARE_GE((*overlayIt).zpos, (*sceneIt).zpos);

    const int gpuFd = static_cast<DrmOutput *>(output)->connector()->gpu()->fd();
    const auto framebufferGemNames = gemNames(gpuFd, window.m_buffer->dmabufAttributes());
    QVERIFY(std::ranges::any_of(enabledPlanes, [&framebufferGemNames](const DrmPlaneState &state) {
        return state.framebufferGemNames == framebufferGemNames;
    }));
}

void DrmTest::testDpms()
{
    // the output should be enabled
    BackendOutput *output = kwinApp()->outputBackend()->outputs().front();
    auto state = DrmOutputState::read(output);
    QVERIFY(state.has_value());
    QVERIFY(state->crtc.has_value());

    Test::XdgToplevelWindow dummy;
    QVERIFY(dummy.show());

    workspace()->requestDpmsState(Workspace::DpmsState::Off);
    QSignalSpy stateChange(workspace(), &Workspace::dpmsStateChanged);
    QVERIFY(stateChange.wait());
#ifndef FORCE_DRM_LEGACY
    QCOMPARE(workspace()->dpmsState(), Workspace::DpmsState::TurningOff);
    QVERIFY(stateChange.wait());
#endif
    QCOMPARE(workspace()->dpmsState(), Workspace::DpmsState::Off);

    state = DrmOutputState::read(output);
    QVERIFY(state.has_value());
    QVERIFY(!state->crtc.has_value() || !state->crtc->active);

    workspace()->requestDpmsState(Workspace::DpmsState::On);
    QVERIFY(dummy.presentWait());

    state = DrmOutputState::read(output);
    QVERIFY(state.has_value());
    QVERIFY(state->crtc.has_value());
    QVERIFY(state->crtc->active);
}

}

WAYLAND_DRM_TEST_MAIN(KWin::DrmTest)

#include "drm_test.moc"
