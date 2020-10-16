/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "../../xwl/databridge.h"

#include <KWaylandServer/datadevice_interface.h>

#include <QProcess>
#include <QProcessEnvironment>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xwayland_selections-0");

class XwaylandSelectionsTest : public QObject
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

void XwaylandSelectionsTest::initTestCase()
{
    QSKIP("Skipped as it fails for unknown reasons on build.kde.org");
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<QProcess::ExitStatus>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
//    QSignalSpy clipboardSyncDevicedCreated{waylandServer(), &WaylandServer::xclipboardSyncDataDeviceCreated};
//    QVERIFY(clipboardSyncDevicedCreated.isValid());
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
//    // wait till the xclipboard sync data device is created
//    if (clipboardSyncDevicedCreated.empty()) {
//        QVERIFY(clipboardSyncDevicedCreated.wait());
//    }
    // wait till the DataBridge sync data device is created
    while (Xwl::DataBridge::self()->dataDeviceIface() == nullptr) {
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    }
    QVERIFY(Xwl::DataBridge::self()->dataDeviceIface() != nullptr);
}

void XwaylandSelectionsTest::cleanup()
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
    QSignalSpy clipboardChangedSpy(Xwl::DataBridge::self()->dataDeviceIface(), &KWaylandServer::DataDeviceInterface::selectionChanged);
    QVERIFY(clipboardChangedSpy.isValid());

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // start the copy process
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
    QVERIFY(clientAddedSpy.wait());
    copyClient = clientAddedSpy.first().first().value<AbstractClient *>();
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
    delete m_pasteProcess;
    m_pasteProcess = nullptr;
    delete m_copyProcess;
    m_copyProcess = nullptr;
}

WAYLANDTEST_MAIN(XwaylandSelectionsTest)
#include "xwayland_selections_test.moc"
