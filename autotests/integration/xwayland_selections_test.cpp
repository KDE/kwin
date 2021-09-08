/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "abstract_output.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xwl/databridge.h"

#include <KWaylandServer/seat_interface.h>

#include <QProcess>
#include <QProcessEnvironment>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xwayland_selections-0");

struct ProcessKillBeforeDeleter {
    static inline void cleanup(QProcess *pointer)
    {
        if (pointer)
            pointer->kill();
        delete pointer;
    }
};

class XwaylandSelectionsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testSync_data();
    void testSync();
};

void XwaylandSelectionsTest::initTestCase()
{
//    QSKIP("Skipped as it fails for unknown reasons on build.kde.org");
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<QProcess::ExitStatus>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
//    QSignalSpy clipboardSyncDevicedCreated{waylandServer(), &WaylandServer::xclipboardSyncDataDeviceCreated};
//    QVERIFY(clipboardSyncDevicedCreated.isValid());
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = kwinApp()->platform()->enabledOutputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    Test::initWaylandWorkspace();
//    // wait till the xclipboard sync data device is created
//    if (clipboardSyncDevicedCreated.empty()) {
//        QVERIFY(clipboardSyncDevicedCreated.wait());
//    }
}

void XwaylandSelectionsTest::testSync_data()
{
    QTest::addColumn<QString>("copyPlatform");
    QTest::addColumn<QString>("pastePlatform");

    QTest::newRow("x11->wayland") << QStringLiteral("xcb") << QStringLiteral("wayland");
    QTest::newRow("wayland->x11") << QStringLiteral("wayland") << QStringLiteral("xcb");
}

void XwaylandSelectionsTest::testSync()
{
    // this test verifies the syncing of X11 to Wayland clipboard
    const QString copy = QFINDTESTDATA(QStringLiteral("copy"));
    QVERIFY(!copy.isEmpty());
    const QString paste = QFINDTESTDATA(QStringLiteral("paste"));
    QVERIFY(!paste.isEmpty());

    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    QSignalSpy clipboardChangedSpy(waylandServer()->seat(), &KWaylandServer::SeatInterface::selectionChanged);
    QVERIFY(clipboardChangedSpy.isValid());

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // start the copy process
    QFETCH(QString, copyPlatform);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), copyPlatform);
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), s_socketName);
    QScopedPointer<QProcess, ProcessKillBeforeDeleter> copyProcess(new QProcess());
    copyProcess->setProcessEnvironment(environment);
    copyProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    copyProcess->setProgram(copy);
    copyProcess->start();
    QVERIFY(copyProcess->waitForStarted());

    AbstractClient *copyClient = nullptr;
    QVERIFY(clientAddedSpy.wait());
    copyClient = clientAddedSpy.first().first().value<AbstractClient *>();
    QVERIFY(copyClient);
    if (workspace()->activeClient() != copyClient) {
        workspace()->activateClient(copyClient);
    }
    QCOMPARE(workspace()->activeClient(), copyClient);
    clipboardChangedSpy.wait();

    // start the paste process
    QScopedPointer<QProcess, ProcessKillBeforeDeleter> pasteProcess(new QProcess());
    QSignalSpy finishedSpy(pasteProcess.data(), static_cast<void(QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished));
    QVERIFY(finishedSpy.isValid());
    QFETCH(QString, pastePlatform);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), pastePlatform);
    pasteProcess->setProcessEnvironment(environment);
    pasteProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    pasteProcess->setProgram(paste);
    pasteProcess->start();
    QVERIFY(pasteProcess->waitForStarted());

    clientAddedSpy.clear();
    AbstractClient *pasteClient = nullptr;
    QVERIFY(clientAddedSpy.wait());
    pasteClient = clientAddedSpy.last().first().value<AbstractClient *>();
    QCOMPARE(clientAddedSpy.count(), 1);
    QVERIFY(pasteClient);

    if (workspace()->activeClient() != pasteClient) {
        QSignalSpy clientActivatedSpy(workspace(), &Workspace::clientActivated);
        QVERIFY(clientActivatedSpy.isValid());
        workspace()->activateClient(pasteClient);
        QVERIFY(clientActivatedSpy.wait());
    }
    QTRY_COMPARE(workspace()->activeClient(), pasteClient);
    QVERIFY(finishedSpy.wait());
    QCOMPARE(finishedSpy.first().first().toInt(), 0);
}

WAYLANDTEST_MAIN(XwaylandSelectionsTest)
#include "xwayland_selections_test.moc"
