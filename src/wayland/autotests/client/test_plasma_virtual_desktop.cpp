/*
    SPDX-FileCopyrightText: 2018 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/plasmavirtualdesktop_interface.h"
#include "wayland/plasmawindowmanagement_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/plasmavirtualdesktop.h"
#include "KWayland/Client/plasmawindowmanagement.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

class TestVirtualDesktop : public QObject
{
    Q_OBJECT
public:
    explicit TestVirtualDesktop(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testSetRows();
    void testConnectNewClient();
    void testDestroy();
    void testActivate();

    void testEnterLeaveDesktop();
    void testAllDesktops();
    void testCreateRequested();
    void testRemoveRequested();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::PlasmaVirtualDesktopManagementInterface *m_plasmaVirtualDesktopManagementInterface;
    KWaylandServer::PlasmaWindowManagementInterface *m_windowManagementInterface;
    KWaylandServer::PlasmaWindowInterface *m_windowInterface;

    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::PlasmaVirtualDesktopManagement *m_plasmaVirtualDesktopManagement;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::PlasmaWindowManagement *m_windowManagement;
    KWayland::Client::PlasmaWindow *m_window;

    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-virtual-desktop-0");

TestVirtualDesktop::TestVirtualDesktop(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestVirtualDesktop::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

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

    KWayland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);

    QSignalSpy plasmaVirtualDesktopManagementSpy(&registry, &KWayland::Client::Registry::plasmaVirtualDesktopManagementAnnounced);

    QSignalSpy windowManagementSpy(&registry, &KWayland::Client::Registry::plasmaWindowManagementAnnounced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_plasmaVirtualDesktopManagementInterface = new PlasmaVirtualDesktopManagementInterface(m_display, m_display);

    QVERIFY(plasmaVirtualDesktopManagementSpy.wait());
    m_plasmaVirtualDesktopManagement = registry.createPlasmaVirtualDesktopManagement(plasmaVirtualDesktopManagementSpy.first().first().value<quint32>(),
                                                                                     plasmaVirtualDesktopManagementSpy.first().last().value<quint32>(),
                                                                                     this);

    m_windowManagementInterface = new PlasmaWindowManagementInterface(m_display, m_display);
    m_windowManagementInterface->setPlasmaVirtualDesktopManagementInterface(m_plasmaVirtualDesktopManagementInterface);

    QVERIFY(windowManagementSpy.wait());
    m_windowManagement =
        registry.createPlasmaWindowManagement(windowManagementSpy.first().first().value<quint32>(), windowManagementSpy.first().last().value<quint32>(), this);

    QSignalSpy windowSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);
    m_windowInterface = m_windowManagementInterface->createWindow(this, QUuid::createUuid());
    m_windowInterface->setPid(1337);

    QVERIFY(windowSpy.wait());
    m_window = windowSpy.first().first().value<KWayland::Client::PlasmaWindow *>();
}

void TestVirtualDesktop::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_plasmaVirtualDesktopManagement)
    CLEANUP(m_windowInterface)
    CLEANUP(m_windowManagement)
    CLEANUP(m_queue)
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
    CLEANUP(m_compositorInterface)
    CLEANUP(m_plasmaVirtualDesktopManagementInterface)
    CLEANUP(m_windowManagementInterface)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestVirtualDesktop::testCreate()
{
    QSignalSpy desktopCreatedSpy(m_plasmaVirtualDesktopManagement, &KWayland::Client::PlasmaVirtualDesktopManagement::desktopCreated);
    QSignalSpy managementDoneSpy(m_plasmaVirtualDesktopManagement, &KWayland::Client::PlasmaVirtualDesktopManagement::done);

    // on this createDesktop bind() isn't called already, the desktopadded signals will be sent after bind happened
    KWaylandServer::PlasmaVirtualDesktopInterface *desktop1Int = m_plasmaVirtualDesktopManagementInterface->createDesktop(QStringLiteral("0-1"));
    desktop1Int->setName("Desktop 1");

    desktopCreatedSpy.wait();
    QList<QVariant> arguments = desktopCreatedSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("0-1"));
    QCOMPARE(arguments.at(1).toUInt(), (quint32)0);
    m_plasmaVirtualDesktopManagementInterface->sendDone();
    managementDoneSpy.wait();

    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 1);

    KWayland::Client::PlasmaVirtualDesktop *desktop1 = m_plasmaVirtualDesktopManagement->desktops().first();
    QSignalSpy desktop1DoneSpy(desktop1, &KWayland::Client::PlasmaVirtualDesktop::done);
    desktop1Int->sendDone();
    desktop1DoneSpy.wait();

    QCOMPARE(desktop1->id(), QStringLiteral("0-1"));
    QCOMPARE(desktop1->name(), QStringLiteral("Desktop 1"));

    // on those createDesktop the bind will already be done
    KWaylandServer::PlasmaVirtualDesktopInterface *desktop2Int = m_plasmaVirtualDesktopManagementInterface->createDesktop(QStringLiteral("0-2"));
    desktop2Int->setName("Desktop 2");
    desktopCreatedSpy.wait();
    arguments = desktopCreatedSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("0-2"));
    QCOMPARE(arguments.at(1).toUInt(), (quint32)1);
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 2);

    KWaylandServer::PlasmaVirtualDesktopInterface *desktop3Int = m_plasmaVirtualDesktopManagementInterface->createDesktop(QStringLiteral("0-3"));
    desktop3Int->setName("Desktop 3");
    desktopCreatedSpy.wait();
    arguments = desktopCreatedSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("0-3"));
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 3);

    m_plasmaVirtualDesktopManagementInterface->sendDone();
    managementDoneSpy.wait();

    // get the clients
    KWayland::Client::PlasmaVirtualDesktop *desktop2 = m_plasmaVirtualDesktopManagement->desktops()[1];
    QSignalSpy desktop2DoneSpy(desktop2, &KWayland::Client::PlasmaVirtualDesktop::done);
    desktop2Int->sendDone();
    desktop2DoneSpy.wait();

    KWayland::Client::PlasmaVirtualDesktop *desktop3 = m_plasmaVirtualDesktopManagement->desktops()[2];
    QSignalSpy desktop3DoneSpy(desktop3, &KWayland::Client::PlasmaVirtualDesktop::done);
    desktop3Int->sendDone();
    desktop3DoneSpy.wait();

    QCOMPARE(desktop1->id(), QStringLiteral("0-1"));
    QCOMPARE(desktop1->name(), QStringLiteral("Desktop 1"));

    QCOMPARE(desktop2->id(), QStringLiteral("0-2"));
    QCOMPARE(desktop2->name(), QStringLiteral("Desktop 2"));

    QCOMPARE(desktop3->id(), QStringLiteral("0-3"));
    QCOMPARE(desktop3->name(), QStringLiteral("Desktop 3"));

    // coherence of order between client and server
    QCOMPARE(m_plasmaVirtualDesktopManagementInterface->desktops().length(), 3);
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 3);

    for (int i = 0; i < m_plasmaVirtualDesktopManagement->desktops().length(); ++i) {
        QCOMPARE(m_plasmaVirtualDesktopManagementInterface->desktops().at(i)->id(), m_plasmaVirtualDesktopManagement->desktops().at(i)->id());
    }
}

void TestVirtualDesktop::testSetRows()
{
    // rebuild some desktops
    testCreate();

    QSignalSpy rowsChangedSpy(m_plasmaVirtualDesktopManagement, &KWayland::Client::PlasmaVirtualDesktopManagement::rowsChanged);

    m_plasmaVirtualDesktopManagementInterface->setRows(3);
    QVERIFY(rowsChangedSpy.wait());
    QCOMPARE(m_plasmaVirtualDesktopManagement->rows(), 3);
}

void TestVirtualDesktop::testConnectNewClient()
{
    // rebuild some desktops
    testCreate();

    KWayland::Client::Registry registry;
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QSignalSpy plasmaVirtualDesktopManagementSpy(&registry, &KWayland::Client::Registry::plasmaVirtualDesktopManagementAnnounced);

    QVERIFY(plasmaVirtualDesktopManagementSpy.wait());

    KWayland::Client::PlasmaVirtualDesktopManagement *otherPlasmaVirtualDesktopManagement =
        registry.createPlasmaVirtualDesktopManagement(plasmaVirtualDesktopManagementSpy.first().first().value<quint32>(),
                                                      plasmaVirtualDesktopManagementSpy.first().last().value<quint32>(),
                                                      this);

    QSignalSpy managementDoneSpy(otherPlasmaVirtualDesktopManagement, &KWayland::Client::PlasmaVirtualDesktopManagement::done);

    managementDoneSpy.wait();
    QCOMPARE(otherPlasmaVirtualDesktopManagement->desktops().length(), 3);

    delete otherPlasmaVirtualDesktopManagement;
}

void TestVirtualDesktop::testDestroy()
{
    // rebuild some desktops
    testCreate();

    KWaylandServer::PlasmaVirtualDesktopInterface *desktop1Int = m_plasmaVirtualDesktopManagementInterface->desktops().first();
    KWayland::Client::PlasmaVirtualDesktop *desktop1 = m_plasmaVirtualDesktopManagement->desktops().first();

    QSignalSpy desktop1IntDestroyedSpy(desktop1Int, &QObject::destroyed);
    QSignalSpy desktop1DestroyedSpy(desktop1, &QObject::destroyed);
    QSignalSpy desktop1RemovedSpy(desktop1, &KWayland::Client::PlasmaVirtualDesktop::removed);
    m_plasmaVirtualDesktopManagementInterface->removeDesktop(QStringLiteral("0-1"));

    // test that both server and client desktoip interfaces go away
    desktop1IntDestroyedSpy.wait();
    desktop1RemovedSpy.wait();
    desktop1DestroyedSpy.wait();

    // coherence of order between client and server
    QCOMPARE(m_plasmaVirtualDesktopManagementInterface->desktops().length(), 2);
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 2);

    for (int i = 0; i < m_plasmaVirtualDesktopManagement->desktops().length(); ++i) {
        QCOMPARE(m_plasmaVirtualDesktopManagementInterface->desktops().at(i)->id(), m_plasmaVirtualDesktopManagement->desktops().at(i)->id());
    }

    // Test the desktopRemoved signal of the manager, remove another desktop as the signals can't be tested at the same time
    QSignalSpy desktopManagerRemovedSpy(m_plasmaVirtualDesktopManagement, &KWayland::Client::PlasmaVirtualDesktopManagement::desktopRemoved);
    m_plasmaVirtualDesktopManagementInterface->removeDesktop(QStringLiteral("0-2"));
    desktopManagerRemovedSpy.wait();
    QCOMPARE(desktopManagerRemovedSpy.takeFirst().at(0).toString(), QStringLiteral("0-2"));

    QCOMPARE(m_plasmaVirtualDesktopManagementInterface->desktops().length(), 1);
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().length(), 1);
}

void TestVirtualDesktop::testActivate()
{
    // rebuild some desktops
    testCreate();

    KWaylandServer::PlasmaVirtualDesktopInterface *desktop1Int = m_plasmaVirtualDesktopManagementInterface->desktops().first();
    KWayland::Client::PlasmaVirtualDesktop *desktop1 = m_plasmaVirtualDesktopManagement->desktops().first();
    QVERIFY(desktop1->isActive());
    QVERIFY(desktop1Int->isActive());

    KWaylandServer::PlasmaVirtualDesktopInterface *desktop2Int = m_plasmaVirtualDesktopManagementInterface->desktops()[1];
    KWayland::Client::PlasmaVirtualDesktop *desktop2 = m_plasmaVirtualDesktopManagement->desktops()[1];
    QVERIFY(!desktop2Int->isActive());

    QSignalSpy requestActivateSpy(desktop2Int, &KWaylandServer::PlasmaVirtualDesktopInterface::activateRequested);
    QSignalSpy activatedSpy(desktop2, &KWayland::Client::PlasmaVirtualDesktop::activated);

    desktop2->requestActivate();
    requestActivateSpy.wait();

    // This simulates a compositor which supports only one active desktop at a time
    for (auto *deskInt : m_plasmaVirtualDesktopManagementInterface->desktops()) {
        if (deskInt->id() == desktop2->id()) {
            deskInt->setActive(true);
        } else {
            deskInt->setActive(false);
        }
    }
    activatedSpy.wait();

    // correct state in the server
    QVERIFY(desktop2Int->isActive());
    QVERIFY(!desktop1Int->isActive());
    // correct state in the client
    QVERIFY(desktop2Int->isActive());
    QVERIFY(!desktop1Int->isActive());

    // Test the deactivated signal
    QSignalSpy deactivatedSpy(desktop2, &KWayland::Client::PlasmaVirtualDesktop::deactivated);

    for (auto *deskInt : m_plasmaVirtualDesktopManagementInterface->desktops()) {
        if (deskInt->id() == desktop1->id()) {
            deskInt->setActive(true);
        } else {
            deskInt->setActive(false);
        }
    }
    deactivatedSpy.wait();
}

void TestVirtualDesktop::testEnterLeaveDesktop()
{
    testCreate();

    QSignalSpy enterRequestedSpy(m_windowInterface, &KWaylandServer::PlasmaWindowInterface::enterPlasmaVirtualDesktopRequested);
    m_window->requestEnterVirtualDesktop(QStringLiteral("0-1"));
    enterRequestedSpy.wait();

    QCOMPARE(enterRequestedSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));

    QSignalSpy virtualDesktopEnteredSpy(m_window, &KWayland::Client::PlasmaWindow::plasmaVirtualDesktopEntered);

    // agree to the request
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("0-1"));
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().first(), QStringLiteral("0-1"));

    // check if the client received the enter
    virtualDesktopEnteredSpy.wait();
    QCOMPARE(virtualDesktopEnteredSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_window->plasmaVirtualDesktops().first(), QStringLiteral("0-1"));

    // add another desktop, server side
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("0-3"));
    virtualDesktopEnteredSpy.wait();
    QCOMPARE(virtualDesktopEnteredSpy.takeFirst().at(0).toString(), QStringLiteral("0-3"));
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().length(), 2);
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 2);
    QCOMPARE(m_window->plasmaVirtualDesktops()[1], QStringLiteral("0-3"));

    // try to add an invalid desktop
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("invalid"));
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 2);

    // remove a desktop
    QSignalSpy leaveRequestedSpy(m_windowInterface, &KWaylandServer::PlasmaWindowInterface::leavePlasmaVirtualDesktopRequested);
    m_window->requestLeaveVirtualDesktop(QStringLiteral("0-1"));
    leaveRequestedSpy.wait();

    QCOMPARE(leaveRequestedSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));

    QSignalSpy virtualDesktopLeftSpy(m_window, &KWayland::Client::PlasmaWindow::plasmaVirtualDesktopLeft);

    // agree to the request
    m_windowInterface->removePlasmaVirtualDesktop(QStringLiteral("0-1"));
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().first(), QStringLiteral("0-3"));

    // check if the client received the leave
    virtualDesktopLeftSpy.wait();
    QCOMPARE(virtualDesktopLeftSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_window->plasmaVirtualDesktops().first(), QStringLiteral("0-3"));

    // Destroy desktop 2
    m_plasmaVirtualDesktopManagementInterface->removeDesktop(QStringLiteral("0-3"));
    // the window should receive a left signal from the destroyed desktop
    virtualDesktopLeftSpy.wait();

    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 0);
}

void TestVirtualDesktop::testAllDesktops()
{
    testCreate();
    QSignalSpy virtualDesktopEnteredSpy(m_window, &KWayland::Client::PlasmaWindow::plasmaVirtualDesktopEntered);
    QSignalSpy virtualDesktopLeftSpy(m_window, &KWayland::Client::PlasmaWindow::plasmaVirtualDesktopLeft);

    // in the beginning the window is on desktop 1 and desktop 3
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("0-1"));
    m_windowInterface->addPlasmaVirtualDesktop(QStringLiteral("0-3"));
    virtualDesktopEnteredSpy.wait();

    // setting on all desktops
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 2);
    m_windowInterface->setOnAllDesktops(true);
    // setting on all desktops, the window will leave every desktop

    virtualDesktopLeftSpy.wait();
    QCOMPARE(virtualDesktopLeftSpy.count(), 2);
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 0);
    QVERIFY(m_window->isOnAllDesktops());

    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 0);
    QVERIFY(m_window->isOnAllDesktops());

    // return to the active desktop (0-1)
    m_windowInterface->setOnAllDesktops(false);
    virtualDesktopEnteredSpy.wait();
    QCOMPARE(m_window->plasmaVirtualDesktops().length(), 1);
    QCOMPARE(m_windowInterface->plasmaVirtualDesktops().first(), QStringLiteral("0-1"));
    QVERIFY(!m_window->isOnAllDesktops());
}

void TestVirtualDesktop::testCreateRequested()
{
    // rebuild some desktops
    testCreate();

    QSignalSpy desktopCreateRequestedSpy(m_plasmaVirtualDesktopManagementInterface,
                                         &KWaylandServer::PlasmaVirtualDesktopManagementInterface::desktopCreateRequested);
    QSignalSpy desktopCreatedSpy(m_plasmaVirtualDesktopManagement, &KWayland::Client::PlasmaVirtualDesktopManagement::desktopCreated);

    // listen for createdRequested
    m_plasmaVirtualDesktopManagement->requestCreateVirtualDesktop(QStringLiteral("Desktop"), 1);
    desktopCreateRequestedSpy.wait();
    QCOMPARE(desktopCreateRequestedSpy.first().first().toString(), QStringLiteral("Desktop"));
    QCOMPARE(desktopCreateRequestedSpy.first().at(1).toUInt(), (quint32)1);

    // actually create
    m_plasmaVirtualDesktopManagementInterface->createDesktop(QStringLiteral("0-4"), 1);
    KWaylandServer::PlasmaVirtualDesktopInterface *desktopInt = m_plasmaVirtualDesktopManagementInterface->desktops().at(1);

    QCOMPARE(desktopInt->id(), QStringLiteral("0-4"));
    desktopInt->setName(QStringLiteral("Desktop"));

    desktopCreatedSpy.wait();

    QCOMPARE(desktopCreatedSpy.first().first().toString(), QStringLiteral("0-4"));
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().count(), 4);

    KWayland::Client::PlasmaVirtualDesktop *desktop = m_plasmaVirtualDesktopManagement->desktops().at(1);
    QSignalSpy desktopDoneSpy(desktop, &KWayland::Client::PlasmaVirtualDesktop::done);
    desktopInt->sendDone();
    // desktopDoneSpy.wait();
    // check the order is correct
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().at(0)->id(), QStringLiteral("0-1"));
    QCOMPARE(desktop->id(), QStringLiteral("0-4"));
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().at(2)->id(), QStringLiteral("0-2"));
    QCOMPARE(m_plasmaVirtualDesktopManagement->desktops().at(3)->id(), QStringLiteral("0-3"));
}

void TestVirtualDesktop::testRemoveRequested()
{
    // rebuild some desktops
    testCreate();

    QSignalSpy desktopRemoveRequestedSpy(m_plasmaVirtualDesktopManagementInterface,
                                         &KWaylandServer::PlasmaVirtualDesktopManagementInterface::desktopRemoveRequested);

    // request a remove, just check the request arrived, ignore the request.
    m_plasmaVirtualDesktopManagement->requestRemoveVirtualDesktop(QStringLiteral("0-1"));
    desktopRemoveRequestedSpy.wait();
    QCOMPARE(desktopRemoveRequestedSpy.first().first().toString(), QStringLiteral("0-1"));
}

QTEST_GUILESS_MAIN(TestVirtualDesktop)
#include "test_plasma_virtual_desktop.moc"
