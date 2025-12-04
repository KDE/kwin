/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "pluginmanager.h"
#include "pointer_input.h"
#include "tablet_input.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <linux/input-event-codes.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_buttonrebind-0");
static const QString s_pluginName = QStringLiteral("buttonsrebind");

class TestButtonRebind : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void initTestCase();

    void testKey_data();
    void testKey();

    void testMouse_data();
    void testMouse();

    void testMouseKeyboardMod_data();
    void testMouseKeyboardMod();

    void testDisabled();

    // NOTE: Mouse buttons are not tested because those are used in the other tests
    void testBindingTabletPad();
    void testBindingTabletTool();
    void testBindingTabletPadDialScroll();
    void testBindingTabletPadDialKey();
    void testBindingTabletRingKey();

    void testMouseTabletCursorSync();

private:
    quint32 timestamp = 1;
};

void TestButtonRebind::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());
}

void TestButtonRebind::cleanup()
{
    Test::destroyWaylandConnection();
    QVERIFY(QFile::remove(QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("kcminputrc"))));
}

void TestButtonRebind::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
}

void TestButtonRebind::testKey_data()
{
    QTest::addColumn<QKeySequence>("boundKeys");
    QTest::addColumn<QList<quint32>>("expectedKeys");

    QTest::newRow("single key") << QKeySequence(Qt::Key_A) << QList<quint32>{KEY_A};
    QTest::newRow("single modifier") << QKeySequence(Qt::Key_Control) << QList<quint32>{KEY_LEFTCTRL};
    QTest::newRow("single modifier plus key") << QKeySequence(Qt::ControlModifier | Qt::Key_N) << QList<quint32>{KEY_LEFTCTRL, KEY_N};
    QTest::newRow("multiple modifiers plus key") << QKeySequence(Qt::ControlModifier | Qt::MetaModifier | Qt::Key_Y) << QList<quint32>{KEY_LEFTCTRL, KEY_LEFTMETA, KEY_Y};
    QTest::newRow("delete") << QKeySequence(Qt::Key_Delete) << QList<quint32>{KEY_DELETE};
    QTest::newRow("keypad delete") << QKeySequence(Qt::KeypadModifier | Qt::Key_Delete) << QList<quint32>{KEY_KPDOT};
    QTest::newRow("keypad enter") << QKeySequence(Qt::KeypadModifier | Qt::Key_Enter) << QList<quint32>{KEY_KPENTER};
    QTest::newRow("exclamation mark") << QKeySequence(Qt::Key_Exclam) << QList<quint32>{KEY_LEFTSHIFT, KEY_1};
}

void TestButtonRebind::testKey()
{
    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("Mouse"));
    QFETCH(QKeySequence, boundKeys);
    buttonGroup.writeEntry("ExtraButton7", QStringList{"Key", boundKeys.toString(QKeySequence::PortableText)}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(enteredSpy.wait());

    // 0x119 is Qt::ExtraButton7
    Test::pointerButtonPressed(0x119, timestamp++);

    QVERIFY(keyChangedSpy.wait());
    QFETCH(QList<quint32>, expectedKeys);
    QCOMPARE(keyChangedSpy.count(), expectedKeys.count());
    for (int i = 0; i < keyChangedSpy.count(); i++) {
        QCOMPARE(keyChangedSpy.at(i).at(0).value<quint32>(), expectedKeys.at(i));
        QCOMPARE(keyChangedSpy.at(i).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    }
    Test::pointerButtonReleased(0x119, timestamp++);
}

void TestButtonRebind::testMouse_data()
{
    QTest::addColumn<int>("mouseButton");

    QTest::newRow("left button") << BTN_LEFT;
    QTest::newRow("middle button") << BTN_MIDDLE;
    QTest::newRow("right button") << BTN_RIGHT;
}

void TestButtonRebind::testMouse()
{
    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("Mouse"));
    QFETCH(int, mouseButton);
    buttonGroup.writeEntry("ExtraButton7", QStringList{"MouseButton", QString::number(mouseButton)}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy buttonChangedSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);

    const RectF startGeometry = window->frameGeometry();
    input()->pointer()->warp(startGeometry.center());

    QVERIFY(enteredSpy.wait());

    // 0x119 is Qt::ExtraButton7
    Test::pointerButtonPressed(0x119, timestamp++);

    QVERIFY(buttonChangedSpy.wait());

    QCOMPARE(buttonChangedSpy.count(), 1);
    Q_ASSERT(buttonChangedSpy.at(0).at(2).value<qint32>() == mouseButton);
    QCOMPARE(buttonChangedSpy.at(0).at(2).value<qint32>(), mouseButton);

    Test::pointerButtonReleased(0x119, timestamp++);
}

void TestButtonRebind::testMouseKeyboardMod_data()
{
    QTest::addColumn<Qt::KeyboardModifiers>("modifiers");
    QTest::addColumn<QList<quint32>>("expectedKeys");

    QTest::newRow("single ctrl") << Qt::KeyboardModifiers(Qt::ControlModifier) << QList<quint32>{KEY_LEFTCTRL};
    QTest::newRow("single alt") << Qt::KeyboardModifiers(Qt::AltModifier) << QList<quint32>{KEY_LEFTALT};
    QTest::newRow("single shift") << Qt::KeyboardModifiers(Qt::ShiftModifier) << QList<quint32>{KEY_LEFTSHIFT};

    // We have to test Meta with another key, because it will most likely trigger KWin to do some window operation.
    QTest::newRow("meta + alt") << Qt::KeyboardModifiers(Qt::MetaModifier | Qt::AltModifier) << QList<quint32>{KEY_LEFTALT, KEY_LEFTMETA};

    QTest::newRow("ctrl + alt + shift + meta") << Qt::KeyboardModifiers(Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier) << QList<quint32>{KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_LEFTALT, KEY_LEFTMETA};
}

void TestButtonRebind::testMouseKeyboardMod()
{
    QFETCH(Qt::KeyboardModifiers, modifiers);

    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("TabletTool")).group(QStringLiteral("Virtual Tablet Tool 1"));
    buttonGroup.writeEntry(QString::number(BTN_STYLUS), QStringList{"MouseButton", QString::number(BTN_LEFT), QString::number(modifiers.toInt())}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyboardKeyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(keyboardEnteredSpy.wait());

    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QSignalSpy pointerEnteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy pointerButtonChangedSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);

    const RectF startGeometry = window->frameGeometry();

    input()->pointer()->warp(startGeometry.center());
    QVERIFY(pointerEnteredSpy.wait());

    // Send the tablet button event so it can be processed by the filter
    Test::tabletToolButtonPressed(BTN_STYLUS, timestamp++);

    // The keyboard modifier is sent first
    QVERIFY(keyboardKeyChangedSpy.wait());

    QFETCH(QList<quint32>, expectedKeys);
    QCOMPARE(keyboardKeyChangedSpy.count(), expectedKeys.count());
    for (int i = 0; i < keyboardKeyChangedSpy.count(); i++) {
        QCOMPARE(keyboardKeyChangedSpy.at(i).at(0).value<quint32>(), expectedKeys.at(i));
        QCOMPARE(keyboardKeyChangedSpy.at(i).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    }

    // Then the mouse button is
    QCOMPARE(pointerButtonChangedSpy.count(), 1);
    QCOMPARE(pointerButtonChangedSpy.at(0).at(2).value<qint32>(), BTN_LEFT);

    Test::tabletToolButtonReleased(BTN_STYLUS, timestamp++);
}

void TestButtonRebind::testDisabled()
{
    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("Mouse"));
    buttonGroup.writeEntry("ExtraButton7", QStringList{"Disabled"}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy buttonChangedSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);

    const RectF startGeometry = window->frameGeometry();
    input()->pointer()->warp(startGeometry.center());

    QVERIFY(enteredSpy.wait());

    // 0x119 is Qt::ExtraButton7
    Test::pointerButtonPressed(0x119, timestamp++);

    // Qt::ExtraButton7 should not have been emitted if this button is disabled
    QVERIFY(!buttonChangedSpy.wait(std::chrono::milliseconds(100)));
    QCOMPARE(buttonChangedSpy.count(), 0);

    Test::pointerButtonReleased(0x119, timestamp++);
}

void TestButtonRebind::testBindingTabletPad()
{
    const QKeySequence sequence(Qt::Key_A);

    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("Tablet")).group(QStringLiteral("Virtual Tablet Pad 1"));
    buttonGroup.writeEntry("1", QStringList{"Key", sequence.toString(QKeySequence::PortableText)}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(enteredSpy.wait());

    Test::tabletPadButtonPressed(1, timestamp++);

    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 1);
    QCOMPARE(keyChangedSpy.at(0).at(0), KEY_A);

    Test::tabletPadButtonReleased(1, timestamp++);
}

void TestButtonRebind::testBindingTabletPadDialScroll()
{
    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("TabletDial")).group(QStringLiteral("Virtual Tablet Pad 1"));
    buttonGroup.writeEntry("0", QStringList{"Scroll"}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy axisChangedSpy(pointer.get(), &KWayland::Client::Pointer::axisChanged);
    QVERIFY(enteredSpy.wait());

    // twisting the dial "up"
    Test::tabletPadDialEvent(120.0, 0, timestamp++);

    using KWayland::Client::Keyboard;

    QVERIFY(axisChangedSpy.wait());
    QCOMPARE(axisChangedSpy.count(), 1);
    QCOMPARE(axisChangedSpy.at(0).at(2).value<qreal>(), 15);

    // twisting the dial "down"
    axisChangedSpy.clear();
    Test::tabletPadDialEvent(-120.0, 0, timestamp++);

    QVERIFY(axisChangedSpy.wait());
    QCOMPARE(axisChangedSpy.count(), 1);
    QCOMPARE(axisChangedSpy.at(0).at(2).value<qreal>(), -15);
}

void TestButtonRebind::testBindingTabletPadDialKey()
{
    const QKeySequence upSequence(Qt::Key_BracketLeft);
    const QKeySequence downSequence(Qt::Key_BracketRight);

    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("TabletDial")).group(QStringLiteral("Virtual Tablet Pad 1"));
    buttonGroup.writeEntry("0", QStringList{"AxisKey", upSequence.toString(QKeySequence::PortableText), downSequence.toString(QKeySequence::PortableText), QStringLiteral("120")}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(enteredSpy.wait());

    // twisting the dial "up"
    Test::tabletPadDialEvent(120.0, 0, timestamp++);

    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 2); // two events are reported because it emulates a press then release
    QCOMPARE(keyChangedSpy.at(0).at(0), KEY_LEFTBRACE);

    // twisting the dial "down"
    keyChangedSpy.clear();
    Test::tabletPadDialEvent(-120.0, 0, timestamp++);

    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 2);
    QCOMPARE(keyChangedSpy.at(0).at(0), KEY_RIGHTBRACE);
}

void TestButtonRebind::testBindingTabletRingKey()
{
    const QKeySequence upSequence(Qt::Key_BracketLeft);
    const QKeySequence downSequence(Qt::Key_BracketRight);

    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("TabletRing")).group(QStringLiteral("Virtual Tablet Pad 1")).group(QStringLiteral("0"));
    buttonGroup.writeEntry("0", QStringList{"AxisKey", upSequence.toString(QKeySequence::PortableText), downSequence.toString(QKeySequence::PortableText), QStringLiteral("600")}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(enteredSpy.wait());

    // touching the dial "clockwise"
    for (int i = 359; i > 353; i--) {
        Test::tabletPadRingEvent(i, 0, 0, 0, timestamp++);
    }
    Test::tabletPadRingEvent(-1, 0, 0, 0, timestamp++); // finger released

    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 2); // two events are reported because it emulates a press then release
    QCOMPARE(keyChangedSpy.at(0).at(0), KEY_LEFTBRACE);

    // touching the dial "counter-clockwise"
    keyChangedSpy.clear();
    for (int i = 1; i < 7; i++) {
        Test::tabletPadRingEvent(i, 0, 0, 0, timestamp++);
    }
    Test::tabletPadRingEvent(-1, 0, 0, 0, timestamp++); // finger released

    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 2);
    QCOMPARE(keyChangedSpy.at(0).at(0), KEY_RIGHTBRACE);
}

void TestButtonRebind::testBindingTabletTool()
{
    const QKeySequence sequence(Qt::Key_A);

    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("TabletTool")).group(QStringLiteral("Virtual Tablet Tool 1"));
    buttonGroup.writeEntry(QString::number(BTN_STYLUS), QStringList{"Key", sequence.toString(QKeySequence::PortableText)}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(enteredSpy.wait());

    const RectF startGeometry = window->frameGeometry();
    Test::tabletToolProximityEvent(startGeometry.center(), 0, 0, 0, 0, false, 0, timestamp++);

    Test::tabletToolButtonPressed(BTN_STYLUS, timestamp++);

    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 1);
    QCOMPARE(keyChangedSpy.at(0).at(0), KEY_A);

    Test::tabletToolButtonReleased(BTN_STYLUS, timestamp++);
}

void TestButtonRebind::testMouseTabletCursorSync()
{
    KConfigGroup buttonGroup = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("ButtonRebinds")).group(QStringLiteral("TabletTool")).group(QStringLiteral("Virtual Tablet Tool 1"));
    buttonGroup.writeEntry(QString::number(BTN_STYLUS), QStringList{"MouseButton", QString::number(BTN_LEFT)}, KConfig::Notify);
    buttonGroup.sync();

    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy buttonChangedSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);

    const RectF startGeometry = window->frameGeometry();

    // Move the mouse cursor to (25, 25)
    input()->pointer()->warp(startGeometry.topLeft() + QPointF{25.f, 25.5f});
    QVERIFY(enteredSpy.wait());

    // Move the tablet cursor to (10,10)
    Test::tabletToolProximityEvent(startGeometry.topLeft() + QPointF{10.f, 10.f}, 0, 0, 0, 0, false, 0, timestamp++);

    // Verify they are not starting in the same place
    QVERIFY(input()->pointer()->pos() != input()->tablet()->position());

    // Send the tablet button event so it can be processed by the filter
    Test::tabletToolButtonPressed(BTN_STYLUS, timestamp++);

    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 1);
    QCOMPARE(buttonChangedSpy.at(0).at(2).value<qint32>(), BTN_LEFT);

    Test::tabletToolButtonReleased(BTN_STYLUS, timestamp++);

    // Verify that by using the mouse button binding, the mouse cursor was moved to the tablet cursor position
    QVERIFY(input()->pointer()->pos() == input()->tablet()->position());
}

WAYLANDTEST_MAIN(TestButtonRebind)
#include "buttonrebind_test.moc"
