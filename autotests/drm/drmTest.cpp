/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSize>
#include <QtTest>

#include "mock_drm.h"

#include "drm_backend.h"
#include "drm_dumb_buffer.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_plane.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_pointer.h"
#include "qpainterbackend.h"
#include "core/session.h"

#include <drm_fourcc.h>

using namespace KWin;

class DrmTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testAmsDetection();
    void testOutputDetection();
    void testZeroModesHandling();
    void testModeGeneration_data();
    void testModeGeneration();
    void testConnectorLifetime();
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
    const auto mockGpu = std::make_unique<MockGpu>(1, 0);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());

    // gpu without planes should use legacy mode
    auto gpu = std::make_unique<DrmGpu>(backend.get(), "legacy", 1, 0);
    QVERIFY(!gpu->atomicModeSetting());

    // gpu with planes should use AMS
    mockGpu->planes << std::make_shared<MockPlane>(mockGpu.get(), PlaneType::Primary, 0);
    gpu = std::make_unique<DrmGpu>(backend.get(), "AMS", 1, 0);
    QVERIFY(gpu->atomicModeSetting());

    // but not if the kernel doesn't allow it
    mockGpu->deviceCaps[MOCKDRM_DEVICE_CAP_ATOMIC] = 0;
    gpu = std::make_unique<DrmGpu>(backend.get(), "legacy 2", 1, 0);
    QVERIFY(!gpu->atomicModeSetting());

    gpu.reset();
    verifyCleanup(mockGpu.get());
}

void DrmTest::testOutputDetection()
{
    const auto mockGpu = std::make_unique<MockGpu>(1, 5);

    const auto one = std::make_shared<MockConnector>(mockGpu.get());
    const auto two = std::make_shared<MockConnector>(mockGpu.get());
    const auto vr = std::make_shared<MockConnector>(mockGpu.get(), true);
    mockGpu->connectors.push_back(one);
    mockGpu->connectors.push_back(two);
    mockGpu->connectors.push_back(vr);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), "test", 1, 0);
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
    const auto mockGpu = std::make_unique<MockGpu>(1, 5);

    const auto conn = std::make_shared<MockConnector>(mockGpu.get());
    mockGpu->connectors.push_back(conn);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), "test", 1, 0);

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

void DrmTest::testModeGeneration_data()
{
    QTest::addColumn<QSize>("nativeMode");
    QTest::addColumn<QVector<QSize>>("expectedModes");

    QTest::newRow("2160p") << QSize(3840, 2160) << QVector<QSize>{
        QSize(1600, 1200),
        QSize(1280, 1024),
        QSize(1024, 768),
        QSize(2560, 1600),
        QSize(1920, 1200),
        QSize(1280, 800),
        QSize(3840, 2160),
        QSize(3200, 1800),
        QSize(2880, 1620),
        QSize(2560, 1440),
        QSize(1920, 1080),
        QSize(1600, 900),
        QSize(1368, 768),
        QSize(1280, 720),
    };
    QTest::newRow("1440p") << QSize(2560, 1440) << QVector<QSize>{
        QSize(1600, 1200),
        QSize(1280, 1024),
        QSize(1024, 768),
        QSize(1920, 1200),
        QSize(1280, 800),
        QSize(2560, 1440),
        QSize(1920, 1080),
        QSize(1600, 900),
        QSize(1368, 768),
        QSize(1280, 720),
    };
    QTest::newRow("1080p") << QSize(1920, 1080) << QVector<QSize>{
        QSize(1280, 1024),
        QSize(1024, 768),
        QSize(1280, 800),
        QSize(1920, 1080),
        QSize(1600, 900),
        QSize(1368, 768),
        QSize(1280, 720),
    };

    QTest::newRow("2160p 21:9") << QSize(5120, 2160) << QVector<QSize>{
        QSize(5120, 2160),
        QSize(1600, 1200),
        QSize(1280, 1024),
        QSize(1024, 768),
        QSize(2560, 1600),
        QSize(1920, 1200),
        QSize(1280, 800),
        QSize(3840, 2160),
        QSize(3200, 1800),
        QSize(2880, 1620),
        QSize(2560, 1440),
        QSize(1920, 1080),
        QSize(1600, 900),
        QSize(1368, 768),
        QSize(1280, 720),
    };
    QTest::newRow("1440p 21:9") << QSize(3440, 1440) << QVector<QSize>{
        QSize(3440, 1440),
        QSize(1600, 1200),
        QSize(1280, 1024),
        QSize(1024, 768),
        QSize(1920, 1200),
        QSize(1280, 800),
        QSize(2560, 1440),
        QSize(1920, 1080),
        QSize(1600, 900),
        QSize(1368, 768),
        QSize(1280, 720),
    };
    QTest::newRow("1080p 21:9") << QSize(2560, 1080) << QVector<QSize>{
        QSize(2560, 1080),
        QSize(1280, 1024),
        QSize(1024, 768),
        QSize(1280, 800),
        QSize(1920, 1080),
        QSize(1600, 900),
        QSize(1368, 768),
        QSize(1280, 720),
    };
}

void DrmTest::testModeGeneration()
{
    const auto mockGpu = std::make_unique<MockGpu>(1, 5);

    const auto conn = std::make_shared<MockConnector>(mockGpu.get());
    mockGpu->connectors.push_back(conn);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), "test", 1, 0);

    QFETCH(QSize, nativeMode);
    QFETCH(QVector<QSize>, expectedModes);

    conn->modes.clear();
    conn->addMode(nativeMode.width(), nativeMode.height(), 60);
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 1);
    // no mode generation without the scaling property
    QCOMPARE(gpu->drmOutputs().front()->modes().size(), 1);

    mockGpu->connectors.removeAll(conn);
    QVERIFY(gpu->updateOutputs());

    conn->props.push_back(MockProperty(conn.get(), QStringLiteral("scaling mode"), 0, 0, QVector<QByteArray>{"None", "Full", "Center", "Full aspect"}));
    mockGpu->connectors.push_back(conn);
    QVERIFY(gpu->updateOutputs());

    DrmOutput *const output = gpu->drmOutputs().front();
    QCOMPARE(output->modes().size(), expectedModes.size());
    for (const auto &mode : output->modes()) {
        QVERIFY(expectedModes.contains(mode->size()));
        QVERIFY(mode->size().width() <= nativeMode.width());
        QVERIFY(mode->size().height() <= nativeMode.height());
        QVERIFY(mode->refreshRate() <= 60000);
    }

    gpu.reset();
    verifyCleanup(mockGpu.get());
}

void DrmTest::testConnectorLifetime()
{
    // don't crash if output lifetime is extended beyond the connector
    const auto mockGpu = std::make_unique<MockGpu>(1, 5);

    const auto conn = std::make_shared<MockConnector>(mockGpu.get());
    mockGpu->connectors.push_back(conn);

    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), "test", 1, 0);

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

QTEST_GUILESS_MAIN(DrmTest)
#include "drmTest.moc"
