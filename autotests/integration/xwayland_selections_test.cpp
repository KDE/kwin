/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "xwayland/databridge.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>

#include <linux/input-event-codes.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xwayland_selections-0");

struct ProcessKillBeforeDeleter
{
    void operator()(QProcess *pointer)
    {
        if (pointer) {
            pointer->kill();
        }
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
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<QProcess::ExitStatus>();
    //    QSignalSpy clipboardSyncDevicedCreated{waylandServer(), &WaylandServer::xclipboardSyncDataDeviceCreated};
    //    QVERIFY(clipboardSyncDevicedCreated.isValid());
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
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

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QSignalSpy clipboardChangedSpy(waylandServer()->seat(), &SeatInterface::selectionChanged);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // start the copy process
    QFETCH(QString, copyPlatform);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), copyPlatform);
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), s_socketName);
    std::unique_ptr<QProcess, ProcessKillBeforeDeleter> copyProcess(new QProcess());
    copyProcess->setProcessEnvironment(environment);
    copyProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    copyProcess->setProgram(copy);
    copyProcess->start();
    QVERIFY(copyProcess->waitForStarted());

    Window *copyWindow = nullptr;
    QVERIFY(windowAddedSpy.wait());
    copyWindow = windowAddedSpy.first().first().value<Window *>();
    QVERIFY(copyWindow);
    if (workspace()->activeWindow() != copyWindow) {
        workspace()->activateWindow(copyWindow);
    }
    QCOMPARE(workspace()->activeWindow(), copyWindow);
    // Wait until the window has a surface so we can pass input to it
    QTRY_VERIFY(copyWindow->surface());
    Test::keyboardKeyPressed(KEY_C, waylandServer()->display()->nextSerial());
    Test::keyboardKeyReleased(KEY_C, waylandServer()->display()->nextSerial());
    clipboardChangedSpy.wait();

    // start the paste process
    std::unique_ptr<QProcess, ProcessKillBeforeDeleter> pasteProcess(new QProcess());
    QSignalSpy finishedSpy(pasteProcess.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished));
    QFETCH(QString, pastePlatform);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), pastePlatform);
    pasteProcess->setProcessEnvironment(environment);
    pasteProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    pasteProcess->setProgram(paste);
    pasteProcess->start();
    QVERIFY(pasteProcess->waitForStarted());

    windowAddedSpy.clear();
    Window *pasteWindow = nullptr;
    QVERIFY(windowAddedSpy.wait());
    pasteWindow = windowAddedSpy.last().first().value<Window *>();
    QCOMPARE(windowAddedSpy.count(), 1);
    QVERIFY(pasteWindow);

    if (workspace()->activeWindow() != pasteWindow) {
        QSignalSpy windowActivatedSpy(workspace(), &Workspace::windowActivated);
        workspace()->activateWindow(pasteWindow);
        QVERIFY(windowActivatedSpy.wait());
    }
    QTRY_COMPARE(workspace()->activeWindow(), pasteWindow);
    QVERIFY(finishedSpy.wait());
    QCOMPARE(finishedSpy.first().first().toInt(), 0);
}

WAYLANDTEST_MAIN(XwaylandSelectionsTest)
#include "xwayland_selections_test.moc"
