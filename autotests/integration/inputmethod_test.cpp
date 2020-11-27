/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
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
#include <KWaylandServer/surface_interface.h>

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>
#include <KWayland/Client/textinput.h>
#include <KWayland/Client/seat.h>

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
};


void InputMethodTest::initTestCase()
{
    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::AbstractClient *>();
    qRegisterMetaType<KWayland::Client::Output *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    static_cast<WaylandTestApplication *>(kwinApp())->setInputMethodServerToStart("internal");
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();

}

void InputMethodTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat |
                                         Test::AdditionalWaylandInterface::TextInputManagerV2 |
                                         Test::AdditionalWaylandInterface::InputMethodV1 |
                                         Test::AdditionalWaylandInterface::TextInputManagerV3));


    screens()->setCurrent(0);
    KWin::Cursors::self()->mouse()->setPos(QPoint(1280, 512));

    const QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kwin.testvirtualkeyboard"),
                                                                QStringLiteral("/VirtualKeyboard"),
                                                                QStringLiteral("org.kde.kwin.VirtualKeyboard"),
                                                                "enable");
    QDBusConnection::sessionBus().call(message);
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
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());

    QScopedPointer<TextInput> textInput(Test::waylandTextInputManager()->createTextInput(Test::waylandSeat()));

    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    QVERIFY(configureRequestedSpy.wait());

    // Show the keyboard
    textInput->showInputPanel();
    QVERIFY(clientAddedSpy.wait());

    AbstractClient *keyboardClient = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(keyboardClient);
    QVERIFY(keyboardClient->isInputMethod());

    // Do the actual resize
    QVERIFY(configureRequestedSpy.wait());

    Test::render(surface.data(), configureRequestedSpy.last().first().value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(client->frameGeometry().height(), 1024 - keyboardClient->inputGeometry().height() + 1);

    // Hide the keyboard
    textInput->hideInputPanel();

    QVERIFY(configureRequestedSpy.wait());
    Test::render(surface.data(), configureRequestedSpy.last().first().value<QSize>(), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(client->frameGeometry().height(), 1024);

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void InputMethodTest::testEnableDisableV3()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QSignalSpy clientRemovedSpy(workspace(), &Workspace::clientRemoved);
    QVERIFY(clientAddedSpy.isValid());

    // Create an xdg_toplevel surface and wait for the compositor to catch up.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->frameGeometry().size(), QSize(1280, 1024));

    Test::TextInputV3 *textInputV3 = new Test::TextInputV3();
    textInputV3->init(Test::waylandTextInputManagerV3()->get_text_input(*(Test::waylandSeat())));
    textInputV3->enable();
    // just enabling the text-input should not show it but rather on commit
    QVERIFY(!clientAddedSpy.wait(100));

    textInputV3->commit();

    QVERIFY(clientAddedSpy.wait());
    AbstractClient *keyboardClient = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(keyboardClient);
    QVERIFY(keyboardClient->isInputMethod());

    // disable text input and ensure that it is not hiding input panel without commit
    textInputV3->disable();
    QVERIFY(!clientRemovedSpy.wait(100));
    textInputV3->commit();
    QVERIFY(clientRemovedSpy.wait());
}

WAYLANDTEST_MAIN(InputMethodTest)

#include "inputmethod_test.moc"
