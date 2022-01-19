/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "abstract_output.h"
#include "cursor.h"
#include "effects.h"
#include "deleted.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "inputmethod.h"
#include "virtualkeyboard_dbus.h"
#include "qwayland-input-method-unstable-v1.h"
#include "qwayland-text-input-unstable-v3.h"

#include <QTest>
#include <QSignalSpy>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <KWaylandServer/clientconnection.h>
#include <KWaylandServer/display.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/surface_interface.h>

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/textinput.h>
#include <linux/input-event-codes.h>

using namespace KWin;
using namespace KWayland::Client;
using KWin::VirtualKeyboardDBus;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_inputmethod-0");

class InputMethodTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testOpenClose();
    void testEnableDisableV3();
    void testEnableActive();
    void testHidePanel();
    void testSwitchFocusedSurfaces();
    void testV3Styling();
    void testDisableShowInputPanel();
    void testModifierForwarding();

private:
    void touchNow() {
        static int time = 0;
        kwinApp()->platform()->touchDown(0, {100, 100}, ++time);
        kwinApp()->platform()->touchUp(0, ++time);
    }
};


void InputMethodTest::initTestCase()
{
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.kwin.testvirtualkeyboard"));

    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::AbstractClient *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    static_cast<WaylandTestApplication *>(kwinApp())->setInputMethodServerToStart("internal");
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = kwinApp()->platform()->enabledOutputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    Test::initWaylandWorkspace();

}

void InputMethodTest::init()
{
    touchNow();
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat |
                                         Test::AdditionalWaylandInterface::TextInputManagerV2 |
                                         Test::AdditionalWaylandInterface::InputMethodV1 |
                                         Test::AdditionalWaylandInterface::TextInputManagerV3));

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));

    InputMethod::self()->setEnabled(true);
}

void InputMethodTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void InputMethodTest::testOpenClose()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QSignalSpy clientRemovedSpy(workspace(), &Workspace::clientRemoved);
    QVERIFY(clientAddedSpy.isValid());

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    QScopedPointer<TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    // Show the keyboard
    touchNow();
    textInput->showInputPanel();
    QVERIFY(clientAddedSpy.wait());

    AbstractClient *keyboardClient = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(keyboardClient);
    QVERIFY(keyboardClient->isInputMethod());

    // Do the actual resize
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(client->frameGeometry().height(), 1024 - keyboardClient->inputGeometry().height() + 1);

    // Hide the keyboard
    textInput->hideInputPanel();

    QVERIFY(surfaceConfigureRequestedSpy.wait());
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(client->frameGeometry().height(), 1024);

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void InputMethodTest::testEnableDisableV3()
{
    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));

    Test::TextInputV3 *textInputV3 = new Test::TextInputV3();
    textInputV3->init(Test::waylandTextInputManagerV3()->get_text_input(*(Test::waylandSeat())));
    textInputV3->enable();

    QSignalSpy inputMethodActiveSpy(InputMethod::self(), &InputMethod::activeChanged);
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!InputMethod::self()->isActive());
    textInputV3->commit();
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(InputMethod::self()->isActive());

    // disable text input and ensure that it is not hiding input panel without commit
    inputMethodActiveSpy.clear();
    QVERIFY(InputMethod::self()->isActive());
    textInputV3->disable();
    textInputV3->commit();
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(!InputMethod::self()->isActive());
}

void InputMethodTest::testEnableActive()
{
    QVERIFY(!InputMethod::self()->isActive());

    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QSignalSpy clientRemovedSpy(workspace(), &Workspace::clientRemoved);

    QSignalSpy activateSpy(InputMethod::self(), &InputMethod::activeChanged);

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    QScopedPointer<TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 1);

    // Show the keyboard
    textInput->showInputPanel();
    QVERIFY(clientAddedSpy.wait());

    QCOMPARE(workspace()->activeClient(), client);

    activateSpy.clear();
    textInput->enable(surface.get());
    textInput->showInputPanel();
    activateSpy.wait(200);
    QVERIFY(activateSpy.isEmpty());
    QVERIFY(InputMethod::self()->isActive());
    auto keyboardClient = Test::inputPanelClient();
    QVERIFY(keyboardClient);
    textInput->enable(surface.get());

    QVERIFY(InputMethod::self()->isActive());

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void InputMethodTest::testHidePanel()
{
    QVERIFY(!InputMethod::self()->isActive());

    touchNow();
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QSignalSpy clientRemovedSpy(workspace(), &Workspace::clientRemoved);
    QVERIFY(clientAddedSpy.isValid());

    QSignalSpy activateSpy(InputMethod::self(), &InputMethod::activeChanged);
    QScopedPointer<TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::red);
    waylandServer()->seat()->setFocusedTextInputSurface(client->surface());

    textInput->enable(surface.get());
    textInput->showInputPanel();
    QVERIFY(clientAddedSpy.wait());

    QCOMPARE(workspace()->activeClient(), client);

    QCOMPARE(clientAddedSpy.count(), 2);
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(InputMethod::self()->isActive());

    auto keyboardClient = Test::inputPanelClient();
    auto ipsurface = Test::inputPanelSurface();
    QVERIFY(keyboardClient);
    clientRemovedSpy.clear();
    delete ipsurface;
    QVERIFY(InputMethod::self()->isVisible());
    QVERIFY(clientRemovedSpy.count() || clientRemovedSpy.wait());
    QVERIFY(!InputMethod::self()->isVisible());

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));

}

void InputMethodTest::testSwitchFocusedSurfaces()
{
    touchNow();
    QVERIFY(!InputMethod::self()->isActive());

    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QSignalSpy clientRemovedSpy(workspace(), &Workspace::clientRemoved);
    QVERIFY(clientAddedSpy.isValid());

    QSignalSpy activateSpy(InputMethod::self(), &InputMethod::activeChanged);
    QScopedPointer<TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    QVector<AbstractClient *> clients;
    QVector<KWayland::Client::Surface *> surfaces;
    QVector<Test::XdgToplevel *> toplevels;
    // We create 3 surfaces
    for (int i = 0; i < 3; ++i) {
        auto surface = Test::createSurface();
        auto shellSurface = Test::createXdgToplevelSurface(surface);
        clients += Test::renderAndWaitForShown(surface, QSize(1280, 1024), Qt::red);
        QCOMPARE(workspace()->activeClient(), clients.constLast());
        surfaces += surface;
        toplevels += shellSurface;
    }
    QCOMPARE(clientAddedSpy.count(), 3);
    waylandServer()->seat()->setFocusedTextInputSurface(clients.constFirst()->surface());

    QVERIFY(!InputMethod::self()->isActive());
    textInput->enable(surfaces.last());
    QVERIFY(!InputMethod::self()->isActive());
    waylandServer()->seat()->setFocusedTextInputSurface(clients.first()->surface());
    QVERIFY(!InputMethod::self()->isActive());
    activateSpy.clear();
    waylandServer()->seat()->setFocusedTextInputSurface(clients.last()->surface());
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(InputMethod::self()->isActive());

    activateSpy.clear();
    waylandServer()->seat()->setFocusedTextInputSurface(clients.first()->surface());
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(!InputMethod::self()->isActive());

    // Destroy the test client.
    for (int i = 0; i < clients.count(); ++i) {
        delete toplevels[i];
        QVERIFY(Test::waitForWindowDestroyed(clients[i]));
    }
}

void InputMethodTest::testV3Styling()
{
    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));

    Test::TextInputV3 *textInputV3 = new Test::TextInputV3();
    textInputV3->init(Test::waylandTextInputManagerV3()->get_text_input(*(Test::waylandSeat())));
    textInputV3->enable();

    QSignalSpy inputMethodActiveSpy(InputMethod::self(), &InputMethod::activeChanged);
    QSignalSpy inputMethodActivateSpy(Test::inputMethod(), &Test::MockInputMethod::activate);
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!InputMethod::self()->isActive());
    textInputV3->commit();
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(InputMethod::self()->isActive());
    QVERIFY(inputMethodActivateSpy.wait());
    auto context = Test::inputMethod()->context();
    QSignalSpy textInputPreeditSpy(textInputV3, &Test::TextInputV3::preeditString);
    zwp_input_method_context_v1_preedit_cursor(context, 0);
    zwp_input_method_context_v1_preedit_styling(context, 0, 3, 7);
    zwp_input_method_context_v1_preedit_string(context, 0, "ABCD", "ABCD");
    QVERIFY(textInputPreeditSpy.wait());
    QCOMPARE(textInputPreeditSpy.last().at(0), QString("ABCD"));
    QCOMPARE(textInputPreeditSpy.last().at(1), 0);
    QCOMPARE(textInputPreeditSpy.last().at(2), 0);

    zwp_input_method_context_v1_preedit_cursor(context, 1);
    zwp_input_method_context_v1_preedit_styling(context, 0, 3, 7);
    zwp_input_method_context_v1_preedit_string(context, 0, "ABCDE", "ABCDE");
    QVERIFY(textInputPreeditSpy.wait());
    QCOMPARE(textInputPreeditSpy.last().at(0), QString("ABCDE"));
    QCOMPARE(textInputPreeditSpy.last().at(1), 1);
    QCOMPARE(textInputPreeditSpy.last().at(2), 1);

    zwp_input_method_context_v1_preedit_cursor(context, 2);
    // Use selection for [2, 2+2)
    zwp_input_method_context_v1_preedit_styling(context, 2, 2, 6);
    // Use high light for [3, 3+3)
    zwp_input_method_context_v1_preedit_styling(context, 3, 3, 4);
    zwp_input_method_context_v1_preedit_string(context, 0, "ABCDEF", "ABCDEF");
    QVERIFY(textInputPreeditSpy.wait());
    QCOMPARE(textInputPreeditSpy.last().at(0), QString("ABCDEF"));
    // Merged range should be [2, 6)
    QCOMPARE(textInputPreeditSpy.last().at(1), 2);
    QCOMPARE(textInputPreeditSpy.last().at(2), 6);

    zwp_input_method_context_v1_preedit_cursor(context, 2);
    // Use selection for [0, 0+2)
    zwp_input_method_context_v1_preedit_styling(context, 0, 2, 6);
    // Use high light for [3, 3+3)
    zwp_input_method_context_v1_preedit_styling(context, 3, 3, 4);
    zwp_input_method_context_v1_preedit_string(context, 0, "ABCDEF", "ABCDEF");
    QVERIFY(textInputPreeditSpy.wait());
    QCOMPARE(textInputPreeditSpy.last().at(0), QString("ABCDEF"));
    // Merged range should be none, because of the disjunction highlight.
    QCOMPARE(textInputPreeditSpy.last().at(1), 2);
    QCOMPARE(textInputPreeditSpy.last().at(2), 2);

    zwp_input_method_context_v1_preedit_cursor(context, 1);
    // Use selection for [0, 0+2)
    zwp_input_method_context_v1_preedit_styling(context, 0, 2, 6);
    // Use high light for [2, 2+3)
    zwp_input_method_context_v1_preedit_styling(context, 2, 3, 4);
    zwp_input_method_context_v1_preedit_string(context, 0, "ABCDEF", "ABCDEF");
    QVERIFY(textInputPreeditSpy.wait());
    QCOMPARE(textInputPreeditSpy.last().at(0), QString("ABCDEF"));
    // Merged range should be none, starting offset does not match.
    QCOMPARE(textInputPreeditSpy.last().at(1), 1);
    QCOMPARE(textInputPreeditSpy.last().at(2), 1);

    // Use different order of styling and cursor
    // Use high light for [3, 3+3)
    zwp_input_method_context_v1_preedit_styling(context, 3, 3, 4);
    zwp_input_method_context_v1_preedit_cursor(context, 1);
    // Use selection for [1, 1+2)
    zwp_input_method_context_v1_preedit_styling(context, 1, 2, 6);
    zwp_input_method_context_v1_preedit_string(context, 0, "ABCDEF", "ABCDEF");
    QVERIFY(textInputPreeditSpy.wait());
    QCOMPARE(textInputPreeditSpy.last().at(0), QString("ABCDEF"));
    // Merged range should be [1,6).
    QCOMPARE(textInputPreeditSpy.last().at(1), 1);
    QCOMPARE(textInputPreeditSpy.last().at(2), 6);
}

void InputMethodTest::testDisableShowInputPanel()
{
    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));

    QScopedPointer<KWayland::Client::TextInput> textInputV2(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    QSignalSpy inputMethodActiveSpy(InputMethod::self(), &InputMethod::activeChanged);
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!InputMethod::self()->isActive());
    textInputV2->enable(surface.get());
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(InputMethod::self()->isActive());

    // disable text input and ensure that it is not hiding input panel without commit
    inputMethodActiveSpy.clear();
    QVERIFY(InputMethod::self()->isActive());
    textInputV2->disable(surface.get());
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(!InputMethod::self()->isActive());

    QSignalSpy requestShowInputPanelSpy(waylandServer()->seat()->textInputV2(), &KWaylandServer::TextInputV2Interface::requestShowInputPanel);
    textInputV2->showInputPanel();
    QVERIFY(requestShowInputPanelSpy.count() || requestShowInputPanelSpy.wait());
    QVERIFY(!InputMethod::self()->isActive());
}

void InputMethodTest::testModifierForwarding()
{
    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));

    Test::TextInputV3 *textInputV3 = new Test::TextInputV3();
    textInputV3->init(Test::waylandTextInputManagerV3()->get_text_input(*(Test::waylandSeat())));
    textInputV3->enable();

    QSignalSpy inputMethodActiveSpy(InputMethod::self(), &InputMethod::activeChanged);
    QSignalSpy inputMethodActivateSpy(Test::inputMethod(), &Test::MockInputMethod::activate);
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!InputMethod::self()->isActive());
    textInputV3->commit();
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(InputMethod::self()->isActive());
    QVERIFY(inputMethodActivateSpy.wait());
    auto context = Test::inputMethod()->context();
    QScopedPointer<KWayland::Client::Keyboard> keyboardGrab(new KWayland::Client::Keyboard);
    keyboardGrab->setup(zwp_input_method_context_v1_grab_keyboard(context));
    QSignalSpy modifierSpy(keyboardGrab.get(), &Keyboard::modifiersChanged);
    // Wait for initial modifiers update
    QVERIFY(modifierSpy.wait());

    quint32 timestamp = 1;

    QSignalSpy keySpy(keyboardGrab.get(), &Keyboard::keyChanged);
    bool keyChanged = false;
    bool modifiersChanged = false;
    // We want to verify the order of two signals, so SignalSpy is not very useful here.
    auto keyChangedConnection = connect(keyboardGrab.get(), &Keyboard::keyChanged, [&keyChanged, &modifiersChanged]() {
        QVERIFY(!modifiersChanged);
        keyChanged = true;
    });
    auto modifiersChangedConnection = connect(keyboardGrab.get(), &Keyboard::modifiersChanged, [&keyChanged, &modifiersChanged]() {
        QVERIFY(keyChanged);
        modifiersChanged = true;
    });
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    QVERIFY(keySpy.count() == 1 || keySpy.wait());
    QVERIFY(modifierSpy.count() == 2 || modifierSpy.wait());
    disconnect(keyChangedConnection);
    disconnect(modifiersChangedConnection);

    kwinApp()->platform()->keyboardKeyPressed(KEY_A, timestamp++);
    QVERIFY(keySpy.count() == 2 || keySpy.wait());
    QVERIFY(modifierSpy.count() == 2 || modifierSpy.wait());

    // verify the order of key and modifiers again. Key first, then modifiers.
    keyChanged = false;
    modifiersChanged = false;
    keyChangedConnection = connect(keyboardGrab.get(), &Keyboard::keyChanged, [&keyChanged, &modifiersChanged]() {
        QVERIFY(!modifiersChanged);
        keyChanged = true;
    });
    modifiersChangedConnection = connect(keyboardGrab.get(), &Keyboard::modifiersChanged, [&keyChanged, &modifiersChanged]() {
        QVERIFY(keyChanged);
        modifiersChanged = true;
    });
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(keySpy.count() == 3 || keySpy.wait());
    QVERIFY(modifierSpy.count() == 3 || modifierSpy.wait());
    disconnect(keyChangedConnection);
    disconnect(modifiersChangedConnection);
}

WAYLANDTEST_MAIN(InputMethodTest)

#include "inputmethod_test.moc"
