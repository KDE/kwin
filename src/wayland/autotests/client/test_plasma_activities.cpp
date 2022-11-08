/*
    SPDX-FileCopyrightText: 2021 Kevin Ottens <kevin.ottens@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/plasmawindowmanagement_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/plasmawindowmanagement.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

class TestActivities : public QObject
{
    Q_OBJECT
public:
    explicit TestActivities(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testEnterLeaveActivity();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::PlasmaWindowManagementInterface *m_windowManagementInterface;
    KWaylandServer::PlasmaWindowInterface *m_windowInterface;

    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::PlasmaWindowManagement *m_windowManagement;
    KWayland::Client::PlasmaWindow *m_window;

    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-activities-0");

TestActivities::TestActivities(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestActivities::init()
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

    m_windowManagementInterface = new PlasmaWindowManagementInterface(m_display, m_display);

    QVERIFY(windowManagementSpy.wait());
    m_windowManagement =
        registry.createPlasmaWindowManagement(windowManagementSpy.first().first().value<quint32>(), windowManagementSpy.first().last().value<quint32>(), this);

    QSignalSpy windowSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);
    m_windowInterface = m_windowManagementInterface->createWindow(this, QUuid::createUuid());
    m_windowInterface->setPid(1337);

    QVERIFY(windowSpy.wait());
    m_window = windowSpy.first().first().value<KWayland::Client::PlasmaWindow *>();
}

void TestActivities::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }
    CLEANUP(m_compositor)
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
    CLEANUP(m_windowManagementInterface)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestActivities::testEnterLeaveActivity()
{
    QSignalSpy enterRequestedSpy(m_windowInterface, &KWaylandServer::PlasmaWindowInterface::enterPlasmaActivityRequested);
    m_window->requestEnterActivity(QStringLiteral("0-1"));
    enterRequestedSpy.wait();

    QCOMPARE(enterRequestedSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));

    QSignalSpy activityEnteredSpy(m_window, &KWayland::Client::PlasmaWindow::plasmaActivityEntered);

    // agree to the request
    m_windowInterface->addPlasmaActivity(QStringLiteral("0-1"));
    QCOMPARE(m_windowInterface->plasmaActivities().length(), 1);
    QCOMPARE(m_windowInterface->plasmaActivities().first(), QStringLiteral("0-1"));

    // check if the client received the enter
    activityEnteredSpy.wait();
    QCOMPARE(activityEnteredSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));
    QCOMPARE(m_window->plasmaActivities().length(), 1);
    QCOMPARE(m_window->plasmaActivities().first(), QStringLiteral("0-1"));

    // add another activity, server side
    m_windowInterface->addPlasmaActivity(QStringLiteral("0-3"));
    activityEnteredSpy.wait();
    QCOMPARE(activityEnteredSpy.takeFirst().at(0).toString(), QStringLiteral("0-3"));
    QCOMPARE(m_windowInterface->plasmaActivities().length(), 2);
    QCOMPARE(m_window->plasmaActivities().length(), 2);
    QCOMPARE(m_window->plasmaActivities()[1], QStringLiteral("0-3"));

    // remove an activity
    QSignalSpy leaveRequestedSpy(m_windowInterface, &KWaylandServer::PlasmaWindowInterface::leavePlasmaActivityRequested);
    m_window->requestLeaveActivity(QStringLiteral("0-1"));
    leaveRequestedSpy.wait();

    QCOMPARE(leaveRequestedSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));

    QSignalSpy activityLeftSpy(m_window, &KWayland::Client::PlasmaWindow::plasmaActivityLeft);

    // agree to the request
    m_windowInterface->removePlasmaActivity(QStringLiteral("0-1"));
    QCOMPARE(m_windowInterface->plasmaActivities().length(), 1);
    QCOMPARE(m_windowInterface->plasmaActivities().first(), QStringLiteral("0-3"));

    // check if the client received the leave
    activityLeftSpy.wait();
    QCOMPARE(activityLeftSpy.takeFirst().at(0).toString(), QStringLiteral("0-1"));
    QCOMPARE(m_window->plasmaActivities().length(), 1);
    QCOMPARE(m_window->plasmaActivities().first(), QStringLiteral("0-3"));
}

QTEST_GUILESS_MAIN(TestActivities)
#include "test_plasma_activities.moc"
