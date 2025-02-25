/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

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

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_input_capture=0");
static QString kwinInputCapturePath = QStringLiteral("/org/kde/KWin/EIS/InputCapture");
static QString kwinInputCaptureManagerInterface = QStringLiteral("org.kde.KWin.EIS.InputCaptureManager");
static QString kwinInputCaptureInterface = QStringLiteral("org.kde.KWin.EIS.InputCapture");

enum class PortalCapabilities {
    Keyboard = 1,
    Pointer = 2,
    Touch = 4,
    All = Keyboard | Pointer | Touch,
};

class InputCapture
{
public:
    InputCapture()
    {
        auto msg = QDBusMessage::createMethodCall(QDBusConnection::sessionBus().baseService(), kwinInputCapturePath, kwinInputCaptureManagerInterface, QStringLiteral("addInputCapture"));
        msg << static_cast<int>(PortalCapabilities::All);
        QDBusReply<QDBusObjectPath> captureReply = QDBusConnection::sessionBus().call(msg);
        QVERIFY2(captureReply.isValid(), QTest::toString(captureReply.error()));
        dbusPath = captureReply.value().path();

        msg = QDBusMessage::createMethodCall(QDBusConnection::sessionBus().baseService(), dbusPath, kwinInputCaptureInterface, QStringLiteral("connectToEIS"));
        QDBusReply<QDBusUnixFileDescriptor> eisReply = QDBusConnection::sessionBus().call(msg);
        QVERIFY2(eisReply.isValid(), QTest::toString(eisReply.error()));
        eifd = eisReply.value().takeFileDescriptor();

        const QList<QPair<QPoint, QPoint>> barriers = {
            {{0, 0}, {0, 1023}},
            {{0, 0}, {1279, 0}},
            {{1279, 0}, {1279, 1023}},
            {{0, 1023}, {1279, 1023}},
        };
        msg = QDBusMessage::createMethodCall(QDBusConnection::sessionBus().baseService(), dbusPath, kwinInputCaptureInterface, QStringLiteral("enable"));
        msg << QVariant::fromValue(barriers);
        QDBusReply<void> enableReply = QDBusConnection::sessionBus().call(msg);
        QVERIFY2(enableReply.isValid(), QTest::toString(enableReply.error()));
    }

    bool setupSuccessfully() const
    {
        return eifd != 1 && !dbusPath.isEmpty();
    }

    int eifd = -1;
    QString dbusPath;
};

class TestInputCapture : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testInputCapture_data();
    void testInputCapture();
    void disconnectingEiRemovesCapture();
};

void TestInputCapture::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
}

void TestInputCapture::cleanup()
{
    if (Test::waylandConnection()) {
        Test::destroyWaylandConnection();
    }
}

void TestInputCapture::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});
    kwinApp()->start();
    QVERIFY(kwinApp()->pluginManager()->loadedPlugins().contains("eis"));
}

void TestInputCapture::testInputCapture_data()
{
    QTest::addColumn<QPoint>("mousePos");
    QTest::addColumn<QPointF>("delta");
    QTest::addRow("left") << QPoint(0, 512) << QPointF(-1, 0);
    QTest::addRow("up") << QPoint(640, 0) << QPointF(0, -1);
    QTest::addRow("right") << QPoint(1279, 512) << QPointF(1, 0);
    QTest::addRow("down") << QPoint(640, 1023) << QPointF(0, 1);
}

void TestInputCapture::testInputCapture()
{
    QFETCH(QPoint, mousePos);
    QFETCH(QPointF, delta);
    int timestamp = 0;

    QVERIFY(Test::waitForWaylandPointer());
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());

    Test::renderAndWaitForShown(surface.get(), {1280, 1024}, Qt::cyan);

    InputCapture capture;
    if (!capture.setupSuccessfully()) {
        return;
    }

    Test::pointerMotion(mousePos, ++timestamp);
    Test::waylandSync();
    QCOMPARE(keyboard->enteredSurface(), surface.get());
    QCOMPARE(pointer->enteredSurface(), surface.get());

    QSignalSpy motionSpy(pointer.get(), &KWayland::Client::Pointer::motion);
    QSignalSpy buttonSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy axisSpy(pointer.get(), &KWayland::Client::Pointer::axisChanged);
    QSignalSpy keySpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(motionSpy.isValid());
    QVERIFY(buttonSpy.isValid());
    QVERIFY(axisSpy.isValid());
    QVERIFY(keySpy.isValid());

    auto ei = ei_new_receiver(nullptr);
    ei_setup_backend_fd(ei, capture.eifd);
    QSocketNotifier eiNotifier(ei_get_fd(ei), QSocketNotifier::Read);
    QSignalSpy eiReadableSpy(&eiNotifier, &QSocketNotifier::activated);

    int numDevices = 0;
    while (numDevices != 3 && eiReadableSpy.wait()) {
        ei_dispatch(ei);
        while (auto event = ei_get_event(ei)) {
            switch (const auto type = ei_event_get_type(event)) {
            case EI_EVENT_CONNECT:
#if HAVE_EI_EVENT_SYNC
            case EI_EVENT_SYNC:
#endif
                break;
            case EI_EVENT_SEAT_ADDED:
                ei_seat_bind_capabilities(ei_event_get_seat(event), EI_DEVICE_CAP_POINTER, EI_DEVICE_CAP_POINTER_ABSOLUTE, EI_DEVICE_CAP_KEYBOARD, EI_DEVICE_CAP_TOUCH, EI_DEVICE_CAP_SCROLL, EI_DEVICE_CAP_BUTTON, nullptr);
            case EI_EVENT_DEVICE_ADDED:
                break;
            case EI_EVENT_DEVICE_RESUMED:
                ++numDevices;
                break;
            default:
                qFatal() << "unexpected event:" << type;
            }
            ei_event_unref(event);
        }
    }

    {
        Test::pointerMotionRelative(delta, ++timestamp);

        Test::waylandSync();
        QVERIFY(motionSpy.empty());

        ei_dispatch(ei);

        for (int i = 0; i < numDevices; ++i) {
            auto event = ei_get_event(ei);
            QCOMPARE(ei_event_get_type(event), EI_EVENT_DEVICE_START_EMULATING);
            ei_event_unref(event);
        }

        auto event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_POINTER_MOTION);
        QCOMPARE(ei_event_pointer_get_dx(event), delta.x());
        QCOMPARE(ei_event_pointer_get_dy(event), delta.y());
        ei_event_unref(event);

        event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_FRAME);
        ei_event_unref(event);
    }

    {
        Test::pointerButtonPressed(BTN_LEFT, ++timestamp);
        Test::pointerButtonReleased(BTN_LEFT, ++timestamp);

        Test::waylandSync();
        QVERIFY(buttonSpy.empty());

        ei_dispatch(ei);

        auto event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_BUTTON_BUTTON);
        QCOMPARE(ei_event_button_get_button(event), BTN_LEFT);
        QCOMPARE(ei_event_button_get_is_press(event), true);
        ei_event_unref(event);

        event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_FRAME);

        ei_event_unref(event);
        event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_BUTTON_BUTTON);
        QCOMPARE(ei_event_button_get_button(event), BTN_LEFT);
        QCOMPARE(ei_event_button_get_is_press(event), false);
        ei_event_unref(event);

        event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_FRAME);
        ei_event_unref(event);
        event = ei_get_event(ei);
    }

    {
        Test::pointerAxisHorizontal(1, ++timestamp);

        Test::waylandSync();
        QVERIFY(axisSpy.empty());

        ei_dispatch(ei);

        auto event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_SCROLL_DELTA);
        QCOMPARE(ei_event_scroll_get_dx(event), 1);
        QCOMPARE(ei_event_scroll_get_dy(event), 0);
        ei_event_unref(event);

        event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_FRAME);
        ei_event_unref(event);
    }

    {
        Test::keyboardKeyPressed(KEY_A, ++timestamp);
        Test::keyboardKeyReleased(KEY_A, ++timestamp);

        Test::waylandSync();
        QVERIFY(buttonSpy.empty());

        ei_dispatch(ei);

        auto event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_KEYBOARD_KEY);
        QCOMPARE(ei_event_keyboard_get_key(event), KEY_A);
        QCOMPARE(ei_event_keyboard_get_key_is_press(event), true);
        ei_event_unref(event);

        event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_FRAME);
        ei_event_unref(event);

        event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_KEYBOARD_KEY);
        QCOMPARE(ei_event_keyboard_get_key(event), KEY_A);
        QCOMPARE(ei_event_keyboard_get_key_is_press(event), false);
        ei_event_unref(event);

        event = ei_get_event(ei);
        QCOMPARE(ei_event_get_type(event), EI_EVENT_FRAME);
        ei_event_unref(event);
    }

    auto msg = QDBusMessage::createMethodCall(QDBusConnection::sessionBus().baseService(), capture.dbusPath, kwinInputCaptureInterface, QStringLiteral("release"));
    msg << QVariant::fromValue(QPointF(1, 1)) << true;
    QDBusReply<void> releaseReply = QDBusConnection::sessionBus().call(msg);
    QVERIFY2(releaseReply.isValid(), QTest::toString(releaseReply.error()));

    QCOMPARE(input()->globalPointer(), QPoint(1, 1));

    QVERIFY(eiReadableSpy.wait());
    eiReadableSpy.clear();
    ei_dispatch(ei);
    // Throw away the disconnection events
    while (auto event = ei_get_event(ei)) {
        ei_event_unref(event);
    }

    Test::pointerMotion({2, 2}, ++timestamp);
    Test::pointerButtonPressed(BTN_LEFT, ++timestamp);
    Test::pointerButtonReleased(BTN_LEFT, ++timestamp);
    Test::pointerAxisHorizontal(1, ++timestamp);
    Test::keyboardKeyPressed(KEY_A, ++timestamp);
    Test::keyboardKeyReleased(KEY_A, ++timestamp);
    QVERIFY(motionSpy.wait());
    QVERIFY(buttonSpy.count());
    QVERIFY(axisSpy.count());
    QVERIFY(keySpy.count());

    if (!eiReadableSpy.empty()) {
        ei_dispatch(ei);
        QVERIFY(!ei_peek_event(ei));
    }

    msg = QDBusMessage::createMethodCall(QDBusConnection::sessionBus().baseService(), kwinInputCapturePath, kwinInputCaptureManagerInterface, QStringLiteral("removeInputCapture"));
    msg << QDBusObjectPath(capture.dbusPath);
    QDBusReply<void> removeReply = QDBusConnection::sessionBus().call(msg);
    QVERIFY2(removeReply.isValid(), QTest::toString(removeReply.error()));

    ei_unref(ei);
}

void TestInputCapture::disconnectingEiRemovesCapture()
{
    InputCapture capture;
    if (!capture.setupSuccessfully()) {
        return;
    }

    auto ei = ei_new_receiver(nullptr);
    ei_setup_backend_fd(ei, capture.eifd);
    QSocketNotifier eiNotifier(ei_get_fd(ei), QSocketNotifier::Read);
    QSignalSpy eiReadableSpy(&eiNotifier, &QSocketNotifier::activated);

    bool connected = false;
    while (!connected && eiReadableSpy.wait()) {
        ei_dispatch(ei);

        if (auto event = ei_get_event(ei)) {
            if (ei_event_get_type(event) == EI_EVENT_CONNECT) {
                connected = true;
            }
            ei_event_unref(event);
        }
    }

    ei_unref(ei);

    QTRY_COMPARE(QDBusConnection::sessionBus().objectRegisteredAt(capture.dbusPath), nullptr);
}

WAYLANDTEST_MAIN(TestInputCapture)

#include "input_capture_test.moc"
