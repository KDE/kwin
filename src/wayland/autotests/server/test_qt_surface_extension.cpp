/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
#include <QProcess>
// WaylandServer
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/output_interface.h"
#include "../../src/server/qtsurfaceextension_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/shell_interface.h"

using namespace KWaylandServer;


class TestQtSurfaceExtension : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCloseWindow();
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-qt-surface-extension-0");

void TestQtSurfaceExtension::testCloseWindow()
{
    // this test verifies that we can close windows through the Qt surface extension interface
    // for this we start a dummy server, launch a QtWayland powered application, wait for the
    // window it opens, close it and verify that the process terminates
    Display display;
    display.setSocketName(s_socketName);
    display.start();
    display.createShm();
    SeatInterface *seat = display.createSeat();
    seat->setHasKeyboard(true);
    seat->setHasPointer(true);
    seat->setHasTouch(true);
    seat->create();
    CompositorInterface *compositor = display.createCompositor();
    compositor->create();
    ShellInterface *shell = display.createShell();
    shell->create();
    OutputInterface *output = display.createOutput();
    output->setManufacturer(QStringLiteral("org.kde"));
    output->setModel(QStringLiteral("QtSurfaceExtensionTestCase"));
    output->addMode(QSize(1280, 1024), OutputInterface::ModeFlag::Preferred | OutputInterface::ModeFlag::Current);
    output->setPhysicalSize(QSize(1280, 1024) / 3.8);
    output->create();
    // surface extension
    QtSurfaceExtensionInterface *surfaceExtension = display.createQtSurfaceExtension();
    surfaceExtension->create();
    // create a signalspy for surfaceCreated
    QSignalSpy surfaceExtensionSpy(surfaceExtension, &QtSurfaceExtensionInterface::surfaceCreated);
    QVERIFY(surfaceExtensionSpy.isValid());

    // now start our application
    QString binary = QFINDTESTDATA("surfaceExtensionHelper");
    QVERIFY(!binary.isEmpty());

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), s_socketName);
    process.setProcessEnvironment(env);
    process.start(binary, QStringList());
    QVERIFY(surfaceExtensionSpy.wait());
    QCOMPARE(surfaceExtensionSpy.count(), 1);
    auto *extension = surfaceExtensionSpy.first().first().value<QtExtendedSurfaceInterface*>();
    QVERIFY(extension);
    QSignalSpy surfaceExtensionDestroyedSpy(extension, &QObject::destroyed);
    QVERIFY(surfaceExtensionSpy.isValid());
    qRegisterMetaType<QProcess::ProcessState>();
    QSignalSpy processStateChangedSpy(&process, &QProcess::stateChanged);
    QVERIFY(processStateChangedSpy.isValid());
    extension->close();
    extension->client()->flush();

    QVERIFY(processStateChangedSpy.wait());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    if (surfaceExtensionDestroyedSpy.count() == 0) {
        QVERIFY(surfaceExtensionDestroyedSpy.wait());
    }
    QCOMPARE(surfaceExtensionSpy.count(), 1);
}

QTEST_GUILESS_MAIN(TestQtSurfaceExtension)
#include "test_qt_surface_extension.moc"
