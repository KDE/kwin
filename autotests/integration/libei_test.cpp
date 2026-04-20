/*
    SPDX-FileCopyrightText: 2026 David Redondo <kde@david-redondo.de>
    SPDX-FileCopyrightText: 2026 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "input.h"
#include "kwin_wayland_test.h"
#include "main.h"
#include "pluginmanager.h"
#include "wayland_server.h"

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QSignalSpy>
#include <QSocketNotifier>

#include <libei.h>

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>

using namespace KWin;

static constexpr auto s_remoteDesktopPath = "/org/kde/KWin/EIS/RemoteDesktop";
static constexpr auto s_remoteDesktopInterface = "org.kde.KWin.EIS.RemoteDesktop";

enum class PortalCapabilities {
    Keyboard = 1,
    Pointer = 2,
};

class RemoteDesktopEiConnection
{
public:
    explicit RemoteDesktopEiConnection(uint capabilities)
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(QDBusConnection::sessionBus().baseService(), QString::fromLatin1(s_remoteDesktopPath), QString::fromLatin1(s_remoteDesktopInterface), QStringLiteral("connectToEIS"));
        msg << static_cast<int>(capabilities);
        const QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
        QVERIFY2(reply.type() == QDBusMessage::ReplyMessage, qPrintable(reply.errorMessage()));
        QCOMPARE(reply.arguments().size(), 2);

        m_cookie = reply.arguments().at(1).toInt();
        m_sender = ei_new_sender(nullptr);
        QVERIFY(m_sender);
        ei_configure_name(m_sender, "kwin-libei-test");
        const int fd = reply.arguments().at(0).value<QDBusUnixFileDescriptor>().takeFileDescriptor();
        QVERIFY(fd != -1);
        QCOMPARE(ei_setup_backend_fd(m_sender, fd), 0);
    }

    RemoteDesktopEiConnection(const RemoteDesktopEiConnection &) = delete;
    RemoteDesktopEiConnection &operator=(const RemoteDesktopEiConnection &) = delete;

    ~RemoteDesktopEiConnection()
    {
        if (m_pointerDevice) {
            ei_device_unref(m_pointerDevice);
        }
        if (m_keyboardDevice) {
            ei_device_unref(m_keyboardDevice);
        }
#if EIS_HAVE_16
        if (m_textDevice) {
            ei_device_unref(m_textDevice);
        }
#endif
        if (m_sender) {
            ei_unref(m_sender);
        }
        if (m_cookie != -1) {
            QDBusMessage msg = QDBusMessage::createMethodCall(QDBusConnection::sessionBus().baseService(), QString::fromLatin1(s_remoteDesktopPath), QString::fromLatin1(s_remoteDesktopInterface), QStringLiteral("disconnect"));
            msg << m_cookie;
            const QDBusReply<void> reply = QDBusConnection::sessionBus().call(msg);
            QVERIFY2(reply.isValid(), QTest::toString(reply.error()));
        }
    }

    void waitForDevices()
    {
        QSocketNotifier notifier(ei_get_fd(m_sender), QSocketNotifier::Read);
        QSignalSpy readableSpy(&notifier, &QSocketNotifier::activated);

        bool pointerResumed = false;
        bool keyboardResumed = false;
#if EIS_HAVE_16
        bool textResumed = false;
#endif

        while ((!m_pointerDevice || !m_keyboardDevice
#if EIS_HAVE_16
                || !m_textDevice || !pointerResumed || !keyboardResumed || !textResumed
#else
                || !pointerResumed || !keyboardResumed
#endif
                )
               && readableSpy.wait()) {
            ei_dispatch(m_sender);
            while (ei_event *event = ei_get_event(m_sender)) {
                switch (ei_event_get_type(event)) {
                case EI_EVENT_CONNECT:
                case EI_EVENT_SYNC:
                    break;
                case EI_EVENT_SEAT_ADDED:
                    ei_seat_bind_capabilities(ei_event_get_seat(event),
                                              EI_DEVICE_CAP_POINTER,
                                              EI_DEVICE_CAP_POINTER_ABSOLUTE,
                                              EI_DEVICE_CAP_BUTTON,
                                              EI_DEVICE_CAP_SCROLL,
                                              EI_DEVICE_CAP_KEYBOARD,
#if EIS_HAVE_16
                                              EI_DEVICE_CAP_TEXT,
#endif
                                              nullptr);
                    break;
                case EI_EVENT_DEVICE_ADDED: {
                    ei_device *device = ei_event_get_device(event);
                    if (!m_pointerDevice && ei_device_has_capability(device, EI_DEVICE_CAP_POINTER) && ei_device_has_capability(device, EI_DEVICE_CAP_BUTTON)) {
                        m_pointerDevice = ei_device_ref(device);
                    } else if (!m_keyboardDevice && ei_device_has_capability(device, EI_DEVICE_CAP_KEYBOARD)) {
                        m_keyboardDevice = ei_device_ref(device);
#if EIS_HAVE_16
                    } else if (!m_textDevice && ei_device_has_capability(device, EI_DEVICE_CAP_TEXT)) {
                        m_textDevice = ei_device_ref(device);
#endif
                    }
                    break;
                }
                case EI_EVENT_DEVICE_RESUMED: {
                    ei_device *device = ei_event_get_device(event);
                    pointerResumed |= device == m_pointerDevice;
                    keyboardResumed |= device == m_keyboardDevice;
#if EIS_HAVE_16
                    textResumed |= device == m_textDevice;
#endif
                    break;
                }
                default:
                    break;
                }
                ei_event_unref(event);
            }
        }

        QVERIFY(m_pointerDevice);
        QVERIFY(m_keyboardDevice);
#if EIS_HAVE_16
        QVERIFY(m_textDevice);
#endif
        QVERIFY(pointerResumed);
        QVERIFY(keyboardResumed);
#if EIS_HAVE_16
        QVERIFY(textResumed);
#endif
    }

    ei *sender() const
    {
        return m_sender;
    }

    ei_device *pointerDevice() const
    {
        return m_pointerDevice;
    }

    ei_device *keyboardDevice() const
    {
        return m_keyboardDevice;
    }

    ei_device *textDevice() const
    {
#if EIS_HAVE_16
        return m_textDevice;
#else
        return nullptr;
#endif
    }

private:
    int m_cookie = -1;
    ei *m_sender = nullptr;
    ei_device *m_pointerDevice = nullptr;
    ei_device *m_keyboardDevice = nullptr;
#if EIS_HAVE_16
    ei_device *m_textDevice = nullptr;
#endif
};

class TestLibei : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testSender();
};

void TestLibei::initTestCase()
{
    QVERIFY(waylandServer()->init(qAppName()));
    static_cast<WaylandTestApplication *>(kwinApp())->setInputMethodServerToStart("internal");
    kwinApp()->start();
    Test::setOutputConfig({Rect(0, 0, 1280, 1024)});
    QVERIFY(kwinApp()->pluginManager()->loadedPlugins().contains("eis"));
}

void TestLibei::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
}

void TestLibei::cleanup()
{
    if (Test::waylandConnection()) {
        Test::destroyWaylandConnection();
    }
}

void TestLibei::testSender()
{
    QVERIFY(Test::waitForWaylandPointer());
    QVERIFY(Test::waitForWaylandKeyboard());

    auto pointer = std::unique_ptr<KWayland::Client::Pointer>(Test::waylandSeat()->createPointer());
    auto keyboard = std::unique_ptr<KWayland::Client::Keyboard>(Test::waylandSeat()->createKeyboard());
    auto simpleKeyboard = std::make_unique<Test::SimpleKeyboard>();
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());

    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::cyan);
    QVERIFY(window);
    window->move(QPoint(0, 0));

    quint32 timestamp = 0;
    Test::pointerMotion(QPoint(100, 100), ++timestamp);
    Test::waylandSync();
    QCOMPARE(pointer->enteredSurface(), surface.get());
    QCOMPARE(keyboard->enteredSurface(), surface.get());
    QCOMPARE(simpleKeyboard->keyboard()->enteredSurface(), surface.get());

    RemoteDesktopEiConnection sender(static_cast<uint>(PortalCapabilities::Keyboard) | static_cast<uint>(PortalCapabilities::Pointer));
    sender.waitForDevices();

    QSignalSpy motionSpy(pointer.get(), &KWayland::Client::Pointer::motion);
    QSignalSpy buttonSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy keySpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
#if EIS_HAVE_16
    QSignalSpy keySymSpy(simpleKeyboard.get(), &Test::SimpleKeyboard::keySymRecevied);
    QSignalSpy receivedTextChangedSpy(simpleKeyboard.get(), &Test::SimpleKeyboard::receviedTextChanged);
#endif

    uint32_t sequence = 1;
    ei_device_start_emulating(sender.pointerDevice(), sequence);
    ei_device_start_emulating(sender.keyboardDevice(), sequence);
#if EIS_HAVE_16
    ei_device_start_emulating(sender.textDevice(), sequence);
#endif

    ei_device_pointer_motion(sender.pointerDevice(), 12, -7);
    ei_device_frame(sender.pointerDevice(), ei_now(sender.sender()));
    QVERIFY(motionSpy.wait());
    QCOMPARE(motionSpy.last().at(0).toPointF(), QPointF(112, 93));
    QCOMPARE(input()->globalPointer(), QPoint(112, 93));

    ei_device_button_button(sender.pointerDevice(), BTN_LEFT, true);
    ei_device_frame(sender.pointerDevice(), ei_now(sender.sender()));
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.last().at(2).value<quint32>(), BTN_LEFT);
    QCOMPARE(buttonSpy.last().at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Pressed);

    ei_device_button_button(sender.pointerDevice(), BTN_LEFT, false);
    ei_device_frame(sender.pointerDevice(), ei_now(sender.sender()));
    QVERIFY(buttonSpy.count() == 2 || buttonSpy.wait());
    QCOMPARE(buttonSpy.last().at(2).value<quint32>(), BTN_LEFT);
    QCOMPARE(buttonSpy.last().at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Released);

    ei_device_keyboard_key(sender.keyboardDevice(), KEY_F1, true);
    ei_device_frame(sender.keyboardDevice(), ei_now(sender.sender()));
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.last().at(0).value<quint32>(), KEY_F1);
    QCOMPARE(keySpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);

    ei_device_keyboard_key(sender.keyboardDevice(), KEY_F1, false);
    ei_device_frame(sender.keyboardDevice(), ei_now(sender.sender()));
    QVERIFY(keySpy.count() == 2 || keySpy.wait());
    QCOMPARE(keySpy.last().at(0).value<quint32>(), KEY_F1);
    QCOMPARE(keySpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);

#if EIS_HAVE_16
    keySymSpy.clear();

    ei_device_text_keysym(sender.textDevice(), XKB_KEY_a, true);
    ei_device_text_keysym(sender.textDevice(), XKB_KEY_a, false);
    ei_device_frame(sender.textDevice(), ei_now(sender.sender()));
    keySymSpy.wait();
    QCOMPARE(keySymSpy.at(0).at(0).toUInt(), XKB_KEY_a);
    keySymSpy.clear();

    ei_device_text_keysym(sender.textDevice(), XKB_KEY_A, true);
    ei_device_text_keysym(sender.textDevice(), XKB_KEY_A, false);
    ei_device_frame(sender.textDevice(), ei_now(sender.sender()));
    keySymSpy.wait();
    QCOMPARE(keySymSpy.at(0).at(0).toUInt(), XKB_KEY_A);
    keySymSpy.clear();

    ei_device_text_keysym(sender.textDevice(), XKB_KEY_Left, true);
    ei_device_text_keysym(sender.textDevice(), XKB_KEY_Left, false);
    ei_device_frame(sender.textDevice(), ei_now(sender.sender()));
    keySymSpy.wait();
    QCOMPARE(keySymSpy.at(0).at(0).toUInt(), XKB_KEY_Left);
    keySymSpy.clear();

    simpleKeyboard->clearReceivedText();
    const QByteArray helloText = QStringLiteral("hello 😀").toUtf8();
    ei_device_text_utf8_with_length(sender.textDevice(), helloText.constData(), helloText.size());
    ei_device_frame(sender.textDevice(), ei_now(sender.sender()));

    bool matched = false;
    for (int i = 0; i < 20; ++i) {
        if (simpleKeyboard->receviedText() == QStringLiteral("hello 😀")) {
            matched = true;
            break;
        }
        receivedTextChangedSpy.wait();
    }
    QVERIFY(matched);

    ei_device_stop_emulating(sender.textDevice());
#endif
    ei_device_stop_emulating(sender.keyboardDevice());
    ei_device_stop_emulating(sender.pointerDevice());
}

WAYLANDTEST_MAIN(TestLibei)

#include "libei_test.moc"
