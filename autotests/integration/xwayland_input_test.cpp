/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "keyboard_input.h"
#include "pointer_input.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"
#include "xkb.h"

#include <QAbstractEventDispatcher>
#include <QSocketNotifier>

#include <linux/input-event-codes.h>
#include <netwm.h>
#include <xcb/xcb_icccm.h>
#define explicit xcb_explicit
#include <xcb/xkb.h>
#undef explicit
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-x11.h>

#include <utility>

namespace KWin
{

class XWaylandInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void testPointerEnterLeaveSsd();
    void testPointerEventLeaveCsd();
    void testKeyboardInput();
};

void XWaylandInputTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(qAppName()));

    kwinApp()->start();
    Test::setOutputConfig({
        Rect(0, 0, 1280, 1024),
        Rect(1280, 0, 1280, 1024),
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), Rect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), Rect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void XWaylandInputTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));

    QVERIFY(waylandServer()->windows().isEmpty());
}

class X11EventReaderHelper : public QObject
{
    Q_OBJECT

public:
    X11EventReaderHelper(xcb_connection_t *c);
    bool enableKeyboard();
    QString receivedText() const;
    xkb_keysym_t keySymForKeycode(xkb_keycode_t keycode) const;

Q_SIGNALS:
    void entered(const QPoint &localPoint);
    void left(const QPoint &localPoint);
    void receivedTextChanged();
    void keymapChanged();

private:
    void processXcbEvents();
    bool updateKeymap();
    xcb_connection_t *m_connection;
    QSocketNotifier *m_notifier;
    uint8_t m_xkbEventBase = 0;
    Test::XkbContextPtr m_xkbContext = Test::XkbContextPtr(nullptr, &xkb_context_unref);
    Test::XkbKeymapPtr m_xkbKeymap = Test::XkbKeymapPtr(nullptr, &xkb_keymap_unref);
    Test::XkbStatePtr m_xkbState = Test::XkbStatePtr(nullptr, &xkb_state_unref);
    QString m_receivedText;
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

bool X11EventReaderHelper::enableKeyboard()
{
    uint16_t majorVersion;
    uint16_t minorVersion;
    uint8_t eventBase;
    uint8_t errorBase;
    if (!xkb_x11_setup_xkb_extension(m_connection,
                                     XKB_X11_MIN_MAJOR_XKB_VERSION,
                                     XKB_X11_MIN_MINOR_XKB_VERSION,
                                     XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
                                     &majorVersion,
                                     &minorVersion,
                                     &eventBase,
                                     &errorBase)) {
        return false;
    }
    m_xkbEventBase = eventBase;

    const uint16_t xkbEvents = XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY;
    const uint16_t mapParts = XCB_XKB_MAP_PART_KEY_TYPES
        | XCB_XKB_MAP_PART_KEY_SYMS
        | XCB_XKB_MAP_PART_MODIFIER_MAP
        | XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS
        | XCB_XKB_MAP_PART_KEY_ACTIONS
        | XCB_XKB_MAP_PART_KEY_BEHAVIORS
        | XCB_XKB_MAP_PART_VIRTUAL_MODS
        | XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP;
    xcb_xkb_select_events(m_connection,
                          XCB_XKB_ID_USE_CORE_KBD,
                          xkbEvents,
                          0,
                          xkbEvents,
                          mapParts,
                          mapParts,
                          nullptr);

    m_xkbContext.reset(xkb_context_new(XKB_CONTEXT_NO_FLAGS));
    if (!m_xkbContext) {
        return false;
    }

    return updateKeymap();
}

bool X11EventReaderHelper::updateKeymap()
{
    const int32_t deviceId = xkb_x11_get_core_keyboard_device_id(m_connection);
    if (deviceId == -1) {
        return false;
    }
    Test::XkbKeymapPtr keymap(xkb_x11_keymap_new_from_device(m_xkbContext.get(), m_connection, deviceId, XKB_KEYMAP_COMPILE_NO_FLAGS), &xkb_keymap_unref);
    if (!keymap) {
        return false;
    }
    Test::XkbStatePtr state(xkb_x11_state_new_from_device(keymap.get(), m_connection, deviceId), &xkb_state_unref);
    if (!state) {
        return false;
    }

    m_xkbState = std::move(state);
    m_xkbKeymap = std::move(keymap);

    return true;
}

QString X11EventReaderHelper::receivedText() const
{
    return m_receivedText;
}

xkb_keysym_t X11EventReaderHelper::keySymForKeycode(xkb_keycode_t keycode) const
{
    return xkb_state_key_get_one_sym(m_xkbState.get(), keycode);
}

void X11EventReaderHelper::processXcbEvents()
{
    while (auto event = xcb_poll_for_event(m_connection)) {
        const uint8_t eventType = event->response_type & ~0x80;
        if (m_xkbEventBase != 0 && eventType == m_xkbEventBase) {
            const auto xkbEvent = reinterpret_cast<xcb_xkb_map_notify_event_t *>(event);
            if (xkbEvent->xkbType == XCB_XKB_MAP_NOTIFY || xkbEvent->xkbType == XCB_XKB_NEW_KEYBOARD_NOTIFY) {
                if (updateKeymap()) {
                    Q_EMIT keymapChanged();
                }
            }
        }
        switch (eventType) {
        case XCB_ENTER_NOTIFY: {
            auto enterEvent = reinterpret_cast<xcb_enter_notify_event_t *>(event);
            Q_EMIT entered(QPoint(enterEvent->event_x, enterEvent->event_y));
            break;
        }
        case XCB_LEAVE_NOTIFY: {
            auto leaveEvent = reinterpret_cast<xcb_leave_notify_event_t *>(event);
            Q_EMIT left(QPoint(leaveEvent->event_x, leaveEvent->event_y));
            break;
        }
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE: {
            if (m_xkbState) {
                const auto keyEvent = reinterpret_cast<xcb_key_press_event_t *>(event);
                const xkb_keycode_t keycode = keyEvent->detail;
                const bool pressed = eventType == XCB_KEY_PRESS;
                xkb_state_update_key(m_xkbState.get(), keycode, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);

                if (pressed) {
                    char text[64];
                    const int length = xkb_state_key_get_utf8(m_xkbState.get(), keycode, text, sizeof(text));
                    if (length > 0) {
                        m_receivedText.append(QString::fromUtf8(text, length));
                        Q_EMIT receivedTextChanged();
                    }
                }
            }
            break;
        }
        }
        free(event);
    }
    xcb_flush(m_connection);
}

void XWaylandInputTest::testPointerEnterLeaveSsd()
{
    // this test simulates a pointer enter and pointer leave on a server-side decorated X11 window

    // create the test window
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    if (xcb_get_setup(c.get())->release_number < 11800000) {
        QSKIP("XWayland 1.18 required");
    }

    xcb_warp_pointer(connection(), XCB_WINDOW_NONE, kwinApp()->x11RootWindow(), 0, 0, 0, 0, 640, 512);
    xcb_flush(connection());

    X11EventReaderHelper eventReader(c.get());
    QSignalSpy enteredSpy(&eventReader, &X11EventReaderHelper::entered);
    QSignalSpy leftSpy(&eventReader, &X11EventReaderHelper::left);

    xcb_window_t windowId = xcb_generate_id(c.get());
    const Rect windowGeometry = Rect(0, 0, 100, 200);
    const uint32_t values[] = {
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW};
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, values);
    xcb_size_hints_t hints{};
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Normal);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->hasStrut());

    QVERIFY(Test::waitForWaylandSurface(window));

    // move pointer into the window, should trigger an enter
    QVERIFY(!window->frameGeometry().contains(Cursors::self()->mouse()->pos()));
    QVERIFY(enteredSpy.isEmpty());
    input()->pointer()->warp(window->frameGeometry().center().toPoint());
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window->surface());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.last().first().toPoint(), (window->frameGeometry().center() - QPointF(window->frameMargins().left(), window->frameMargins().top())).toPoint());

    // move out of window
    input()->pointer()->warp(window->frameGeometry().bottomRight() + QPointF(10, 10));
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.last().first().toPoint(), (window->frameGeometry().center() - QPointF(window->frameMargins().left(), window->frameMargins().top())).toPoint());

    // destroy window again
    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
}

void XWaylandInputTest::testPointerEventLeaveCsd()
{
    // this test simulates a pointer enter and pointer leave on a client-side decorated X11 window

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    xcb_warp_pointer(connection(), XCB_WINDOW_NONE, kwinApp()->x11RootWindow(), 0, 0, 0, 0, 640, 512);
    xcb_flush(connection());

    if (xcb_get_setup(c.get())->release_number < 11800000) {
        QSKIP("XWayland 1.18 required");
    }

    X11EventReaderHelper eventReader(c.get());
    QSignalSpy enteredSpy(&eventReader, &X11EventReaderHelper::entered);
    QSignalSpy leftSpy(&eventReader, &X11EventReaderHelper::left);

    // Extents of the client-side drop-shadow.
    NETStrut clientFrameExtent;
    clientFrameExtent.left = 10;
    clientFrameExtent.right = 10;
    clientFrameExtent.top = 5;
    clientFrameExtent.bottom = 20;

    xcb_rectangle_t boundingRect;
    boundingRect.x = 0;
    boundingRect.y = 0;
    boundingRect.width = 100 + clientFrameExtent.left + clientFrameExtent.right;
    boundingRect.height = 200 + clientFrameExtent.top + clientFrameExtent.bottom;

    xcb_window_t windowId = xcb_generate_id(c.get());
    const uint32_t values[] = {
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW};
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      boundingRect.x, boundingRect.y, boundingRect.width, boundingRect.height,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, values);
    xcb_size_hints_t hints{};
    xcb_icccm_size_hints_set_position(&hints, 1, boundingRect.x, boundingRect.y);
    xcb_icccm_size_hints_set_size(&hints, 1, boundingRect.width, boundingRect.height);
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    Test::applyMotifHints(c.get(), windowId, Test::MotifHints{
                                                 .flags = Test::MWM_HINTS_DECORATIONS,
                                                 .decorations = 0,
                                             });
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Normal);
    info.setGtkFrameExtents(clientFrameExtent);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(!window->isDecorated());
    QVERIFY(window->isClientSideDecorated());
    QCOMPARE(window->bufferGeometry(), RectF(0, 0, 120, 225));
    QCOMPARE(window->frameGeometry(), RectF(10, 5, 100, 200));

    QVERIFY(Test::waitForWaylandSurface(window));

    // Move pointer into the window, should trigger an enter.
    QVERIFY(!window->frameGeometry().contains(Cursors::self()->mouse()->pos()));
    QVERIFY(enteredSpy.isEmpty());
    input()->pointer()->warp(window->frameGeometry().center().toPoint());
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window->surface());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.last().first().toPoint(), QPoint(60, 105));

    // Move out of the window, should trigger a leave.
    QVERIFY(leftSpy.isEmpty());
    input()->pointer()->warp(window->frameGeometry().bottomRight() + QPoint(100, 100));
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.last().first().toPoint(), QPoint(60, 105));

    // Destroy the window.
    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
}

void XWaylandInputTest::testKeyboardInput()
{
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    X11EventReaderHelper eventReader(c.get());
    QVERIFY(eventReader.enableKeyboard());
    QSignalSpy keymapChangedSpy(&eventReader, &X11EventReaderHelper::keymapChanged);

    const xcb_window_t windowId = xcb_generate_id(c.get());
    const uint32_t values[] = {XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE};
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      100, 100, 400, 300, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                      XCB_CW_EVENT_MASK, values);
    xcb_size_hints_t hints{};
    xcb_icccm_size_hints_set_position(&hints, 1, 100, 100);
    xcb_icccm_size_hints_set_size(&hints, 1, 400, 300);
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Normal);

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(Test::waitForWaylandSurface(window));
    workspace()->activateWindow(window);
    QVERIFY(window->isActive());

    quint32 timestamp = 0;
    auto tap = [&timestamp](quint32 key) {
        Test::keyboardKeyPressed(key, timestamp++);
        Test::keyboardKeyReleased(key, timestamp++);
    };
    auto shiftedTap = [&timestamp](quint32 key) {
        Test::keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
        Test::keyboardKeyPressed(key, timestamp++);
        Test::keyboardKeyReleased(key, timestamp++);
        Test::keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    };

    // Exercise ordinary keys using the default keymap before generating custom keymaps.
    tap(KEY_A);
    shiftedTap(KEY_B);
    tap(KEY_SPACE);

    QTRY_COMPARE(eventReader.receivedText(), QStringLiteral("aB "));
    auto sendCustomKeysym = [&](xkb_keysym_t keysym) {
        input()->keyboard()->processKeySym(keysym, KeyboardKeyState::Pressed, std::chrono::milliseconds(timestamp++), nullptr);
        input()->keyboard()->processKeySym(keysym, KeyboardKeyState::Released, std::chrono::milliseconds(timestamp++), nullptr);
        QVERIFY(keymapChangedSpy.wait()); // IRL we can't wait this to happen in the client, but X is racey and this keeps the test working.
    };

    sendCustomKeysym(XKB_KEY_adiaeresis);
    sendCustomKeysym(XKB_KEY_Adiaeresis);
    sendCustomKeysym(0x01000000 | 0xC548); // 안"
    sendCustomKeysym(0x01000000 | 0x1F60A); // 😊
    tap(KEY_F);

    QTRY_COMPARE(eventReader.receivedText(), QStringLiteral("aB äÄ안😊f"));

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::XWaylandInputTest)
#include "xwayland_input_test.moc"
