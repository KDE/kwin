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
#include "client.h"
#include "cursor.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"
#include <kwineffects.h>

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/plasmawindowmanagement.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
//screenlocker
#include <KScreenLocker/KsldApp>

#include <QPainter>
#include <QRasterWindow>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace KWayland::Client;

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_plasma-window-0");

class PlasmaWindowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testCreateDestroyX11PlasmaWindow();
    void testInternalWindowNoPlasmaWindow();
    void testPopupWindowNoPlasmaWindow();
    void testLockScreenNoPlasmaWindow();
    void testDestroyedButNotUnmapped();

private:
    ConnectionThread *m_connection = nullptr;
    PlasmaWindowManagement *m_windowManagement = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    Shell *m_shell = nullptr;
    ShmPool *m_shm = nullptr;
    EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

void PlasmaWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    waylandServer()->init(s_socketName.toLocal8Bit());

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    setenv("QMLSCENE_DEVICE", "softwarecontext", true);
    waylandServer()->initWorkspace();
}

void PlasmaWindowTest::init()
{
    // setup connection
    m_connection = new ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy allAnnounced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(allAnnounced.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());

    m_windowManagement = registry.createPlasmaWindowManagement(registry.interface(Registry::Interface::PlasmaWindowManagement).name,
                                                               registry.interface(Registry::Interface::PlasmaWindowManagement).version,
                                                               this);
    QVERIFY(m_windowManagement);
    m_compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name,
                                             registry.interface(Registry::Interface::Compositor).version,
                                             this);
    QVERIFY(m_compositor);
    m_shm = registry.createShmPool(registry.interface(Registry::Interface::Shm).name,
                                   registry.interface(Registry::Interface::Shm).version,
                                   this);
    QVERIFY(m_shm);
    m_shell = registry.createShell(registry.interface(Registry::Interface::Shell).name,
                                   registry.interface(Registry::Interface::Shell).version,
                                   this);
    QVERIFY(m_shell);

    screens()->setCurrent(0);
    Cursor::setPos(QPoint(640, 512));
}

void PlasmaWindowTest::cleanup()
{
#define CLEANUP(name) \
    if (name) { \
        delete name; \
        name = nullptr; \
    }
    CLEANUP(m_windowManagement)
    CLEANUP(m_shm)
    CLEANUP(m_shell)
    CLEANUP(m_compositor)
    CLEANUP(m_queue)
#undef CLEANUP
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
}

void PlasmaWindowTest::testCreateDestroyX11PlasmaWindow()
{
    // this test verifies that a PlasmaWindow gets unmapped on Client side when an X11 client is destroyed
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &PlasmaWindowManagement::windowCreated);
    QVERIFY(plasmaWindowCreatedSpy.isValid());

    // create an xcb window
    struct XcbConnectionDeleter
    {
        static inline void cleanup(xcb_connection_t *pointer)
        {
            xcb_disconnect(pointer);
        }
    };
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    Client *client = windowCreatedSpy.first().first().value<Client*>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    QVERIFY(client->isDecorated());
    QVERIFY(client->isActive());

    // now that should also give it to us on client side
    QVERIFY(plasmaWindowCreatedSpy.wait());
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);
    QCOMPARE(m_windowManagement->windows().count(), 1);

    QSignalSpy unmappedSpy(m_windowManagement->windows().first(), &PlasmaWindow::unmapped);
    QVERIFY(unmappedSpy.isValid());
    QSignalSpy destroyedSpy(m_windowManagement->windows().first(), &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());

    // now shade the window
    const QRect geoBeforeShade = client->geometry();
    QVERIFY(geoBeforeShade.isValid());
    QVERIFY(!geoBeforeShade.isEmpty());
    workspace()->slotWindowShade();
    QVERIFY(client->isShade());
    QVERIFY(client->geometry() != geoBeforeShade);
    // and unshade again
    workspace()->slotWindowShade();
    QVERIFY(!client->isShade());
    QCOMPARE(client->geometry(), geoBeforeShade);

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_flush(c.data());

    QSignalSpy windowClosedSpy(client, &Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.data(), w);
    c.reset();

    QVERIFY(unmappedSpy.wait());
    QCOMPARE(unmappedSpy.count(), 1);

    QVERIFY(destroyedSpy.wait());
}

class HelperWindow : public QRasterWindow
{
    Q_OBJECT
public:
    HelperWindow();
    ~HelperWindow();

protected:
    void paintEvent(QPaintEvent *event) override;
};

HelperWindow::HelperWindow()
    : QRasterWindow(nullptr)
{
}

HelperWindow::~HelperWindow() = default;

void HelperWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(0, 0, width(), height(), Qt::red);
}

void PlasmaWindowTest::testInternalWindowNoPlasmaWindow()
{
    // this test verifies that an internal window is not added as a PlasmaWindow to the client
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &PlasmaWindowManagement::windowCreated);
    QVERIFY(plasmaWindowCreatedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();

    QVERIFY(!plasmaWindowCreatedSpy.wait());
}

void PlasmaWindowTest::testPopupWindowNoPlasmaWindow()
{
    // this test verifies that for a popup window no PlasmaWindow is sent to the client
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &PlasmaWindowManagement::windowCreated);
    QVERIFY(plasmaWindowCreatedSpy.isValid());

    // first create the parent window
    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());
    QScopedPointer<ShellSurface> parentShellSurface(m_shell->createSurface(parentSurface.data()));
    // map that window
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    parentSurface->attachBuffer(m_shm->createBuffer(img));
    parentSurface->damage(QRect(0, 0, 100, 50));
    parentSurface->commit();
    // this should create a plasma window
    QVERIFY(plasmaWindowCreatedSpy.wait());

    // now let's create a popup window for it
    QScopedPointer<Surface> popupSurface(m_compositor->createSurface());
    QScopedPointer<ShellSurface> popupShellSurface(m_shell->createSurface(popupSurface.data()));
    popupShellSurface->setTransient(parentSurface.data(), QPoint(0, 0), ShellSurface::TransientFlag::NoFocus);
    // let's map it
    popupSurface->attachBuffer(m_shm->createBuffer(img));
    popupSurface->damage(QRect(0, 0, 100, 50));
    popupSurface->commit();

    // this should not create a plasma window
    QVERIFY(!plasmaWindowCreatedSpy.wait());

    // now the same with an already mapped surface when we create the shell surface
    QScopedPointer<Surface> popup2Surface(m_compositor->createSurface());
    popup2Surface->attachBuffer(m_shm->createBuffer(img));
    popup2Surface->damage(QRect(0, 0, 100, 50));
    popup2Surface->commit();
    QScopedPointer<ShellSurface> popup2ShellSurface(m_shell->createSurface(popup2Surface.data()));
    popup2ShellSurface->setTransient(popupSurface.data(), QPoint(0, 0), ShellSurface::TransientFlag::NoFocus);

    // this should not create a plasma window
    QEXPECT_FAIL("", "The call to setTransient comes to late the window is already mapped then", Continue);
    QVERIFY(!plasmaWindowCreatedSpy.wait());

    // let's destroy the windows
    QCOMPARE(waylandServer()->clients().count(), 3);
    QSignalSpy destroyed1Spy(waylandServer()->clients().last(), &QObject::destroyed);
    QVERIFY(destroyed1Spy.isValid());
    popup2Surface->attachBuffer(Buffer::Ptr());
    popup2Surface->commit(Surface::CommitFlag::None);
    popup2ShellSurface.reset();
    popup2Surface.reset();
    QVERIFY(destroyed1Spy.wait());
    QCOMPARE(waylandServer()->clients().count(), 2);
    QSignalSpy destroyed2Spy(waylandServer()->clients().last(), &QObject::destroyed);
    QVERIFY(destroyed2Spy.isValid());
    popupSurface->attachBuffer(Buffer::Ptr());
    popupSurface->commit(Surface::CommitFlag::None);
    popupShellSurface.reset();
    popupSurface.reset();
    QVERIFY(destroyed2Spy.wait());
    QCOMPARE(waylandServer()->clients().count(), 1);
    QSignalSpy destroyed3Spy(waylandServer()->clients().last(), &QObject::destroyed);
    QVERIFY(destroyed3Spy.isValid());
    parentSurface->attachBuffer(Buffer::Ptr());
    parentSurface->commit(Surface::CommitFlag::None);
    parentShellSurface.reset();
    parentSurface.reset();
    QVERIFY(destroyed3Spy.wait());
}

void PlasmaWindowTest::testLockScreenNoPlasmaWindow()
{
    // this test verifies that lock screen windows are not exposed to PlasmaWindow
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &PlasmaWindowManagement::windowCreated);
    QVERIFY(plasmaWindowCreatedSpy.isValid());

    // this time we use a QSignalSpy on ShellClient as it'a a little bit more complex setup
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    // lock
    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    QVERIFY(clientAddedSpy.wait());
    QVERIFY(clientAddedSpy.first().first().value<ShellClient*>()->isLockScreen());
    // should not be sent to the client
    QVERIFY(plasmaWindowCreatedSpy.isEmpty());
    QVERIFY(!plasmaWindowCreatedSpy.wait());

    // fake unlock
    QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockStateChangedSpy.isValid());
    const auto children = ScreenLocker::KSldApp::self()->children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
    QVERIFY(lockStateChangedSpy.wait());
    QVERIFY(!waylandServer()->isScreenLocked());
}

void PlasmaWindowTest::testDestroyedButNotUnmapped()
{
    // this test verifies that also when a ShellSurface gets destroyed without a prior unmap
    // the PlasmaWindow gets destroyed on Client side
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &PlasmaWindowManagement::windowCreated);
    QVERIFY(plasmaWindowCreatedSpy.isValid());

    // first create the parent window
    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());
    QScopedPointer<ShellSurface> parentShellSurface(m_shell->createSurface(parentSurface.data()));
    // map that window
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    parentSurface->attachBuffer(m_shm->createBuffer(img));
    parentSurface->damage(QRect(0, 0, 100, 50));
    parentSurface->commit();
    // this should create a plasma window
    QVERIFY(plasmaWindowCreatedSpy.wait());
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);
    auto window = plasmaWindowCreatedSpy.first().first().value<PlasmaWindow*>();
    QVERIFY(window);
    QSignalSpy destroyedSpy(window, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());

    // now destroy without an unmap
    parentShellSurface.reset();
    parentSurface.reset();
    QVERIFY(destroyedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::PlasmaWindowTest)
#include "plasmawindow_test.moc"
