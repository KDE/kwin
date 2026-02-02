/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>
#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_mouseactions-0");

class MouseActionsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMouseActivate();
    void testMouseActivateInteractiveMoveResize();
    void testMouseActivateAndRaise();
    void testMouseActivateRaiseOnReleaseAndPassClick();
    void testMouseActivateRaiseOnReleaseAndPassClickInteractiveMoveResize();
};

void MouseActionsTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(s_socketName));

    kwinApp()->start();
    Test::setOutputConfig({
        Rect(0, 0, 1280, 1024),
    });
}

void MouseActionsTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void MouseActionsTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void MouseActionsTest::testMouseActivate()
{
    // This test verifies that a window will not be raised with the MouseActivate command.

    options->setCommandWindow1(Options::MouseActivate);

    // Create two windows, window1 is covered by window2.
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show(QSize(100, 100)));
    QVERIFY(window1.m_window->isActive());
    window1.m_window->move(QPoint(0, 0));

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show(QSize(100, 100)));
    QVERIFY(window2.m_window->isActive());
    window2.m_window->move(QPoint(50, 50));

    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));

    // Click left mouse button over the first window.
    uint32_t time = 0;
    Test::pointerMotion(QPoint(25, 25), time++);
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QVERIFY(window1.m_window->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));

    // Release the button.
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));
}

void MouseActionsTest::testMouseActivateInteractiveMoveResize()
{
    // This test verifies that a window will not be accidentally raised after starting an interactive
    // move resize operation while the MouseActivate command is assigned to LMB.

    options->setCommandWindow1(Options::MouseActivate);

    // Create two windows, window1 is covered by window2.
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show(QSize(100, 100)));
    QVERIFY(window1.m_window->isActive());
    window1.m_window->move(QPoint(0, 0));

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show(QSize(100, 100)));
    QVERIFY(window2.m_window->isActive());
    window2.m_window->move(QPoint(50, 50));

    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));

    // Click left mouse button over the first window.
    uint32_t time = 0;
    Test::pointerMotion(QPoint(25, 25), time++);
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));

    // Start an interactive move operation, window1 should not be raised.
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));

    // Finish the interactive move operation.
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));
}

void MouseActionsTest::testMouseActivateAndRaise()
{
    // This test verifies that the MouseActivateAndRaise command works as expected. That is, the window
    // gets immediately activated and raised on a button press.

    options->setCommandWindow1(Options::MouseActivateAndRaise);

    // Create two windows, window1 is covered by window2.
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show(QSize(100, 100)));
    QVERIFY(window1.m_window->isActive());
    window1.m_window->move(QPoint(0, 0));

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show(QSize(100, 100)));
    QVERIFY(window2.m_window->isActive());
    window2.m_window->move(QPoint(50, 50));

    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));

    // Click left mouse button over the first window.
    uint32_t time = 0;
    Test::pointerMotion(QPoint(25, 25), time++);
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QVERIFY(window1.m_window->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList{window2.m_window, window1.m_window}));

    // Release the button.
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QCOMPARE(workspace()->stackingOrder(), (QList{window2.m_window, window1.m_window}));
}

void MouseActionsTest::testMouseActivateRaiseOnReleaseAndPassClick()
{
    // This test verifies that MouseActivateRaiseOnReleaseAndPassClick works as expected. In other
    // words, the window gets activated imediately on a button press, but raised on the button release.

    options->setCommandWindow1(Options::MouseActivateRaiseOnReleaseAndPassClick);

    // Create two windows on the left screen.
    auto surface1 = Test::createSurface();
    auto shellSurface1 = Test::createXdgToplevelSurface(surface1.get());
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 100), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    auto surface2 = Test::createSurface();
    auto shellSurface2 = Test::createXdgToplevelSurface(surface2.get());
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 100), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(200, 200));
    window2->move(QPoint(300, 300));

    workspace()->activateWindow(window2);
    workspace()->raiseWindow(window2);
    QVERIFY(window2->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList{window1, window2}));

    uint32_t time = 0;

    Test::pointerMotion(QPoint(250, 250), time++);

    // the window should be activated on press, and raised on release
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QVERIFY(window1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList{window1, window2}));
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QVERIFY(window1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList{window2, window1}));

    workspace()->activateWindow(window2);
    workspace()->raiseWindow(window2);

    // the window should not be raised if the cursor's moved to a different window in between
    Test::pointerMotion(QPoint(250, 250), time++);
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QVERIFY(window1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList{window1, window2}));
    Test::pointerMotion(QPoint(350, 350), time++);
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QCOMPARE(workspace()->stackingOrder(), (QList{window1, window2}));

    workspace()->activateWindow(window2);
    workspace()->raiseWindow(window2);

    // also check if the windows overlap: only the topmost one should be raised on button release
    window1->move(QPoint(250, 250));
    Test::pointerMotion(QPoint(275, 275), time++);
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QVERIFY(window1->isActive());
    QCOMPARE(workspace()->stackingOrder(), (QList{window1, window2}));
    Test::pointerMotion(QPoint(325, 325), time++);
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QCOMPARE(workspace()->stackingOrder(), (QList{window1, window2}));
}

void MouseActionsTest::testMouseActivateRaiseOnReleaseAndPassClickInteractiveMoveResize()
{
    // This test verifies that a window will be immediately raised if an interactive move resize
    // operation is started while the left mouse button is being held. The window should be raised
    // when the move resize operation starts not when the left mouse button is released.

    options->setCommandWindow1(Options::MouseActivateRaiseOnReleaseAndPassClick);

    // Create two windows, window1 is covered by window2.
    Test::XdgToplevelWindow window1;
    QVERIFY(window1.show(QSize(100, 100)));
    QVERIFY(window1.m_window->isActive());
    window1.m_window->move(QPoint(0, 0));

    Test::XdgToplevelWindow window2;
    QVERIFY(window2.show(QSize(100, 100)));
    QVERIFY(window2.m_window->isActive());
    window2.m_window->move(QPoint(50, 50));

    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));

    // Click left mouse button over the first window.
    uint32_t time = 0;
    Test::pointerMotion(QPoint(25, 25), time++);
    Test::pointerButtonPressed(BTN_LEFT, time++);
    QCOMPARE(workspace()->stackingOrder(), (QList{window1.m_window, window2.m_window}));

    // Start an interactive move operation, window1 should be raised now.
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->stackingOrder(), (QList{window2.m_window, window1.m_window}));

    // Finish the interactive move operation.
    Test::pointerButtonReleased(BTN_LEFT, time++);
    QCOMPARE(workspace()->stackingOrder(), (QList{window2.m_window, window1.m_window}));
}
}

WAYLANDTEST_MAIN(KWin::MouseActionsTest)
#include "mouseactions_test.moc"
