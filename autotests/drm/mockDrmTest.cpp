/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSignalSpy>
#include <QSize>
#include <QTest>

#include "mock_drm.h"

#include "core/outputlayer.h"
#include "core/session.h"
#include "drm_backend.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_plane.h"
#include "drm_pointer.h"
#include "qpainter/qpainterbackend.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland/drmlease_v1.h"
#include "wayland/drmlease_v1_p.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/utsname.h>

using namespace KWin;

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_OPENGL, "kwin_scene_opengl", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_QPAINTER, "kwin_scene_qpainter", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_VIRTUALKEYBOARD, "kwin_virtualkeyboard", QtWarningMsg)

static std::unique_ptr<MockGpu> findPrimaryDevice(int crtcCount)
{
#if !HAVE_LIBDRM_FAUX
#if defined(Q_OS_LINUX)
    // Workaround for libdrm being unaware of faux bus.
    if (qEnvironmentVariableIsSet("CI")) {
        int fd = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);
        if (fd == -1) {
            return nullptr;
        } else {
            return std::make_unique<MockGpu>(fd, "/dev/dri/card1", crtcCount);
        }
    }
#endif
#endif

    const int deviceCount = drmGetDevices2(0, nullptr, 0);
    if (deviceCount <= 0) {
        return nullptr;
    }

    QList<drmDevice *> devices(deviceCount);
    if (drmGetDevices2(0, devices.data(), devices.size()) < 0) {
        return nullptr;
    }
    auto deviceCleanup = qScopeGuard([&devices]() {
        drmFreeDevices(devices.data(), devices.size());
    });

    for (drmDevice *device : std::as_const(devices)) {
        if (device->available_nodes & (1 << DRM_NODE_PRIMARY)) {
            int fd = open(device->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC);
            if (fd != -1) {
                return std::make_unique<MockGpu>(fd, device->nodes[DRM_NODE_PRIMARY], crtcCount);
            }
        }
    }

    return nullptr;
}

class DrmTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testAmsDetection();
    void testOutputDetection_data();
    void testOutputDetection();
    void testZeroModesHandling();
    void testModeGeneration_data();
    void testModeGeneration();
    void testConnectorLifetime();
    void testModeset_data();
    void testModeset();
    void testVrrChange();
    void testLeaseDisconnectReconnect();
    void testLeaseAvailableAfterMasterToggleAndClientExit();
    void testLeaseAvailableWhenClientExitsBeforeQueuedReoffer();
};

static void verifyCleanup(MockGpu *mockGpu)
{
    QVERIFY(mockGpu->drmConnectors.isEmpty());
    QVERIFY(mockGpu->drmEncoders.isEmpty());
    QVERIFY(mockGpu->drmCrtcs.isEmpty());
    QVERIFY(mockGpu->drmPlanes.isEmpty());
    QVERIFY(mockGpu->drmPlaneRes.isEmpty());
    QVERIFY(mockGpu->fbs.isEmpty());
    QVERIFY(mockGpu->drmProps.isEmpty());
    QVERIFY(mockGpu->drmObjectProperties.isEmpty());
    QVERIFY(mockGpu->drmPropertyBlobs.isEmpty());
}

void DrmTest::testAmsDetection()
{
    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());

    // gpu without planes should use legacy mode
    {
        const auto mockGpu = findPrimaryDevice(0);
        auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));
        QVERIFY(!gpu->atomicModeSetting());
    }

    // gpu with planes should use AMS
    {
        const auto mockGpu = findPrimaryDevice(0);
        mockGpu->planes << std::make_shared<MockPlane>(mockGpu.get(), PlaneType::Primary, 0);
        auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));
        gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));
        QVERIFY(gpu->atomicModeSetting());
    }

    // but not if the kernel doesn't allow it
    {
        const auto mockGpu = findPrimaryDevice(0);
        mockGpu->deviceCaps[MOCKDRM_DEVICE_CAP_ATOMIC] = 0;
        auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));
        QVERIFY(!gpu->atomicModeSetting());
        gpu.reset();
        verifyCleanup(mockGpu.get());
    }
}

void DrmTest::testOutputDetection_data()
{
    QTest::addColumn<bool>("atomicModeSetting");

    QTest::addRow("AMS") << true;
    QTest::addRow("legacy") << false;
}

void DrmTest::testOutputDetection()
{
    const auto mockGpu = findPrimaryDevice(5);
    QFETCH(bool, atomicModeSetting);
    mockGpu->deviceCaps[MOCKDRM_DEVICE_CAP_ATOMIC] = atomicModeSetting;

    const auto one = std::make_shared<MockConnector>(mockGpu.get());
    const auto two = std::make_shared<MockConnector>(mockGpu.get());
    const auto vr = std::make_shared<MockConnector>(mockGpu.get(), true);
    mockGpu->connectors.push_back(one);
    mockGpu->connectors.push_back(two);
    mockGpu->connectors.push_back(vr);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));
    QVERIFY(gpu->updateOutputs());

    // 3 outputs should be detected, one of them non-desktop
    const auto outputs = gpu->drmOutputs();
    QCOMPARE(outputs.size(), 3);
    const auto vrOutput = std::find_if(outputs.begin(), outputs.end(), [](const auto &output) {
        return output->isNonDesktop();
    });
    QVERIFY(vrOutput != outputs.end());
    QVERIFY(static_cast<DrmOutput *>(*vrOutput)->connector()->id() == vr->id);

    // test hotunplugging
    mockGpu->connectors.removeOne(one);
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 2);

    // test hotplugging
    mockGpu->connectors.push_back(one);
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 3);

    // connector state changing to disconnected should count as a hotunplug
    one->connection = DRM_MODE_DISCONNECTED;
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 2);

    // don't crash if all connectors are disconnected
    two->connection = DRM_MODE_DISCONNECTED;
    vr->connection = DRM_MODE_DISCONNECTED;
    QVERIFY(gpu->updateOutputs());
    QVERIFY(gpu->drmOutputs().empty());

    gpu.reset();
    verifyCleanup(mockGpu.get());
}

void DrmTest::testZeroModesHandling()
{
    const auto mockGpu = findPrimaryDevice(5);

    const auto conn = std::make_shared<MockConnector>(mockGpu.get());
    mockGpu->connectors.push_back(conn);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));

    // connector with zero modes should be ignored
    conn->modes.clear();
    QVERIFY(gpu->updateOutputs());
    QVERIFY(gpu->drmOutputs().empty());

    // once it has modes, it should be detected
    conn->addMode(1920, 1080, 60);
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 1);

    // if an update says it has no modes anymore but it's still connected, ignore that
    conn->modes.clear();
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 1);
    QVERIFY(!gpu->drmOutputs().constFirst()->modes().empty());

    gpu.reset();
    verifyCleanup(mockGpu.get());
}

// because QFETCH as a macro doesn't like commas in the type name
using Mode = std::pair<QSize, uint32_t>;

void DrmTest::testModeGeneration_data()
{
    QTest::addColumn<Mode>("nativeMode");
    QTest::addColumn<QList<Mode>>("expectedModes");

    QTest::newRow("2160p") << std::make_pair(QSize(3840, 2160), 60u) << QList<Mode>{
        std::make_pair(QSize(1600, 1200), 60),
        std::make_pair(QSize(1280, 1024), 60),
        std::make_pair(QSize(1024, 768), 60),
        std::make_pair(QSize(2560, 1600), 60),
        std::make_pair(QSize(1920, 1200), 60),
        std::make_pair(QSize(1280, 800), 60),
        std::make_pair(QSize(3840, 2160), 60),
        std::make_pair(QSize(3200, 1800), 60),
        std::make_pair(QSize(2880, 1620), 60),
        std::make_pair(QSize(2560, 1440), 60),
        std::make_pair(QSize(1920, 1080), 60),
        std::make_pair(QSize(1600, 900), 60),
        std::make_pair(QSize(1368, 768), 60),
        std::make_pair(QSize(1280, 720), 60),
    };

    QTest::newRow("2160p") << std::make_pair(QSize(3840, 2160), 120u) << QList<Mode>{
        std::make_pair(QSize(1600, 1200), 120),
        std::make_pair(QSize(1280, 1024), 120),
        std::make_pair(QSize(1024, 768), 120),
        std::make_pair(QSize(2560, 1600), 120),
        std::make_pair(QSize(1920, 1200), 120),
        std::make_pair(QSize(1280, 800), 120),
        std::make_pair(QSize(3840, 2160), 120),
        std::make_pair(QSize(3200, 1800), 120),
        std::make_pair(QSize(2880, 1620), 120),
        std::make_pair(QSize(2560, 1440), 120),
        std::make_pair(QSize(1920, 1080), 120),
        std::make_pair(QSize(1600, 900), 120),
        std::make_pair(QSize(1368, 768), 120),
        std::make_pair(QSize(1280, 720), 120),
        std::make_pair(QSize(1600, 1200), 60),
        std::make_pair(QSize(1280, 1024), 60),
        std::make_pair(QSize(1024, 768), 60),
        std::make_pair(QSize(2560, 1600), 60),
        std::make_pair(QSize(1920, 1200), 60),
        std::make_pair(QSize(1280, 800), 60),
        std::make_pair(QSize(3840, 2160), 60),
        std::make_pair(QSize(3200, 1800), 60),
        std::make_pair(QSize(2880, 1620), 60),
        std::make_pair(QSize(2560, 1440), 60),
        std::make_pair(QSize(1920, 1080), 60),
        std::make_pair(QSize(1600, 900), 60),
        std::make_pair(QSize(1368, 768), 60),
        std::make_pair(QSize(1280, 720), 60),
    };

    QTest::newRow("1440p") << std::make_pair(QSize(2560, 1440), 60u) << QList<Mode>{
        std::make_pair(QSize(1600, 1200), 60),
        std::make_pair(QSize(1280, 1024), 60),
        std::make_pair(QSize(1024, 768), 60),
        std::make_pair(QSize(1920, 1200), 60),
        std::make_pair(QSize(1280, 800), 60),
        std::make_pair(QSize(2560, 1440), 60),
        std::make_pair(QSize(1920, 1080), 60),
        std::make_pair(QSize(1600, 900), 60),
        std::make_pair(QSize(1368, 768), 60),
        std::make_pair(QSize(1280, 720), 60),
    };
    QTest::newRow("1080p") << std::make_pair(QSize(1920, 1080), 60u) << QList<Mode>{
        std::make_pair(QSize(1280, 1024), 60),
        std::make_pair(QSize(1024, 768), 60),
        std::make_pair(QSize(1280, 800), 60),
        std::make_pair(QSize(1920, 1080), 60),
        std::make_pair(QSize(1600, 900), 60),
        std::make_pair(QSize(1368, 768), 60),
        std::make_pair(QSize(1280, 720), 60),
    };

    QTest::newRow("2160p 21:9") << std::make_pair(QSize(5120, 2160), 60u) << QList<Mode>{
        std::make_pair(QSize(5120, 2160), 60),
        std::make_pair(QSize(1600, 1200), 60),
        std::make_pair(QSize(1280, 1024), 60),
        std::make_pair(QSize(1024, 768), 60),
        std::make_pair(QSize(2560, 1600), 60),
        std::make_pair(QSize(1920, 1200), 60),
        std::make_pair(QSize(1280, 800), 60),
        std::make_pair(QSize(3840, 2160), 60),
        std::make_pair(QSize(3200, 1800), 60),
        std::make_pair(QSize(2880, 1620), 60),
        std::make_pair(QSize(2560, 1440), 60),
        std::make_pair(QSize(1920, 1080), 60),
        std::make_pair(QSize(1600, 900), 60),
        std::make_pair(QSize(1368, 768), 60),
        std::make_pair(QSize(1280, 720), 60),
    };
    QTest::newRow("1440p 21:9") << std::make_pair(QSize(3440, 1440), 60u) << QList<Mode>{
        std::make_pair(QSize(3440, 1440), 60),
        std::make_pair(QSize(1600, 1200), 60),
        std::make_pair(QSize(1280, 1024), 60),
        std::make_pair(QSize(1024, 768), 60),
        std::make_pair(QSize(1920, 1200), 60),
        std::make_pair(QSize(1280, 800), 60),
        std::make_pair(QSize(2560, 1440), 60),
        std::make_pair(QSize(1920, 1080), 60),
        std::make_pair(QSize(1600, 900), 60),
        std::make_pair(QSize(1368, 768), 60),
        std::make_pair(QSize(1280, 720), 60),
    };
    QTest::newRow("1080p 21:9") << std::make_pair(QSize(2560, 1080), 60u) << QList<Mode>{
        std::make_pair(QSize(2560, 1080), 60),
        std::make_pair(QSize(1280, 1024), 60),
        std::make_pair(QSize(1024, 768), 60),
        std::make_pair(QSize(1280, 800), 60),
        std::make_pair(QSize(1920, 1080), 60),
        std::make_pair(QSize(1600, 900), 60),
        std::make_pair(QSize(1368, 768), 60),
        std::make_pair(QSize(1280, 720), 60),
    };
}

void DrmTest::testModeGeneration()
{
    const auto mockGpu = findPrimaryDevice(5);

    const auto conn = std::make_shared<MockConnector>(mockGpu.get());
    mockGpu->connectors.push_back(conn);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));

    QFETCH(Mode, nativeMode);
    QFETCH(QList<Mode>, expectedModes);

    const auto &[nativeSize, nativeRefreshHz] = nativeMode;

    conn->modes.clear();
    conn->addMode(nativeSize.width(), nativeSize.height(), nativeRefreshHz);
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 1);
    // no mode generation without the scaling property
    QCOMPARE(gpu->drmOutputs().front()->modes().size(), 1);

    mockGpu->connectors.removeAll(conn);
    QVERIFY(gpu->updateOutputs());

    conn->props.emplace_back(conn.get(), QStringLiteral("scaling mode"), 0, DRM_MODE_PROP_ENUM, QList<QByteArray>{"None", "Full", "Center", "Full aspect"});
    mockGpu->connectors.push_back(conn);
    QVERIFY(gpu->updateOutputs());

    DrmOutput *const output = gpu->drmOutputs().front();
    QCOMPARE(output->modes().size(), expectedModes.size());
    for (const auto &mode : output->modes()) {
        const auto it = std::ranges::find_if(expectedModes, [mode](const auto &expectedMode) {
            return mode->size() == expectedMode.first
                && std::round(mode->refreshRate() / 1000.0) == expectedMode.second;
        });
        QVERIFY(it != expectedModes.end());
        QVERIFY(mode->size().width() <= nativeSize.width());
        QVERIFY(mode->size().height() <= nativeSize.height());
        QCOMPARE_LE(mode->refreshRate(), nativeRefreshHz * 1000);
    }

    gpu.reset();
    verifyCleanup(mockGpu.get());
}

void DrmTest::testConnectorLifetime()
{
    // don't crash if output lifetime is extended beyond the connector
    const auto mockGpu = findPrimaryDevice(5);

    const auto conn = std::make_shared<MockConnector>(mockGpu.get());
    mockGpu->connectors.push_back(conn);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));

    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 1);

    DrmOutput *const output = gpu->drmOutputs().front();

    output->ref();
    mockGpu->connectors.clear();
    QVERIFY(gpu->updateOutputs());
    output->unref();

    gpu.reset();
    verifyCleanup(mockGpu.get());
}

void DrmTest::testModeset_data()
{
    QTest::addColumn<int>("AMS");
    // TODO to uncomment this, implement page flip callbacks
    // QTest::newRow("disabled") << 0;
    QTest::newRow("enabled") << 1;
}

void DrmTest::testModeset()
{
    // to reenable, make this part of an integration test, so that kwinApp() isn't nullptr
    QSKIP("this test needs output pipelines to be enabled by default, which is no longer the case");
    // test if doing a modeset would succeed
    QFETCH(int, AMS);
    const auto mockGpu = findPrimaryDevice(5);
    mockGpu->deviceCaps[MOCKDRM_DEVICE_CAP_ATOMIC] = AMS;

    const auto conn = std::make_shared<MockConnector>(mockGpu.get());
    mockGpu->connectors.push_back(conn);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));

    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 1);
    const auto output = gpu->drmOutputs().front();
    const auto layer = renderBackend->compatibleOutputLayers(output).front();
    layer->beginFrame();
    output->renderLoop()->prepareNewFrame();
    const auto frame = std::make_shared<OutputFrame>(output->renderLoop(), std::chrono::nanoseconds(1'000'000'000'000 / output->refreshRate()));
    layer->endFrame(Region::infinite(), Region::infinite(), frame.get());
    QVERIFY(output->present({layer}, frame));

    gpu.reset();
    verifyCleanup(mockGpu.get());
}

void DrmTest::testVrrChange()
{
    const auto mockGpu = findPrimaryDevice(5);
    mockGpu->deviceCaps[MOCKDRM_DEVICE_CAP_ATOMIC] = 1;

    const auto conn = std::make_shared<MockConnector>(mockGpu.get());
    conn->setVrrCapable(false);
    mockGpu->connectors.push_back(conn);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));

    QVERIFY(gpu->updateOutputs());
    const auto output = gpu->drmOutputs().front();
    QVERIFY(!(output->capabilities() & BackendOutput::Capability::Vrr));

    QSignalSpy capsChanged(output, &BackendOutput::capabilitiesChanged);

    conn->setVrrCapable(true);
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().front(), output);
    QCOMPARE(capsChanged.count(), 1);
    QVERIFY(output->capabilities() & BackendOutput::Capability::Vrr);
}

void DrmTest::testLeaseDisconnectReconnect()
{
    const auto mockGpu = findPrimaryDevice(1);
    const auto nonDesktop = std::make_shared<MockConnector>(mockGpu.get(), true);
    mockGpu->connectors.push_back(nonDesktop);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));

    QVERIFY(gpu->updateOutputs());
    const auto outputs = gpu->drmOutputs();
    QCOMPARE(outputs.size(), 1);
    DrmOutput *nonDesktopOutput = outputs.front();
    QVERIFY(nonDesktopOutput->isNonDesktop());

    KWin::Display display;
    const QString socketName = QStringLiteral("kwin-test-mockdrm-lease-%1").arg(QCoreApplication::applicationPid());
    QVERIFY(display.addSocketName(socketName));
    QVERIFY(display.start());
    auto leaseManager = std::make_unique<DrmLeaseManagerV1>(backend.get(), &display);
    Q_EMIT backend->gpuAdded(gpu.get());
    auto *leaseDevice = leaseManager->leaseDeviceForGpu(gpu.get());
    QVERIFY(leaseDevice);

    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) == 0);
    ClientConnection *client = display.createClient(sv[0]);
    QVERIFY(client);
    close(sv[1]);

    QVERIFY(leaseDevice->add(client->client(), 2, 1));
    auto *connector = leaseDevice->connectorForOutput(nonDesktopOutput);
    QVERIFY(connector);
    const int advertisedBeforeLease = connector->resourceMap().size();
    QVERIFY(advertisedBeforeLease >= 1);

    auto drmLease = std::make_unique<DrmLease>(gpu.get(), FileDescriptor{dup(gpu->fd())}, 4242, QList<DrmOutput *>{nonDesktopOutput});
    wl_resource *leaseResource = wl_resource_create(client->client(), &wp_drm_lease_v1_interface, 1, 3);
    QVERIFY(leaseResource);
    auto *lease = new DrmLeaseV1Interface(leaseDevice, QList<DrmLeaseConnectorV1Interface *>{connector}, leaseResource);
    leaseDevice->addLease(lease);

    // Simulate an active lease: the connector must become withdrawn.
    lease->grant(std::move(drmLease));
    QVERIFY(connector->withdrawn());

    // Hot-unplug while leased: revoke must not immediately re-advertise.
    nonDesktop->connection = DRM_MODE_DISCONNECTED;
    QVERIFY(nonDesktopOutput->connector()->updateProperties());
    const int advertisedBeforeRevoke = connector->resourceMap().size();
    lease->revoke();
    QCOMPARE(connector->resourceMap().size(), advertisedBeforeRevoke);
    QVERIFY(connector->withdrawn());

    // After reconnect, outputsQueried should re-offer available connectors.
    nonDesktop->connection = DRM_MODE_CONNECTED;
    QVERIFY(nonDesktopOutput->connector()->updateProperties());
    Q_EMIT backend->outputsQueried();
    QCOMPARE(connector->resourceMap().size(), advertisedBeforeRevoke + 1);
    QVERIFY(!connector->withdrawn());

    // Explicitly destroy the lease resource so the lease object is removed
    // before backend teardown. This avoids a second revoke during cleanup.
    wl_resource_destroy(leaseResource);
    leaseManager.reset();

    gpu.reset();
    verifyCleanup(mockGpu.get());
}

void DrmTest::testLeaseAvailableAfterMasterToggleAndClientExit()
{
    const auto mockGpu = findPrimaryDevice(1);
    const auto nonDesktop = std::make_shared<MockConnector>(mockGpu.get(), true);
    mockGpu->connectors.push_back(nonDesktop);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));

    QVERIFY(gpu->updateOutputs());
    const auto outputs = gpu->drmOutputs();
    QCOMPARE(outputs.size(), 1);
    DrmOutput *nonDesktopOutput = outputs.front();
    QVERIFY(nonDesktopOutput->isNonDesktop());

    KWin::Display display;
    const QString socketName = QStringLiteral("kwin-test-mockdrm-lease-master-toggle-%1").arg(QCoreApplication::applicationPid());
    QVERIFY(display.addSocketName(socketName));
    QVERIFY(display.start());
    auto leaseManager = std::make_unique<DrmLeaseManagerV1>(backend.get(), &display);
    Q_EMIT backend->gpuAdded(gpu.get());
    auto *leaseDevice = leaseManager->leaseDeviceForGpu(gpu.get());
    QVERIFY(leaseDevice);

    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) == 0);
    ClientConnection *client = display.createClient(sv[0]);
    QVERIFY(client);
    close(sv[1]);

    auto *deviceResource = leaseDevice->add(client->client(), 2, 1);
    QVERIFY(deviceResource);

    auto *connector = leaseDevice->connectorForOutput(nonDesktopOutput);
    QVERIFY(connector);
    QVERIFY(!connector->resourceMap().empty());

    wl_resource *leaseResource = wl_resource_create(client->client(), &wp_drm_lease_v1_interface, 1, 3);
    QVERIFY(leaseResource);
    auto *lease = new DrmLeaseV1Interface(leaseDevice, QList<DrmLeaseConnectorV1Interface *>{connector}, leaseResource);
    leaseDevice->addLease(lease);
    auto drmLease = std::make_unique<DrmLease>(gpu.get(), FileDescriptor{dup(gpu->fd())}, 4242, QList<DrmOutput *>{nonDesktopOutput});
    lease->grant(std::move(drmLease));
    QVERIFY(connector->withdrawn());

    Q_EMIT gpu->activeChanged(false);
    QVERIFY(connector->withdrawn());

    // Simulate client exiting while DRM master is lost.
    client->destroy();
    QCoreApplication::processEvents();
    QVERIFY(connector->resourceMap().empty());

    // Re-acquiring DRM master followed by outputsQueried must make the
    // connector available again, even if there are no bound resources.
    Q_EMIT gpu->activeChanged(true);
    Q_EMIT backend->outputsQueried();
    QVERIFY(!connector->withdrawn());

    leaseManager.reset();
    gpu.reset();
    verifyCleanup(mockGpu.get());
}

void DrmTest::testLeaseAvailableWhenClientExitsBeforeQueuedReoffer()
{
    const auto mockGpu = findPrimaryDevice(1);
    const auto nonDesktop = std::make_shared<MockConnector>(mockGpu.get(), true);
    mockGpu->connectors.push_back(nonDesktop);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), mockGpu->fd, DrmDevice::open(mockGpu->devNode));

    QVERIFY(gpu->updateOutputs());
    const auto outputs = gpu->drmOutputs();
    QCOMPARE(outputs.size(), 1);
    DrmOutput *nonDesktopOutput = outputs.front();
    QVERIFY(nonDesktopOutput->isNonDesktop());

    KWin::Display display;
    const QString socketName = QStringLiteral("kwin-test-mockdrm-lease-client-exit-%1").arg(QCoreApplication::applicationPid());
    QVERIFY(display.addSocketName(socketName));
    QVERIFY(display.start());
    auto leaseManager = std::make_unique<DrmLeaseManagerV1>(backend.get(), &display);
    Q_EMIT backend->gpuAdded(gpu.get());
    auto *leaseDevice = leaseManager->leaseDeviceForGpu(gpu.get());
    QVERIFY(leaseDevice);

    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) == 0);
    ClientConnection *client = display.createClient(sv[0]);
    QVERIFY(client);
    close(sv[1]);

    auto *deviceResource = leaseDevice->add(client->client(), 2, 1);
    QVERIFY(deviceResource);

    auto *connector = leaseDevice->connectorForOutput(nonDesktopOutput);
    QVERIFY(connector);
    QVERIFY(!connector->resourceMap().empty());

    wl_resource *leaseResource = wl_resource_create(client->client(), &wp_drm_lease_v1_interface, 1, 3);
    QVERIFY(leaseResource);
    auto *lease = new DrmLeaseV1Interface(leaseDevice, QList<DrmLeaseConnectorV1Interface *>{connector}, leaseResource);
    leaseDevice->addLease(lease);
    auto drmLease = std::make_unique<DrmLease>(gpu.get(), FileDescriptor{dup(gpu->fd())}, 4242, QList<DrmOutput *>{nonDesktopOutput});

    lease->grant(std::move(drmLease));
    QVERIFY(connector->withdrawn());

    // Disconnect the client before processing events.
    // Lease resource destruction triggers DrmLeaseV1Interface teardown,
    // which revokes the lease. Connectors stay withdrawn until re-offered.
    client->destroy();
    QCoreApplication::processEvents();
    QVERIFY(connector->resourceMap().empty());

    // Trigger outputsQueried -> handleOutputsQueried() which calls
    // offerAvailableConnectors() to re-offer withdrawn connected connectors.
    Q_EMIT backend->outputsQueried();
    QVERIFY(!connector->withdrawn());

    // A newly connecting client should receive the connector again.
    int sv2[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv2) == 0);
    ClientConnection *client2 = display.createClient(sv2[0]);
    QVERIFY(client2);
    close(sv2[1]);
    auto *deviceResource2 = leaseDevice->add(client2->client(), 2, 5);
    QVERIFY(deviceResource2);
    QVERIFY(!connector->resourceMap().empty());
    QVERIFY(!connector->withdrawn());

    leaseManager.reset();
    gpu.reset();
    verifyCleanup(mockGpu.get());
}

QTEST_GUILESS_MAIN(DrmTest)
#include "mockDrmTest.moc"
