/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "deleted.h"
#include "effects.h"
#include "inputmethod.h"
#include "keyboard_input.h"
#include "qwayland-input-method-unstable-v1.h"
#include "qwayland-text-input-unstable-v3.h"
#include "virtualkeyboard_dbus.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "xkb.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QSignalSpy>
#include <QTest>

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/textinput.h>
#include <linux/input-event-codes.h>

using namespace KWin;
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
    void testV2V3SameClient();
    void testV3Styling();
    void testDisableShowInputPanel();
    void testModifierForwarding();
    void testFakeEventFallback();

private:
    void touchNow()
    {
        static int time = 0;
        Test::touchDown(0, {100, 100}, ++time);
        Test::touchUp(0, ++time);
    }
};

void InputMethodTest::initTestCase()
{
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.kwin.testvirtualkeyboard"));

    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    static_cast<WaylandTestApplication *>(kwinApp())->setInputMethodServerToStart("internal");
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void InputMethodTest::init()
{
    touchNow();
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::TextInputManagerV2 | Test::AdditionalWaylandInterface::InputMethodV1 | Test::AdditionalWaylandInterface::TextInputManagerV3));

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));

    kwinApp()->inputMethod()->setEnabled(true);
}

void InputMethodTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void InputMethodTest::testOpenClose()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QSignalSpy windowRemovedSpy(workspace(), &Workspace::windowRemoved);

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->frameGeometry().size(), QSize(1280, 1024));
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    std::unique_ptr<KWayland::Client::TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));
    QVERIFY(textInput != nullptr);

    // Show the keyboard
    touchNow();
    textInput->enable(surface.get());
    textInput->showInputPanel();
    QSignalSpy paneladded(kwinApp()->inputMethod(), &KWin::InputMethod::panelChanged);
    QVERIFY(windowAddedSpy.wait());
    QCOMPARE(paneladded.count(), 1);

    Window *keyboardClient = windowAddedSpy.last().first().value<Window *>();
    QVERIFY(keyboardClient);
    QVERIFY(keyboardClient->isInputMethod());

    // Do the actual resize
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(window->frameGeometry().height(), 1024 - keyboardClient->inputGeometry().height());

    // Hide the keyboard
    textInput->hideInputPanel();

    QVERIFY(surfaceConfigureRequestedSpy.wait());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().first().value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(window->frameGeometry().height(), 1024);

    // show the keyboard again
    touchNow();
    textInput->enable(surface.get());
    textInput->showInputPanel();

    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QVERIFY(keyboardClient->isShown());

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void InputMethodTest::testEnableDisableV3()
{
    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->frameGeometry().size(), QSize(1280, 1024));

    auto textInputV3 = std::make_unique<Test::TextInputV3>();
    textInputV3->init(Test::waylandTextInputManagerV3()->get_text_input(*(Test::waylandSeat())));

    // Show the keyboard
    touchNow();
    textInputV3->enable();

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QSignalSpy inputMethodActiveSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!kwinApp()->inputMethod()->isActive());
    textInputV3->commit();
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    QVERIFY(windowAddedSpy.wait());
    Window *keyboardClient = windowAddedSpy.last().first().value<Window *>();
    QVERIFY(keyboardClient);
    QVERIFY(keyboardClient->isInputMethod());
    QVERIFY(keyboardClient->isShown());

    // Text input v3 doesn't have hideInputPanel, just simiulate the hide from dbus call
    kwinApp()->inputMethod()->hide();
    QVERIFY(!keyboardClient->isShown());

    QSignalSpy windowShownSpy(keyboardClient, &Window::windowShown);
    // Force enable the text input object. This is what's done by Gtk.
    textInputV3->enable();
    textInputV3->commit();

    windowShownSpy.wait();
    QVERIFY(keyboardClient->isShown());

    // disable text input and ensure that it is not hiding input panel without commit
    inputMethodActiveSpy.clear();
    QVERIFY(kwinApp()->inputMethod()->isActive());
    textInputV3->disable();
    textInputV3->commit();
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(!kwinApp()->inputMethod()->isActive());
}

void InputMethodTest::testEnableActive()
{
    // This test verifies that enabling text-input twice won't change the active input method status.
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());

    // Show the keyboard
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));
    textInput->enable(surface.get());
    QSignalSpy paneladded(kwinApp()->inputMethod(), &KWin::InputMethod::panelChanged);
    QVERIFY(paneladded.wait());
    textInput->showInputPanel();
    QVERIFY(windowAddedSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    // Ask the keyboard to be shown again.
    QSignalSpy activateSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    textInput->enable(surface.get());
    textInput->showInputPanel();
    activateSpy.wait(200);
    QVERIFY(activateSpy.isEmpty());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void InputMethodTest::testHidePanel()
{
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    touchNow();
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QSignalSpy windowRemovedSpy(workspace(), &Workspace::windowRemoved);

    QSignalSpy activateSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    std::unique_ptr<KWayland::Client::TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    waylandServer()->seat()->setFocusedTextInputSurface(window->surface());

    textInput->enable(surface.get());
    QSignalSpy paneladded(kwinApp()->inputMethod(), &KWin::InputMethod::panelChanged);
    QVERIFY(paneladded.wait());
    textInput->showInputPanel();
    QVERIFY(windowAddedSpy.wait());

    QCOMPARE(workspace()->activeWindow(), window);

    QCOMPARE(windowAddedSpy.count(), 2);
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    auto keyboardWindow = kwinApp()->inputMethod()->panel();
    auto ipsurface = Test::inputPanelSurface();
    QVERIFY(keyboardWindow);
    windowRemovedSpy.clear();
    delete ipsurface;
    QVERIFY(kwinApp()->inputMethod()->isVisible());
    QVERIFY(windowRemovedSpy.count() || windowRemovedSpy.wait());
    QVERIFY(!kwinApp()->inputMethod()->isVisible());

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void InputMethodTest::testSwitchFocusedSurfaces()
{
    touchNow();
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QSignalSpy windowRemovedSpy(workspace(), &Workspace::windowRemoved);

    QSignalSpy activateSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    std::unique_ptr<KWayland::Client::TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    QVector<Window *> windows;
    std::vector<std::unique_ptr<KWayland::Client::Surface>> surfaces;
    QVector<Test::XdgToplevel *> toplevels;
    // We create 3 surfaces
    for (int i = 0; i < 3; ++i) {
        std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
        auto shellSurface = Test::createXdgToplevelSurface(surface.get());
        windows += Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
        QCOMPARE(workspace()->activeWindow(), windows.constLast());
        surfaces.push_back(std::move(surface));
        toplevels += shellSurface;
    }
    QCOMPARE(windowAddedSpy.count(), 3);
    waylandServer()->seat()->setFocusedTextInputSurface(windows.constFirst()->surface());

    QVERIFY(!kwinApp()->inputMethod()->isActive());
    textInput->enable(surfaces.back().get());
    QVERIFY(!kwinApp()->inputMethod()->isActive());
    waylandServer()->seat()->setFocusedTextInputSurface(windows.first()->surface());
    QVERIFY(!kwinApp()->inputMethod()->isActive());
    activateSpy.clear();
    waylandServer()->seat()->setFocusedTextInputSurface(windows.last()->surface());
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    activateSpy.clear();
    waylandServer()->seat()->setFocusedTextInputSurface(windows.first()->surface());
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    // Destroy the test window.
    for (int i = 0; i < windows.count(); ++i) {
        delete toplevels[i];
        QVERIFY(Test::waitForWindowDestroyed(windows[i]));
    }
}

void InputMethodTest::testV2V3SameClient()
{
    touchNow();
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QSignalSpy windowRemovedSpy(workspace(), &Workspace::windowRemoved);

    QSignalSpy activateSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    std::unique_ptr<KWayland::Client::TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    auto textInputV3 = std::make_unique<Test::TextInputV3>();
    textInputV3->init(Test::waylandTextInputManagerV3()->get_text_input(*(Test::waylandSeat())));

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> toplevel(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(windowAddedSpy.count(), 1);
    waylandServer()->seat()->setFocusedTextInputSurface(window->surface());
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    // Enable and disable v2
    textInput->enable(surface.get());
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    activateSpy.clear();
    textInput->disable(surface.get());
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    // Enable and disable v3
    activateSpy.clear();
    textInputV3->enable();
    textInputV3->commit();
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    activateSpy.clear();
    textInputV3->disable();
    textInputV3->commit();
    activateSpy.clear();
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    // Enable v2 and v3
    activateSpy.clear();
    textInputV3->enable();
    textInputV3->commit();
    textInput->enable(surface.get());
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    // Disable v3, should still be active since v2 is active.
    activateSpy.clear();
    textInputV3->disable();
    textInputV3->commit();
    activateSpy.wait(200);
    QVERIFY(kwinApp()->inputMethod()->isActive());

    // Disable v2
    activateSpy.clear();
    textInput->disable(surface.get());
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    toplevel.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void InputMethodTest::testV3Styling()
{
    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->frameGeometry().size(), QSize(1280, 1024));

    auto textInputV3 = std::make_unique<Test::TextInputV3>();
    textInputV3->init(Test::waylandTextInputManagerV3()->get_text_input(*(Test::waylandSeat())));
    textInputV3->enable();

    QSignalSpy inputMethodActiveSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    QSignalSpy inputMethodActivateSpy(Test::inputMethod(), &Test::MockInputMethod::activate);
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!kwinApp()->inputMethod()->isActive());
    textInputV3->commit();
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());
    QVERIFY(inputMethodActivateSpy.wait());
    auto context = Test::inputMethod()->context();
    QSignalSpy textInputPreeditSpy(textInputV3.get(), &Test::TextInputV3::preeditString);
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
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->frameGeometry().size(), QSize(1280, 1024));

    std::unique_ptr<KWayland::Client::TextInput> textInputV2(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    QSignalSpy inputMethodActiveSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!kwinApp()->inputMethod()->isActive());
    textInputV2->enable(surface.get());
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    // disable text input and ensure that it is not hiding input panel without commit
    inputMethodActiveSpy.clear();
    QVERIFY(kwinApp()->inputMethod()->isActive());
    textInputV2->disable(surface.get());
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    QSignalSpy requestShowInputPanelSpy(waylandServer()->seat()->textInputV2(), &KWaylandServer::TextInputV2Interface::requestShowInputPanel);
    textInputV2->showInputPanel();
    QVERIFY(requestShowInputPanelSpy.count() || requestShowInputPanelSpy.wait());
    QVERIFY(!kwinApp()->inputMethod()->isActive());
}

void InputMethodTest::testModifierForwarding()
{
    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->frameGeometry().size(), QSize(1280, 1024));

    auto textInputV3 = std::make_unique<Test::TextInputV3>();
    textInputV3->init(Test::waylandTextInputManagerV3()->get_text_input(*(Test::waylandSeat())));
    textInputV3->enable();

    QSignalSpy inputMethodActiveSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    QSignalSpy inputMethodActivateSpy(Test::inputMethod(), &Test::MockInputMethod::activate);
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!kwinApp()->inputMethod()->isActive());
    textInputV3->commit();
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());
    QVERIFY(inputMethodActivateSpy.wait());
    auto context = Test::inputMethod()->context();
    std::unique_ptr<KWayland::Client::Keyboard> keyboardGrab(new KWayland::Client::Keyboard);
    keyboardGrab->setup(zwp_input_method_context_v1_grab_keyboard(context));
    QSignalSpy modifierSpy(keyboardGrab.get(), &KWayland::Client::Keyboard::modifiersChanged);
    // Wait for initial modifiers update
    QVERIFY(modifierSpy.wait());

    quint32 timestamp = 1;

    QSignalSpy keySpy(keyboardGrab.get(), &KWayland::Client::Keyboard::keyChanged);
    bool keyChanged = false;
    bool modifiersChanged = false;
    // We want to verify the order of two signals, so SignalSpy is not very useful here.
    auto keyChangedConnection = connect(keyboardGrab.get(), &KWayland::Client::Keyboard::keyChanged, [&keyChanged, &modifiersChanged]() {
        QVERIFY(!modifiersChanged);
        keyChanged = true;
    });
    auto modifiersChangedConnection = connect(keyboardGrab.get(), &KWayland::Client::Keyboard::modifiersChanged, [&keyChanged, &modifiersChanged]() {
        QVERIFY(keyChanged);
        modifiersChanged = true;
    });
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    QVERIFY(keySpy.count() == 1 || keySpy.wait());
    QVERIFY(modifierSpy.count() == 2 || modifierSpy.wait());
    disconnect(keyChangedConnection);
    disconnect(modifiersChangedConnection);

    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QVERIFY(keySpy.count() == 2 || keySpy.wait());
    QVERIFY(modifierSpy.count() == 2 || modifierSpy.wait());

    // verify the order of key and modifiers again. Key first, then modifiers.
    keyChanged = false;
    modifiersChanged = false;
    keyChangedConnection = connect(keyboardGrab.get(), &KWayland::Client::Keyboard::keyChanged, [&keyChanged, &modifiersChanged]() {
        QVERIFY(!modifiersChanged);
        keyChanged = true;
    });
    modifiersChangedConnection = connect(keyboardGrab.get(), &KWayland::Client::Keyboard::modifiersChanged, [&keyChanged, &modifiersChanged]() {
        QVERIFY(keyChanged);
        modifiersChanged = true;
    });
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(keySpy.count() == 3 || keySpy.wait());
    QVERIFY(modifierSpy.count() == 3 || modifierSpy.wait());
    disconnect(keyChangedConnection);
    disconnect(modifiersChangedConnection);
}

void InputMethodTest::testFakeEventFallback()
{
    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->frameGeometry().size(), QSize(1280, 1024));

    // Since we don't have a way to communicate with the client, manually activate
    // the input method.
    QSignalSpy inputMethodActiveSpy(Test::inputMethod(), &Test::MockInputMethod::activate);
    kwinApp()->inputMethod()->setActive(true);
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());

    // Without a way to communicate to the client, we send fake key events. This
    // means the client needs to be able to receive them, so create a keyboard for
    // the client and listen whether it gets the right events.
    auto keyboard = Test::waylandSeat()->createKeyboard(window);
    QSignalSpy keySpy(keyboard, &KWayland::Client::Keyboard::keyChanged);

    auto context = Test::inputMethod()->context();
    QVERIFY(context);

    // First, send a simple one-character string and check to see if that
    // generates a key press followed by a key release on the client side.
    zwp_input_method_context_v1_commit_string(context, 0, "a");

    keySpy.wait();
    QVERIFY(keySpy.count() == 2);

    auto compare = [](const QList<QVariant> &input, quint32 key, KWayland::Client::Keyboard::KeyState state) {
        auto inputKey = input.at(0).toInt();
        auto inputState = input.at(1).value<KWayland::Client::Keyboard::KeyState>();
        QCOMPARE(inputKey, key);
        QCOMPARE(inputState, state);
    };

    compare(keySpy.at(0), KEY_A, KWayland::Client::Keyboard::KeyState::Pressed);
    compare(keySpy.at(1), KEY_A, KWayland::Client::Keyboard::KeyState::Released);

    keySpy.clear();

    // Capital letters are recognised and sent as a combination of Shift + the
    // letter.

    zwp_input_method_context_v1_commit_string(context, 0, "A");

    keySpy.wait();
    QVERIFY(keySpy.count() == 4);

    compare(keySpy.at(0), KEY_LEFTSHIFT, KWayland::Client::Keyboard::KeyState::Pressed);
    compare(keySpy.at(1), KEY_A, KWayland::Client::Keyboard::KeyState::Pressed);
    compare(keySpy.at(2), KEY_A, KWayland::Client::Keyboard::KeyState::Released);
    compare(keySpy.at(3), KEY_LEFTSHIFT, KWayland::Client::Keyboard::KeyState::Released);

    keySpy.clear();

    // Special keys are not sent through commit_string but instead use keysym.
    auto enter = input()->keyboard()->xkb()->toKeysym(KEY_ENTER);
    zwp_input_method_context_v1_keysym(context, 0, 0, enter, uint32_t(KWaylandServer::KeyboardKeyState::Pressed), 0);
    zwp_input_method_context_v1_keysym(context, 0, 1, enter, uint32_t(KWaylandServer::KeyboardKeyState::Released), 0);

    keySpy.wait();
    QVERIFY(keySpy.count() == 2);

    compare(keySpy.at(0), KEY_ENTER, KWayland::Client::Keyboard::KeyState::Pressed);
    compare(keySpy.at(1), KEY_ENTER, KWayland::Client::Keyboard::KeyState::Released);
}

WAYLANDTEST_MAIN(InputMethodTest)

#include "inputmethod_test.moc"
