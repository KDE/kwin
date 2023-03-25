/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/plasmawindowmanagement_interface.h"
#include "wayland/surface_interface.h"
#include <wayland-plasma-window-management-client-protocol.h>

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/plasmawindowmanagement.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

typedef void (KWaylandServer::PlasmaWindowInterface::*ServerWindowSignal)();
Q_DECLARE_METATYPE(ServerWindowSignal)
typedef void (KWaylandServer::PlasmaWindowInterface::*ServerWindowBooleanSignal)(bool);
Q_DECLARE_METATYPE(ServerWindowBooleanSignal)
typedef void (KWayland::Client::PlasmaWindow::*ClientWindowVoidSetter)();
Q_DECLARE_METATYPE(ClientWindowVoidSetter)

class TestWindowManagement : public QObject
{
    Q_OBJECT
public:
    explicit TestWindowManagement(QObject *parent = nullptr);
private Q_SLOTS:
    void init();

    void testWindowTitle();
    void testReallyLongTitle();
    void testMinimizedGeometry();
    void testUseAfterUnmap();
    void testServerDelete();
    void testActiveWindowOnUnmapped();
    void testDeleteActiveWindow();
    void testCreateAfterUnmap();
    void testRequests_data();
    void testRequests();
    void testRequestsBoolean_data();
    void testRequestsBoolean();
    void testKeepAbove();
    void testKeepBelow();
    void testShowingDesktop();
    void testRequestShowingDesktop_data();
    void testRequestShowingDesktop();
    void testParentWindow();
    void testGeometry();
    void testIcon();
    void testPid();
    void testApplicationMenu();

    void cleanup();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::PlasmaWindowManagementInterface *m_windowManagementInterface;
    KWaylandServer::PlasmaWindowInterface *m_windowInterface;
    QPointer<KWaylandServer::SurfaceInterface> m_surfaceInterface;

    KWayland::Client::Surface *m_surface = nullptr;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::PlasmaWindowManagement *m_windowManagement;
    KWayland::Client::PlasmaWindow *m_window;
    QThread *m_thread;
    KWayland::Client::Registry *m_registry;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-windowmanagement-0");

TestWindowManagement::TestWindowManagement(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestWindowManagement::init()
{
    using namespace KWaylandServer;
    qRegisterMetaType<KWaylandServer::PlasmaWindowManagementInterface::ShowingDesktopState>("ShowingDesktopState");
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

    m_registry = new KWayland::Client::Registry(this);
    QSignalSpy compositorSpy(m_registry, &KWayland::Client::Registry::compositorAnnounced);

    QSignalSpy windowManagementSpy(m_registry, &KWayland::Client::Registry::plasmaWindowManagementAnnounced);

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection->display());
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_windowManagementInterface = new PlasmaWindowManagementInterface(m_display, m_display);

    QVERIFY(windowManagementSpy.wait());
    m_windowManagement = m_registry->createPlasmaWindowManagement(windowManagementSpy.first().first().value<quint32>(),
                                                                  windowManagementSpy.first().last().value<quint32>(),
                                                                  this);

    QSignalSpy windowSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);
    m_windowInterface = m_windowManagementInterface->createWindow(this, QUuid::createUuid());
    m_windowInterface->setPid(1337);

    QVERIFY(windowSpy.wait());
    m_window = windowSpy.first().first().value<KWayland::Client::PlasmaWindow *>();

    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);
    m_surface = m_compositor->createSurface(this);
    QVERIFY(m_surface);

    QVERIFY(serverSurfaceCreated.wait());
    m_surfaceInterface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(m_surfaceInterface);
}

void TestWindowManagement::testWindowTitle()
{
    m_windowInterface->setTitle(QStringLiteral("Test Title"));

    QSignalSpy titleSpy(m_window, &KWayland::Client::PlasmaWindow::titleChanged);

    QVERIFY(titleSpy.wait());

    QCOMPARE(m_window->title(), QString::fromUtf8("Test Title"));
}

void TestWindowManagement::testReallyLongTitle()
{
    QString title;
    title.fill(QLatin1Char('t'), 500000);
    m_windowInterface->setTitle(title);

    QSignalSpy titleSpy(m_window, &KWayland::Client::PlasmaWindow::titleChanged);

    QVERIFY(titleSpy.wait());
    QVERIFY(m_window->title().startsWith("t"));
}

void TestWindowManagement::testMinimizedGeometry()
{
    m_window->setMinimizedGeometry(m_surface, QRect(5, 10, 100, 200));

    QSignalSpy geometrySpy(m_windowInterface, &KWaylandServer::PlasmaWindowInterface::minimizedGeometriesChanged);

    QVERIFY(geometrySpy.wait());
    QCOMPARE(m_windowInterface->minimizedGeometries().values().first(), QRect(5, 10, 100, 200));

    m_window->unsetMinimizedGeometry(m_surface);
    QVERIFY(geometrySpy.wait());
    QVERIFY(m_windowInterface->minimizedGeometries().isEmpty());
}

void TestWindowManagement::cleanup()
{
    if (m_surface) {
        delete m_surface;
        m_surface = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_windowManagement) {
        delete m_windowManagement;
        m_windowManagement = nullptr;
    }
    if (m_registry) {
        delete m_registry;
        m_registry = nullptr;
    }
    if (m_thread) {
        if (m_connection) {
            m_connection->flush();
            m_connection->deleteLater();
        }
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    m_connection = nullptr;

    m_display->dispatchEvents();

    QVERIFY(m_surfaceInterface.isNull());

    delete m_display;
    m_display = nullptr;

    // these are the children of the display
    m_windowManagementInterface = nullptr;
    m_windowInterface = nullptr;
}

void TestWindowManagement::testUseAfterUnmap()
{
    // this test verifies that when the client uses a window after it's unmapped, things don't break
    QSignalSpy unmappedSpy(m_window, &KWayland::Client::PlasmaWindow::unmapped);
    QSignalSpy destroyedSpy(m_window, &QObject::destroyed);
    m_windowInterface->deleteLater();
    m_windowInterface = nullptr;
    m_window->requestClose();
    QVERIFY(unmappedSpy.wait());
    QVERIFY(destroyedSpy.wait());
    m_window = nullptr;
}

void TestWindowManagement::testServerDelete()
{
    QSignalSpy unmappedSpy(m_window, &KWayland::Client::PlasmaWindow::unmapped);
    QSignalSpy destroyedSpy(m_window, &QObject::destroyed);
    delete m_windowInterface;
    m_windowInterface = nullptr;
    QVERIFY(unmappedSpy.wait());
    QVERIFY(destroyedSpy.wait());
    m_window = nullptr;
}

void TestWindowManagement::testActiveWindowOnUnmapped()
{
    // This test verifies that unmapping the active window changes the active window.
    // first make the window active
    QVERIFY(!m_windowManagement->activeWindow());
    QVERIFY(!m_window->isActive());
    QSignalSpy activeWindowChangedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::activeWindowChanged);
    m_windowInterface->setActive(true);
    QVERIFY(activeWindowChangedSpy.wait());
    QCOMPARE(m_windowManagement->activeWindow(), m_window);
    QVERIFY(m_window->isActive());

    // now unmap should change the active window
    QSignalSpy destroyedSpy(m_window, &QObject::destroyed);
    QSignalSpy serverDestroyedSpy(m_windowInterface, &QObject::destroyed);
    delete m_windowInterface;
    m_windowInterface = nullptr;
    QVERIFY(activeWindowChangedSpy.wait());
    QVERIFY(!m_windowManagement->activeWindow());
}

void TestWindowManagement::testDeleteActiveWindow()
{
    // This test verifies that deleting the active window on client side changes the active window
    // first make the window active
    QVERIFY(!m_windowManagement->activeWindow());
    QVERIFY(!m_window->isActive());
    QSignalSpy activeWindowChangedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::activeWindowChanged);
    m_windowInterface->setActive(true);
    QVERIFY(activeWindowChangedSpy.wait());
    QCOMPARE(activeWindowChangedSpy.count(), 1);
    QCOMPARE(m_windowManagement->activeWindow(), m_window);
    QVERIFY(m_window->isActive());

    // delete the client side window - that's semantically kind of wrong, but shouldn't make our code crash
    delete m_window;
    m_window = nullptr;
    QCOMPARE(activeWindowChangedSpy.count(), 2);
    QVERIFY(!m_windowManagement->activeWindow());
}

void TestWindowManagement::testCreateAfterUnmap()
{
    // this test verifies that we don't get a protocol error on client side when creating an already unmapped window.
    QCOMPARE(m_windowManagement->children().count(), 1);

    QSignalSpy windowAddedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);

    // create and unmap in one go
    // client will first handle the create, the unmap will be sent once the server side is bound
    auto serverWindow = m_windowManagementInterface->createWindow(m_windowManagementInterface, QUuid::createUuid());
    delete serverWindow;
    QCOMPARE(m_windowManagementInterface->children().count(), 0);

    windowAddedSpy.wait();
    auto window = dynamic_cast<KWayland::Client::PlasmaWindow *>(m_windowManagement->children().last());
    QVERIFY(window);
    // now this is not yet on the server, on the server it will be after next roundtrip
    // which we can trigger by waiting for destroy of the newly created window.
    // why destroy? Because we will get the unmap which triggers a destroy
    QSignalSpy clientDestroyedSpy(window, &QObject::destroyed);
    QVERIFY(clientDestroyedSpy.wait());
    // Verify that any wrappers created for our temporary window are now gone
    QCOMPARE(m_windowManagement->children().count(), 1);
}

void TestWindowManagement::testRequests_data()
{
    using namespace KWaylandServer;
    QTest::addColumn<ServerWindowSignal>("changedSignal");
    QTest::addColumn<ClientWindowVoidSetter>("requester");

    QTest::newRow("close") << &PlasmaWindowInterface::closeRequested << &KWayland::Client::PlasmaWindow::requestClose;
    QTest::newRow("move") << &PlasmaWindowInterface::moveRequested << &KWayland::Client::PlasmaWindow::requestMove;
    QTest::newRow("resize") << &PlasmaWindowInterface::resizeRequested << &KWayland::Client::PlasmaWindow::requestResize;
}

void TestWindowManagement::testRequests()
{
    // this test case verifies all the different requests on a PlasmaWindow
    QFETCH(ServerWindowSignal, changedSignal);
    QSignalSpy requestSpy(m_windowInterface, changedSignal);
    QFETCH(ClientWindowVoidSetter, requester);
    (m_window->*(requester))();
    QVERIFY(requestSpy.wait());
}

void TestWindowManagement::testRequestsBoolean_data()
{
    using namespace KWaylandServer;
    QTest::addColumn<ServerWindowBooleanSignal>("changedSignal");
    QTest::addColumn<int>("flag");

    QTest::newRow("activate") << &PlasmaWindowInterface::activeRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
    QTest::newRow("minimized") << &PlasmaWindowInterface::minimizedRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
    QTest::newRow("maximized") << &PlasmaWindowInterface::maximizedRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED);
    QTest::newRow("fullscreen") << &PlasmaWindowInterface::fullscreenRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN);
    QTest::newRow("keepAbove") << &PlasmaWindowInterface::keepAboveRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE);
    QTest::newRow("keepBelow") << &PlasmaWindowInterface::keepBelowRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW);
    QTest::newRow("demandsAttention") << &PlasmaWindowInterface::demandsAttentionRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION);
    QTest::newRow("closeable") << &PlasmaWindowInterface::closeableRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE);
    QTest::newRow("minimizable") << &PlasmaWindowInterface::minimizeableRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE);
    QTest::newRow("maximizable") << &PlasmaWindowInterface::maximizeableRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE);
    QTest::newRow("fullscreenable") << &PlasmaWindowInterface::fullscreenableRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE);
    QTest::newRow("skiptaskbar") << &PlasmaWindowInterface::skipTaskbarRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR);
    QTest::newRow("skipSwitcher") << &PlasmaWindowInterface::skipSwitcherRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER);
    QTest::newRow("shadeable") << &PlasmaWindowInterface::shadeableRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE);
    QTest::newRow("shaded") << &PlasmaWindowInterface::shadedRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED);
    QTest::newRow("movable") << &PlasmaWindowInterface::movableRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE);
    QTest::newRow("resizable") << &PlasmaWindowInterface::resizableRequested << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE);
    QTest::newRow("virtualDesktopChangeable") << &PlasmaWindowInterface::virtualDesktopChangeableRequested
                                              << int(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE);
}

void TestWindowManagement::testRequestsBoolean()
{
    // this test case verifies all the different requests on a PlasmaWindow
    QFETCH(ServerWindowBooleanSignal, changedSignal);
    QSignalSpy requestSpy(m_windowInterface, changedSignal);
    QFETCH(int, flag);
    org_kde_plasma_window_set_state(*m_window, flag, flag);
    QVERIFY(requestSpy.wait());
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.first().first().toBool(), true);
    org_kde_plasma_window_set_state(*m_window, flag, 0);
    QVERIFY(requestSpy.wait());
    QCOMPARE(requestSpy.count(), 2);
    QCOMPARE(requestSpy.last().first().toBool(), false);
}

void TestWindowManagement::testShowingDesktop()
{
    using namespace KWaylandServer;
    // this test verifies setting the showing desktop state
    QVERIFY(!m_windowManagement->isShowingDesktop());
    QSignalSpy showingDesktopChangedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::showingDesktopChanged);
    m_windowManagementInterface->setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState::Enabled);
    QVERIFY(showingDesktopChangedSpy.wait());
    QCOMPARE(showingDesktopChangedSpy.count(), 1);
    QCOMPARE(showingDesktopChangedSpy.first().first().toBool(), true);
    QVERIFY(m_windowManagement->isShowingDesktop());
    // setting to same should not change
    m_windowManagementInterface->setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState::Enabled);
    QVERIFY(!showingDesktopChangedSpy.wait(100));
    QCOMPARE(showingDesktopChangedSpy.count(), 1);
    // setting to other state should change
    m_windowManagementInterface->setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState::Disabled);
    QVERIFY(showingDesktopChangedSpy.wait());
    QCOMPARE(showingDesktopChangedSpy.count(), 2);
    QCOMPARE(showingDesktopChangedSpy.first().first().toBool(), true);
    QCOMPARE(showingDesktopChangedSpy.last().first().toBool(), false);
    QVERIFY(!m_windowManagement->isShowingDesktop());
}

void TestWindowManagement::testRequestShowingDesktop_data()
{
    using namespace KWaylandServer;
    QTest::addColumn<bool>("value");
    QTest::addColumn<PlasmaWindowManagementInterface::ShowingDesktopState>("expectedValue");

    QTest::newRow("enable") << true << PlasmaWindowManagementInterface::ShowingDesktopState::Enabled;
    QTest::newRow("disable") << false << PlasmaWindowManagementInterface::ShowingDesktopState::Disabled;
}

void TestWindowManagement::testRequestShowingDesktop()
{
    // this test verifies requesting show desktop state
    using namespace KWaylandServer;
    QSignalSpy requestSpy(m_windowManagementInterface, &PlasmaWindowManagementInterface::requestChangeShowingDesktop);
    QFETCH(bool, value);
    m_windowManagement->setShowingDesktop(value);
    QVERIFY(requestSpy.wait());
    QCOMPARE(requestSpy.count(), 1);
    QTEST(requestSpy.first().first().value<PlasmaWindowManagementInterface::ShowingDesktopState>(), "expectedValue");
}

void TestWindowManagement::testKeepAbove()
{
    using namespace KWaylandServer;
    // this test verifies setting the keep above state
    QVERIFY(!m_window->isKeepAbove());
    QSignalSpy keepAboveChangedSpy(m_window, &KWayland::Client::PlasmaWindow::keepAboveChanged);
    m_windowInterface->setKeepAbove(true);
    QVERIFY(keepAboveChangedSpy.wait());
    QCOMPARE(keepAboveChangedSpy.count(), 1);
    QVERIFY(m_window->isKeepAbove());
    // setting to same should not change
    m_windowInterface->setKeepAbove(true);
    QVERIFY(!keepAboveChangedSpy.wait(100));
    QCOMPARE(keepAboveChangedSpy.count(), 1);
    // setting to other state should change
    m_windowInterface->setKeepAbove(false);
    QVERIFY(keepAboveChangedSpy.wait());
    QCOMPARE(keepAboveChangedSpy.count(), 2);
    QVERIFY(!m_window->isKeepAbove());
}

void TestWindowManagement::testKeepBelow()
{
    using namespace KWaylandServer;
    // this test verifies setting the keep below state
    QVERIFY(!m_window->isKeepBelow());
    QSignalSpy keepBelowChangedSpy(m_window, &KWayland::Client::PlasmaWindow::keepBelowChanged);
    m_windowInterface->setKeepBelow(true);
    QVERIFY(keepBelowChangedSpy.wait());
    QCOMPARE(keepBelowChangedSpy.count(), 1);
    QVERIFY(m_window->isKeepBelow());
    // setting to same should not change
    m_windowInterface->setKeepBelow(true);
    QVERIFY(!keepBelowChangedSpy.wait(100));
    QCOMPARE(keepBelowChangedSpy.count(), 1);
    // setting to other state should change
    m_windowInterface->setKeepBelow(false);
    QVERIFY(keepBelowChangedSpy.wait());
    QCOMPARE(keepBelowChangedSpy.count(), 2);
    QVERIFY(!m_window->isKeepBelow());
}

void TestWindowManagement::testParentWindow()
{
    // this test verifies the functionality of ParentWindows
    QCOMPARE(m_windowManagement->windows().count(), 1);
    auto parentWindow = m_windowManagement->windows().first();
    QVERIFY(parentWindow);
    QVERIFY(parentWindow->parentWindow().isNull());

    // now let's create a second window
    QSignalSpy windowAddedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);
    std::unique_ptr<KWaylandServer::PlasmaWindowInterface> serverTransient(m_windowManagementInterface->createWindow(this, QUuid::createUuid()));
    serverTransient->setParentWindow(m_windowInterface);
    QVERIFY(windowAddedSpy.wait());
    auto transient = windowAddedSpy.first().first().value<KWayland::Client::PlasmaWindow *>();
    QCOMPARE(transient->parentWindow().data(), parentWindow);

    // let's unset the parent
    QSignalSpy parentWindowChangedSpy(transient, &KWayland::Client::PlasmaWindow::parentWindowChanged);
    serverTransient->setParentWindow(nullptr);
    QVERIFY(parentWindowChangedSpy.wait());
    QVERIFY(transient->parentWindow().isNull());

    // and set it again
    serverTransient->setParentWindow(m_windowInterface);
    QVERIFY(parentWindowChangedSpy.wait());
    QCOMPARE(transient->parentWindow().data(), parentWindow);

    // now let's try to unmap the parent
    m_windowInterface->deleteLater();
    m_window = nullptr;
    m_windowInterface = nullptr;
    QVERIFY(parentWindowChangedSpy.wait());
    QVERIFY(transient->parentWindow().isNull());
}

void TestWindowManagement::testGeometry()
{
    QVERIFY(m_window);
    QCOMPARE(m_window->geometry(), QRect());
    QSignalSpy windowGeometryChangedSpy(m_window, &KWayland::Client::PlasmaWindow::geometryChanged);
    m_windowInterface->setGeometry(QRect(20, -10, 30, 40));
    QVERIFY(windowGeometryChangedSpy.wait());
    QCOMPARE(m_window->geometry(), QRect(20, -10, 30, 40));
    // setting an empty geometry should not be sent to the client
    m_windowInterface->setGeometry(QRect());
    QVERIFY(!windowGeometryChangedSpy.wait(10));
    // setting to the geometry which the client still has should not trigger signal
    m_windowInterface->setGeometry(QRect(20, -10, 30, 40));
    QVERIFY(!windowGeometryChangedSpy.wait(10));
    // setting another geometry should work, though
    m_windowInterface->setGeometry(QRect(0, 0, 35, 45));
    QVERIFY(windowGeometryChangedSpy.wait());
    QCOMPARE(windowGeometryChangedSpy.count(), 2);
    QCOMPARE(m_window->geometry(), QRect(0, 0, 35, 45));

    // let's bind a second PlasmaWindowManagement to verify the initial setting
    std::unique_ptr<KWayland::Client::PlasmaWindowManagement> pm(
        m_registry->createPlasmaWindowManagement(m_registry->interface(KWayland::Client::Registry::Interface::PlasmaWindowManagement).name,
                                                 m_registry->interface(KWayland::Client::Registry::Interface::PlasmaWindowManagement).version));
    QVERIFY(pm != nullptr);
    QSignalSpy windowAddedSpy(pm.get(), &KWayland::Client::PlasmaWindowManagement::windowCreated);
    QVERIFY(windowAddedSpy.wait());
    auto window = pm->windows().first();
    QCOMPARE(window->geometry(), QRect(0, 0, 35, 45));
}

void TestWindowManagement::testIcon()
{
    // initially, there shouldn't be any icon
    QSignalSpy iconChangedSpy(m_window, &KWayland::Client::PlasmaWindow::iconChanged);
    QVERIFY(m_window->icon().isNull());

    // create an icon with a pixmap
    QImage p(32, 32, QImage::Format_ARGB32_Premultiplied);
    p.fill(Qt::red);
    const QIcon dummyIcon(QPixmap::fromImage(p));
    m_windowInterface->setIcon(dummyIcon);
    QVERIFY(iconChangedSpy.wait());
    QCOMPARE(iconChangedSpy.count(), 1);
    QCOMPARE(m_window->icon().pixmap(32, 32), dummyIcon.pixmap(32, 32));

    // let's set a themed icon
    m_windowInterface->setIcon(QIcon::fromTheme(QStringLiteral("wayland")));
    QVERIFY(iconChangedSpy.wait());
    QCOMPARE(iconChangedSpy.count(), 2);
    if (!QIcon::hasThemeIcon(QStringLiteral("wayland"))) {
        QEXPECT_FAIL("", "no wayland icon", Continue);
    }
    QCOMPARE(m_window->icon().name(), QStringLiteral("wayland"));
}

void TestWindowManagement::testPid()
{
    QVERIFY(m_window);
    QVERIFY(m_window->pid() == 1337);

    // test server not setting a PID for whatever reason
    std::unique_ptr<KWaylandServer::PlasmaWindowInterface> newWindowInterface(m_windowManagementInterface->createWindow(this, QUuid::createUuid()));
    QSignalSpy windowSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);
    QVERIFY(windowSpy.wait());
    std::unique_ptr<KWayland::Client::PlasmaWindow> newWindow(windowSpy.first().first().value<KWayland::Client::PlasmaWindow *>());
    QVERIFY(newWindow);
    QVERIFY(newWindow->pid() == 0);
}

void TestWindowManagement::testApplicationMenu()
{
    const auto serviceName = QStringLiteral("org.kde.foo");
    const auto objectPath = QStringLiteral("/org/kde/bar");

    m_windowInterface->setApplicationMenuPaths(serviceName, objectPath);

    QSignalSpy applicationMenuChangedSpy(m_window, &KWayland::Client::PlasmaWindow::applicationMenuChanged);
    QVERIFY(applicationMenuChangedSpy.wait());

    QCOMPARE(m_window->applicationMenuServiceName(), serviceName);
    QCOMPARE(m_window->applicationMenuObjectPath(), objectPath);
}

QTEST_MAIN(TestWindowManagement)
#include "test_wayland_windowmanagement.moc"
