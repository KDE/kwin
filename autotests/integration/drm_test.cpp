/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "compositor.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "core/outputlayer.h"
#include "core/renderbackend.h"
#include "kwin_wayland_test.h"
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

void DrmTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PresentationTime
                                         | Test::AdditionalWaylandInterface::Seat
                                         | Test::AdditionalWaylandInterface::CursorShapeV1));
}

void DrmTest::initTestCase()
{
#ifdef FORCE_DRM_LEGACY
    qputenv("KWIN_DRM_NO_AMS", "1");
#endif

    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();

    const auto allOutputs = kwinApp()->outputBackend()->outputs();
    QVERIFY(!allOutputs.isEmpty());

    // Create a simple setup that's useful for most tests.
    OutputConfiguration cfg;
    cfg.changeSet(allOutputs.front())->enabled = true;
    cfg.changeSet(allOutputs.front())->scale = 1;
    for (Output *output : allOutputs | std::views::drop(1)) {
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

    Output *output = kwinApp()->outputBackend()->outputs().front();
    window.m_window->move(output->geometryF().center());

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

    Output *output = kwinApp()->outputBackend()->outputs().front();
    window.m_window->move(output->geometryF().center());

    for (int i = 0; i < 5; i++) {
        QVERIFY(window.presentWait());
    }
}

void DrmTest::testCursorLayer()
{
    // verify that the cursor layer is used, if it is available
    uint32_t time = 0;
    Output *output = kwinApp()->outputBackend()->outputs().front();

    const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
    if (layers.size() == 1) {
        QSKIP("The driver only advertises a primary plane");
    }

    Test::XdgToplevelWindow window;
    QVERIFY(window.show());
    window.m_window->move(output->geometryF().center());

    // if there's a visible cursor, a non-primary plane should be used to present it
    std::unique_ptr<KWayland::Client::Pointer> pointer{Test::waylandSeat()->createPointer()};
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    Test::pointerMotion(output->geometryF().center(), time++);
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
    QSKIP("Need the test client to attach a dmabuf and implement dmabuf feedback first...");
    uint32_t time = 0;
    Output *output = kwinApp()->outputBackend()->outputs().front();

    const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
    if (layers.size() == 1) {
        QSKIP("The driver only advertises a primary plane");
    }

    Test::XdgToplevelWindow window{[](Test::XdgToplevel *toplevel) {
        toplevel->set_fullscreen(nullptr);
    }};
    QVERIFY(window.show(output->pixelSize()));
    window.m_window->move(output->geometryF().center());

    // if there's a visible cursor, a non-primary plane should be used to present it
    std::unique_ptr<KWayland::Client::Pointer> pointer{Test::waylandSeat()->createPointer()};
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    Test::pointerMotion(output->geometryF().center(), time++);
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
    QSKIP("Need the test client to attach a dmabuf and implement dmabuf feedback first...");
    uint32_t time = 0;
    Output *output = kwinApp()->outputBackend()->outputs().front();

    const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
    if (layers.size() < 3) {
        QSKIP("The driver doesn't advertise an overlay plane");
    }

    Test::XdgToplevelWindow window;
    QVERIFY(window.show());
    window.m_window->move(output->geometryF().center());

    // if there's a visible cursor, a non-primary plane should be used to present it
    std::unique_ptr<KWayland::Client::Pointer> pointer{Test::waylandSeat()->createPointer()};
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    Test::pointerMotion(output->geometryF().center(), time++);
    QVERIFY(enteredSpy.wait());
    auto cursorShapeDevice = Test::createCursorShapeDeviceV1(pointer.get());
    cursorShapeDevice->set_shape(enteredSpy.last().at(0).value<quint32>(), Test::CursorShapeDeviceV1::shape_default);

    // emulate a quickly updating surface
    for (int i = 0; i < 100; i++) {
        window.m_surface->damage(QRect(0, 0, 10, 10));
        QVERIFY(window.presentWait());
    }

    const int enabledLayers = std::ranges::count_if(layers, [](OutputLayer *layer) {
        return layer->isEnabled();
    });
    QCOMPARE(enabledLayers, 3);
}
}

WAYLAND_DRM_TEST_MAIN(KWin::DrmTest)

#include "drm_test.moc"
