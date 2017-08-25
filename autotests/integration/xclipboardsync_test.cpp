/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "shell_client.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Server/datadevice_interface.h>

#include <QProcess>
#include <QProcessEnvironment>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xclipboard_sync-0");

class XClipboardSyncTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testSync_data();
    void testSync();

private:
    QProcess *m_copyProcess = nullptr;
    QProcess *m_pasteProcess = nullptr;
};

void XClipboardSyncTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<QProcess::ExitStatus>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
    // wait till the xclipboard sync data device is created
    while (waylandServer()->xclipboardSyncDataDevice().isNull()) {
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    }
    QVERIFY(!waylandServer()->xclipboardSyncDataDevice().isNull());
}

void XClipboardSyncTest::cleanup()
{
    if (m_copyProcess) {
        m_copyProcess->terminate();
        QVERIFY(m_copyProcess->waitForFinished());
        m_copyProcess = nullptr;
    }
    if (m_pasteProcess) {
        m_pasteProcess->terminate();
        QVERIFY(m_pasteProcess->waitForFinished());
        m_pasteProcess = nullptr;
    }
}

void XClipboardSyncTest::testSync_data()
{
    QTest::addColumn<QString>("copyPlatform");
    QTest::addColumn<QString>("pastePlatform");

    QTest::newRow("x11-wayland") << QStringLiteral("xcb") << QStringLiteral("wayland");
    QTest::newRow("wayland-x11") << QStringLiteral("wayland") << QStringLiteral("xcb");
}

void XClipboardSyncTest::testSync()
{
    // this test verifies the syncing of X11 to Wayland clipboard
    const QString copy = QFINDTESTDATA(QStringLiteral("copy"));
    QVERIFY(!copy.isEmpty());
    const QString paste = QFINDTESTDATA(QStringLiteral("paste"));
    QVERIFY(!paste.isEmpty());

    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    QSignalSpy shellClientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(shellClientAddedSpy.isValid());
    QSignalSpy clipboardChangedSpy(waylandServer()->xclipboardSyncDataDevice().data(), &KWayland::Server::DataDeviceInterface::selectionChanged);
    QVERIFY(clipboardChangedSpy.isValid());

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QFETCH(QString, copyPlatform);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), copyPlatform);
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), s_socketName);
    m_copyProcess = new QProcess();
    m_copyProcess->setProcessEnvironment(environment);
    m_copyProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    m_copyProcess->setProgram(copy);
    m_copyProcess->start();
    QVERIFY(m_copyProcess->waitForStarted());

    AbstractClient *copyClient = nullptr;
    if (copyPlatform == QLatin1String("xcb")) {
        QVERIFY(clientAddedSpy.wait());
        copyClient = clientAddedSpy.first().first().value<AbstractClient*>();
    } else {
        QVERIFY(shellClientAddedSpy.wait());
        copyClient = shellClientAddedSpy.first().first().value<AbstractClient*>();
    }
    QVERIFY(copyClient);
    if (workspace()->activeClient() != copyClient) {
        workspace()->activateClient(copyClient);
    }
    QCOMPARE(workspace()->activeClient(), copyClient);
    if (copyPlatform == QLatin1String("xcb")) {
        QVERIFY(clipboardChangedSpy.isEmpty());
        QVERIFY(clipboardChangedSpy.wait());
    } else {
        // TODO: it would be better to be able to connect to a signal, instead of waiting
        // the idea is to make sure that the clipboard is updated, thus we need to give it
        // enough time before starting the paste process which creates another window
        QTest::qWait(250);
    }

    // start the paste process
    m_pasteProcess = new QProcess();
    QSignalSpy finishedSpy(m_pasteProcess, static_cast<void(QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished));
    QVERIFY(finishedSpy.isValid());
    QFETCH(QString, pastePlatform);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), pastePlatform);
    m_pasteProcess->setProcessEnvironment(environment);
    m_pasteProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    m_pasteProcess->setProgram(paste);
    m_pasteProcess->start();
    QVERIFY(m_pasteProcess->waitForStarted());

    AbstractClient *pasteClient = nullptr;
    if (pastePlatform == QLatin1String("xcb")) {
        QVERIFY(clientAddedSpy.wait());
        pasteClient = clientAddedSpy.last().first().value<AbstractClient*>();
    } else {
        QVERIFY(shellClientAddedSpy.wait());
        pasteClient = shellClientAddedSpy.last().first().value<AbstractClient*>();
    }
    QCOMPARE(clientAddedSpy.count(), 1);
    QCOMPARE(shellClientAddedSpy.count(), 1);
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
    delete m_pasteProcess;
    m_pasteProcess = nullptr;
}

WAYLANDTEST_MAIN(XClipboardSyncTest)
#include "xclipboardsync_test.moc"
