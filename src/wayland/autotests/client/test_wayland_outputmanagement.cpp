/********************************************************************
Copyright 2014 Martin Gräßlin <mgraesslin@kde.org>
Copyright 2015 Sebastian Kügler <sebas@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest/QtTest>
// KWin
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/outputdevice.h"
#include "../../src/client/outputconfiguration.h"
#include "../../src/client/outputmanagement.h"
#include "../../src/client/output.h"
#include "../../src/client/registry.h"
#include "../../src/server/display.h"
#include "../../src/server/shell_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/outputconfiguration_interface.h"
#include "../../src/server/outputdevice_interface.h"
#include "../../src/server/outputmanagement_interface.h"

// Wayland
#include <wayland-client-protocol.h>

using namespace KWayland::Client;
using namespace KWayland::Server;

class TestWaylandOutputManagement : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandOutputManagement(QObject *parent = nullptr);
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testCreate();
    void testOutputDevices();
    void createConfig();

    void testMultipleSettings();
    void testConfigFailed();
    void testApplied();
    void testFailed();

    void testExampleConfig();
    void testScale();

    void testRemoval();

private:
    void testEnable();
    void applyPendingChanges();

    KWayland::Server::Display *m_display;
    KWayland::Server::OutputConfigurationInterface *m_outputConfigurationInterface;
    KWayland::Server::OutputManagementInterface *m_outputManagementInterface;
    QList<KWayland::Server::OutputDeviceInterface *> m_serverOutputs;


    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::OutputDevice *m_outputDevice;
    KWayland::Client::OutputManagement *m_outputManagement = nullptr;
    KWayland::Client::OutputConfiguration *m_outputConfiguration;
    QList<KWayland::Client::OutputDevice *> m_clientOutputs;
    QList<KWayland::Server::OutputDeviceInterface::Mode> m_modes;

    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;

    QSignalSpy *m_announcedSpy;
    QSignalSpy *m_omSpy;
    QSignalSpy *m_configSpy;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-output-0");

TestWaylandOutputManagement::TestWaylandOutputManagement(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_outputConfigurationInterface(nullptr)
    , m_outputManagementInterface(nullptr)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
    , m_announcedSpy(nullptr)
{
}

void TestWaylandOutputManagement::initTestCase()
{
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    auto shell = m_display->createShell(this);
    shell->create();
    auto comp = m_display->createCompositor(this);
    comp->create();

    auto outputDeviceInterface = m_display->createOutputDevice(this);

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

    m_outputManagementInterface = m_display->createOutputManagement(this);
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
}

void TestWaylandOutputManagement::cleanupTestCase()
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

    if (m_outputManagementInterface) {
        delete m_outputConfigurationInterface;
        m_outputConfigurationInterface = nullptr;
    }
    delete m_display;
    m_display = nullptr;
}

void TestWaylandOutputManagement::applyPendingChanges()
{
    auto changes = m_outputConfigurationInterface->changes();
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
    }
}

void TestWaylandOutputManagement::testCreate()
{
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
}

void TestWaylandOutputManagement::testOutputDevices()
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
    QCOMPARE(output->scale(), 1);
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
    connect(m_outputManagementInterface, &KWayland::Server::OutputManagementInterface::configurationChangeRequested,
            [this] (KWayland::Server::OutputConfigurationInterface *config) {
                m_outputConfigurationInterface = config;
            });

    m_outputConfiguration = m_outputManagement->createConfiguration();
    QVERIFY(m_outputConfiguration->isValid());
}

void TestWaylandOutputManagement::testApplied()
{
    QVERIFY(m_outputConfiguration->isValid());
    QSignalSpy appliedSpy(m_outputConfiguration, &KWayland::Client::OutputConfiguration::applied);

    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested,
            [=](KWayland::Server::OutputConfigurationInterface *configurationInterface) {
                configurationInterface->setApplied();
        });
    m_outputConfiguration->apply();
    QVERIFY(appliedSpy.wait(200));
    QCOMPARE(appliedSpy.count(), 1);
}

void TestWaylandOutputManagement::testFailed()
{
    QVERIFY(m_outputConfiguration->isValid());
    QSignalSpy failedSpy(m_outputConfiguration, &KWayland::Client::OutputConfiguration::failed);

    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested,
            [=](KWayland::Server::OutputConfigurationInterface *configurationInterface) {
                configurationInterface->setFailed();
        });
    m_outputConfiguration->apply();
    QVERIFY(failedSpy.wait(200));
    QCOMPARE(failedSpy.count(), 1);

}

void TestWaylandOutputManagement::testEnable()
{
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

    QSignalSpy serverApplySpy(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested);
    QVERIFY(serverApplySpy.isValid());
    connect(m_outputManagementInterface, &OutputManagementInterface::configurationChangeRequested, [=](KWayland::Server::OutputConfigurationInterface *configurationInterface) {
        applyPendingChanges();
        m_outputConfigurationInterface = configurationInterface;
    });

    config->setMode(output, m_modes.first().id);
    config->setTransform(output, OutputDevice::Transform::Rotated90);
    config->setPosition(output, QPoint(13, 37));
    config->setScale(output, 2);
    config->setEnabled(output, OutputDevice::Enablement::Disabled);
    config->apply();

    QVERIFY(serverApplySpy.wait(200));
    QCOMPARE(serverApplySpy.count(), 1);

    m_outputConfigurationInterface->setApplied();

    QSignalSpy configAppliedSpy(config, &OutputConfiguration::applied);
    QVERIFY(configAppliedSpy.isValid());
    QVERIFY(configAppliedSpy.wait(200));
    QCOMPARE(configAppliedSpy.count(), 1);
    QCOMPARE(outputChangedSpy.count(), 5);

    config->setMode(output, m_modes.at(1).id);
    config->setTransform(output, OutputDevice::Transform::Normal);
    config->setPosition(output, QPoint(0, 1920));
    config->setScale(output, 1);
    config->setEnabled(output, OutputDevice::Enablement::Enabled);
    config->apply();

    QVERIFY(serverApplySpy.wait(200));
    QCOMPARE(serverApplySpy.count(), 2);

    m_outputConfigurationInterface->setApplied();

    QVERIFY(configAppliedSpy.wait(200));
    QCOMPARE(configAppliedSpy.count(), 2);
    QCOMPARE(outputChangedSpy.count(), 10);

}

void TestWaylandOutputManagement::testConfigFailed()
{
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
    QVERIFY(serverApplySpy.wait(200));

    // Artificialy make the server fail to apply the settings
    m_outputConfigurationInterface->setFailed();
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
    config->apply();

    QSignalSpy configAppliedSpy(config, &OutputConfiguration::applied);
    m_outputConfigurationInterface->setApplied();
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
    m_outputConfigurationInterface->setApplied();
    QVERIFY(configAppliedSpy.isValid());
    QVERIFY(configAppliedSpy.wait(200));

    QCOMPARE(output->scale(), 2); //test backwards compatibility
    QCOMPARE(wl_fixed_from_double(output->scaleF()), wl_fixed_from_double(2.3));

    config->setScale(output, 3);
    config->apply();

    m_outputConfigurationInterface->setApplied();
    QVERIFY(configAppliedSpy.isValid());
    QVERIFY(configAppliedSpy.wait(200));

    QCOMPARE(output->scale(), 3);
    QCOMPARE(output->scaleF(), 3.0); //test fowards compatibility
}

QTEST_GUILESS_MAIN(TestWaylandOutputManagement)
#include "test_wayland_outputmanagement.moc"
