/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/output.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/xdgoutput.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_screen_changes-0");

class ScreenChangesTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testScreenAddRemove();
};

void ScreenChangesTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void ScreenChangesTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void ScreenChangesTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void ScreenChangesTest::testScreenAddRemove()
{
    // this test verifies that when a new screen is added it gets synced to Wayland

    // first create a registry to get signals about Outputs announced/removed
    KWayland::Client::Registry registry;
    QSignalSpy allAnnounced(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QSignalSpy outputAnnouncedSpy(&registry, &KWayland::Client::Registry::outputAnnounced);
    QSignalSpy outputRemovedSpy(&registry, &KWayland::Client::Registry::outputRemoved);
    registry.create(Test::waylandConnection());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    const auto xdgOMData = registry.interface(KWayland::Client::Registry::Interface::XdgOutputUnstableV1);
    auto xdgOutputManager = registry.createXdgOutputManager(xdgOMData.name, xdgOMData.version);

    // should be one output
    QCOMPARE(workspace()->outputs().count(), 1);
    QCOMPARE(outputAnnouncedSpy.count(), 1);
    const quint32 firstOutputId = outputAnnouncedSpy.first().first().value<quint32>();
    QVERIFY(firstOutputId != 0u);
    outputAnnouncedSpy.clear();

    // let's announce a new output
    const QVector<QRect> geometries{QRect(0, 0, 1280, 1024), QRect(1280, 0, 1280, 1024)};
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs",
                              Qt::DirectConnection,
                              Q_ARG(QVector<QRect>, geometries));
    auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), geometries[0]);
    QCOMPARE(outputs[1]->geometry(), geometries[1]);

    // this should result in it getting announced, two new outputs are added...
    QVERIFY(outputAnnouncedSpy.wait());
    if (outputAnnouncedSpy.count() < 2) {
        QVERIFY(outputAnnouncedSpy.wait());
    }
    QCOMPARE(outputAnnouncedSpy.count(), 2);
    // ... and afterward the previous output gets removed
    if (outputRemovedSpy.isEmpty()) {
        QVERIFY(outputRemovedSpy.wait());
    }
    QCOMPARE(outputRemovedSpy.count(), 1);
    QCOMPARE(outputRemovedSpy.first().first().value<quint32>(), firstOutputId);

    // let's wait a little bit to ensure we don't get more events
    QTest::qWait(100);
    QCOMPARE(outputAnnouncedSpy.count(), 2);
    QCOMPARE(outputRemovedSpy.count(), 1);

    // let's create the output objects to ensure they are correct
    std::unique_ptr<KWayland::Client::Output> o1(registry.createOutput(outputAnnouncedSpy.first().first().value<quint32>(), outputAnnouncedSpy.first().last().value<quint32>()));
    QVERIFY(o1->isValid());
    QSignalSpy o1ChangedSpy(o1.get(), &KWayland::Client::Output::changed);
    QVERIFY(o1ChangedSpy.wait());
    KWin::Output *serverOutput1 = kwinApp()->outputBackend()->findOutput(o1->name()); // use wl_output.name to find the compositor side output
    QCOMPARE(o1->globalPosition(), serverOutput1->geometry().topLeft());
    QCOMPARE(o1->pixelSize(), serverOutput1->modeSize());
    std::unique_ptr<KWayland::Client::Output> o2(registry.createOutput(outputAnnouncedSpy.last().first().value<quint32>(), outputAnnouncedSpy.last().last().value<quint32>()));
    QVERIFY(o2->isValid());
    QSignalSpy o2ChangedSpy(o2.get(), &KWayland::Client::Output::changed);
    QVERIFY(o2ChangedSpy.wait());
    KWin::Output *serverOutput2 = kwinApp()->outputBackend()->findOutput(o2->name()); // use wl_output.name to find the compositor side output
    QCOMPARE(o2->globalPosition(), serverOutput2->geometry().topLeft());
    QCOMPARE(o2->pixelSize(), serverOutput2->modeSize());

    // and check XDGOutput is synced
    std::unique_ptr<KWayland::Client::XdgOutput> xdgO1(xdgOutputManager->getXdgOutput(o1.get()));
    QSignalSpy xdgO1ChangedSpy(xdgO1.get(), &KWayland::Client::XdgOutput::changed);
    QVERIFY(xdgO1ChangedSpy.wait());
    QCOMPARE(xdgO1->logicalPosition(), serverOutput1->geometry().topLeft());
    QCOMPARE(xdgO1->logicalSize(), serverOutput1->geometry().size());
    std::unique_ptr<KWayland::Client::XdgOutput> xdgO2(xdgOutputManager->getXdgOutput(o2.get()));
    QSignalSpy xdgO2ChangedSpy(xdgO2.get(), &KWayland::Client::XdgOutput::changed);
    QVERIFY(xdgO2ChangedSpy.wait());
    QCOMPARE(xdgO2->logicalPosition(), serverOutput2->geometry().topLeft());
    QCOMPARE(xdgO2->logicalSize(), serverOutput2->geometry().size());

    QVERIFY(xdgO1->name().startsWith("Virtual-"));
    QVERIFY(xdgO1->name() != xdgO2->name());
    QVERIFY(!xdgO1->description().isEmpty());

    // now let's try to remove one output again
    outputAnnouncedSpy.clear();
    outputRemovedSpy.clear();

    QSignalSpy o1RemovedSpy(o1.get(), &KWayland::Client::Output::removed);
    QSignalSpy o2RemovedSpy(o2.get(), &KWayland::Client::Output::removed);

    const QVector<QRect> geometries2{QRect(0, 0, 1280, 1024)};
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs",
                              Qt::DirectConnection,
                              Q_ARG(QVector<QRect>, geometries2));
    outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 1);
    QCOMPARE(outputs[0]->geometry(), geometries2.at(0));

    QVERIFY(outputAnnouncedSpy.wait());
    QCOMPARE(outputAnnouncedSpy.count(), 1);
    if (o1RemovedSpy.isEmpty()) {
        QVERIFY(o1RemovedSpy.wait());
    }
    if (o2RemovedSpy.isEmpty()) {
        QVERIFY(o2RemovedSpy.wait());
    }
    // now wait a bit to ensure we don't get more events
    QTest::qWait(100);
    QCOMPARE(outputAnnouncedSpy.count(), 1);
    QCOMPARE(o1RemovedSpy.count(), 1);
    QCOMPARE(o2RemovedSpy.count(), 1);
    QCOMPARE(outputRemovedSpy.count(), 2);
}

WAYLANDTEST_MAIN(ScreenChangesTest)
#include "screen_changes_test.moc"
