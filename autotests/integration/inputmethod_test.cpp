/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "inputmethod.h"
#include "inputpanelv1window.h"
#include "keyboard_input.h"
#include "pointer_input.h"
#include "qwayland-input-method-unstable-v1.h"
#include "qwayland-text-input-unstable-v3.h"
#include "virtualkeyboard_dbus.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
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
    void testReactivateFocus();
    void testSwitchFocusedSurfaces();
    void testV2V3SameClient();
    void testV3Styling();
    void testDisableShowInputPanel();
    void testModifierForwarding();
    void testFakeEventFallback();
    void testOverlayPositioning_data();
    void testOverlayPositioning();
    void testV3AutoCommit();

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

    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    static_cast<WaylandTestApplication *>(kwinApp())->setInputMethodServerToStart("internal");
    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void InputMethodTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::input()->pointer()->warp(QPoint(640, 512));

    touchNow();

    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::TextInputManagerV2 | Test::AdditionalWaylandInterface::InputMethodV1 | Test::AdditionalWaylandInterface::TextInputManagerV3));

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

    QCOMPARE(window->frameGeometry().height(), 1024 - keyboardClient->frameGeometry().height());

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
    QVERIFY(Test::waitForWindowClosed(window));
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

    QSignalSpy hiddenChangedSpy(keyboardClient, &Window::hiddenChanged);
    // Force enable the text input object. This is what's done by Gtk.
    textInputV3->enable();
    textInputV3->commit();

    hiddenChangedSpy.wait();
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
    QVERIFY(Test::waitForWindowClosed(window));
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
    QVERIFY(Test::waitForWindowClosed(window));
}

void InputMethodTest::testReactivateFocus()
{
    touchNow();
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->frameGeometry().size(), QSize(1280, 1024));

    // Show the keyboard
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));
    textInput->enable(surface.get());
    QSignalSpy paneladded(kwinApp()->inputMethod(), &KWin::InputMethod::panelChanged);
    QVERIFY(paneladded.wait());
    textInput->showInputPanel();
    QVERIFY(windowAddedSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    QSignalSpy activeSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);

    // Hide keyboard like keyboardToggle button on navigation panel
    kwinApp()->inputMethod()->setActive(false);
    activeSpy.wait(200);
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    // Reactivate
    textInput->enable(surface.get());
    textInput->showInputPanel();
    activeSpy.wait(200);
    QVERIFY(kwinApp()->inputMethod()->isActive());

    // Destroy the test window
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

void InputMethodTest::testSwitchFocusedSurfaces()
{
    touchNow();
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QSignalSpy windowRemovedSpy(workspace(), &Workspace::windowRemoved);

    QSignalSpy activateSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    std::unique_ptr<KWayland::Client::TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    QList<Window *> windows;
    std::vector<std::unique_ptr<KWayland::Client::Surface>> surfaces;
    std::vector<std::unique_ptr<Test::XdgToplevel>> toplevels;
    // We create 3 surfaces
    for (int i = 0; i < 3; ++i) {
        std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
        auto shellSurface = Test::createXdgToplevelSurface(surface.get());
        windows += Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
        QCOMPARE(workspace()->activeWindow(), windows.constLast());
        surfaces.push_back(std::move(surface));
        toplevels.push_back(std::move(shellSurface));
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
    QVERIFY(Test::waitForWindowClosed(window));
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

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QVERIFY(!kwinApp()->inputMethod()->isActive());
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

    QSignalSpy requestShowInputPanelSpy(waylandServer()->seat()->textInputV2(), &TextInputV2Interface::requestShowInputPanel);
    textInputV2->showInputPanel();
    QVERIFY(requestShowInputPanelSpy.count() || requestShowInputPanelSpy.wait());
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
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

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    Test::keyboardKeyReleased(KEY_A, timestamp++);
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
    zwp_input_method_context_v1_keysym(context, 0, 0, enter, uint32_t(WL_KEYBOARD_KEY_STATE_PRESSED), 0);
    zwp_input_method_context_v1_keysym(context, 0, 1, enter, uint32_t(WL_KEYBOARD_KEY_STATE_RELEASED), 0);

    keySpy.wait();
    QVERIFY(keySpy.count() == 2);

    compare(keySpy.at(0), KEY_ENTER, KWayland::Client::Keyboard::KeyState::Pressed);
    compare(keySpy.at(1), KEY_ENTER, KWayland::Client::Keyboard::KeyState::Released);

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    kwinApp()->inputMethod()->setActive(false);
    QVERIFY(!kwinApp()->inputMethod()->isActive());
}

void InputMethodTest::testOverlayPositioning_data()
{
    QTest::addColumn<QRect>("cursorRectangle");
    QTest::addColumn<QRect>("result");

    QTest::newRow("regular") << QRect(10, 20, 30, 40) << QRect(60, 160, 200, 50);
    QTest::newRow("offscreen-left") << QRect(-200, 40, 30, 40) << QRect(0, 180, 200, 50);
    QTest::newRow("offscreen-right") << QRect(1200, 40, 30, 40) << QRect(1080, 180, 200, 50);
    QTest::newRow("offscreen-top") << QRect(1200, -400, 30, 40) << QRect(1080, 0, 200, 50);
    // Check it is flipped near the bottom of screen (anchor point 844 + 100 + 40 = 1024 - 40)
    QTest::newRow("offscreen-bottom-flip") << QRect(1200, 844, 30, 40) << QRect(1080, 894, 200, 50);
    // Top is (screen height 1024 - window height 50) = 984
    QTest::newRow("offscreen-bottom-slide") << QRect(1200, 1200, 30, 40) << QRect(1080, 974, 200, 50);
}

void InputMethodTest::testOverlayPositioning()
{
    QFETCH(QRect, cursorRectangle);
    QFETCH(QRect, result);
    Test::inputMethod()->setMode(Test::MockInputMethod::Mode::Overlay);
    QVERIFY(!kwinApp()->inputMethod()->isActive());

    touchNow();
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    QSignalSpy windowRemovedSpy(workspace(), &Workspace::windowRemoved);

    QSignalSpy activateSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    std::unique_ptr<KWayland::Client::TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    // Make the window smaller than the screen and move it.
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1080, 824), Qt::red);
    window->move(QPointF(50, 100));
    waylandServer()->seat()->setFocusedTextInputSurface(window->surface());

    textInput->setCursorRectangle(cursorRectangle);
    textInput->enable(surface.get());
    // Overlay is shown upon activate
    QVERIFY(windowAddedSpy.wait());

    QCOMPARE(workspace()->activeWindow(), window);

    QCOMPARE(windowAddedSpy.count(), 2);
    QVERIFY(activateSpy.count() || activateSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());

    auto keyboardWindow = kwinApp()->inputMethod()->panel();
    QVERIFY(keyboardWindow);
    // Check the overlay window is placed with cursor rectangle + window position.
    QCOMPARE(keyboardWindow->frameGeometry(), result);

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));

    Test::inputMethod()->setMode(Test::MockInputMethod::Mode::TopLevel);
}

void InputMethodTest::testV3AutoCommit()
{
    Test::inputMethod()->setMode(Test::MockInputMethod::Mode::Overlay);

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 1024), Qt::red);
    QVERIFY(window);
    QVERIFY(window->isActive());

    auto textInputV3 = std::make_unique<Test::TextInputV3>();
    textInputV3->init(Test::waylandTextInputManagerV3()->get_text_input(*(Test::waylandSeat())));
    textInputV3->enable();

    QSignalSpy textInputPreeditSpy(textInputV3.get(), &Test::TextInputV3::preeditString);
    QSignalSpy textInputCommitTextSpy(textInputV3.get(), &Test::TextInputV3::commitString);

    QSignalSpy inputMethodActiveSpy(kwinApp()->inputMethod(), &InputMethod::activeChanged);
    QSignalSpy inputMethodActivateSpy(Test::inputMethod(), &Test::MockInputMethod::activate);
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!kwinApp()->inputMethod()->isActive());
    textInputV3->commit();
    QVERIFY(inputMethodActiveSpy.count() || inputMethodActiveSpy.wait());
    QVERIFY(kwinApp()->inputMethod()->isActive());
    QVERIFY(inputMethodActivateSpy.wait());
    auto context = Test::inputMethod()->context();

    zwp_input_method_context_v1_preedit_string(context, 1, "preedit1", "commit1");
    QVERIFY(textInputPreeditSpy.wait());
    QVERIFY(textInputCommitTextSpy.count() == 0);

    // ******************
    // Non-grabbing key press
    int timestamp = 0;
    Test::keyboardKeyPressed(KEY_A, timestamp++);
    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QVERIFY(textInputCommitTextSpy.wait());
    QCOMPARE(textInputCommitTextSpy.last()[0].toString(), "commit1");
    QCOMPARE(textInputPreeditSpy.last()[0].toString(), QString());

    // **************
    // Mouse clicks
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    textInputV3->disable();
    textInputV3->enable();
    const QList<Window *> windows = workspace()->windows();
    auto it = std::find_if(windows.begin(), windows.end(), [](Window *w) {
        return w->isInputMethod();
    });
    QVERIFY(it != windows.end());
    auto textInputWindow = *it;

    textInputV3->commit();
    zwp_input_method_context_v1_preedit_string(context, 1, "preedit2", "commit2");
    QVERIFY(textInputPreeditSpy.wait());
    QCOMPARE(textInputPreeditSpy.last()[0].toString(), QString("preedit2"));

    // mouse clicks on a VK does not submit
    Test::pointerMotion(textInputWindow->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(1, timestamp++);
    Test::pointerButtonReleased(1, timestamp++);
    QVERIFY(!textInputCommitTextSpy.wait(20));

    // mouse clicks on our main window submits the string
    Test::pointerMotion(window->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(1, timestamp++);
    Test::pointerButtonReleased(1, timestamp++);

    QVERIFY(textInputCommitTextSpy.wait());
    QCOMPARE(textInputCommitTextSpy.last()[0].toString(), "commit2");
    QCOMPARE(textInputPreeditSpy.last()[0].toString(), QString());

    // *****************
    // Change focus
    textInputV3->commit();
    zwp_input_method_context_v1_preedit_string(context, 1, "preedit3", "commit3");
    QVERIFY(textInputPreeditSpy.wait());
    QCOMPARE(textInputPreeditSpy.last()[0].toString(), QString("preedit3"));

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(window2->isActive());

    // these variables refer to the old window
    QVERIFY(textInputCommitTextSpy.wait());
    QCOMPARE(textInputCommitTextSpy.last()[0].toString(), "commit3");
    QCOMPARE(textInputPreeditSpy.last()[0].toString(), QString());

    shellSurface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(window2));
}

WAYLANDTEST_MAIN(InputMethodTest)

#include "inputmethod_test.moc"
