/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "x11client.h"
#include "cursor.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include <kwineffects.h>

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/plasmawindowmanagement.h>
#include <KWayland/Client/surface.h>
#include <KWaylandServer/seat_interface.h>
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
    PlasmaWindowManagement *m_windowManagement = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
};

void PlasmaWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    setenv("QMLSCENE_DEVICE", "softwarecontext", true);
    waylandServer()->initWorkspace();
}

void PlasmaWindowTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::WindowManagement));
    m_windowManagement = Test::waylandWindowManagement();
    m_compositor = Test::waylandCompositor();

    screens()->setCurrent(0);
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void PlasmaWindowTest::cleanup()
{
    Test::destroyWaylandConnection();
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
    X11Client *client = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    QVERIFY(client->isDecorated());
    QVERIFY(client->isActive());
    // verify that it gets the keyboard focus
    if (!client->surface()) {
        // we don't have a surface yet, so focused keyboard surface if set is not ours
        QVERIFY(!waylandServer()->seat()->focusedKeyboardSurface());
        QSignalSpy surfaceChangedSpy(client, &Toplevel::surfaceChanged);
        QVERIFY(surfaceChangedSpy.isValid());
        QVERIFY(surfaceChangedSpy.wait());
    }
    QVERIFY(client->surface());
    QCOMPARE(waylandServer()->seat()->focusedKeyboardSurface(), client->surface());

    // now that should also give it to us on client side
    QVERIFY(plasmaWindowCreatedSpy.wait());
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);
    QCOMPARE(m_windowManagement->windows().count(), 1);
    auto pw = m_windowManagement->windows().first();
    QCOMPARE(pw->geometry(), client->frameGeometry());
    QSignalSpy geometryChangedSpy(pw, &PlasmaWindow::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());

    QSignalSpy unmappedSpy(m_windowManagement->windows().first(), &PlasmaWindow::unmapped);
    QVERIFY(unmappedSpy.isValid());
    QSignalSpy destroyedSpy(m_windowManagement->windows().first(), &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());

    // now shade the window
    const QRect geoBeforeShade = client->frameGeometry();
    QVERIFY(geoBeforeShade.isValid());
    QVERIFY(!geoBeforeShade.isEmpty());
    workspace()->slotWindowShade();
    QVERIFY(client->isShade());
    QVERIFY(client->frameGeometry() != geoBeforeShade);
    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(pw->geometry(), client->frameGeometry());
    // and unshade again
    workspace()->slotWindowShade();
    QVERIFY(!client->isShade());
    QCOMPARE(client->frameGeometry(), geoBeforeShade);
    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(pw->geometry(), geoBeforeShade);

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_flush(c.data());

    QSignalSpy windowClosedSpy(client, &X11Client::windowClosed);
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
    ~HelperWindow() override;

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
    QScopedPointer<Surface> parentSurface(Test::createSurface());
    QScopedPointer<XdgShellSurface> parentShellSurface(Test::createXdgShellStableSurface(parentSurface.data()));
    AbstractClient *parentClient = Test::renderAndWaitForShown(parentSurface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(parentClient);
    QVERIFY(plasmaWindowCreatedSpy.wait());
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);

    // now let's create a popup window for it
    XdgPositioner positioner(QSize(10, 10), QRect(0, 0, 10, 10));
    positioner.setAnchorEdge(Qt::BottomEdge | Qt::RightEdge);
    positioner.setGravity(Qt::BottomEdge | Qt::RightEdge);
    QScopedPointer<Surface> popupSurface(Test::createSurface());
    QScopedPointer<XdgShellPopup> popupShellSurface(Test::createXdgShellStablePopup(popupSurface.data(), parentShellSurface.data(), positioner));
    AbstractClient *popupClient = Test::renderAndWaitForShown(popupSurface.data(), positioner.initialSize(), Qt::blue);
    QVERIFY(popupClient);
    QVERIFY(!plasmaWindowCreatedSpy.wait(100));
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);

    // let's destroy the windows
    popupShellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(popupClient));
    parentShellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(parentClient));
}

void PlasmaWindowTest::testLockScreenNoPlasmaWindow()
{
    // this test verifies that lock screen windows are not exposed to PlasmaWindow
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &PlasmaWindowManagement::windowCreated);
    QVERIFY(plasmaWindowCreatedSpy.isValid());

    // this time we use a QSignalSpy on XdgShellClient as it'a a little bit more complex setup
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    // lock
    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    QVERIFY(clientAddedSpy.wait());
    QVERIFY(clientAddedSpy.first().first().value<AbstractClient *>()->isLockScreen());
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
    QScopedPointer<Surface> parentSurface(Test::createSurface());
    QScopedPointer<XdgShellSurface> parentShellSurface(Test::createXdgShellStableSurface(parentSurface.data()));
    // map that window
    Test::render(parentSurface.data(), QSize(100, 50), Qt::blue);
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
