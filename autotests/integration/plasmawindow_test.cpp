/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/plasmawindowmanagement.h>
#include <KWayland/Client/surface.h>
// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif

#include <QPainter>
#include <QRasterWindow>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

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
    KWayland::Client::PlasmaWindowManagement *m_windowManagement = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
};

void PlasmaWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
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
    setenv("QT_QPA_PLATFORM", "wayland", true);
    setenv("QMLSCENE_DEVICE", "softwarecontext", true);
}

void PlasmaWindowTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::WindowManagement));
    m_windowManagement = Test::waylandWindowManagement();
    m_compositor = Test::waylandCompositor();

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void PlasmaWindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void PlasmaWindowTest::testCreateDestroyX11PlasmaWindow()
{
    // this test verifies that a PlasmaWindow gets unmapped on Client side when an X11 window is destroyed
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);

    // create an xcb window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(window->isDecorated());
    QVERIFY(window->isActive());
    // verify that it gets the keyboard focus
    if (!window->surface()) {
        // we don't have a surface yet, so focused keyboard surface if set is not ours
        QVERIFY(!waylandServer()->seat()->focusedKeyboardSurface());
        QVERIFY(Test::waitForWaylandSurface(window));
    }
    QCOMPARE(waylandServer()->seat()->focusedKeyboardSurface(), window->surface());

    // now that should also give it to us on client side
    QVERIFY(plasmaWindowCreatedSpy.wait());
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);
    QCOMPARE(m_windowManagement->windows().count(), 1);
    auto pw = m_windowManagement->windows().first();
    QCOMPARE(pw->geometry(), window->frameGeometry());
    QSignalSpy geometryChangedSpy(pw, &KWayland::Client::PlasmaWindow::geometryChanged);

    QSignalSpy unmappedSpy(m_windowManagement->windows().first(), &KWayland::Client::PlasmaWindow::unmapped);
    QSignalSpy destroyedSpy(m_windowManagement->windows().first(), &QObject::destroyed);

    // now shade the window
    const QRectF geoBeforeShade = window->frameGeometry();
    QVERIFY(geoBeforeShade.isValid());
    QVERIFY(!geoBeforeShade.isEmpty());
    workspace()->slotWindowShade();
    QVERIFY(window->isShade());
    QVERIFY(window->frameGeometry() != geoBeforeShade);
    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(pw->geometry(), window->frameGeometry());
    // and unshade again
    workspace()->slotWindowShade();
    QVERIFY(!window->isShade());
    QCOMPARE(window->frameGeometry(), geoBeforeShade);
    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(pw->geometry(), geoBeforeShade);

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.get(), windowId);
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
    QPainter p(this);
    p.fillRect(0, 0, width(), height(), Qt::red);
}

void PlasmaWindowTest::testInternalWindowNoPlasmaWindow()
{
    // this test verifies that an internal window is not added as a PlasmaWindow
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();

    QVERIFY(!plasmaWindowCreatedSpy.wait(100));
}

void PlasmaWindowTest::testPopupWindowNoPlasmaWindow()
{
    // this test verifies that a popup window is not added as a PlasmaWindow
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);

    // first create the parent window
    std::unique_ptr<KWayland::Client::Surface> parentSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> parentShellSurface(Test::createXdgToplevelSurface(parentSurface.get()));
    Window *parentClient = Test::renderAndWaitForShown(parentSurface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(parentClient);
    QVERIFY(plasmaWindowCreatedSpy.wait());
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);

    // now let's create a popup window for it
    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(10, 10);
    positioner->set_anchor_rect(0, 0, 10, 10);
    positioner->set_anchor(Test::XdgPositioner::anchor_bottom_right);
    positioner->set_gravity(Test::XdgPositioner::gravity_bottom_right);
    std::unique_ptr<KWayland::Client::Surface> popupSurface(Test::createSurface());
    std::unique_ptr<Test::XdgPopup> popupShellSurface(Test::createXdgPopupSurface(popupSurface.get(), parentShellSurface->xdgSurface(), positioner.get()));
    Window *popupWindow = Test::renderAndWaitForShown(popupSurface.get(), QSize(10, 10), Qt::blue);
    QVERIFY(popupWindow);
    QVERIFY(!plasmaWindowCreatedSpy.wait(100));
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);

    // let's destroy the windows
    popupShellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(popupWindow));
    parentShellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(parentClient));
}

void PlasmaWindowTest::testLockScreenNoPlasmaWindow()
{
#if KWIN_BUILD_SCREENLOCKER
    // this test verifies that lock screen windows are not exposed to PlasmaWindow
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);

    // this time we use a QSignalSpy on XdgShellClient as it'a a little bit more complex setup
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    // lock
    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    QVERIFY(windowAddedSpy.wait());
    QVERIFY(windowAddedSpy.first().first().value<Window *>()->isLockScreen());
    // should not be sent to the window
    QVERIFY(plasmaWindowCreatedSpy.isEmpty());
    QVERIFY(!plasmaWindowCreatedSpy.wait(100));

    // fake unlock
    QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged);
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
#else
    QSKIP("KWin was built without lockscreen support");
#endif
}

void PlasmaWindowTest::testDestroyedButNotUnmapped()
{
    // this test verifies that also when a ShellSurface gets destroyed without a prior unmap
    // the PlasmaWindow gets destroyed on Client side
    QSignalSpy plasmaWindowCreatedSpy(m_windowManagement, &KWayland::Client::PlasmaWindowManagement::windowCreated);

    // first create the parent window
    std::unique_ptr<KWayland::Client::Surface> parentSurface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> parentShellSurface(Test::createXdgToplevelSurface(parentSurface.get()));
    // map that window
    Test::render(parentSurface.get(), QSize(100, 50), Qt::blue);
    // this should create a plasma window
    QVERIFY(plasmaWindowCreatedSpy.wait());
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);
    auto window = plasmaWindowCreatedSpy.first().first().value<KWayland::Client::PlasmaWindow *>();
    QVERIFY(window);
    QSignalSpy destroyedSpy(window, &QObject::destroyed);

    // now destroy without an unmap
    parentShellSurface.reset();
    parentSurface.reset();
    QVERIFY(destroyedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::PlasmaWindowTest)
#include "plasmawindow_test.moc"
