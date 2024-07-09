/*
    KWin - the KDE window manager
    This file is part of the KDE project.

 SPDX-FileCopyrightText: 2024 David Edmundson <davidedmundson@kde.org>

 SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "KWayland/Client/keyboard.h"
#include "KWayland/Client/seat.h"
#include "kwin_wayland_test.h"

#include "input.h"
#include "options.h"
#include "pointer_input.h"
#include "qabstracteventdispatcher.h"
#include "qsocketnotifier.h"
#include "wayland/keyboard.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "workspace.h"
#include <KConfigGroup>
#include <linux/input.h>

#define explicit xcb_explicit
#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/xinput.h>
#include <xcb/xkb.h>
#undef explicit

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_x11-key-read-0");

enum class State {
    Press,
    Release
} state;
typedef QPair<State, int> KeyAction;
Q_DECLARE_METATYPE(KeyAction);

/*
 * This tests the "Legacy App Support" feature of allowing X11 apps to get notified of some key press events
 */
class X11KeyReadTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase_data();
    void initTestCase();
    void init();
    void cleanup();

    void testSimpleLetter();
    void onlyModifier();
    void letterWithModifier();
    void testWaylandWindowHasFocus();
    void tabBox();

private:
    QList<KeyAction> recievedX11EventsForInput(const QList<KeyAction> &keyEventsIn);
};

void X11KeyReadTest::initTestCase_data()
{
    QTest::addColumn<XwaylandEavesdropsMode>("operatingMode");
    QTest::newRow("all") << XwaylandEavesdropsMode::All;
    QTest::newRow("allWithModifier") << XwaylandEavesdropsMode::AllKeysWithModifier;
    QTest::newRow("nonCharacter") << XwaylandEavesdropsMode::NonCharacterKeys;
    QTest::newRow("none") << XwaylandEavesdropsMode::None;
}

void X11KeyReadTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
    });

    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
}

void X11KeyReadTest::init()
{
    QFETCH_GLOBAL(XwaylandEavesdropsMode, operatingMode);
    options->setXwaylandEavesdrops(operatingMode);
    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::input()->pointer()->warp(QPoint(640, 512));

    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandKeyboard());
}

void X11KeyReadTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void X11KeyReadTest::testSimpleLetter()
{
    // press a
    QList<KeyAction> keyEvents = {
        {State::Press, KEY_A},
        {State::Release, KEY_A},
    };
    auto received = recievedX11EventsForInput(keyEvents);

    QList<KeyAction> expected;
    QFETCH_GLOBAL(XwaylandEavesdropsMode, operatingMode);
    switch (operatingMode) {
    case XwaylandEavesdropsMode::None:
    case XwaylandEavesdropsMode::NonCharacterKeys:
    case XwaylandEavesdropsMode::AllKeysWithModifier:
        expected = {};
        break;
    case XwaylandEavesdropsMode::All:
        expected = keyEvents;
        break;
    }

    QCOMPARE(received, expected);
}

void X11KeyReadTest::onlyModifier()
{
    QList<KeyAction> keyEvents = {
                                  {State::Press, KEY_LEFTALT},
                                  {State::Release, KEY_LEFTALT},
                                  };
    auto received = recievedX11EventsForInput(keyEvents);

    QList<KeyAction> expected;
    QFETCH_GLOBAL(XwaylandEavesdropsMode, operatingMode);
    switch (operatingMode) {
    case XwaylandEavesdropsMode::None:
        expected = {};
        break;
    case XwaylandEavesdropsMode::NonCharacterKeys:
    case XwaylandEavesdropsMode::AllKeysWithModifier:
    case XwaylandEavesdropsMode::All:
        expected = keyEvents;
        break;
    }

    QCOMPARE(received, expected);
}

void X11KeyReadTest::letterWithModifier()
{
    QList<KeyAction> keyEvents = {
                                  {State::Press, KEY_LEFTALT},
                                  {State::Press, KEY_F},
                                  {State::Release, KEY_F},
                                  {State::Release, KEY_LEFTALT},
                                  };
    auto received = recievedX11EventsForInput(keyEvents);

    QList<KeyAction> expected;
    QFETCH_GLOBAL(XwaylandEavesdropsMode, operatingMode);
    switch (operatingMode) {
    case XwaylandEavesdropsMode::None:
        expected = {};
        break;
    case XwaylandEavesdropsMode::NonCharacterKeys:
        expected = {
            {State::Press, KEY_LEFTALT},
            {State::Release, KEY_LEFTALT},
        };
        break;
    case XwaylandEavesdropsMode::AllKeysWithModifier:
    case XwaylandEavesdropsMode::All:
        expected = keyEvents;
        break;
    }
    QCOMPARE(received, expected);
}

void X11KeyReadTest::testWaylandWindowHasFocus()
{
    // A wayland window should be unaffected
    int timestamp = 0;

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy modifierSpy(keyboard.get(), &KWayland::Client::Keyboard::modifiersChanged);
    QSignalSpy keyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    Window *waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(10, 10), Qt::blue);
    QVERIFY(waylandWindow->isActive());
    modifierSpy.wait(); // initial modifiers
    modifierSpy.clear();

    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.last()[0], KEY_LEFTALT);
    QCOMPARE(keyChangedSpy.last()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);

    QCOMPARE(modifierSpy.count(), 1);
    QCOMPARE(modifierSpy.last()[0], 8);
    QCOMPARE(modifierSpy.last()[1], 0);
    QCOMPARE(modifierSpy.last()[2], 0);
    QCOMPARE(modifierSpy.last()[3], 0);

    Test::keyboardKeyPressed(KEY_G, timestamp++);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.last()[0], KEY_G);
    QCOMPARE(keyChangedSpy.last()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);

    Test::keyboardKeyReleased(KEY_G, timestamp++);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.last()[0], KEY_G);
    QCOMPARE(keyChangedSpy.last()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);

    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.last()[0], KEY_LEFTALT);
    QCOMPARE(keyChangedSpy.last()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);

    QCOMPARE(modifierSpy.count(), 2);
    QCOMPARE(modifierSpy.last()[0], 0);
    QCOMPARE(modifierSpy.last()[1], 0);
    QCOMPARE(modifierSpy.last()[2], 0);
    QCOMPARE(modifierSpy.last()[3], 0);
}

void X11KeyReadTest::tabBox()
{
    // Note this test will fail if you forget to use dbus-run-session!
#if KWIN_BUILD_TABBOX
    QList<KeyAction> keyEvents = {
        {State::Press, KEY_LEFTALT},
        {State::Press, KEY_TAB},
        {State::Release, KEY_TAB},
        {State::Press, KEY_TAB},
        {State::Release, KEY_TAB},
        {State::Release, KEY_LEFTALT},
    };
    auto received = recievedX11EventsForInput(keyEvents);

    QList<KeyAction> expected;
    QFETCH_GLOBAL(XwaylandEavesdropsMode, operatingMode);
    switch (operatingMode) {
    case XwaylandEavesdropsMode::None:
        expected = {};
        break;
    // even though tab is a regular key whilst holding alt, the tab switcher should be grabbing
    case XwaylandEavesdropsMode::AllKeysWithModifier:
    case XwaylandEavesdropsMode::NonCharacterKeys:
    case XwaylandEavesdropsMode::All:
        expected = {
            {State::Press, KEY_LEFTALT},
            {State::Release, KEY_LEFTALT},
        };
        break;
    }

    QCOMPARE(received, expected);
#endif
}

class X11EventRecorder :
    public
        QObject
        {
            Q_OBJECT
        public:
            X11EventRecorder(xcb_connection_t * c);
            QList<KeyAction> keyEvents() const
            {
                return m_keyEvents;
            }
        Q_SIGNALS:
            void fenceReceived();

        private:
            void processXcbEvents();
            QList<KeyAction> m_keyEvents;
            xcb_connection_t *m_connection;
            QSocketNotifier *m_notifier;
        };

        X11EventRecorder::X11EventRecorder(xcb_connection_t * c)
            : QObject()
            , m_connection(c)
            , m_notifier(new QSocketNotifier(xcb_get_file_descriptor(m_connection), QSocketNotifier::Read, this))
        {
            struct
            {
                xcb_input_event_mask_t head;
                xcb_input_xi_event_mask_t mask;
            } mask;
            mask.head.deviceid = XCB_INPUT_DEVICE_ALL;
            mask.head.mask_len = sizeof(mask.mask) / sizeof(uint32_t);
            mask.mask = static_cast<xcb_input_xi_event_mask_t>(
                XCB_INPUT_XI_EVENT_MASK_KEY_PRESS
                | XCB_INPUT_XI_EVENT_MASK_KEY_RELEASE
                | XCB_INPUT_RAW_KEY_PRESS
                | XCB_INPUT_RAW_KEY_RELEASE);

            xcb_input_xi_select_events(c, kwinApp()->x11RootWindow(), 1, &mask.head);
            xcb_flush(c);

            connect(m_notifier, &QSocketNotifier::activated, this, &X11EventRecorder::processXcbEvents);
            connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &X11EventRecorder::processXcbEvents);
            connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, &X11EventRecorder::processXcbEvents);
        }

        void X11EventRecorder::processXcbEvents()
        {
            xcb_generic_event_t *event;
            while ((event = xcb_poll_for_event(m_connection))) {
                u_int8_t responseType = event->response_type & ~0x80;
                if (responseType == XCB_GE_GENERIC) {
                    auto *geEvent = reinterpret_cast<xcb_ge_generic_event_t *>(event);
                    if (geEvent->event_type == XCB_INPUT_KEY_PRESS || geEvent->event_type == XCB_INPUT_KEY_RELEASE) {

                        auto keyEvent = reinterpret_cast<xcb_input_key_press_event_t *>(geEvent);
                        int nativeKeyCode = keyEvent->detail - 0x08;

                        if (nativeKeyCode == 0) {
                            Q_EMIT fenceReceived();
                        } else {
                            KeyAction action({geEvent->event_type == XCB_INPUT_KEY_PRESS ? State::Press : State::Release, nativeKeyCode});
                            m_keyEvents << action;
                        }
                    }
                }
                std::free(event);
            }

            xcb_flush(m_connection);
        }

QList<KeyAction> X11KeyReadTest::recievedX11EventsForInput(const QList<KeyAction> &keysIn)
{
    quint32 timestamp = 1;
    Test::XcbConnectionPtr c = Test::createX11Connection();

    X11EventRecorder eventReader(c.get());

    QSignalSpy fenceEventSpy(&eventReader, &X11EventRecorder::fenceReceived);

    for (const KeyAction &action : keysIn) {
        if (action.first == State::Press) {
            Test::keyboardKeyPressed(action.second, timestamp++);
        } else {
            Test::keyboardKeyReleased(action.second, timestamp++);
        }
        Test::flushWaylandConnection();
        QTest::qWait(5);
    }
    // special case, explicitly send key 0, to use as a fence
    ClientConnection *xwaylandClient = waylandServer()->xWaylandConnection();
    waylandServer()->seat()->keyboard()->sendKey(0, KeyboardKeyState::Pressed, xwaylandClient);

    Test::flushWaylandConnection();

    bool fenceComplete = fenceEventSpy.wait();
    Q_ASSERT(fenceComplete);

    return eventReader.keyEvents();
}

WAYLANDTEST_MAIN(X11KeyReadTest)
#include "x11keyread.moc"
