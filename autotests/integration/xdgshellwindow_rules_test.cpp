/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "cursor.h"
#include "rules.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

#include <linux/input.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xdgshellwindow_rules-0");

class TestXdgShellWindowRules : public QObject
{
    Q_OBJECT

    enum ClientFlag {
        None = 0,
        ClientShouldBeInactive = 1 << 0, // Window should be inactive. Used on Minimize tests
        ServerSideDecoration = 1 << 1, // Create window with server side decoration. Used on noBorder tests
        ReturnAfterSurfaceConfiguration = 1 << 2, // Do not create the window now, but return after surface configuration.
    };
    Q_DECLARE_FLAGS(ClientFlags, ClientFlag)

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testPositionDontAffect();
    void testPositionApply();
    void testPositionRemember();
    void testPositionForce();
    void testPositionApplyNow();
    void testPositionForceTemporarily();

    void testSizeDontAffect();
    void testSizeApply();
    void testSizeRemember();
    void testSizeForce();
    void testSizeApplyNow();
    void testSizeForceTemporarily();

    void testMaximizeDontAffect();
    void testMaximizeApply();
    void testMaximizeRemember();
    void testMaximizeForce();
    void testMaximizeApplyNow();
    void testMaximizeForceTemporarily();

    void testDesktopsDontAffect();
    void testDesktopsApply();
    void testDesktopsRemember();
    void testDesktopsForce();
    void testDesktopsApplyNow();
    void testDesktopsForceTemporarily();

    void testMinimizeDontAffect();
    void testMinimizeApply();
    void testMinimizeRemember();
    void testMinimizeForce();
    void testMinimizeApplyNow();
    void testMinimizeForceTemporarily();

    void testSkipTaskbarDontAffect();
    void testSkipTaskbarApply();
    void testSkipTaskbarRemember();
    void testSkipTaskbarForce();
    void testSkipTaskbarApplyNow();
    void testSkipTaskbarForceTemporarily();

    void testSkipPagerDontAffect();
    void testSkipPagerApply();
    void testSkipPagerRemember();
    void testSkipPagerForce();
    void testSkipPagerApplyNow();
    void testSkipPagerForceTemporarily();

    void testSkipSwitcherDontAffect();
    void testSkipSwitcherApply();
    void testSkipSwitcherRemember();
    void testSkipSwitcherForce();
    void testSkipSwitcherApplyNow();
    void testSkipSwitcherForceTemporarily();

    void testKeepAboveDontAffect();
    void testKeepAboveApply();
    void testKeepAboveRemember();
    void testKeepAboveForce();
    void testKeepAboveApplyNow();
    void testKeepAboveForceTemporarily();

    void testKeepBelowDontAffect();
    void testKeepBelowApply();
    void testKeepBelowRemember();
    void testKeepBelowForce();
    void testKeepBelowApplyNow();
    void testKeepBelowForceTemporarily();

    void testShortcutDontAffect();
    void testShortcutApply();
    void testShortcutRemember();
    void testShortcutForce();
    void testShortcutApplyNow();
    void testShortcutForceTemporarily();

    void testDesktopFileDontAffect();
    void testDesktopFileApply();
    void testDesktopFileRemember();
    void testDesktopFileForce();
    void testDesktopFileApplyNow();
    void testDesktopFileForceTemporarily();

    void testActiveOpacityDontAffect();
    void testActiveOpacityForce();
    void testActiveOpacityForceTemporarily();

    void testInactiveOpacityDontAffect();
    void testInactiveOpacityForce();
    void testInactiveOpacityForceTemporarily();

    void testNoBorderDontAffect();
    void testNoBorderApply();
    void testNoBorderRemember();
    void testNoBorderForce();
    void testNoBorderApplyNow();
    void testNoBorderForceTemporarily();

    void testScreenDontAffect();
    void testScreenApply();
    void testScreenRemember();
    void testScreenForce();
    void testScreenApplyNow();
    void testScreenForceTemporarily();

    void testMatchAfterNameChange();

private:
    void createTestWindow(ClientFlags flags = None);
    void mapClientToSurface(QSize clientSize, ClientFlags flags = None);
    void destroyTestWindow();

    template<typename T>
    void setWindowRule(const QString &property, const T &value, int policy);

private:
    KSharedConfig::Ptr m_config;

    Window *m_window;
    std::unique_ptr<KWayland::Client::Surface> m_surface;
    std::unique_ptr<Test::XdgToplevel> m_shellSurface;

    std::unique_ptr<QSignalSpy> m_toplevelConfigureRequestedSpy;
    std::unique_ptr<QSignalSpy> m_surfaceConfigureRequestedSpy;
};

void TestXdgShellWindowRules::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));

    m_config = KSharedConfig::openConfig(QStringLiteral("kwinrulesrc"), KConfig::SimpleConfig);
    workspace()->rulebook()->setConfig(m_config);
}

void TestXdgShellWindowRules::init()
{
    VirtualDesktopManager::self()->setCurrent(VirtualDesktopManager::self()->desktops().first());
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgDecorationV1));

    workspace()->setActiveOutput(QPoint(640, 512));
}

void TestXdgShellWindowRules::cleanup()
{
    if (m_shellSurface) {
        destroyTestWindow();
    }

    Test::destroyWaylandConnection();

    // Wipe the window rule config clean.
    for (const QString &group : m_config->groupList()) {
        m_config->deleteGroup(group);
    }
    workspace()->slotReconfigure();

    // Restore virtual desktops to the initial state.
    VirtualDesktopManager::self()->setCount(1);
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
}

void TestXdgShellWindowRules::createTestWindow(ClientFlags flags)
{
    // Apply flags for special windows and rules
    const bool createClient = !(flags & ReturnAfterSurfaceConfiguration);
    const auto decorationMode = (flags & ServerSideDecoration) ? Test::XdgToplevelDecorationV1::mode_server_side
                                                               : Test::XdgToplevelDecorationV1::mode_client_side;
    // Create an xdg surface.
    m_surface = Test::createSurface();
    m_shellSurface.reset(Test::createXdgToplevelSurface(m_surface.get(), Test::CreationSetup::CreateOnly, m_surface.get()));
    Test::XdgToplevelDecorationV1 *decoration = Test::createXdgToplevelDecorationV1(m_shellSurface.get(), m_shellSurface.get());

    // Add signal watchers
    m_toplevelConfigureRequestedSpy.reset(new QSignalSpy(m_shellSurface.get(), &Test::XdgToplevel::configureRequested));
    m_surfaceConfigureRequestedSpy.reset(new QSignalSpy(m_shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));

    m_shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    decoration->set_mode(decorationMode);

    // Wait for the initial configure event
    m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());

    if (createClient) {
        mapClientToSurface(QSize(100, 50), flags);
    }
}

void TestXdgShellWindowRules::mapClientToSurface(QSize clientSize, ClientFlags flags)
{
    const bool clientShouldBeActive = !(flags & ClientShouldBeInactive);

    QVERIFY(m_surface != nullptr);
    QVERIFY(m_shellSurface != nullptr);
    QVERIFY(m_surfaceConfigureRequestedSpy != nullptr);

    // Draw content of the surface.
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());

    // Create the window
    m_window = Test::renderAndWaitForShown(m_surface.get(), clientSize, Qt::blue);
    QVERIFY(m_window);
    QCOMPARE(m_window->isActive(), clientShouldBeActive);
}

void TestXdgShellWindowRules::destroyTestWindow()
{
    m_surfaceConfigureRequestedSpy.reset();
    m_toplevelConfigureRequestedSpy.reset();
    m_shellSurface.reset();
    m_surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(m_window));
}

template<typename T>
void TestXdgShellWindowRules::setWindowRule(const QString &property, const T &value, int policy)
{
    // Initialize RuleBook with the test rule.
    m_config->group("General").writeEntry("count", 1);
    KConfigGroup group = m_config->group("1");

    group.writeEntry(property, value);
    group.writeEntry(QStringLiteral("%1rule").arg(property), policy);

    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();

    workspace()->slotReconfigure();
}

void TestXdgShellWindowRules::testPositionDontAffect()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::DontAffect));

    createTestWindow();

    // The position of the window should not be affected by the rule. The default
    // placement policy will put the window in the top-left corner of the screen.
    QVERIFY(m_window->isMovable());
    QVERIFY(m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(0, 0));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testPositionApply()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::Apply));

    createTestWindow();

    // The window should be moved to the position specified by the rule.
    QVERIFY(m_window->isMovable());
    QVERIFY(m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(42, 42));

    // One should still be able to move the window around.
    QSignalSpy clientStartMoveResizedSpy(m_window, &Window::clientStartUserMovedResized);
    QSignalSpy clientStepUserMovedResizedSpy(m_window, &Window::clientStepUserMovedResized);
    QSignalSpy clientFinishUserMovedResizedSpy(m_window, &Window::clientFinishUserMovedResized);

    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeWindow(), m_window);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_window->keyPressEvent(Qt::Key_Right);
    m_window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(m_window->pos(), QPoint(50, 42));

    m_window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    QCOMPARE(m_window->pos(), QPoint(50, 42));

    // The rule should be applied again if the window appears after it's been closed.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(m_window->isMovable());
    QVERIFY(m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(42, 42));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testPositionRemember()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::Remember));
    createTestWindow();

    // The window should be moved to the position specified by the rule.
    QVERIFY(m_window->isMovable());
    QVERIFY(m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(42, 42));

    // One should still be able to move the window around.
    QSignalSpy clientStartMoveResizedSpy(m_window, &Window::clientStartUserMovedResized);
    QSignalSpy clientStepUserMovedResizedSpy(m_window, &Window::clientStepUserMovedResized);
    QSignalSpy clientFinishUserMovedResizedSpy(m_window, &Window::clientFinishUserMovedResized);

    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeWindow(), m_window);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_window->keyPressEvent(Qt::Key_Right);
    m_window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(m_window->pos(), QPoint(50, 42));

    m_window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    QCOMPARE(m_window->pos(), QPoint(50, 42));

    // The window should be placed at the last know position if we reopen it.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(m_window->isMovable());
    QVERIFY(m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(50, 42));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testPositionForce()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::Force));

    createTestWindow();

    // The window should be moved to the position specified by the rule.
    QVERIFY(!m_window->isMovable());
    QVERIFY(!m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(42, 42));

    // User should not be able to move the window.
    QSignalSpy clientStartMoveResizedSpy(m_window, &Window::clientStartUserMovedResized);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());

    // The position should still be forced if we reopen the window.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(!m_window->isMovable());
    QVERIFY(!m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(42, 42));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testPositionApplyNow()
{
    createTestWindow();

    // The position of the window isn't set by any rule, thus the default placement
    // policy will try to put the window in the top-left corner of the screen.
    QVERIFY(m_window->isMovable());
    QVERIFY(m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(0, 0));

    QSignalSpy frameGeometryChangedSpy(m_window, &Window::frameGeometryChanged);

    setWindowRule("position", QPoint(42, 42), int(Rules::ApplyNow));

    // The window should be moved to the position specified by the rule.
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(m_window->pos(), QPoint(42, 42));

    // We still have to be able to move the window around.
    QVERIFY(m_window->isMovable());
    QVERIFY(m_window->isMovableAcrossScreens());
    QSignalSpy clientStartMoveResizedSpy(m_window, &Window::clientStartUserMovedResized);
    QSignalSpy clientStepUserMovedResizedSpy(m_window, &Window::clientStepUserMovedResized);
    QSignalSpy clientFinishUserMovedResizedSpy(m_window, &Window::clientFinishUserMovedResized);

    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeWindow(), m_window);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_window->keyPressEvent(Qt::Key_Right);
    m_window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(m_window->pos(), QPoint(50, 42));

    m_window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    QCOMPARE(m_window->pos(), QPoint(50, 42));

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QCOMPARE(m_window->pos(), QPoint(50, 42));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testPositionForceTemporarily()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::ForceTemporarily));

    createTestWindow();

    // The window should be moved to the position specified by the rule.
    QVERIFY(!m_window->isMovable());
    QVERIFY(!m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(42, 42));

    // User should not be able to move the window.
    QSignalSpy clientStartMoveResizedSpy(m_window, &Window::clientStartUserMovedResized);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());

    // The rule should be discarded if we close the window.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(m_window->isMovable());
    QVERIFY(m_window->isMovableAcrossScreens());
    QCOMPARE(m_window->pos(), QPoint(0, 0));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSizeDontAffect()
{
    setWindowRule("size", QSize(480, 640), int(Rules::DontAffect));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The window size shouldn't be enforced by the rule.
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(0, 0));

    // Map the window.
    mapClientToSurface(QSize(100, 50));
    QVERIFY(m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(100, 50));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSizeApply()
{
    setWindowRule("size", QSize(480, 640), int(Rules::Apply));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The initial configure event should contain size hint set by the rule.
    Test::XdgToplevel::States states;
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(480, 640));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // Map the window.
    mapClientToSurface(QSize(480, 640));
    QVERIFY(m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(480, 640));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // One still should be able to resize the window.
    QSignalSpy frameGeometryChangedSpy(m_window, &Window::frameGeometryChanged);
    QSignalSpy clientStartMoveResizedSpy(m_window, &Window::clientStartUserMovedResized);
    QSignalSpy clientStepUserMovedResizedSpy(m_window, &Window::clientStepUserMovedResized);
    QSignalSpy clientFinishUserMovedResizedSpy(m_window, &Window::clientFinishUserMovedResized);

    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), m_window);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(m_window->isInteractiveResize());
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_window->keyPressEvent(Qt::Key_Right);
    m_window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 4);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 4);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.get(), QSize(488, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_window->size(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    m_window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 5);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 5);

    // The rule should be applied again if the window appears after it's been closed.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    mapClientToSurface(QSize(480, 640));
    QVERIFY(m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(480, 640));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSizeRemember()
{
    setWindowRule("size", QSize(480, 640), int(Rules::Remember));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The initial configure event should contain size hint set by the rule.
    Test::XdgToplevel::States states;
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // Map the window.
    mapClientToSurface(QSize(480, 640));
    QVERIFY(m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(480, 640));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // One should still be able to resize the window.
    QSignalSpy frameGeometryChangedSpy(m_window, &Window::frameGeometryChanged);
    QSignalSpy clientStartMoveResizedSpy(m_window, &Window::clientStartUserMovedResized);
    QSignalSpy clientStepUserMovedResizedSpy(m_window, &Window::clientStepUserMovedResized);
    QSignalSpy clientFinishUserMovedResizedSpy(m_window, &Window::clientFinishUserMovedResized);

    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), m_window);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(m_window->isInteractiveResize());
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_window->keyPressEvent(Qt::Key_Right);
    m_window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 4);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 4);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.get(), QSize(488, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_window->size(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    m_window->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 5);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 5);

    // If the window appears again, it should have the last known size.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(488, 640));

    mapClientToSurface(QSize(488, 640));
    QVERIFY(m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(488, 640));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSizeForce()
{
    setWindowRule("size", QSize(480, 640), int(Rules::Force));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The initial configure event should contain size hint set by the rule.
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Map the window.
    mapClientToSurface(QSize(480, 640));
    QVERIFY(!m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(480, 640));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    // Any attempt to resize the window should not succeed.
    QSignalSpy clientStartMoveResizedSpy(m_window, &Window::clientStartUserMovedResized);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));

    // If the window appears again, the size should still be forced.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    mapClientToSurface(QSize(480, 640));
    QVERIFY(!m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(480, 640));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSizeApplyNow()
{
    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The expected surface dimensions should be set by the rule.
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(0, 0));

    // Map the window.
    mapClientToSurface(QSize(100, 50));
    QVERIFY(m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(100, 50));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    setWindowRule("size", QSize(480, 640), int(Rules::ApplyNow));

    // The compositor should send a configure event with a new size.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Draw the surface with the new size.
    QSignalSpy frameGeometryChangedSpy(m_window, &Window::frameGeometryChanged);
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.get(), QSize(480, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_window->size(), QSize(480, 640));
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSizeForceTemporarily()
{
    setWindowRule("size", QSize(480, 640), int(Rules::ForceTemporarily));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The initial configure event should contain size hint set by the rule.
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Map the window.
    mapClientToSurface(QSize(480, 640));
    QVERIFY(!m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(480, 640));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    // Any attempt to resize the window should not succeed.
    QSignalSpy clientStartMoveResizedSpy(m_window, &Window::clientStartUserMovedResized);
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeWindow(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!m_window->isInteractiveMove());
    QVERIFY(!m_window->isInteractiveResize());
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(0, 0));

    mapClientToSurface(QSize(100, 50));
    QVERIFY(m_window->isResizable());
    QCOMPARE(m_window->size(), QSize(100, 50));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMaximizeDontAffect()
{
    setWindowRule("maximizehoriz", true, int(Rules::DontAffect));
    setWindowRule("maximizevert", true, int(Rules::DontAffect));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    mapClientToSurface(QSize(100, 50));

    QVERIFY(m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->size(), QSize(100, 50));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMaximizeApply()
{
    setWindowRule("maximizehoriz", true, int(Rules::Apply));
    setWindowRule("maximizevert", true, int(Rules::Apply));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->size(), QSize(1280, 1024));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // One should still be able to change the maximized state of the window.
    workspace()->slotWindowMaximize();
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(m_window, &Window::frameGeometryChanged);
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_window->size(), QSize(100, 50));
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // If we create the window again, it should be initially maximized.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    mapClientToSurface(QSize(1280, 1024));
    QVERIFY(m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->size(), QSize(1280, 1024));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMaximizeRemember()
{
    setWindowRule("maximizehoriz", true, int(Rules::Remember));
    setWindowRule("maximizevert", true, int(Rules::Remember));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->size(), QSize(1280, 1024));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // One should still be able to change the maximized state of the window.
    workspace()->slotWindowMaximize();
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(m_window, &Window::frameGeometryChanged);
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_window->size(), QSize(100, 50));
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // If we create the window again, it should not be maximized (because last time it wasn't).
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    mapClientToSurface(QSize(100, 50));

    QVERIFY(m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->size(), QSize(100, 50));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMaximizeForce()
{
    setWindowRule("maximizehoriz", true, int(Rules::Force));
    setWindowRule("maximizevert", true, int(Rules::Force));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(!m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->size(), QSize(1280, 1024));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Any attempt to change the maximized state should not succeed.
    const QRectF oldGeometry = m_window->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->frameGeometry(), oldGeometry);

    // If we create the window again, the maximized state should still be forced.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(!m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->size(), QSize(1280, 1024));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMaximizeApplyNow()
{
    createTestWindow(ReturnAfterSurfaceConfiguration);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    mapClientToSurface(QSize(100, 50));

    QVERIFY(m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->size(), QSize(100, 50));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    setWindowRule("maximizehoriz", true, int(Rules::ApplyNow));
    setWindowRule("maximizevert", true, int(Rules::ApplyNow));

    // We should receive a configure event with a new surface size.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Draw contents of the maximized client.
    QSignalSpy frameGeometryChangedSpy(m_window, &Window::frameGeometryChanged);
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_window->size(), QSize(1280, 1024));
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);

    // The window still has to be maximizeable.
    QVERIFY(m_window->isMaximizable());

    // Restore the window.
    workspace()->slotWindowMaximize();
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 4);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 4);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(100, 50));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_window->size(), QSize(100, 50));
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // The rule should be discarded after it's been applied.
    const QRectF oldGeometry = m_window->frameGeometry();
    m_window->evaluateWindowRules();
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->frameGeometry(), oldGeometry);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMaximizeForceTemporarily()
{
    setWindowRule("maximizehoriz", true, int(Rules::ForceTemporarily));
    setWindowRule("maximizevert", true, int(Rules::ForceTemporarily));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(!m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->size(), QSize(1280, 1024));

    // We should receive a configure event when the window becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Any attempt to change the maximized state should not succeed.
    const QRectF oldGeometry = m_window->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_window->frameGeometry(), oldGeometry);

    // The rule should be discarded if we close the window.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    mapClientToSurface(QSize(100, 50));

    QVERIFY(m_window->isMaximizable());
    QCOMPARE(m_window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_window->size(), QSize(100, 50));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testDesktopsDontAffect()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktop *vd1 = VirtualDesktopManager::self()->desktops().at(0);
    VirtualDesktop *vd2 = VirtualDesktopManager::self()->desktops().at(1);

    VirtualDesktopManager::self()->setCurrent(vd1);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    setWindowRule("desktops", QStringList{vd2->id()}, int(Rules::DontAffect));

    createTestWindow();

    // The window should appear on the current virtual desktop.
    QCOMPARE(m_window->desktops(), {vd1});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testDesktopsApply()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktop *vd1 = VirtualDesktopManager::self()->desktops().at(0);
    VirtualDesktop *vd2 = VirtualDesktopManager::self()->desktops().at(1);

    VirtualDesktopManager::self()->setCurrent(vd1);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    setWindowRule("desktops", QStringList{vd2->id()}, int(Rules::Apply));

    createTestWindow();

    // The window should appear on the second virtual desktop.
    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    // We still should be able to move the window between desktops.
    m_window->setDesktops({vd1});
    QCOMPARE(m_window->desktops(), {vd1});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    // If we re-open the window, it should appear on the second virtual desktop again.
    destroyTestWindow();
    VirtualDesktopManager::self()->setCurrent(vd1);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);
    createTestWindow();

    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testDesktopsRemember()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktop *vd1 = VirtualDesktopManager::self()->desktops().at(0);
    VirtualDesktop *vd2 = VirtualDesktopManager::self()->desktops().at(1);

    VirtualDesktopManager::self()->setCurrent(vd1);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    setWindowRule("desktops", QStringList{vd2->id()}, int(Rules::Remember));

    createTestWindow();

    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    // Move the window to the first virtual desktop.
    m_window->setDesktops({vd1});
    QCOMPARE(m_window->desktops(), {vd1});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    // If we create the window again, it should appear on the first virtual desktop.
    destroyTestWindow();
    createTestWindow();

    QCOMPARE(m_window->desktops(), {vd1});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testDesktopsForce()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktop *vd1 = VirtualDesktopManager::self()->desktops().at(0);
    VirtualDesktop *vd2 = VirtualDesktopManager::self()->desktops().at(1);

    VirtualDesktopManager::self()->setCurrent(vd1);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    setWindowRule("desktops", QStringList{vd2->id()}, int(Rules::Force));

    createTestWindow();

    // The window should appear on the second virtual desktop.
    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    // Any attempt to move the window to another virtual desktop should fail.
    m_window->setDesktops({vd1});
    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    // If we re-open the window, it should appear on the second virtual desktop again.
    destroyTestWindow();
    VirtualDesktopManager::self()->setCurrent(vd1);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);
    createTestWindow();

    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testDesktopsApplyNow()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktop *vd1 = VirtualDesktopManager::self()->desktops().at(0);
    VirtualDesktop *vd2 = VirtualDesktopManager::self()->desktops().at(1);

    VirtualDesktopManager::self()->setCurrent(vd1);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    createTestWindow();

    QCOMPARE(m_window->desktops(), {vd1});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    setWindowRule("desktops", QStringList{vd2->id()}, int(Rules::ApplyNow));

    // The window should have been moved to the second virtual desktop.
    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    // One should still be able to move the window between desktops.
    m_window->setDesktops({vd1});
    QCOMPARE(m_window->desktops(), {vd1});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QCOMPARE(m_window->desktops(), {vd1});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testDesktopsForceTemporarily()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktop *vd1 = VirtualDesktopManager::self()->desktops().at(0);
    VirtualDesktop *vd2 = VirtualDesktopManager::self()->desktops().at(1);

    VirtualDesktopManager::self()->setCurrent(vd1);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    setWindowRule("desktops", QStringList{vd2->id()}, int(Rules::ForceTemporarily));

    createTestWindow();

    // The window should appear on the second virtual desktop.
    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    // Any attempt to move the window to another virtual desktop should fail.
    m_window->setDesktops({vd1});
    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd2);

    // The rule should be discarded when the window is withdrawn.
    destroyTestWindow();
    VirtualDesktopManager::self()->setCurrent(vd1);
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);
    createTestWindow();

    QCOMPARE(m_window->desktops(), {vd1});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    // One should be able to move the window between desktops.
    m_window->setDesktops({vd2});
    QCOMPARE(m_window->desktops(), {vd2});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    m_window->setDesktops({vd1});
    QCOMPARE(m_window->desktops(), {vd1});
    QCOMPARE(VirtualDesktopManager::self()->currentDesktop(), vd1);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMinimizeDontAffect()
{
    setWindowRule("minimize", true, int(Rules::DontAffect));

    createTestWindow();
    QVERIFY(m_window->isMinimizable());

    // The window should not be minimized.
    QVERIFY(!m_window->isMinimized());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMinimizeApply()
{
    setWindowRule("minimize", true, int(Rules::Apply));

    createTestWindow(ClientShouldBeInactive);
    QVERIFY(m_window->isMinimizable());

    // The window should be minimized.
    QVERIFY(m_window->isMinimized());

    // We should still be able to unminimize the window.
    m_window->unminimize();
    QVERIFY(!m_window->isMinimized());

    // If we re-open the window, it should be minimized back again.
    destroyTestWindow();
    createTestWindow(ClientShouldBeInactive);
    QVERIFY(m_window->isMinimizable());
    QVERIFY(m_window->isMinimized());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMinimizeRemember()
{
    setWindowRule("minimize", false, int(Rules::Remember));

    createTestWindow();
    QVERIFY(m_window->isMinimizable());
    QVERIFY(!m_window->isMinimized());

    // Minimize the window.
    m_window->minimize();
    QVERIFY(m_window->isMinimized());

    // If we open the window again, it should be minimized.
    destroyTestWindow();
    createTestWindow(ClientShouldBeInactive);
    QVERIFY(m_window->isMinimizable());
    QVERIFY(m_window->isMinimized());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMinimizeForce()
{
    setWindowRule("minimize", false, int(Rules::Force));

    createTestWindow();
    QVERIFY(!m_window->isMinimizable());
    QVERIFY(!m_window->isMinimized());

    // Any attempt to minimize the window should fail.
    m_window->minimize();
    QVERIFY(!m_window->isMinimized());

    // If we re-open the window, the minimized state should still be forced.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_window->isMinimizable());
    QVERIFY(!m_window->isMinimized());
    m_window->minimize();
    QVERIFY(!m_window->isMinimized());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMinimizeApplyNow()
{
    createTestWindow();
    QVERIFY(m_window->isMinimizable());
    QVERIFY(!m_window->isMinimized());

    setWindowRule("minimize", true, int(Rules::ApplyNow));

    // The window should be minimized now.
    QVERIFY(m_window->isMinimizable());
    QVERIFY(m_window->isMinimized());

    // One is still able to unminimize the window.
    m_window->unminimize();
    QVERIFY(!m_window->isMinimized());

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QVERIFY(m_window->isMinimizable());
    QVERIFY(!m_window->isMinimized());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMinimizeForceTemporarily()
{
    setWindowRule("minimize", false, int(Rules::ForceTemporarily));

    createTestWindow();
    QVERIFY(!m_window->isMinimizable());
    QVERIFY(!m_window->isMinimized());

    // Any attempt to minimize the window should fail until the window is closed.
    m_window->minimize();
    QVERIFY(!m_window->isMinimized());

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->isMinimizable());
    QVERIFY(!m_window->isMinimized());
    m_window->minimize();
    QVERIFY(m_window->isMinimized());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipTaskbarDontAffect()
{
    setWindowRule("skiptaskbar", true, int(Rules::DontAffect));

    createTestWindow();

    // The window should not be affected by the rule.
    QVERIFY(!m_window->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipTaskbarApply()
{
    setWindowRule("skiptaskbar", true, int(Rules::Apply));

    createTestWindow();

    // The window should not be included on a taskbar.
    QVERIFY(m_window->skipTaskbar());

    // Though one can change that.
    m_window->setOriginalSkipTaskbar(false);
    QVERIFY(!m_window->skipTaskbar());

    // Reopen the window, the rule should be applied again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipTaskbarRemember()
{
    setWindowRule("skiptaskbar", true, int(Rules::Remember));

    createTestWindow();

    // The window should not be included on a taskbar.
    QVERIFY(m_window->skipTaskbar());

    // Change the skip-taskbar state.
    m_window->setOriginalSkipTaskbar(false);
    QVERIFY(!m_window->skipTaskbar());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The window should be included on a taskbar.
    QVERIFY(!m_window->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipTaskbarForce()
{
    setWindowRule("skiptaskbar", true, int(Rules::Force));

    createTestWindow();

    // The window should not be included on a taskbar.
    QVERIFY(m_window->skipTaskbar());

    // Any attempt to change the skip-taskbar state should not succeed.
    m_window->setOriginalSkipTaskbar(false);
    QVERIFY(m_window->skipTaskbar());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The skip-taskbar state should be still forced.
    QVERIFY(m_window->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipTaskbarApplyNow()
{
    createTestWindow();
    QVERIFY(!m_window->skipTaskbar());

    setWindowRule("skiptaskbar", true, int(Rules::ApplyNow));

    // The window should not be on a taskbar now.
    QVERIFY(m_window->skipTaskbar());

    // Also, one change the skip-taskbar state.
    m_window->setOriginalSkipTaskbar(false);
    QVERIFY(!m_window->skipTaskbar());

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QVERIFY(!m_window->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipTaskbarForceTemporarily()
{
    setWindowRule("skiptaskbar", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // The window should not be included on a taskbar.
    QVERIFY(m_window->skipTaskbar());

    // Any attempt to change the skip-taskbar state should not succeed.
    m_window->setOriginalSkipTaskbar(false);
    QVERIFY(m_window->skipTaskbar());

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_window->skipTaskbar());

    // The skip-taskbar state is no longer forced.
    m_window->setOriginalSkipTaskbar(true);
    QVERIFY(m_window->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipPagerDontAffect()
{
    setWindowRule("skippager", true, int(Rules::DontAffect));

    createTestWindow();

    // The window should not be affected by the rule.
    QVERIFY(!m_window->skipPager());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipPagerApply()
{
    setWindowRule("skippager", true, int(Rules::Apply));

    createTestWindow();

    // The window should not be included on a pager.
    QVERIFY(m_window->skipPager());

    // Though one can change that.
    m_window->setSkipPager(false);
    QVERIFY(!m_window->skipPager());

    // Reopen the window, the rule should be applied again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->skipPager());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipPagerRemember()
{
    setWindowRule("skippager", true, int(Rules::Remember));

    createTestWindow();

    // The window should not be included on a pager.
    QVERIFY(m_window->skipPager());

    // Change the skip-pager state.
    m_window->setSkipPager(false);
    QVERIFY(!m_window->skipPager());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The window should be included on a pager.
    QVERIFY(!m_window->skipPager());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipPagerForce()
{
    setWindowRule("skippager", true, int(Rules::Force));

    createTestWindow();

    // The window should not be included on a pager.
    QVERIFY(m_window->skipPager());

    // Any attempt to change the skip-pager state should not succeed.
    m_window->setSkipPager(false);
    QVERIFY(m_window->skipPager());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The skip-pager state should be still forced.
    QVERIFY(m_window->skipPager());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipPagerApplyNow()
{
    createTestWindow();
    QVERIFY(!m_window->skipPager());

    setWindowRule("skippager", true, int(Rules::ApplyNow));

    // The window should not be on a pager now.
    QVERIFY(m_window->skipPager());

    // Also, one change the skip-pager state.
    m_window->setSkipPager(false);
    QVERIFY(!m_window->skipPager());

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QVERIFY(!m_window->skipPager());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipPagerForceTemporarily()
{
    setWindowRule("skippager", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // The window should not be included on a pager.
    QVERIFY(m_window->skipPager());

    // Any attempt to change the skip-pager state should not succeed.
    m_window->setSkipPager(false);
    QVERIFY(m_window->skipPager());

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_window->skipPager());

    // The skip-pager state is no longer forced.
    m_window->setSkipPager(true);
    QVERIFY(m_window->skipPager());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipSwitcherDontAffect()
{
    setWindowRule("skipswitcher", true, int(Rules::DontAffect));

    createTestWindow();

    // The window should not be affected by the rule.
    QVERIFY(!m_window->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipSwitcherApply()
{
    setWindowRule("skipswitcher", true, int(Rules::Apply));

    createTestWindow();

    // The window should be excluded from window switching effects.
    QVERIFY(m_window->skipSwitcher());

    // Though one can change that.
    m_window->setSkipSwitcher(false);
    QVERIFY(!m_window->skipSwitcher());

    // Reopen the window, the rule should be applied again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipSwitcherRemember()
{
    setWindowRule("skipswitcher", true, int(Rules::Remember));

    createTestWindow();

    // The window should be excluded from window switching effects.
    QVERIFY(m_window->skipSwitcher());

    // Change the skip-switcher state.
    m_window->setSkipSwitcher(false);
    QVERIFY(!m_window->skipSwitcher());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The window should be included in window switching effects.
    QVERIFY(!m_window->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipSwitcherForce()
{
    setWindowRule("skipswitcher", true, int(Rules::Force));

    createTestWindow();

    // The window should be excluded from window switching effects.
    QVERIFY(m_window->skipSwitcher());

    // Any attempt to change the skip-switcher state should not succeed.
    m_window->setSkipSwitcher(false);
    QVERIFY(m_window->skipSwitcher());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The skip-switcher state should be still forced.
    QVERIFY(m_window->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipSwitcherApplyNow()
{
    createTestWindow();
    QVERIFY(!m_window->skipSwitcher());

    setWindowRule("skipswitcher", true, int(Rules::ApplyNow));

    // The window should be excluded from window switching effects now.
    QVERIFY(m_window->skipSwitcher());

    // Also, one change the skip-switcher state.
    m_window->setSkipSwitcher(false);
    QVERIFY(!m_window->skipSwitcher());

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QVERIFY(!m_window->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testSkipSwitcherForceTemporarily()
{
    setWindowRule("skipswitcher", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // The window should be excluded from window switching effects.
    QVERIFY(m_window->skipSwitcher());

    // Any attempt to change the skip-switcher state should not succeed.
    m_window->setSkipSwitcher(false);
    QVERIFY(m_window->skipSwitcher());

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_window->skipSwitcher());

    // The skip-switcher state is no longer forced.
    m_window->setSkipSwitcher(true);
    QVERIFY(m_window->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepAboveDontAffect()
{
    setWindowRule("above", true, int(Rules::DontAffect));

    createTestWindow();

    // The keep-above state of the window should not be affected by the rule.
    QVERIFY(!m_window->keepAbove());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepAboveApply()
{
    setWindowRule("above", true, int(Rules::Apply));

    createTestWindow();

    // Initially, the window should be kept above.
    QVERIFY(m_window->keepAbove());

    // One should also be able to alter the keep-above state.
    m_window->setKeepAbove(false);
    QVERIFY(!m_window->keepAbove());

    // If one re-opens the window, it should be kept above back again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->keepAbove());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepAboveRemember()
{
    setWindowRule("above", true, int(Rules::Remember));

    createTestWindow();

    // Initially, the window should be kept above.
    QVERIFY(m_window->keepAbove());

    // Unset the keep-above state.
    m_window->setKeepAbove(false);
    QVERIFY(!m_window->keepAbove());
    destroyTestWindow();

    // Re-open the window, it should not be kept above.
    createTestWindow();
    QVERIFY(!m_window->keepAbove());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepAboveForce()
{
    setWindowRule("above", true, int(Rules::Force));

    createTestWindow();

    // Initially, the window should be kept above.
    QVERIFY(m_window->keepAbove());

    // Any attemt to unset the keep-above should not succeed.
    m_window->setKeepAbove(false);
    QVERIFY(m_window->keepAbove());

    // If we re-open the window, it should still be kept above.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->keepAbove());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepAboveApplyNow()
{
    createTestWindow();
    QVERIFY(!m_window->keepAbove());

    setWindowRule("above", true, int(Rules::ApplyNow));

    // The window should now be kept above other windows.
    QVERIFY(m_window->keepAbove());

    // One is still able to change the keep-above state of the window.
    m_window->setKeepAbove(false);
    QVERIFY(!m_window->keepAbove());

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QVERIFY(!m_window->keepAbove());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepAboveForceTemporarily()
{
    setWindowRule("above", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // Initially, the window should be kept above.
    QVERIFY(m_window->keepAbove());

    // Any attempt to alter the keep-above state should not succeed.
    m_window->setKeepAbove(false);
    QVERIFY(m_window->keepAbove());

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_window->keepAbove());

    // The keep-above state is no longer forced.
    m_window->setKeepAbove(true);
    QVERIFY(m_window->keepAbove());
    m_window->setKeepAbove(false);
    QVERIFY(!m_window->keepAbove());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepBelowDontAffect()
{
    setWindowRule("below", true, int(Rules::DontAffect));

    createTestWindow();

    // The keep-below state of the window should not be affected by the rule.
    QVERIFY(!m_window->keepBelow());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepBelowApply()
{
    setWindowRule("below", true, int(Rules::Apply));

    createTestWindow();

    // Initially, the window should be kept below.
    QVERIFY(m_window->keepBelow());

    // One should also be able to alter the keep-below state.
    m_window->setKeepBelow(false);
    QVERIFY(!m_window->keepBelow());

    // If one re-opens the window, it should be kept above back again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->keepBelow());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepBelowRemember()
{
    setWindowRule("below", true, int(Rules::Remember));

    createTestWindow();

    // Initially, the window should be kept below.
    QVERIFY(m_window->keepBelow());

    // Unset the keep-below state.
    m_window->setKeepBelow(false);
    QVERIFY(!m_window->keepBelow());
    destroyTestWindow();

    // Re-open the window, it should not be kept below.
    createTestWindow();
    QVERIFY(!m_window->keepBelow());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepBelowForce()
{
    setWindowRule("below", true, int(Rules::Force));

    createTestWindow();

    // Initially, the window should be kept below.
    QVERIFY(m_window->keepBelow());

    // Any attemt to unset the keep-below should not succeed.
    m_window->setKeepBelow(false);
    QVERIFY(m_window->keepBelow());

    // If we re-open the window, it should still be kept below.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->keepBelow());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepBelowApplyNow()
{
    createTestWindow();
    QVERIFY(!m_window->keepBelow());

    setWindowRule("below", true, int(Rules::ApplyNow));

    // The window should now be kept below other windows.
    QVERIFY(m_window->keepBelow());

    // One is still able to change the keep-below state of the window.
    m_window->setKeepBelow(false);
    QVERIFY(!m_window->keepBelow());

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QVERIFY(!m_window->keepBelow());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testKeepBelowForceTemporarily()
{
    setWindowRule("below", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // Initially, the window should be kept below.
    QVERIFY(m_window->keepBelow());

    // Any attempt to alter the keep-below state should not succeed.
    m_window->setKeepBelow(false);
    QVERIFY(m_window->keepBelow());

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_window->keepBelow());

    // The keep-below state is no longer forced.
    m_window->setKeepBelow(true);
    QVERIFY(m_window->keepBelow());
    m_window->setKeepBelow(false);
    QVERIFY(!m_window->keepBelow());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testShortcutDontAffect()
{
    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::DontAffect));

    createTestWindow();
    QCOMPARE(m_window->shortcut(), QKeySequence());
    m_window->minimize();
    QVERIFY(m_window->isMinimized());

    // If we press the window shortcut, nothing should happen.
    QSignalSpy clientUnminimizedSpy(m_window, &Window::clientUnminimized);
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(m_window->isMinimized());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testShortcutApply()
{
    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::Apply));

    createTestWindow();

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(m_window, &Window::clientUnminimized);
    quint32 timestamp = 1;
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_window->isMinimized());

    // One can also change the shortcut.
    m_window->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_window->isMinimized());

    // The old shortcut should do nothing.
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(m_window->isMinimized());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The window shortcut should be set back to Ctrl+Alt+1.
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testShortcutRemember()
{
    QSKIP("KWin core doesn't try to save the last used window shortcut");

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::Remember));

    createTestWindow();

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(m_window, &Window::clientUnminimized);
    quint32 timestamp = 1;
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_window->isMinimized());

    // Change the window shortcut to Ctrl+Alt+2.
    m_window->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_window->isMinimized());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The window shortcut should be set to the last known value.
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testShortcutForce()
{
    QSKIP("KWin core can't release forced window shortcuts");

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::Force));

    createTestWindow();

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(m_window, &Window::clientUnminimized);
    quint32 timestamp = 1;
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_window->isMinimized());

    // Any attempt to change the window shortcut should not succeed.
    m_window->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(m_window->isMinimized());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The window shortcut should still be forced.
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testShortcutApplyNow()
{
    createTestWindow();
    QVERIFY(m_window->shortcut().isEmpty());

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::ApplyNow));

    // The window should now have a window shortcut assigned.
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    QSignalSpy clientUnminimizedSpy(m_window, &Window::clientUnminimized);
    quint32 timestamp = 1;
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_window->isMinimized());

    // Assign a different shortcut.
    m_window->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_window->isMinimized());

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));

    destroyTestWindow();
}

void TestXdgShellWindowRules::testShortcutForceTemporarily()
{
    QSKIP("KWin core can't release forced window shortcuts");

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::ForceTemporarily));

    createTestWindow();

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(m_window, &Window::clientUnminimized);
    quint32 timestamp = 1;
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_window->isMinimized());

    // Any attempt to change the window shortcut should not succeed.
    m_window->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_window->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_window->minimize();
    QVERIFY(m_window->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(m_window->isMinimized());

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->shortcut().isEmpty());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testDesktopFileDontAffect()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland windows.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellWindowRules::testDesktopFileApply()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland windows.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellWindowRules::testDesktopFileRemember()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland windows.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellWindowRules::testDesktopFileForce()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland windows.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellWindowRules::testDesktopFileApplyNow()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland windows.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellWindowRules::testDesktopFileForceTemporarily()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland windows.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellWindowRules::testActiveOpacityDontAffect()
{
    setWindowRule("opacityactive", 90, int(Rules::DontAffect));

    createTestWindow();
    QVERIFY(m_window->isActive());

    // The opacity should not be affected by the rule.
    QCOMPARE(m_window->opacity(), 1.0);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testActiveOpacityForce()
{
    setWindowRule("opacityactive", 90, int(Rules::Force));

    createTestWindow();
    QVERIFY(m_window->isActive());
    QCOMPARE(m_window->opacity(), 0.9);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testActiveOpacityForceTemporarily()
{
    setWindowRule("opacityactive", 90, int(Rules::ForceTemporarily));

    createTestWindow();
    QVERIFY(m_window->isActive());
    QCOMPARE(m_window->opacity(), 0.9);

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_window->isActive());
    QCOMPARE(m_window->opacity(), 1.0);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testInactiveOpacityDontAffect()
{
    setWindowRule("opacityinactive", 80, int(Rules::DontAffect));

    createTestWindow();
    QVERIFY(m_window->isActive());

    // Make the window inactive.
    workspace()->setActiveWindow(nullptr);
    QVERIFY(!m_window->isActive());

    // The opacity of the window should not be affected by the rule.
    QCOMPARE(m_window->opacity(), 1.0);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testInactiveOpacityForce()
{
    setWindowRule("opacityinactive", 80, int(Rules::Force));

    createTestWindow();
    QVERIFY(m_window->isActive());
    QCOMPARE(m_window->opacity(), 1.0);

    // Make the window inactive.
    workspace()->setActiveWindow(nullptr);
    QVERIFY(!m_window->isActive());

    // The opacity should be forced by the rule.
    QCOMPARE(m_window->opacity(), 0.8);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testInactiveOpacityForceTemporarily()
{
    setWindowRule("opacityinactive", 80, int(Rules::ForceTemporarily));

    createTestWindow();
    QVERIFY(m_window->isActive());
    QCOMPARE(m_window->opacity(), 1.0);

    // Make the window inactive.
    workspace()->setActiveWindow(nullptr);
    QVERIFY(!m_window->isActive());

    // The opacity should be forced by the rule.
    QCOMPARE(m_window->opacity(), 0.8);

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(m_window->isActive());
    QCOMPARE(m_window->opacity(), 1.0);
    workspace()->setActiveWindow(nullptr);
    QVERIFY(!m_window->isActive());
    QCOMPARE(m_window->opacity(), 1.0);

    destroyTestWindow();
}

void TestXdgShellWindowRules::testNoBorderDontAffect()
{
    setWindowRule("noborder", true, int(Rules::DontAffect));
    createTestWindow(ServerSideDecoration);

    // The window should not be affected by the rule.
    QVERIFY(!m_window->noBorder());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testNoBorderApply()
{
    setWindowRule("noborder", true, int(Rules::Apply));
    createTestWindow(ServerSideDecoration);

    // Initially, the window should not be decorated.
    QVERIFY(m_window->noBorder());
    QVERIFY(!m_window->isDecorated());

    // But you should be able to change "no border" property afterwards.
    QVERIFY(m_window->userCanSetNoBorder());
    m_window->setNoBorder(false);
    QVERIFY(!m_window->noBorder());

    // If one re-opens the window, it should have no border again.
    destroyTestWindow();
    createTestWindow(ServerSideDecoration);
    QVERIFY(m_window->noBorder());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testNoBorderRemember()
{
    setWindowRule("noborder", true, int(Rules::Remember));
    createTestWindow(ServerSideDecoration);

    // Initially, the window should not be decorated.
    QVERIFY(m_window->noBorder());
    QVERIFY(!m_window->isDecorated());

    // Unset the "no border" property.
    QVERIFY(m_window->userCanSetNoBorder());
    m_window->setNoBorder(false);
    QVERIFY(!m_window->noBorder());

    // Re-open the window, it should be decorated.
    destroyTestWindow();
    createTestWindow(ServerSideDecoration);
    QVERIFY(m_window->isDecorated());
    QVERIFY(!m_window->noBorder());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testNoBorderForce()
{
    setWindowRule("noborder", true, int(Rules::Force));
    createTestWindow(ServerSideDecoration);

    // The window should not be decorated.
    QVERIFY(m_window->noBorder());
    QVERIFY(!m_window->isDecorated());

    // And the user should not be able to change the "no border" property.
    m_window->setNoBorder(false);
    QVERIFY(m_window->noBorder());

    // Reopen the window.
    destroyTestWindow();
    createTestWindow(ServerSideDecoration);

    // The "no border" property should be still forced.
    QVERIFY(m_window->noBorder());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testNoBorderApplyNow()
{
    createTestWindow(ServerSideDecoration);
    QVERIFY(!m_window->noBorder());

    // Initialize RuleBook with the test rule.
    setWindowRule("noborder", true, int(Rules::ApplyNow));

    // The "no border" property should be set now.
    QVERIFY(m_window->noBorder());

    // One should be still able to change the "no border" property.
    m_window->setNoBorder(false);
    QVERIFY(!m_window->noBorder());

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QVERIFY(!m_window->noBorder());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testNoBorderForceTemporarily()
{
    setWindowRule("noborder", true, int(Rules::ForceTemporarily));
    createTestWindow(ServerSideDecoration);

    // The "no border" property should be set.
    QVERIFY(m_window->noBorder());

    // And you should not be able to change it.
    m_window->setNoBorder(false);
    QVERIFY(m_window->noBorder());

    // The rule should be discarded when the window is closed.
    destroyTestWindow();
    createTestWindow(ServerSideDecoration);
    QVERIFY(!m_window->noBorder());

    // The "no border" property is no longer forced.
    m_window->setNoBorder(true);
    QVERIFY(m_window->noBorder());
    m_window->setNoBorder(false);
    QVERIFY(!m_window->noBorder());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testScreenDontAffect()
{
    const QList<KWin::Output *> outputs = workspace()->outputs();

    setWindowRule("screen", int(1), int(Rules::DontAffect));

    createTestWindow();

    // The window should not be affected by the rule.
    QCOMPARE(m_window->output()->name(), outputs.at(0)->name());

    // The user can still move the window to another screen.
    workspace()->sendWindowToOutput(m_window, outputs.at(1));
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testScreenApply()
{
    const QList<KWin::Output *> outputs = workspace()->outputs();

    setWindowRule("screen", int(1), int(Rules::Apply));

    createTestWindow();

    // The window should be in the screen specified by the rule.
    QEXPECT_FAIL("", "Applying a screen rule on a new client fails on Wayland", Continue);
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    // The user can move the window to another screen.
    workspace()->sendWindowToOutput(m_window, outputs.at(0));
    QCOMPARE(m_window->output()->name(), outputs.at(0)->name());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testScreenRemember()
{
    const QList<KWin::Output *> outputs = workspace()->outputs();

    setWindowRule("screen", int(1), int(Rules::Remember));

    createTestWindow();

    // Initially, the window should be in the first screen
    QCOMPARE(m_window->output()->name(), outputs.at(0)->name());

    // Move the window to the second screen.
    workspace()->sendWindowToOutput(m_window, outputs.at(1));
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    // Close and reopen the window.
    destroyTestWindow();
    createTestWindow();

    QEXPECT_FAIL("", "Applying a screen rule on a new client fails on Wayland", Continue);
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testScreenForce()
{
    const QList<KWin::Output *> outputs = workspace()->outputs();

    createTestWindow();
    QVERIFY(m_window->isActive());

    setWindowRule("screen", int(1), int(Rules::Force));

    // The window should be forced to the screen specified by the rule.
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    // User should not be able to move the window to another screen.
    workspace()->sendWindowToOutput(m_window, outputs.at(0));
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    // Disable the output where the window is on, so the window is moved the other screen
    OutputConfiguration config;
    auto changeSet = config.changeSet(outputs.at(1));
    changeSet->enabled = false;
    workspace()->applyOutputConfiguration(config);

    QVERIFY(!outputs.at(1)->isEnabled());
    QCOMPARE(m_window->output()->name(), outputs.at(0)->name());

    // Enable the output and check that the window is moved there again
    changeSet->enabled = true;
    workspace()->applyOutputConfiguration(config);

    QVERIFY(outputs.at(1)->isEnabled());
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    // Close and reopen the window.
    destroyTestWindow();
    createTestWindow();

    QEXPECT_FAIL("", "Applying a screen rule on a new client fails on Wayland", Continue);
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testScreenApplyNow()
{
    const QList<KWin::Output *> outputs = workspace()->outputs();

    createTestWindow();

    QCOMPARE(m_window->output()->name(), outputs.at(0)->name());

    // Set the rule so the window should move to the screen specified by the rule.
    setWindowRule("screen", int(1), int(Rules::ApplyNow));
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    // The user can move the window to another screen.
    workspace()->sendWindowToOutput(m_window, outputs.at(0));
    QCOMPARE(m_window->output()->name(), outputs.at(0)->name());

    // The rule should not be applied again.
    m_window->evaluateWindowRules();
    QCOMPARE(m_window->output()->name(), outputs.at(0)->name());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testScreenForceTemporarily()
{
    const QList<KWin::Output *> outputs = workspace()->outputs();

    createTestWindow();

    setWindowRule("screen", int(1), int(Rules::ForceTemporarily));

    // The window should be forced the second screen
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    // User is not allowed to move it
    workspace()->sendWindowToOutput(m_window, outputs.at(0));
    QCOMPARE(m_window->output()->name(), outputs.at(1)->name());

    // Close and reopen the window.
    destroyTestWindow();
    createTestWindow();

    // The rule should be discarded now
    QCOMPARE(m_window->output()->name(), outputs.at(0)->name());

    destroyTestWindow();
}

void TestXdgShellWindowRules::testMatchAfterNameChange()
{
    setWindowRule("above", true, int(Rules::Force));

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->keepAbove(), false);

    QSignalSpy desktopFileNameSpy(window, &Window::desktopFileNameChanged);

    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    QVERIFY(desktopFileNameSpy.wait());
    QCOMPARE(window->keepAbove(), true);
}

WAYLANDTEST_MAIN(TestXdgShellWindowRules)
#include "xdgshellwindow_rules_test.moc"
