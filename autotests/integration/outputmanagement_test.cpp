/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "abstract_wayland_output.h"
#include "deleted.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"

#include <KWayland/Client/outputmanagement.h>
#include <KWayland/Client/outputconfiguration.h>
#include <KWayland/Client/outputdevice.h>

#include <KWaylandServer/outputmanagement_interface.h>
#include <KWaylandServer/outputconfiguration_interface.h>
#include <KWaylandServer/outputdevice_interface.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/outputdevice.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>

#include <KWaylandServer/display.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_outputmanagement-0");

class TestOutputManagement : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testOutputDeviceDisabled();
    void testOutputDeviceRemoved();
};

void TestOutputManagement::initTestCase()
{
    qRegisterMetaType<KWin::Deleted*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::AbstractOutput*>();
    qRegisterMetaType<AbstractOutput *>();
    qRegisterMetaType<KWin::AbstractOutput*>("AbstractOutput *");
    qRegisterMetaType<KWayland::Client::Output*>();
    qRegisterMetaType<KWayland::Client::OutputDevice::Enablement>();
    qRegisterMetaType<OutputDevice::Enablement>("OutputDevice::Enablement");
    qRegisterMetaType<KWaylandServer::OutputDeviceInterface::Enablement>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestOutputManagement::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::OutputManagement |
                                         Test::AdditionalWaylandInterface::OutputDevice));

    screens()->setCurrent(0);
    //put mouse in the middle of screen one
    KWin::Cursors::self()->mouse()->setPos(QPoint(512, 512));
}

void TestOutputManagement::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestOutputManagement::testOutputDeviceDisabled()
{
    // This tests checks that OutputConfiguration::apply aka Platform::requestOutputsChange works as expected
    // when disabling and enabling virtual OutputDevice

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto size = QSize(200,200);

    QSignalSpy outputEnteredSpy(surface.data(), &Surface::outputEntered);
    QSignalSpy outputLeftSpy(surface.data(), &Surface::outputLeft);

    QSignalSpy outputEnabledSpy(kwinApp()->platform(), &Platform::outputEnabled);
    QSignalSpy outputDisabledSpy(kwinApp()->platform(), &Platform::outputDisabled);

    auto c = Test::renderAndWaitForShown(surface.data(), size, Qt::blue);
    //move to be in the first screen
    c->setFrameGeometry(QRect(QPoint(100,100), size));
    //we don't don't know where the compositor first placed this window,
    //this might fire, it might not
    outputEnteredSpy.wait(5);
    outputEnteredSpy.clear();
    QCOMPARE(waylandServer()->display()->outputs().count(), 2);

    QCOMPARE(surface->outputs().count(), 1);
    Output *firstOutput = surface->outputs().first();
    QCOMPARE(firstOutput->globalPosition(), QPoint(0,0));
    QSignalSpy modesChangedSpy(firstOutput, &Output::modeChanged);

    QSignalSpy screenChangedSpy(screens(), &KWin::Screens::changed);

    OutputManagement *outManagement = Test::waylandOutputManagement();

    auto outputDevices = Test::waylandOutputDevices();
    QCOMPARE(outputDevices.count(), 2);

    OutputDevice *device = outputDevices.first();
    QCOMPARE(device->enabled(), OutputDevice::Enablement::Enabled);
    QSignalSpy outputDeviceEnabledChangedSpy(device, &OutputDevice::enabledChanged);
    OutputConfiguration *config;

    // Disables an output
    config = outManagement->createConfiguration();
    QSignalSpy configAppliedSpy (config, &OutputConfiguration::applied);
    config->setEnabled(device, OutputDevice::Enablement::Disabled);
    config->apply();
    QVERIFY(configAppliedSpy.wait());

    QCOMPARE(outputDeviceEnabledChangedSpy.count(), 1);
    QCOMPARE(device->enabled(), OutputDevice::Enablement::Disabled);
    QCOMPARE(screenChangedSpy.count(), 3);
    QCOMPARE(outputLeftSpy.count(), 1);
    QCOMPARE(outputEnteredSpy.count(), 1); // surface was moved to other screen
    QCOMPARE(surface->outputs().count(), 1);
    QCOMPARE(screens()->count(), 1);
    QCOMPARE(modesChangedSpy.count(), 0);
    QCOMPARE(outputEnabledSpy.count(), 0);
    QCOMPARE(outputDisabledSpy.count(), 1);

    screenChangedSpy.clear();
    outputLeftSpy.clear();
    outputEnteredSpy.clear();
    outputDeviceEnabledChangedSpy.clear();
    outputEnabledSpy.clear();
    outputDisabledSpy.clear();

    // Enable the disabled output
    config = outManagement->createConfiguration();
    QSignalSpy configAppliedSpy2 (config, &OutputConfiguration::applied);
    config->setEnabled(device, OutputDevice::Enablement::Enabled);
    config->apply();
    QVERIFY(configAppliedSpy2.wait());

    QVERIFY(outputEnteredSpy.wait());

    QCOMPARE(outputDeviceEnabledChangedSpy.count(), 1);
    QCOMPARE(device->enabled(), OutputDevice::Enablement::Enabled);
    QCOMPARE(screenChangedSpy.count(), 3);
    QCOMPARE(outputLeftSpy.count(), 1);
    QCOMPARE(outputEnteredSpy.count(), 1); // surface moved back to first screen
    QCOMPARE(surface->outputs().count(), 1);
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(modesChangedSpy.count(), 0);
    QCOMPARE(outputEnabledSpy.count(), 1);
    QCOMPARE(outputDisabledSpy.count(), 0);
}

void TestOutputManagement::testOutputDeviceRemoved()
{
    // This tests checks that OutputConfiguration::apply aka Platform::requestOutputsChange works as expected
    // when removing a virtual OutputDevice

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto size = QSize(200,200);

    QSignalSpy outputEnteredSpy(surface.data(), &Surface::outputEntered);
    QSignalSpy outputLeftSpy(surface.data(), &Surface::outputLeft);

    QSignalSpy outputEnabledSpy(kwinApp()->platform(), &Platform::outputEnabled);
    QSignalSpy outputDisabledSpy(kwinApp()->platform(), &Platform::outputDisabled);
    QSignalSpy outputRemovedSpy(kwinApp()->platform(), &Platform::outputRemoved);

    auto c = Test::renderAndWaitForShown(surface.data(), size, Qt::blue);
    //move to be in the first screen
    c->move(QPoint(100,100));
    //we don't don't know where the compositor first placed this window,
    //this might fire, it might not
    outputEnteredSpy.wait(5);
    outputEnteredSpy.clear();
    QCOMPARE(waylandServer()->display()->outputs().count(), 2);

    QCOMPARE(surface->outputs().count(), 1);
    Output *firstOutput = surface->outputs().first();
    QCOMPARE(firstOutput->globalPosition(), QPoint(0,0));
    QSignalSpy modesChangedSpy(firstOutput, &Output::modeChanged);

    QSignalSpy screenChangedSpy(screens(), &KWin::Screens::changed);

    QCOMPARE(Test::waylandOutputDevices().count(), 2);

    OutputDevice *device = Test::waylandOutputDevices().first();
    QCOMPARE(device->enabled(), OutputDevice::Enablement::Enabled);

    QSignalSpy outputDeviceEnabledChangedSpy(device, &OutputDevice::enabledChanged);

    AbstractOutput *output = kwinApp()->platform()->outputs().first();
    // Removes an output
    QMetaObject::invokeMethod(kwinApp()->platform(), "removeOutput", Qt::DirectConnection, Q_ARG(AbstractOutput *, output));

    // Let the new state propagate
    QVERIFY(outputEnteredSpy.wait());

    QCOMPARE(waylandServer()->display()->outputs().count(), 1);
    QCOMPARE(waylandServer()->display()->outputDevices().count(), 1);

    QCOMPARE(Test::waylandOutputDevices().count(), 1);
    QCOMPARE(outputDeviceEnabledChangedSpy.count(), 1);
    QCOMPARE(device->enabled(), OutputDevice::Enablement::Disabled);
    QCOMPARE(outputLeftSpy.count(), 1);
    QCOMPARE(outputEnteredSpy.count(), 1); // surface moved to the other screen
    QCOMPARE(surface->outputs().count(), 1);
    QCOMPARE(screens()->count(), 1);
    QCOMPARE(screenChangedSpy.count(), 3);
    QCOMPARE(modesChangedSpy.count(), 0);
    QCOMPARE(outputEnabledSpy.count(), 0);
    QCOMPARE(outputDisabledSpy.count(), 1);
    QCOMPARE(outputRemovedSpy.count(), 1);
}

WAYLANDTEST_MAIN(TestOutputManagement)
#include "outputmanagement_test.moc"
