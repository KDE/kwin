/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "config-kwin.h"

#include "kwin_wayland_test.h"

#include "atoms.h"
#include "core/outputbackend.h"
#include "deleted.h"
#include "rules.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/surface.h>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QUuid>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_dbus_interface-0");

const QString s_destination{QStringLiteral("org.kde.KWin")};
const QString s_path{QStringLiteral("/KWin")};
const QString s_interface{QStringLiteral("org.kde.KWin")};

class TestDbusInterface : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testGetWindowInfoInvalidUuid();
    void testGetWindowInfoXdgShellClient();
    void testGetWindowInfoX11Client();
};

void TestDbusInterface::initTestCase()
{
    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::Window *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    VirtualDesktopManager::self()->setCount(4);
}

void TestDbusInterface::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void TestDbusInterface::cleanup()
{
    Test::destroyWaylandConnection();
}

namespace
{
QDBusPendingCall getWindowInfo(const QUuid &uuid)
{
    auto msg = QDBusMessage::createMethodCall(s_destination, s_path, s_interface, QStringLiteral("getWindowInfo"));
    msg.setArguments({uuid.toString()});
    return QDBusConnection::sessionBus().asyncCall(msg);
}
}

void TestDbusInterface::testGetWindowInfoInvalidUuid()
{
    QDBusPendingReply<QVariantMap> reply{getWindowInfo(QUuid::createUuid())};
    reply.waitForFinished();
    QVERIFY(reply.isValid());
    QVERIFY(!reply.isError());
    const auto windowData = reply.value();
    QVERIFY(windowData.empty());
}

void TestDbusInterface::testGetWindowInfoXdgShellClient()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    shellSurface->set_title(QStringLiteral("Test window"));

    // now let's render
    Test::render(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(windowAddedSpy.isEmpty());
    QVERIFY(windowAddedSpy.wait());
    auto window = windowAddedSpy.first().first().value<Window *>();
    QVERIFY(window);

    const QVariantMap expectedData = {
        {QStringLiteral("type"), int(NET::Normal)},
        {QStringLiteral("x"), window->x()},
        {QStringLiteral("y"), window->y()},
        {QStringLiteral("width"), window->width()},
        {QStringLiteral("height"), window->height()},
        {QStringLiteral("desktops"), window->desktopIds()},
        {QStringLiteral("minimized"), false},
        {QStringLiteral("shaded"), false},
        {QStringLiteral("fullscreen"), false},
        {QStringLiteral("keepAbove"), false},
        {QStringLiteral("keepBelow"), false},
        {QStringLiteral("skipTaskbar"), false},
        {QStringLiteral("skipPager"), false},
        {QStringLiteral("skipSwitcher"), false},
        {QStringLiteral("maximizeHorizontal"), false},
        {QStringLiteral("maximizeVertical"), false},
        {QStringLiteral("noBorder"), false},
        {QStringLiteral("clientMachine"), QString()},
        {QStringLiteral("localhost"), true},
        {QStringLiteral("role"), QString()},
        {QStringLiteral("resourceName"), QStringLiteral("testDbusInterface")},
        {QStringLiteral("resourceClass"), QStringLiteral("org.kde.foo")},
        {QStringLiteral("desktopFile"), QStringLiteral("org.kde.foo")},
        {QStringLiteral("caption"), QStringLiteral("Test window")},
#if KWIN_BUILD_ACTIVITIES
        {QStringLiteral("activities"), QStringList()},
#endif
    };

    // let's get the window info
    QDBusPendingReply<QVariantMap> reply{getWindowInfo(window->internalId())};
    reply.waitForFinished();
    QVERIFY(reply.isValid());
    QVERIFY(!reply.isError());
    auto windowData = reply.value();
    windowData.remove(QStringLiteral("uuid"));
    QCOMPARE(windowData, expectedData);

    auto verifyProperty = [window](const QString &name) {
        QDBusPendingReply<QVariantMap> reply{getWindowInfo(window->internalId())};
        reply.waitForFinished();
        return reply.value().value(name).toBool();
    };

    QVERIFY(!window->isMinimized());
    window->setMinimized(true);
    QVERIFY(window->isMinimized());
    QCOMPARE(verifyProperty(QStringLiteral("minimized")), true);

    QVERIFY(!window->keepAbove());
    window->setKeepAbove(true);
    QVERIFY(window->keepAbove());
    QCOMPARE(verifyProperty(QStringLiteral("keepAbove")), true);

    QVERIFY(!window->keepBelow());
    window->setKeepBelow(true);
    QVERIFY(window->keepBelow());
    QCOMPARE(verifyProperty(QStringLiteral("keepBelow")), true);

    QVERIFY(!window->skipTaskbar());
    window->setSkipTaskbar(true);
    QVERIFY(window->skipTaskbar());
    QCOMPARE(verifyProperty(QStringLiteral("skipTaskbar")), true);

    QVERIFY(!window->skipPager());
    window->setSkipPager(true);
    QVERIFY(window->skipPager());
    QCOMPARE(verifyProperty(QStringLiteral("skipPager")), true);

    QVERIFY(!window->skipSwitcher());
    window->setSkipSwitcher(true);
    QVERIFY(window->skipSwitcher());
    QCOMPARE(verifyProperty(QStringLiteral("skipSwitcher")), true);

    // not testing shaded as that's X11
    // not testing fullscreen, maximizeHorizontal, maximizeVertical and noBorder as those require window geometry changes

    QCOMPARE(window->desktop(), 1);
    workspace()->sendWindowToDesktop(window, 2, false);
    QCOMPARE(window->desktop(), 2);
    reply = getWindowInfo(window->internalId());
    reply.waitForFinished();
    QCOMPARE(reply.value().value(QStringLiteral("desktops")).toStringList(), window->desktopIds());

    window->move(QPoint(10, 20));
    reply = getWindowInfo(window->internalId());
    reply.waitForFinished();
    QCOMPARE(reply.value().value(QStringLiteral("x")).toInt(), window->x());
    QCOMPARE(reply.value().value(QStringLiteral("y")).toInt(), window->y());
    // not testing width, height as that would require window geometry change

    // finally close window
    const auto id = window->internalId();
    QSignalSpy windowClosedSpy(window, &Window::windowClosed);
    shellSurface.reset();
    surface.reset();
    QVERIFY(windowClosedSpy.wait());
    QCOMPARE(windowClosedSpy.count(), 1);

    reply = getWindowInfo(id);
    reply.waitForFinished();
    QVERIFY(reply.value().empty());
}

struct XcbConnectionDeleter
{
    void operator()(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void TestDbusInterface::testGetWindowInfoX11Client()
{
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 600, 400);
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
    xcb_icccm_set_wm_class(c.get(), windowId, 7, "foo\0bar");
    NETWinInfo winInfo(c.get(), windowId, rootWindow(), NET::Properties(), NET::Properties2());
    winInfo.setName("Some caption");
    winInfo.setDesktopFileName("org.kde.foo");
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QCOMPARE(window->clientSize(), windowGeometry.size());

    const QVariantMap expectedData = {
        {QStringLiteral("type"), NET::Normal},
        {QStringLiteral("x"), window->x()},
        {QStringLiteral("y"), window->y()},
        {QStringLiteral("width"), window->width()},
        {QStringLiteral("height"), window->height()},
        {QStringLiteral("desktops"), window->desktopIds()},
        {QStringLiteral("minimized"), false},
        {QStringLiteral("shaded"), false},
        {QStringLiteral("fullscreen"), false},
        {QStringLiteral("keepAbove"), false},
        {QStringLiteral("keepBelow"), false},
        {QStringLiteral("skipTaskbar"), false},
        {QStringLiteral("skipPager"), false},
        {QStringLiteral("skipSwitcher"), false},
        {QStringLiteral("maximizeHorizontal"), false},
        {QStringLiteral("maximizeVertical"), false},
        {QStringLiteral("noBorder"), false},
        {QStringLiteral("role"), QString()},
        {QStringLiteral("resourceName"), QStringLiteral("foo")},
        {QStringLiteral("resourceClass"), QStringLiteral("bar")},
        {QStringLiteral("desktopFile"), QStringLiteral("org.kde.foo")},
        {QStringLiteral("caption"), QStringLiteral("Some caption")},
#if KWIN_BUILD_ACTIVITIES
        {QStringLiteral("activities"), QStringList()},
#endif
    };

    // let's get the window info
    QDBusPendingReply<QVariantMap> reply{getWindowInfo(window->internalId())};
    reply.waitForFinished();
    QVERIFY(reply.isValid());
    QVERIFY(!reply.isError());
    auto windowData = reply.value();
    // not testing clientmachine as that is system dependent due to that also not testing localhost
    windowData.remove(QStringLiteral("clientMachine"));
    windowData.remove(QStringLiteral("localhost"));
    windowData.remove(QStringLiteral("uuid"));
    QCOMPARE(windowData, expectedData);

    auto verifyProperty = [window](const QString &name) {
        QDBusPendingReply<QVariantMap> reply{getWindowInfo(window->internalId())};
        reply.waitForFinished();
        return reply.value().value(name).toBool();
    };

    QVERIFY(!window->isMinimized());
    window->setMinimized(true);
    QVERIFY(window->isMinimized());
    QCOMPARE(verifyProperty(QStringLiteral("minimized")), true);

    QVERIFY(!window->keepAbove());
    window->setKeepAbove(true);
    QVERIFY(window->keepAbove());
    QCOMPARE(verifyProperty(QStringLiteral("keepAbove")), true);

    QVERIFY(!window->keepBelow());
    window->setKeepBelow(true);
    QVERIFY(window->keepBelow());
    QCOMPARE(verifyProperty(QStringLiteral("keepBelow")), true);

    QVERIFY(!window->skipTaskbar());
    window->setSkipTaskbar(true);
    QVERIFY(window->skipTaskbar());
    QCOMPARE(verifyProperty(QStringLiteral("skipTaskbar")), true);

    QVERIFY(!window->skipPager());
    window->setSkipPager(true);
    QVERIFY(window->skipPager());
    QCOMPARE(verifyProperty(QStringLiteral("skipPager")), true);

    QVERIFY(!window->skipSwitcher());
    window->setSkipSwitcher(true);
    QVERIFY(window->skipSwitcher());
    QCOMPARE(verifyProperty(QStringLiteral("skipSwitcher")), true);

    QVERIFY(!window->isShade());
    window->setShade(ShadeNormal);
    QVERIFY(window->isShade());
    QCOMPARE(verifyProperty(QStringLiteral("shaded")), true);
    window->setShade(ShadeNone);
    QVERIFY(!window->isShade());

    QVERIFY(!window->noBorder());
    window->setNoBorder(true);
    QVERIFY(window->noBorder());
    QCOMPARE(verifyProperty(QStringLiteral("noBorder")), true);
    window->setNoBorder(false);
    QVERIFY(!window->noBorder());

    QVERIFY(!window->isFullScreen());
    window->setFullScreen(true);
    QVERIFY(window->isFullScreen());
    QVERIFY(window->clientSize() != windowGeometry.size());
    QCOMPARE(verifyProperty(QStringLiteral("fullscreen")), true);
    reply = getWindowInfo(window->internalId());
    reply.waitForFinished();
    QCOMPARE(reply.value().value(QStringLiteral("width")).toInt(), window->width());
    QCOMPARE(reply.value().value(QStringLiteral("height")).toInt(), window->height());
    window->setFullScreen(false);
    QVERIFY(!window->isFullScreen());

    // maximize
    window->setMaximize(true, false);
    QCOMPARE(verifyProperty(QStringLiteral("maximizeVertical")), true);
    QCOMPARE(verifyProperty(QStringLiteral("maximizeHorizontal")), false);
    window->setMaximize(false, true);
    QCOMPARE(verifyProperty(QStringLiteral("maximizeVertical")), false);
    QCOMPARE(verifyProperty(QStringLiteral("maximizeHorizontal")), true);

    const auto id = window->internalId();
    // destroy the window
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.get(), windowId);
    c.reset();

    reply = getWindowInfo(id);
    reply.waitForFinished();
    QVERIFY(reply.value().empty());
}

WAYLANDTEST_MAIN(TestDbusInterface)
#include "dbus_interface_test.moc"
