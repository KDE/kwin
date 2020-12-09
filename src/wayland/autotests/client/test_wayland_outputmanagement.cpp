/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/outputdevice.h"
#include "KWayland/Client/outputconfiguration.h"
#include "KWayland/Client/outputmanagement.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/registry.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/outputconfiguration_interface.h"
#include "../../src/server/outputdevice_interface.h"
#include "../../src/server/outputmanagement_interface.h"

// Wayland
#include <wayland-client-protocol.h>

using namespace KWayland::Client;
using namespace KWaylandServer;

class TestWaylandOutputManagement : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandOutputManagement(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();
    void createConfig();

    void testBasicMemoryManagement();
    void testMultipleSettings();
    void testConfigFailed();
    void testApplied();
    void testFailed();

    void testExampleConfig();
    void testScale();

    void testRemoval();

private:
    void createOutputDevices();
    void testEnable();
    void applyPendingChanges(KWaylandServer::OutputConfigurationInterface *configurationInterface);

    KWaylandServer::Display *m_display;
    KWaylandServer::OutputManagementInterface *m_outputManagementInterface;
    QList<KWaylandServer::OutputDeviceInterface *> m_serverOutputs;


    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::OutputDevice *m_outputDevice = nullptr;
    KWayland::Client::OutputManagement *m_outputManagement = nullptr;
    KWayland::Client::OutputConfiguration *m_outputConfiguration = nullptr;
    QList<KWayland::Client::OutputDevice *> m_clientOutputs;
    QList<KWaylandServer::OutputDeviceInterface::Mode> m_modes;

    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread;

    QSignalSpy *m_announcedSpy;
    QSignalSpy *m_omSpy;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-output-0");

TestWaylandOutputManagement::TestWaylandOutputManagement(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_outputManagementInterface(nullptr)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
    , m_announcedSpy(nullptr)
{
    qRegisterMetaType<KWaylandServer::OutputConfigurationInterface*>();
}

void TestWaylandOutputManagement::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    new CompositorInterface(m_display, this);
    auto outputDeviceInterface = new OutputDeviceInterface(m_display, this);

    OutputDeviceInterface::Mode m0;
    m0.id = 0;
    m0.size = QSize(800, 600);
    m0.flags = OutputDeviceInterface::ModeFlags(OutputDeviceInterface::ModeFlag::Preferred);
    outputDeviceInterface->addMode(m0);

    OutputDeviceInterface::Mode m1;
    m1.id = 1;
    m1.size = QSize(1024, 768);
    outputDeviceInterface->addMode(m1);

    OutputDeviceInterface::Mode m2;
    m2.id = 2;
    m2.size = QSize(1280, 1024);
    m2.refreshRate = 90000;
    outputDeviceInterface->addMode(m2);

    OutputDeviceInterface::Mode m3;
    m3.id = 3;
    m3.size = QSize(1920, 1080);
    m3.flags = OutputDeviceInterface::ModeFlags();
    m3.refreshRate = 100000;
    outputDeviceInterface->addMode(m3);

    m_modes << m0 << m1 << m2 << m3;

    outputDeviceInterface->setCurrentMode(1);
    outputDeviceInterface->setGlobalPosition(QPoint(0, 1920));
    outputDeviceInterface->create();
    m_serverOutputs << outputDeviceInterface;

    m_outputManagementInterface = new OutputManagementInterface(m_display, this);
    m_outputManagementInterface->create();
    QVERIFY(m_outputManagementInterface->isValid());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new Registry();

    m_announcedSpy = new QSignalSpy(m_registry, &KWayland::Client::Registry::outputManagementAnnounced);
    m_omSpy = new QSignalSpy(m_registry, &KWayland::Client::Registry::outputDeviceAnnounced);

    QVERIFY(m_announcedSpy->isValid());
    QVERIFY(m_omSpy->isValid());

    m_registry->create(m_connection->display());
    QVERIFY(m_registry->isValid());
    m_registry->setEventQueue(m_queue);
    m_registry->setup();
    wl_display_flush(m_connection->display());

    QVERIFY(m_announcedSpy->wait());
    QCOMPARE(m_announcedSpy->count(), 1);

    m_outputManagement = m_registry->createOutputManagement(m_announcedSpy->first().first().value<quint32>(), m_announcedSpy->first().last().value<quint32>());
    createOutputDevices();
}

void TestWaylandOutputManagement::cleanup()
{
    if (m_outputConfiguration) {
        delete m_outputConfiguration;
        m_outputConfiguration = nullptr;
    }
    if (m_outputManagement) {
        delete m_outputManagement;
        m_outputManagement = nullptr;
    }
    if (m_registry) {
        delete m_registry;
        m_registry = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    delete m_display;
    m_display = nullptr;
    m_serverOutputs.clear();
    m_clientOutputs.clear();

    // these are the children of the display
    m_outputManagementInterface = nullptr;
}

void TestWaylandOutputManagement::applyPendingChanges(KWaylandServer::OutputConfigurationInterface *configurationInterface)
{
    auto changes = configurationInterface->changes();
    for (auto outputdevice: changes.keys()) {
        auto c = changes[outputdevice];
        if (c->enabledChanged()) {
            outputdevice->setEnabled(c->enabled());
        }
        if (c->modeChanged()) {
            outputdevice->setCurrentMode(c->mode());
        }
        if (c->transformChanged()) {
            outputdevice->setTransform(c->transform());
        }
        if (c->positionChanged()) {
            outputdevice->setGlobalPosition(c->position());
        }
        if (c->scaleChanged()) {
            outputdevice->setScaleF(c->scaleF());
        }
        if (c->colorCurvesChanged()) {
            outputdevice->setColorCurves(c->colorCurves());
        }
    }
}

void TestWaylandOutputManagement::createOutputDevices()
{
    QCOMPARE(m_omSpy->count(), 1);
    QCOMPARE(m_registry->interfaces(KWayland::Client::Registry::Interface::OutputDevice).count(), m_serverOutputs.count());

    auto output = new KWayland::Client::OutputDevice();
    QVERIFY(!output->isValid());
    QCOMPARE(output->geometry(), QRect());
    QCOMPARE(output->globalPosition(), QPoint());
    QCOMPARE(output->manufacturer(), QString());
    QCOMPARE(output->model(), QString());
    QCOMPARE(output->physicalSize(), QSize());
    QCOMPARE(output->pixelSize(), QSize());
    QCOMPARE(output->refreshRate(), 0);
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 50)
    QCOMPARE(output->scale(), 1);
#endif
    QCOMPARE(output->scaleF(), 1.0);
    QCOMPARE(output->colorCurves().red, QVector<quint16>());
    QCOMPARE(output->colorCurves().green, QVector<quint16>());
    QCOMPARE(output->colorCurves().blue, QVector<quint16>());
    QCOMPARE(output->subPixel(), KWayland::Client::OutputDevice::SubPixel::Unknown);
    QCOMPARE(output->transform(), KWayland::Client::OutputDevice::Transform::Normal);
    QCOMPARE(output->enabled(), OutputDevice::Enablement::Enabled);
    QCOMPARE(output->edid(), QByteArray());
    QCOMPARE(output->uuid(), QByteArray());

    QSignalSpy outputChanged(output, &KWayland::Client::OutputDevice::changed);
    QVERIFY(outputChanged.isValid());

    output->setup(m_registry->bindOutputDevice(m_omSpy->first().first().value<quint32>(), m_omSpy->first().last().value<quint32>()));
    wl_display_flush(m_connection->display());

    QVERIFY(outputChanged.wait());
    QCOMPARE(output->globalPosition(), QPoint(0, 1920));
    QCOMPARE(output->enabled(), OutputDevice::Enablement::Enabled);

    m_clientOutputs << output;
    m_outputDevice = output;

    QVERIFY(m_outputManagement->isValid());
}

void TestWaylandOutputManagement::testBasicMemoryManagement()
{
    createConfig();

    QSignalSpy serverApplySpy(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested);
    KWaylandServer::OutputConfigurationInterface *configurationInterface = nullptr;
    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested, [=, &configurationInterface](KWaylandServer::OutputConfigurationInterface *c) {
        configurationInterface = c;
    });
    m_outputConfiguration->apply();

    QVERIFY(serverApplySpy.wait());
    QVERIFY(configurationInterface);
    QSignalSpy interfaceDeletedSpy(configurationInterface, &QObject::destroyed);

    delete m_outputConfiguration;
    m_outputConfiguration = nullptr;
    QVERIFY(interfaceDeletedSpy.wait());
}

void TestWaylandOutputManagement::testRemoval()
{
    QSignalSpy outputManagementRemovedSpy(m_registry, &KWayland::Client::Registry::outputManagementRemoved);
    QVERIFY(outputManagementRemovedSpy.isValid());

    delete m_outputManagementInterface;
    m_outputManagementInterface = nullptr;
    QVERIFY(outputManagementRemovedSpy.wait(200));
    QCOMPARE(outputManagementRemovedSpy.first().first(), m_announcedSpy->first().first());
    QVERIFY(!m_registry->hasInterface(KWayland::Client::Registry::Interface::OutputManagement));
    QVERIFY(m_registry->interfaces(KWayland::Client::Registry::Interface::OutputManagement).isEmpty());
}

void TestWaylandOutputManagement::createConfig()
{
    m_outputConfiguration = m_outputManagement->createConfiguration();
}

void TestWaylandOutputManagement::testApplied()
{
    createConfig();
    QVERIFY(m_outputConfiguration->isValid());
    QSignalSpy appliedSpy(m_outputConfiguration, &KWayland::Client::OutputConfiguration::applied);

    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested,
            [=](KWaylandServer::OutputConfigurationInterface *configurationInterface) {
                configurationInterface->setApplied();
        });
    m_outputConfiguration->apply();
    QVERIFY(appliedSpy.wait(200));
    QCOMPARE(appliedSpy.count(), 1);
}

void TestWaylandOutputManagement::testFailed()
{
    createConfig();
    QVERIFY(m_outputConfiguration->isValid());
    QSignalSpy failedSpy(m_outputConfiguration, &KWayland::Client::OutputConfiguration::failed);

    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested,
            [=](KWaylandServer::OutputConfigurationInterface *configurationInterface) {
                configurationInterface->setFailed();
        });
    m_outputConfiguration->apply();
    QVERIFY(failedSpy.wait(200));
    QCOMPARE(failedSpy.count(), 1);

}

void TestWaylandOutputManagement::testEnable()
{
    createConfig();
    auto config = m_outputConfiguration;
    QVERIFY(config->isValid());

    KWayland::Client::OutputDevice *output = m_clientOutputs.first();
    QCOMPARE(output->enabled(), OutputDevice::Enablement::Enabled);

    QSignalSpy enabledChanged(output, &KWayland::Client::OutputDevice::enabledChanged);
    QVERIFY(enabledChanged.isValid());

    config->setEnabled(output, OutputDevice::Enablement::Disabled);

    QVERIFY(!enabledChanged.wait(200));

    QCOMPARE(enabledChanged.count(), 0);

    // Reset
    config->setEnabled(output, OutputDevice::Enablement::Disabled);
    config->apply();
}



void TestWaylandOutputManagement::testMultipleSettings()
{
    createConfig();
    auto config = m_outputConfiguration;
    QVERIFY(config->isValid());

    KWayland::Client::OutputDevice *output = m_clientOutputs.first();
    QSignalSpy outputChangedSpy(output, &KWayland::Client::OutputDevice::changed);

    KWaylandServer::OutputConfigurationInterface *configurationInterface;
    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested, [=, &configurationInterface](KWaylandServer::OutputConfigurationInterface *c) {
        applyPendingChanges(c);
        configurationInterface = c;
    });
    QSignalSpy serverApplySpy(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested);
    QVERIFY(serverApplySpy.isValid());

    config->setMode(output, m_modes.first().id);
    config->setTransform(output, OutputDevice::Transform::Rotated90);
    config->setPosition(output, QPoint(13, 37));
    config->setScaleF(output, 2.0);
    const auto zeroVector = QVector<quint16>(256, 0);
    config->setColorCurves(output, zeroVector, zeroVector, zeroVector);
    config->setEnabled(output, OutputDevice::Enablement::Disabled);
    config->apply();

    QVERIFY(serverApplySpy.wait(200));
    QCOMPARE(serverApplySpy.count(), 1);

    configurationInterface->setApplied();

    QSignalSpy configAppliedSpy(config, &OutputConfiguration::applied);
    QVERIFY(configAppliedSpy.isValid());
    QVERIFY(configAppliedSpy.wait(200));
    QCOMPARE(configAppliedSpy.count(), 1);
    QCOMPARE(outputChangedSpy.count(), 6);

    config->setMode(output, m_modes.at(1).id);
    config->setTransform(output, OutputDevice::Transform::Normal);
    config->setPosition(output, QPoint(0, 1920));
    config->setScaleF(output, 1.0);
    const auto oneVector = QVector<quint16>(256, 1);
    config->setColorCurves(output, oneVector, oneVector, oneVector);
    config->setEnabled(output, OutputDevice::Enablement::Enabled);
    config->apply();

    QVERIFY(serverApplySpy.wait(200));
    QCOMPARE(serverApplySpy.count(), 2);

    configurationInterface->setApplied();

    QVERIFY(configAppliedSpy.wait(200));
    QCOMPARE(configAppliedSpy.count(), 2);
    QCOMPARE(outputChangedSpy.count(), 12);

}

void TestWaylandOutputManagement::testConfigFailed()
{
    createConfig();
    auto config = m_outputConfiguration;
    auto s_o = m_serverOutputs.first();
    KWayland::Client::OutputDevice *output = m_clientOutputs.first();

    QVERIFY(config->isValid());
    QVERIFY(s_o->isValid());
    QVERIFY(output->isValid());

    QSignalSpy serverApplySpy(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested);
    QVERIFY(serverApplySpy.isValid());
    QSignalSpy outputChangedSpy(output, &KWayland::Client::OutputDevice::changed);
    QVERIFY(outputChangedSpy.isValid());
    QSignalSpy configAppliedSpy(config, &OutputConfiguration::applied);
    QVERIFY(configAppliedSpy.isValid());
    QSignalSpy configFailedSpy(config, &KWayland::Client::OutputConfiguration::failed);
    QVERIFY(configFailedSpy.isValid());

    config->setMode(output, m_modes.last().id);
    config->setTransform(output, OutputDevice::Transform::Normal);
    config->setPosition(output, QPoint(-1, -1));

    config->apply();

    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested, [=](KWaylandServer::OutputConfigurationInterface *c) {
        c->setFailed();
    });

    QVERIFY(serverApplySpy.wait(200));

    // Artificially make the server fail to apply the settings
    // Make sure the applied signal never comes, and that failed has been received
    QVERIFY(!configAppliedSpy.wait(200));
    QCOMPARE(configFailedSpy.count(), 1);
    QCOMPARE(configAppliedSpy.count(), 0);
}

void TestWaylandOutputManagement::testExampleConfig()
{
    createConfig();
    auto config = m_outputConfiguration;
    KWayland::Client::OutputDevice *output = m_clientOutputs.first();

    config->setMode(output, m_clientOutputs.first()->modes().last().id);
    config->setTransform(output, OutputDevice::Transform::Normal);
    config->setPosition(output, QPoint(-1, -1));

    QSignalSpy configAppliedSpy(config, &OutputConfiguration::applied);
    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested, [=](KWaylandServer::OutputConfigurationInterface *c) {
        c->setApplied();
    });
    config->apply();

    QVERIFY(configAppliedSpy.isValid());
    QVERIFY(configAppliedSpy.wait(200));

}

void TestWaylandOutputManagement::testScale()
{
    createConfig();

    auto config = m_outputConfiguration;
    KWayland::Client::OutputDevice *output = m_clientOutputs.first();

    config->setScaleF(output, 2.3);
    config->apply();

    QSignalSpy configAppliedSpy(config, &OutputConfiguration::applied);
    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested, [=](KWaylandServer::OutputConfigurationInterface *c) {
        applyPendingChanges(c);
        c->setApplied();
    });
    QVERIFY(configAppliedSpy.isValid());
    QVERIFY(configAppliedSpy.wait(200));

#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 50)
    QCOMPARE(output->scale(), 2); //test backwards compatibility
#endif
    QCOMPARE(wl_fixed_from_double(output->scaleF()), wl_fixed_from_double(2.3));

#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 50)
    config->setScale(output, 3);
    config->apply();

    QVERIFY(configAppliedSpy.isValid());
    QVERIFY(configAppliedSpy.wait(200));

    //will be setApplied using the connect above

    QCOMPARE(output->scale(), 3);
    QCOMPARE(output->scaleF(), 3.0); //test forward compatibility
#endif
}


QTEST_GUILESS_MAIN(TestWaylandOutputManagement)
#include "test_wayland_outputmanagement.moc"
