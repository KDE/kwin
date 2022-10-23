/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    qDebug() << "a";

    const auto one = std::make_shared<MockConnector>(mockGpu.get());
    const auto two = std::make_shared<MockConnector>(mockGpu.get());
    const auto vr = std::make_shared<MockConnector>(mockGpu.get(), true);
    mockGpu->connectors.push_back(one);
    mockGpu->connectors.push_back(two);
    mockGpu->connectors.push_back(vr);
    qDebug() << "b";


    const auto session = Session::create(Session::Type::Noop);
    const auto backend = std::make_unique<DrmBackend>(session.get());
    const auto renderBackend = backend->createQPainterBackend();
    auto gpu = std::make_unique<DrmGpu>(backend.get(), "test", 1, 0);
    QVERIFY(gpu->updateOutputs());
    qDebug() << "c";

    // 3 outputs should be detected, one of them non-desktop
    const auto outputs = gpu->drmOutputs();
    QCOMPARE(outputs.size(), 3);
    const auto vrOutput = std::find_if(outputs.begin(), outputs.end(), [](const auto &output) {
        return output->isNonDesktop();
    });
    QVERIFY(vrOutput != outputs.end());
    QVERIFY(static_cast<DrmOutput *>(*vrOutput)->connector()->id() == vr->id);
    qDebug() << "d";

    // test hotunplugging
    mockGpu->connectors.removeOne(one);
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 2);
    qDebug() << "e";

    // test hotplugging
    mockGpu->connectors.push_back(one);
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 3);
    qDebug() << "f";

    // connector state changing to disconnected should count as a hotunplug
    one->connection = DRM_MODE_DISCONNECTED;
    QVERIFY(gpu->updateOutputs());
    QCOMPARE(gpu->drmOutputs().size(), 2);
    qDebug() << "g";

    // don't crash if all connectors are disconnected
    two->connection = DRM_MODE_DISCONNECTED;
    vr->connection = DRM_MODE_DISCONNECTED;
    QVERIFY(gpu->updateOutputs());
    QVERIFY(gpu->drmOutputs().empty());
    qDebug() << "h";

    gpu.reset();
    qDebug() << "i";
    verifyCleanup(mockGpu.get());
    qDebug() << "j";
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

QTEST_GUILESS_MAIN(DrmTest)
#include "drmTest.moc"
