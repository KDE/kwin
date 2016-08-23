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
#include "deleted.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"

#include <KWayland/Server/seat_interface.h>

#include <QSocketNotifier>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xwayland_input-0");

class XWaylandInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void testPointerEnterLeave();
};

void XWaylandInputTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::Deleted*>();
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
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void XWaylandInputTest::init()
{
    screens()->setCurrent(0);
    Cursor::setPos(QPoint(640, 512));
    QVERIFY(waylandServer()->clients().isEmpty());
}


struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

class X11EventReaderHelper : public QObject
{
    Q_OBJECT
public:
    X11EventReaderHelper(xcb_connection_t *c);

Q_SIGNALS:
    void entered();
    void left();

private:
    void processXcbEvents();
    xcb_connection_t *m_connection;
    QSocketNotifier *m_notifier;
};

X11EventReaderHelper::X11EventReaderHelper(xcb_connection_t *c)
    : QObject()
    , m_connection(c)
    , m_notifier(new QSocketNotifier(xcb_get_file_descriptor(m_connection), QSocketNotifier::Read, this))
{
    connect(m_notifier, &QSocketNotifier::activated, this, &X11EventReaderHelper::processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &X11EventReaderHelper::processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, &X11EventReaderHelper::processXcbEvents);
}

void X11EventReaderHelper::processXcbEvents()
{
    while (auto event = xcb_poll_for_event(m_connection)) {
        const uint8_t eventType = event->response_type & ~0x80;
        switch (eventType) {
            case XCB_ENTER_NOTIFY:
                emit entered();
                break;
            case XCB_LEAVE_NOTIFY:
                emit left();
                break;
        }
        free(event);
    }
    xcb_flush(m_connection);
}

void XWaylandInputTest::testPointerEnterLeave()
{
    // this test simulates a pointer enter and pointer leave on an X11 window

    // create the test window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    X11EventReaderHelper eventReader(c.data());
    QSignalSpy enteredSpy(&eventReader, &X11EventReaderHelper::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(&eventReader, &X11EventReaderHelper::left);
    QVERIFY(leftSpy.isValid());
    // atom for the screenedge show hide functionality
    Xcb::Atom atom(QByteArrayLiteral("_KDE_NET_WM_SCREEN_EDGE_SHOW"), false, c.data());

    xcb_window_t w = xcb_generate_id(c.data());
    const QRect windowGeometry = QRect(0, 0, 10, 20);
    const uint32_t values[] = {
        XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_LEAVE_WINDOW
    };
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, values);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    NETWinInfo info(c.data(), w, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Normal);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    Client *client = windowCreatedSpy.last().first().value<Client*>();
    QVERIFY(client);
    QVERIFY(client->isDecorated());
    QVERIFY(!client->hasStrut());
    QVERIFY(!client->isHiddenInternal());
    QVERIFY(!client->readyForPainting());
    QMetaObject::invokeMethod(client, "setReadyForPainting");
    QVERIFY(client->readyForPainting());
    QVERIFY(!client->surface());
    QSignalSpy surfaceChangedSpy(client, &Toplevel::surfaceChanged);
    QVERIFY(surfaceChangedSpy.isValid());
    QVERIFY(surfaceChangedSpy.wait());
    QVERIFY(client->surface());

    // move pointer into the window, should trigger an enter
    QVERIFY(!client->geometry().contains(Cursor::pos()));
    QVERIFY(enteredSpy.isEmpty());
    Cursor::setPos(client->geometry().center());
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), client->surface());
    QVERIFY(waylandServer()->seat()->focusedPointer());
    QVERIFY(enteredSpy.wait());

    // move out of window
    Cursor::setPos(client->geometry().bottomRight() + QPoint(10, 10));
    QEXPECT_FAIL("", "Xwayland doesn't send leave events when the surface gets a leave", Continue);
    QVERIFY(leftSpy.wait());

    // destroy window again
    QSignalSpy windowClosedSpy(client, &Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    xcb_unmap_window(c.data(), w);
    xcb_destroy_window(c.data(), w);
    xcb_flush(c.data());
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::XWaylandInputTest)
#include "xwayland_input_test.moc"
