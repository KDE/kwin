/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "abstract_output.h"
#include "cursor.h"
#include "platform.h"
#include "rules.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "waylandoutputconfig.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

#include <linux/input.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xdgshellclient_rules-0");

class TestXdgShellClientRules : public QObject
{
    Q_OBJECT

    enum ClientFlag {
        None = 0,
        ClientShouldBeInactive = 1 << 0, // Client should be inactive. Used on Minimize tests
        ServerSideDecoration = 1 << 1, // Create window with server side decoration. Used on noBorder tests
        ReturnAfterSurfaceConfiguration = 1 << 2, // Do not create the client now, but return after surface configuration.
    };
    Q_DECLARE_FLAGS(ClientFlags, ClientFlag);

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

    void testDesktopDontAffect();
    void testDesktopApply();
    void testDesktopRemember();
    void testDesktopForce();
    void testDesktopApplyNow();
    void testDesktopForceTemporarily();

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

    AbstractClient *m_client;
    QScopedPointer<KWayland::Client::Surface> m_surface;
    QScopedPointer<Test::XdgToplevel> m_shellSurface;

    QScopedPointer<QSignalSpy> m_toplevelConfigureRequestedSpy;
    QScopedPointer<QSignalSpy> m_surfaceConfigureRequestedSpy;
};

void TestXdgShellClientRules::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = kwinApp()->platform()->enabledOutputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    Test::initWaylandWorkspace();

    m_config = KSharedConfig::openConfig(QStringLiteral("kwinrulesrc"), KConfig::SimpleConfig);
    RuleBook::self()->setConfig(m_config);
}

void TestXdgShellClientRules::init()
{
    VirtualDesktopManager::self()->setCurrent(VirtualDesktopManager::self()->desktops().first());
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgDecorationV1));

    workspace()->setActiveOutput(QPoint(640, 512));
}

void TestXdgShellClientRules::cleanup()
{
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

void TestXdgShellClientRules::createTestWindow(ClientFlags flags)
{
    // Apply flags for special windows and rules
    const bool createClient = !(flags & ReturnAfterSurfaceConfiguration);
    const auto decorationMode = (flags & ServerSideDecoration) ? Test::XdgToplevelDecorationV1::mode_server_side
                                                               : Test::XdgToplevelDecorationV1::mode_client_side;
    // Create an xdg surface.
    m_surface.reset(Test::createSurface());
    m_shellSurface.reset(Test::createXdgToplevelSurface(m_surface.data(), Test::CreationSetup::CreateOnly, m_surface.data()));
    Test::XdgToplevelDecorationV1 *decoration = Test::createXdgToplevelDecorationV1(m_shellSurface.data(), m_shellSurface.data());

    // Add signal watchers
    m_toplevelConfigureRequestedSpy.reset(new QSignalSpy(m_shellSurface.data(), &Test::XdgToplevel::configureRequested));
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

void TestXdgShellClientRules::mapClientToSurface(QSize clientSize, ClientFlags flags)
{
    const bool clientShouldBeActive = !(flags & ClientShouldBeInactive);

    QVERIFY(!m_surface.isNull());
    QVERIFY(!m_shellSurface.isNull());
    QVERIFY(!m_surfaceConfigureRequestedSpy.isNull());

    // Draw content of the surface.
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());

    // Create the client
    m_client = Test::renderAndWaitForShown(m_surface.data(), clientSize, Qt::blue);
    QVERIFY(m_client);
    QCOMPARE(m_client->isActive(), clientShouldBeActive);
}

void TestXdgShellClientRules::destroyTestWindow()
{
    m_surfaceConfigureRequestedSpy.reset();
    m_toplevelConfigureRequestedSpy.reset();
    m_shellSurface.reset();
    m_surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(m_client));
}

template<typename T>
void TestXdgShellClientRules::setWindowRule(const QString &property, const T &value, int policy)
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

void TestXdgShellClientRules::testPositionDontAffect()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::DontAffect));

    createTestWindow();

    // The position of the client should not be affected by the rule. The default
    // placement policy will put the client in the top-left corner of the screen.
    QVERIFY(m_client->isMovable());
    QVERIFY(m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(0, 0));

    destroyTestWindow();
}

void TestXdgShellClientRules::testPositionApply()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::Apply));

    createTestWindow();

    // The client should be moved to the position specified by the rule.
    QVERIFY(m_client->isMovable());
    QVERIFY(m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(42, 42));

    // One should still be able to move the client around.
    QSignalSpy clientStartMoveResizedSpy(m_client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(m_client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(m_client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), m_client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_client->keyPressEvent(Qt::Key_Right);
    m_client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(m_client->pos(), QPoint(50, 42));

    m_client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    QCOMPARE(m_client->pos(), QPoint(50, 42));

    // The rule should be applied again if the client appears after it's been closed.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(m_client->isMovable());
    QVERIFY(m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(42, 42));

    destroyTestWindow();
}

void TestXdgShellClientRules::testPositionRemember()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::Remember));
    createTestWindow();

    // The client should be moved to the position specified by the rule.
    QVERIFY(m_client->isMovable());
    QVERIFY(m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(42, 42));

    // One should still be able to move the client around.
    QSignalSpy clientStartMoveResizedSpy(m_client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(m_client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(m_client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), m_client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_client->keyPressEvent(Qt::Key_Right);
    m_client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(m_client->pos(), QPoint(50, 42));

    m_client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    QCOMPARE(m_client->pos(), QPoint(50, 42));

    // The client should be placed at the last know position if we reopen it.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(m_client->isMovable());
    QVERIFY(m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(50, 42));

    destroyTestWindow();
}

void TestXdgShellClientRules::testPositionForce()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::Force));

    createTestWindow();

    // The client should be moved to the position specified by the rule.
    QVERIFY(!m_client->isMovable());
    QVERIFY(!m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(42, 42));

    // User should not be able to move the client.
    QSignalSpy clientStartMoveResizedSpy(m_client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());

    // The position should still be forced if we reopen the client.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(!m_client->isMovable());
    QVERIFY(!m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(42, 42));

    destroyTestWindow();
}

void TestXdgShellClientRules::testPositionApplyNow()
{
    createTestWindow();

    // The position of the client isn't set by any rule, thus the default placement
    // policy will try to put the client in the top-left corner of the screen.
    QVERIFY(m_client->isMovable());
    QVERIFY(m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(0, 0));

    QSignalSpy frameGeometryChangedSpy(m_client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());

    setWindowRule("position", QPoint(42, 42), int(Rules::ApplyNow));

    // The client should be moved to the position specified by the rule.
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(m_client->pos(), QPoint(42, 42));

    // We still have to be able to move the client around.
    QVERIFY(m_client->isMovable());
    QVERIFY(m_client->isMovableAcrossScreens());
    QSignalSpy clientStartMoveResizedSpy(m_client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(m_client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(m_client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), m_client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_client->keyPressEvent(Qt::Key_Right);
    m_client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(m_client->pos(), QPoint(50, 42));

    m_client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    QCOMPARE(m_client->pos(), QPoint(50, 42));

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QCOMPARE(m_client->pos(), QPoint(50, 42));

    destroyTestWindow();
}

void TestXdgShellClientRules::testPositionForceTemporarily()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::ForceTemporarily));

    createTestWindow();

    // The client should be moved to the position specified by the rule.
    QVERIFY(!m_client->isMovable());
    QVERIFY(!m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(42, 42));

    // User should not be able to move the client.
    QSignalSpy clientStartMoveResizedSpy(m_client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());

    // The rule should be discarded if we close the client.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(m_client->isMovable());
    QVERIFY(m_client->isMovableAcrossScreens());
    QCOMPARE(m_client->pos(), QPoint(0, 0));

    destroyTestWindow();
}

void TestXdgShellClientRules::testSizeDontAffect()
{
    setWindowRule("size", QSize(480, 640), int(Rules::DontAffect));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The window size shouldn't be enforced by the rule.
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(0, 0));

    // Map the client.
    mapClientToSurface(QSize(100, 50));
    QVERIFY(m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellClientRules::testSizeApply()
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

    // Map the client.
    mapClientToSurface(QSize(480, 640));
    QVERIFY(m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // One still should be able to resize the client.
    QSignalSpy frameGeometryChangedSpy(m_client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy clientStartMoveResizedSpy(m_client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(m_client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(m_client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), m_client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(m_client->isInteractiveResize());
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_client->keyPressEvent(Qt::Key_Right);
    m_client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
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
    Test::render(m_surface.data(), QSize(488, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_client->size(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    m_client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 5);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 5);

    // The rule should be applied again if the client appears after it's been closed.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    mapClientToSurface(QSize(480, 640));
    QVERIFY(m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(480, 640));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellClientRules::testSizeRemember()
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

    // Map the client.
    mapClientToSurface(QSize(480, 640));
    QVERIFY(m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // One should still be able to resize the client.
    QSignalSpy frameGeometryChangedSpy(m_client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy clientStartMoveResizedSpy(m_client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(m_client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(m_client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), m_client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(m_client->isInteractiveResize());
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    m_client->keyPressEvent(Qt::Key_Right);
    m_client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
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
    Test::render(m_surface.data(), QSize(488, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_client->size(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    m_client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 5);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 5);

    // If the client appears again, it should have the last known size.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(488, 640));

    mapClientToSurface(QSize(488, 640));
    QVERIFY(m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(488, 640));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellClientRules::testSizeForce()
{
    setWindowRule("size", QSize(480, 640), int(Rules::Force));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The initial configure event should contain size hint set by the rule.
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Map the client.
    mapClientToSurface(QSize(480, 640));
    QVERIFY(!m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    // Any attempt to resize the client should not succeed.
    QSignalSpy clientStartMoveResizedSpy(m_client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));

    // If the client appears again, the size should still be forced.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    mapClientToSurface(QSize(480, 640));
    QVERIFY(!m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(480, 640));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellClientRules::testSizeApplyNow()
{
    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The expected surface dimensions should be set by the rule.
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(0, 0));

    // Map the client.
    mapClientToSurface(QSize(100, 50));
    QVERIFY(m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
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
    QSignalSpy frameGeometryChangedSpy(m_client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_client->size(), QSize(480, 640));
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));

    destroyTestWindow();
}

void TestXdgShellClientRules::testSizeForceTemporarily()
{
    setWindowRule("size", QSize(480, 640), int(Rules::ForceTemporarily));

    createTestWindow(ReturnAfterSurfaceConfiguration);

    // The initial configure event should contain size hint set by the rule.
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Map the client.
    mapClientToSurface(QSize(480, 640));
    QVERIFY(!m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    // Any attempt to resize the client should not succeed.
    QSignalSpy clientStartMoveResizedSpy(m_client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!m_client->isInteractiveMove());
    QVERIFY(!m_client->isInteractiveResize());
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().first().toSize(), QSize(0, 0));

    mapClientToSurface(QSize(100, 50));
    QVERIFY(m_client->isResizable());
    QCOMPARE(m_client->size(), QSize(100, 50));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);

    destroyTestWindow();
}

void TestXdgShellClientRules::testMaximizeDontAffect()
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

    // Map the client.
    mapClientToSurface(QSize(100, 50));

    QVERIFY(m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellClientRules::testMaximizeApply()
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

    // Map the client.
    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // One should still be able to change the maximized state of the client.
    workspace()->slotWindowMaximize();
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(m_client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_client->size(), QSize(100, 50));
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // If we create the client again, it should be initially maximized.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    mapClientToSurface(QSize(1280, 1024));
    QVERIFY(m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->size(), QSize(1280, 1024));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellClientRules::testMaximizeRemember()
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

    // Map the client.
    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // One should still be able to change the maximized state of the client.
    workspace()->slotWindowMaximize();
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(m_client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_client->size(), QSize(100, 50));
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // If we create the client again, it should not be maximized (because last time it wasn't).
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    mapClientToSurface(QSize(100, 50));

    QVERIFY(m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->size(), QSize(100, 50));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellClientRules::testMaximizeForce()
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

    // Map the client.
    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(!m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Any attempt to change the maximized state should not succeed.
    const QRect oldGeometry = m_client->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->frameGeometry(), oldGeometry);

    // If we create the client again, the maximized state should still be forced.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(!m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->size(), QSize(1280, 1024));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellClientRules::testMaximizeApplyNow()
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

    // Map the client.
    mapClientToSurface(QSize(100, 50));

    QVERIFY(m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
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
    QSignalSpy frameGeometryChangedSpy(m_client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_client->size(), QSize(1280, 1024));
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);

    // The client still has to be maximizeable.
    QVERIFY(m_client->isMaximizable());

    // Restore the client.
    workspace()->slotWindowMaximize();
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 4);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 4);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(100, 50));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    m_shellSurface->xdgSurface()->ack_configure(m_surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(m_surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(m_client->size(), QSize(100, 50));
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // The rule should be discarded after it's been applied.
    const QRect oldGeometry = m_client->frameGeometry();
    m_client->evaluateWindowRules();
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->frameGeometry(), oldGeometry);

    destroyTestWindow();
}

void TestXdgShellClientRules::testMaximizeForceTemporarily()
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

    // Map the client.
    mapClientToSurface(QSize(1280, 1024));

    QVERIFY(!m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Any attempt to change the maximized state should not succeed.
    const QRect oldGeometry = m_client->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(!m_surfaceConfigureRequestedSpy->wait(100));
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(m_client->frameGeometry(), oldGeometry);

    // The rule should be discarded if we close the client.
    destroyTestWindow();
    createTestWindow(ReturnAfterSurfaceConfiguration);

    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(m_toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    mapClientToSurface(QSize(100, 50));

    QVERIFY(m_client->isMaximizable());
    QCOMPARE(m_client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(m_client->size(), QSize(100, 50));

    QVERIFY(m_surfaceConfigureRequestedSpy->wait());
    QCOMPARE(m_surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(m_toplevelConfigureRequestedSpy->count(), 2);
    states = m_toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    destroyTestWindow();
}

void TestXdgShellClientRules::testDesktopDontAffect()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::DontAffect));

    createTestWindow();

    // The client should appear on the current virtual desktop.
    QCOMPARE(m_client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    destroyTestWindow();
}

void TestXdgShellClientRules::testDesktopApply()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::Apply));

    createTestWindow();

    // The client should appear on the second virtual desktop.
    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // We still should be able to move the client between desktops.
    workspace()->sendClientToDesktop(m_client, 1, true);
    QCOMPARE(m_client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // If we re-open the client, it should appear on the second virtual desktop again.
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    destroyTestWindow();
    createTestWindow();
    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    destroyTestWindow();
}

void TestXdgShellClientRules::testDesktopRemember()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::Remember));

    createTestWindow();

    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // Move the client to the first virtual desktop.
    workspace()->sendClientToDesktop(m_client, 1, true);
    QCOMPARE(m_client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // If we create the client again, it should appear on the first virtual desktop.
    destroyTestWindow();
    createTestWindow();

    QCOMPARE(m_client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    destroyTestWindow();
}

void TestXdgShellClientRules::testDesktopForce()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::Force));

    createTestWindow();

    // The client should appear on the second virtual desktop.
    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // Any attempt to move the client to another virtual desktop should fail.
    workspace()->sendClientToDesktop(m_client, 1, true);
    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // If we re-open the client, it should appear on the second virtual desktop again.
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    destroyTestWindow();
    createTestWindow();

    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    destroyTestWindow();
}

void TestXdgShellClientRules::testDesktopApplyNow()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    createTestWindow();
    QCOMPARE(m_client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::ApplyNow));

    // The client should have been moved to the second virtual desktop.
    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // One should still be able to move the client between desktops.
    workspace()->sendClientToDesktop(m_client, 1, true);
    QCOMPARE(m_client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QCOMPARE(m_client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    destroyTestWindow();
}

void TestXdgShellClientRules::testDesktopForceTemporarily()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::ForceTemporarily));

    createTestWindow();

    // The client should appear on the second virtual desktop.
    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // Any attempt to move the client to another virtual desktop should fail.
    workspace()->sendClientToDesktop(m_client, 1, true);
    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // The rule should be discarded when the client is withdrawn.
    destroyTestWindow();
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);
    createTestWindow();
    QCOMPARE(m_client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // One should be able to move the client between desktops.
    workspace()->sendClientToDesktop(m_client, 2, true);
    QCOMPARE(m_client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);
    workspace()->sendClientToDesktop(m_client, 1, true);
    QCOMPARE(m_client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    destroyTestWindow();
}

void TestXdgShellClientRules::testMinimizeDontAffect()
{
    setWindowRule("minimize", true, int(Rules::DontAffect));

    createTestWindow();
    QVERIFY(m_client->isMinimizable());

    // The client should not be minimized.
    QVERIFY(!m_client->isMinimized());

    destroyTestWindow();
}

void TestXdgShellClientRules::testMinimizeApply()
{
    setWindowRule("minimize", true, int(Rules::Apply));

    createTestWindow(ClientShouldBeInactive);
    QVERIFY(m_client->isMinimizable());

    // The client should be minimized.
    QVERIFY(m_client->isMinimized());

    // We should still be able to unminimize the client.
    m_client->unminimize();
    QVERIFY(!m_client->isMinimized());

    // If we re-open the client, it should be minimized back again.
    destroyTestWindow();
    createTestWindow(ClientShouldBeInactive);
    QVERIFY(m_client->isMinimizable());
    QVERIFY(m_client->isMinimized());

    destroyTestWindow();
}

void TestXdgShellClientRules::testMinimizeRemember()
{
    setWindowRule("minimize", false, int(Rules::Remember));

    createTestWindow();
    QVERIFY(m_client->isMinimizable());
    QVERIFY(!m_client->isMinimized());

    // Minimize the client.
    m_client->minimize();
    QVERIFY(m_client->isMinimized());

    // If we open the client again, it should be minimized.
    destroyTestWindow();
    createTestWindow(ClientShouldBeInactive);
    QVERIFY(m_client->isMinimizable());
    QVERIFY(m_client->isMinimized());

    destroyTestWindow();
}

void TestXdgShellClientRules::testMinimizeForce()
{
    setWindowRule("minimize", false, int(Rules::Force));

    createTestWindow();
    QVERIFY(!m_client->isMinimizable());
    QVERIFY(!m_client->isMinimized());

    // Any attempt to minimize the client should fail.
    m_client->minimize();
    QVERIFY(!m_client->isMinimized());

    // If we re-open the client, the minimized state should still be forced.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_client->isMinimizable());
    QVERIFY(!m_client->isMinimized());
    m_client->minimize();
    QVERIFY(!m_client->isMinimized());

    destroyTestWindow();
}

void TestXdgShellClientRules::testMinimizeApplyNow()
{
    createTestWindow();
    QVERIFY(m_client->isMinimizable());
    QVERIFY(!m_client->isMinimized());

    setWindowRule("minimize", true, int(Rules::ApplyNow));

    // The client should be minimized now.
    QVERIFY(m_client->isMinimizable());
    QVERIFY(m_client->isMinimized());

    // One is still able to unminimize the client.
    m_client->unminimize();
    QVERIFY(!m_client->isMinimized());

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QVERIFY(m_client->isMinimizable());
    QVERIFY(!m_client->isMinimized());

    destroyTestWindow();
}

void TestXdgShellClientRules::testMinimizeForceTemporarily()
{
    setWindowRule("minimize", false, int(Rules::ForceTemporarily));

    createTestWindow();
    QVERIFY(!m_client->isMinimizable());
    QVERIFY(!m_client->isMinimized());

    // Any attempt to minimize the client should fail until the client is closed.
    m_client->minimize();
    QVERIFY(!m_client->isMinimized());

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->isMinimizable());
    QVERIFY(!m_client->isMinimized());
    m_client->minimize();
    QVERIFY(m_client->isMinimized());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipTaskbarDontAffect()
{
    setWindowRule("skiptaskbar", true, int(Rules::DontAffect));

    createTestWindow();

    // The client should not be affected by the rule.
    QVERIFY(!m_client->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipTaskbarApply()
{
    setWindowRule("skiptaskbar", true, int(Rules::Apply));

    createTestWindow();

    // The client should not be included on a taskbar.
    QVERIFY(m_client->skipTaskbar());

    // Though one can change that.
    m_client->setOriginalSkipTaskbar(false);
    QVERIFY(!m_client->skipTaskbar());

    // Reopen the client, the rule should be applied again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipTaskbarRemember()
{
    setWindowRule("skiptaskbar", true, int(Rules::Remember));

    createTestWindow();

    // The client should not be included on a taskbar.
    QVERIFY(m_client->skipTaskbar());

    // Change the skip-taskbar state.
    m_client->setOriginalSkipTaskbar(false);
    QVERIFY(!m_client->skipTaskbar());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The client should be included on a taskbar.
    QVERIFY(!m_client->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipTaskbarForce()
{
    setWindowRule("skiptaskbar", true, int(Rules::Force));

    createTestWindow();

    // The client should not be included on a taskbar.
    QVERIFY(m_client->skipTaskbar());

    // Any attempt to change the skip-taskbar state should not succeed.
    m_client->setOriginalSkipTaskbar(false);
    QVERIFY(m_client->skipTaskbar());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The skip-taskbar state should be still forced.
    QVERIFY(m_client->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipTaskbarApplyNow()
{
    createTestWindow();
    QVERIFY(!m_client->skipTaskbar());

    setWindowRule("skiptaskbar", true, int(Rules::ApplyNow));

    // The client should not be on a taskbar now.
    QVERIFY(m_client->skipTaskbar());

    // Also, one change the skip-taskbar state.
    m_client->setOriginalSkipTaskbar(false);
    QVERIFY(!m_client->skipTaskbar());

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QVERIFY(!m_client->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipTaskbarForceTemporarily()
{
    setWindowRule("skiptaskbar", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // The client should not be included on a taskbar.
    QVERIFY(m_client->skipTaskbar());

    // Any attempt to change the skip-taskbar state should not succeed.
    m_client->setOriginalSkipTaskbar(false);
    QVERIFY(m_client->skipTaskbar());

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_client->skipTaskbar());

    // The skip-taskbar state is no longer forced.
    m_client->setOriginalSkipTaskbar(true);
    QVERIFY(m_client->skipTaskbar());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipPagerDontAffect()
{
    setWindowRule("skippager", true, int(Rules::DontAffect));

    createTestWindow();

    // The client should not be affected by the rule.
    QVERIFY(!m_client->skipPager());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipPagerApply()
{
    setWindowRule("skippager", true, int(Rules::Apply));

    createTestWindow();

    // The client should not be included on a pager.
    QVERIFY(m_client->skipPager());

    // Though one can change that.
    m_client->setSkipPager(false);
    QVERIFY(!m_client->skipPager());

    // Reopen the client, the rule should be applied again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->skipPager());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipPagerRemember()
{
    setWindowRule("skippager", true, int(Rules::Remember));

    createTestWindow();

    // The client should not be included on a pager.
    QVERIFY(m_client->skipPager());

    // Change the skip-pager state.
    m_client->setSkipPager(false);
    QVERIFY(!m_client->skipPager());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The client should be included on a pager.
    QVERIFY(!m_client->skipPager());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipPagerForce()
{
    setWindowRule("skippager", true, int(Rules::Force));

    createTestWindow();

    // The client should not be included on a pager.
    QVERIFY(m_client->skipPager());

    // Any attempt to change the skip-pager state should not succeed.
    m_client->setSkipPager(false);
    QVERIFY(m_client->skipPager());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The skip-pager state should be still forced.
    QVERIFY(m_client->skipPager());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipPagerApplyNow()
{
    createTestWindow();
    QVERIFY(!m_client->skipPager());

    setWindowRule("skippager", true, int(Rules::ApplyNow));

    // The client should not be on a pager now.
    QVERIFY(m_client->skipPager());

    // Also, one change the skip-pager state.
    m_client->setSkipPager(false);
    QVERIFY(!m_client->skipPager());

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QVERIFY(!m_client->skipPager());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipPagerForceTemporarily()
{
    setWindowRule("skippager", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // The client should not be included on a pager.
    QVERIFY(m_client->skipPager());

    // Any attempt to change the skip-pager state should not succeed.
    m_client->setSkipPager(false);
    QVERIFY(m_client->skipPager());

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_client->skipPager());

    // The skip-pager state is no longer forced.
    m_client->setSkipPager(true);
    QVERIFY(m_client->skipPager());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipSwitcherDontAffect()
{
    setWindowRule("skipswitcher", true, int(Rules::DontAffect));

    createTestWindow();

    // The client should not be affected by the rule.
    QVERIFY(!m_client->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipSwitcherApply()
{
    setWindowRule("skipswitcher", true, int(Rules::Apply));

    createTestWindow();

    // The client should be excluded from window switching effects.
    QVERIFY(m_client->skipSwitcher());

    // Though one can change that.
    m_client->setSkipSwitcher(false);
    QVERIFY(!m_client->skipSwitcher());

    // Reopen the client, the rule should be applied again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipSwitcherRemember()
{
    setWindowRule("skipswitcher", true, int(Rules::Remember));

    createTestWindow();

    // The client should be excluded from window switching effects.
    QVERIFY(m_client->skipSwitcher());

    // Change the skip-switcher state.
    m_client->setSkipSwitcher(false);
    QVERIFY(!m_client->skipSwitcher());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The client should be included in window switching effects.
    QVERIFY(!m_client->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipSwitcherForce()
{
    setWindowRule("skipswitcher", true, int(Rules::Force));

    createTestWindow();

    // The client should be excluded from window switching effects.
    QVERIFY(m_client->skipSwitcher());

    // Any attempt to change the skip-switcher state should not succeed.
    m_client->setSkipSwitcher(false);
    QVERIFY(m_client->skipSwitcher());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The skip-switcher state should be still forced.
    QVERIFY(m_client->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipSwitcherApplyNow()
{
    createTestWindow();
    QVERIFY(!m_client->skipSwitcher());

    setWindowRule("skipswitcher", true, int(Rules::ApplyNow));

    // The client should be excluded from window switching effects now.
    QVERIFY(m_client->skipSwitcher());

    // Also, one change the skip-switcher state.
    m_client->setSkipSwitcher(false);
    QVERIFY(!m_client->skipSwitcher());

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QVERIFY(!m_client->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellClientRules::testSkipSwitcherForceTemporarily()
{
    setWindowRule("skipswitcher", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // The client should be excluded from window switching effects.
    QVERIFY(m_client->skipSwitcher());

    // Any attempt to change the skip-switcher state should not succeed.
    m_client->setSkipSwitcher(false);
    QVERIFY(m_client->skipSwitcher());

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_client->skipSwitcher());

    // The skip-switcher state is no longer forced.
    m_client->setSkipSwitcher(true);
    QVERIFY(m_client->skipSwitcher());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepAboveDontAffect()
{
    setWindowRule("above", true, int(Rules::DontAffect));

    createTestWindow();

    // The keep-above state of the client should not be affected by the rule.
    QVERIFY(!m_client->keepAbove());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepAboveApply()
{
    setWindowRule("above", true, int(Rules::Apply));

    createTestWindow();

    // Initially, the client should be kept above.
    QVERIFY(m_client->keepAbove());

    // One should also be able to alter the keep-above state.
    m_client->setKeepAbove(false);
    QVERIFY(!m_client->keepAbove());

    // If one re-opens the client, it should be kept above back again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->keepAbove());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepAboveRemember()
{
    setWindowRule("above", true, int(Rules::Remember));

    createTestWindow();

    // Initially, the client should be kept above.
    QVERIFY(m_client->keepAbove());

    // Unset the keep-above state.
    m_client->setKeepAbove(false);
    QVERIFY(!m_client->keepAbove());
    destroyTestWindow();

    // Re-open the client, it should not be kept above.
    createTestWindow();
    QVERIFY(!m_client->keepAbove());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepAboveForce()
{
    setWindowRule("above", true, int(Rules::Force));

    createTestWindow();

    // Initially, the client should be kept above.
    QVERIFY(m_client->keepAbove());

    // Any attemt to unset the keep-above should not succeed.
    m_client->setKeepAbove(false);
    QVERIFY(m_client->keepAbove());

    // If we re-open the client, it should still be kept above.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->keepAbove());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepAboveApplyNow()
{
    createTestWindow();
    QVERIFY(!m_client->keepAbove());

    setWindowRule("above", true, int(Rules::ApplyNow));

    // The client should now be kept above other clients.
    QVERIFY(m_client->keepAbove());

    // One is still able to change the keep-above state of the client.
    m_client->setKeepAbove(false);
    QVERIFY(!m_client->keepAbove());

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QVERIFY(!m_client->keepAbove());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepAboveForceTemporarily()
{
    setWindowRule("above", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // Initially, the client should be kept above.
    QVERIFY(m_client->keepAbove());

    // Any attempt to alter the keep-above state should not succeed.
    m_client->setKeepAbove(false);
    QVERIFY(m_client->keepAbove());

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_client->keepAbove());

    // The keep-above state is no longer forced.
    m_client->setKeepAbove(true);
    QVERIFY(m_client->keepAbove());
    m_client->setKeepAbove(false);
    QVERIFY(!m_client->keepAbove());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepBelowDontAffect()
{
    setWindowRule("below", true, int(Rules::DontAffect));

    createTestWindow();

    // The keep-below state of the client should not be affected by the rule.
    QVERIFY(!m_client->keepBelow());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepBelowApply()
{
    setWindowRule("below", true, int(Rules::Apply));

    createTestWindow();

    // Initially, the client should be kept below.
    QVERIFY(m_client->keepBelow());

    // One should also be able to alter the keep-below state.
    m_client->setKeepBelow(false);
    QVERIFY(!m_client->keepBelow());

    // If one re-opens the client, it should be kept above back again.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->keepBelow());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepBelowRemember()
{
    setWindowRule("below", true, int(Rules::Remember));

    createTestWindow();

    // Initially, the client should be kept below.
    QVERIFY(m_client->keepBelow());

    // Unset the keep-below state.
    m_client->setKeepBelow(false);
    QVERIFY(!m_client->keepBelow());
    destroyTestWindow();

    // Re-open the client, it should not be kept below.
    createTestWindow();
    QVERIFY(!m_client->keepBelow());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepBelowForce()
{
    setWindowRule("below", true, int(Rules::Force));

    createTestWindow();

    // Initially, the client should be kept below.
    QVERIFY(m_client->keepBelow());

    // Any attemt to unset the keep-below should not succeed.
    m_client->setKeepBelow(false);
    QVERIFY(m_client->keepBelow());

    // If we re-open the client, it should still be kept below.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->keepBelow());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepBelowApplyNow()
{
    createTestWindow();
    QVERIFY(!m_client->keepBelow());

    setWindowRule("below", true, int(Rules::ApplyNow));

    // The client should now be kept below other clients.
    QVERIFY(m_client->keepBelow());

    // One is still able to change the keep-below state of the client.
    m_client->setKeepBelow(false);
    QVERIFY(!m_client->keepBelow());

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QVERIFY(!m_client->keepBelow());

    destroyTestWindow();
}

void TestXdgShellClientRules::testKeepBelowForceTemporarily()
{
    setWindowRule("below", true, int(Rules::ForceTemporarily));

    createTestWindow();

    // Initially, the client should be kept below.
    QVERIFY(m_client->keepBelow());

    // Any attempt to alter the keep-below state should not succeed.
    m_client->setKeepBelow(false);
    QVERIFY(m_client->keepBelow());

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(!m_client->keepBelow());

    // The keep-below state is no longer forced.
    m_client->setKeepBelow(true);
    QVERIFY(m_client->keepBelow());
    m_client->setKeepBelow(false);
    QVERIFY(!m_client->keepBelow());

    destroyTestWindow();
}

void TestXdgShellClientRules::testShortcutDontAffect()
{
    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::DontAffect));

    createTestWindow();
    QCOMPARE(m_client->shortcut(), QKeySequence());
    m_client->minimize();
    QVERIFY(m_client->isMinimized());

    // If we press the window shortcut, nothing should happen.
    QSignalSpy clientUnminimizedSpy(m_client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(m_client->isMinimized());

    destroyTestWindow();
}

void TestXdgShellClientRules::testShortcutApply()
{
    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::Apply));

    createTestWindow();

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(m_client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_client->isMinimized());

    // One can also change the shortcut.
    m_client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_client->isMinimized());

    // The old shortcut should do nothing.
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(m_client->isMinimized());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The window shortcut should be set back to Ctrl+Alt+1.
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));

    destroyTestWindow();
}

void TestXdgShellClientRules::testShortcutRemember()
{
    QSKIP("KWin core doesn't try to save the last used window shortcut");

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::Remember));

    createTestWindow();

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(m_client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_client->isMinimized());

    // Change the window shortcut to Ctrl+Alt+2.
    m_client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_client->isMinimized());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The window shortcut should be set to the last known value.
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));

    destroyTestWindow();
}

void TestXdgShellClientRules::testShortcutForce()
{
    QSKIP("KWin core can't release forced window shortcuts");

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::Force));

    createTestWindow();

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(m_client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_client->isMinimized());

    // Any attempt to change the window shortcut should not succeed.
    m_client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(m_client->isMinimized());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The window shortcut should still be forced.
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));

    destroyTestWindow();
}

void TestXdgShellClientRules::testShortcutApplyNow()
{
    createTestWindow();
    QVERIFY(m_client->shortcut().isEmpty());

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::ApplyNow));

    // The client should now have a window shortcut assigned.
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    QSignalSpy clientUnminimizedSpy(m_client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_client->isMinimized());

    // Assign a different shortcut.
    m_client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_client->isMinimized());

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_2}));

    destroyTestWindow();
}

void TestXdgShellClientRules::testShortcutForceTemporarily()
{
    QSKIP("KWin core can't release forced window shortcuts");

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::ForceTemporarily));

    createTestWindow();

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(m_client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_1, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!m_client->isMinimized());

    // Any attempt to change the window shortcut should not succeed.
    m_client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(m_client->shortcut(), (QKeySequence{Qt::CTRL | Qt::ALT | Qt::Key_1}));
    m_client->minimize();
    QVERIFY(m_client->isMinimized());
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_2, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(m_client->isMinimized());

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->shortcut().isEmpty());

    destroyTestWindow();
}

void TestXdgShellClientRules::testDesktopFileDontAffect()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland clients.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellClientRules::testDesktopFileApply()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland clients.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellClientRules::testDesktopFileRemember()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland clients.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellClientRules::testDesktopFileForce()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland clients.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellClientRules::testDesktopFileApplyNow()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland clients.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellClientRules::testDesktopFileForceTemporarily()
{
    // Currently, the desktop file name is derived from the app id. If the app id is
    // changed, then the old rules will be lost. Either setDesktopFileName should
    // be exposed or the desktop file name rule should be removed for wayland clients.
    QSKIP("Needs changes in KWin core to pass");
}

void TestXdgShellClientRules::testActiveOpacityDontAffect()
{
    setWindowRule("opacityactive", 90, int(Rules::DontAffect));

    createTestWindow();
    QVERIFY(m_client->isActive());

    // The opacity should not be affected by the rule.
    QCOMPARE(m_client->opacity(), 1.0);

    destroyTestWindow();
}

void TestXdgShellClientRules::testActiveOpacityForce()
{
    setWindowRule("opacityactive", 90, int(Rules::Force));

    createTestWindow();
    QVERIFY(m_client->isActive());
    QCOMPARE(m_client->opacity(), 0.9);

    destroyTestWindow();
}

void TestXdgShellClientRules::testActiveOpacityForceTemporarily()
{
    setWindowRule("opacityactive", 90, int(Rules::ForceTemporarily));

    createTestWindow();
    QVERIFY(m_client->isActive());
    QCOMPARE(m_client->opacity(), 0.9);

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow();
    QVERIFY(m_client->isActive());
    QCOMPARE(m_client->opacity(), 1.0);

    destroyTestWindow();
}

void TestXdgShellClientRules::testInactiveOpacityDontAffect()
{
    setWindowRule("opacityinactive", 80, int(Rules::DontAffect));

    createTestWindow();
    QVERIFY(m_client->isActive());

    // Make the client inactive.
    workspace()->setActiveClient(nullptr);
    QVERIFY(!m_client->isActive());

    // The opacity of the client should not be affected by the rule.
    QCOMPARE(m_client->opacity(), 1.0);

    destroyTestWindow();
}

void TestXdgShellClientRules::testInactiveOpacityForce()
{
    setWindowRule("opacityinactive", 80, int(Rules::Force));

    createTestWindow();
    QVERIFY(m_client->isActive());
    QCOMPARE(m_client->opacity(), 1.0);

    // Make the client inactive.
    workspace()->setActiveClient(nullptr);
    QVERIFY(!m_client->isActive());

    // The opacity should be forced by the rule.
    QCOMPARE(m_client->opacity(), 0.8);

    destroyTestWindow();
}

void TestXdgShellClientRules::testInactiveOpacityForceTemporarily()
{
    setWindowRule("opacityinactive", 80, int(Rules::ForceTemporarily));

    createTestWindow();
    QVERIFY(m_client->isActive());
    QCOMPARE(m_client->opacity(), 1.0);

    // Make the client inactive.
    workspace()->setActiveClient(nullptr);
    QVERIFY(!m_client->isActive());

    // The opacity should be forced by the rule.
    QCOMPARE(m_client->opacity(), 0.8);

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow();

    QVERIFY(m_client->isActive());
    QCOMPARE(m_client->opacity(), 1.0);
    workspace()->setActiveClient(nullptr);
    QVERIFY(!m_client->isActive());
    QCOMPARE(m_client->opacity(), 1.0);

    destroyTestWindow();
}

void TestXdgShellClientRules::testNoBorderDontAffect()
{
    setWindowRule("noborder", true, int(Rules::DontAffect));
    createTestWindow(ServerSideDecoration);

    // The client should not be affected by the rule.
    QVERIFY(!m_client->noBorder());

    destroyTestWindow();
}

void TestXdgShellClientRules::testNoBorderApply()
{
    setWindowRule("noborder", true, int(Rules::Apply));
    createTestWindow(ServerSideDecoration);

    // Initially, the client should not be decorated.
    QVERIFY(m_client->noBorder());
    QVERIFY(!m_client->isDecorated());

    // But you should be able to change "no border" property afterwards.
    QVERIFY(m_client->userCanSetNoBorder());
    m_client->setNoBorder(false);
    QVERIFY(!m_client->noBorder());

    // If one re-opens the client, it should have no border again.
    destroyTestWindow();
    createTestWindow(ServerSideDecoration);
    QVERIFY(m_client->noBorder());

    destroyTestWindow();
}

void TestXdgShellClientRules::testNoBorderRemember()
{
    setWindowRule("noborder", true, int(Rules::Remember));
    createTestWindow(ServerSideDecoration);

    // Initially, the client should not be decorated.
    QVERIFY(m_client->noBorder());
    QVERIFY(!m_client->isDecorated());

    // Unset the "no border" property.
    QVERIFY(m_client->userCanSetNoBorder());
    m_client->setNoBorder(false);
    QVERIFY(!m_client->noBorder());

    // Re-open the client, it should be decorated.
    destroyTestWindow();
    createTestWindow(ServerSideDecoration);
    QVERIFY(m_client->isDecorated());
    QVERIFY(!m_client->noBorder());

    destroyTestWindow();
}

void TestXdgShellClientRules::testNoBorderForce()
{
    setWindowRule("noborder", true, int(Rules::Force));
    createTestWindow(ServerSideDecoration);

    // The client should not be decorated.
    QVERIFY(m_client->noBorder());
    QVERIFY(!m_client->isDecorated());

    // And the user should not be able to change the "no border" property.
    m_client->setNoBorder(false);
    QVERIFY(m_client->noBorder());

    // Reopen the client.
    destroyTestWindow();
    createTestWindow(ServerSideDecoration);

    // The "no border" property should be still forced.
    QVERIFY(m_client->noBorder());

    destroyTestWindow();
}

void TestXdgShellClientRules::testNoBorderApplyNow()
{
    createTestWindow(ServerSideDecoration);
    QVERIFY(!m_client->noBorder());

    // Initialize RuleBook with the test rule.
    setWindowRule("noborder", true, int(Rules::ApplyNow));

    // The "no border" property should be set now.
    QVERIFY(m_client->noBorder());

    // One should be still able to change the "no border" property.
    m_client->setNoBorder(false);
    QVERIFY(!m_client->noBorder());

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QVERIFY(!m_client->noBorder());

    destroyTestWindow();
}

void TestXdgShellClientRules::testNoBorderForceTemporarily()
{
    setWindowRule("noborder", true, int(Rules::ForceTemporarily));
    createTestWindow(ServerSideDecoration);

    // The "no border" property should be set.
    QVERIFY(m_client->noBorder());

    // And you should not be able to change it.
    m_client->setNoBorder(false);
    QVERIFY(m_client->noBorder());

    // The rule should be discarded when the client is closed.
    destroyTestWindow();
    createTestWindow(ServerSideDecoration);
    QVERIFY(!m_client->noBorder());

    // The "no border" property is no longer forced.
    m_client->setNoBorder(true);
    QVERIFY(m_client->noBorder());
    m_client->setNoBorder(false);
    QVERIFY(!m_client->noBorder());

    destroyTestWindow();
}

void TestXdgShellClientRules::testScreenDontAffect()
{
    const KWin::Outputs outputs = kwinApp()->platform()->enabledOutputs();

    setWindowRule("screen", int(1), int(Rules::DontAffect));

    createTestWindow();

    // The client should not be affected by the rule.
    QCOMPARE(m_client->output()->name(), outputs.at(0)->name());

    // The user can still move the client to another screen.
    workspace()->sendClientToOutput(m_client, outputs.at(1));
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    destroyTestWindow();
}

void TestXdgShellClientRules::testScreenApply()
{
    const KWin::Outputs outputs = kwinApp()->platform()->enabledOutputs();

    setWindowRule("screen", int(1), int(Rules::Apply));

    createTestWindow();

    // The client should be in the screen specified by the rule.
    QEXPECT_FAIL("", "Applying a screen rule on a new client fails on Wayland", Continue);
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    // The user can move the client to another screen.
    workspace()->sendClientToOutput(m_client, outputs.at(0));
    QCOMPARE(m_client->output()->name(), outputs.at(0)->name());

    destroyTestWindow();
}

void TestXdgShellClientRules::testScreenRemember()
{
    const KWin::Outputs outputs = kwinApp()->platform()->enabledOutputs();

    setWindowRule("screen", int(1), int(Rules::Remember));

    createTestWindow();

    // Initially, the client should be in the first screen
    QCOMPARE(m_client->output()->name(), outputs.at(0)->name());

    // Move the client to the second screen.
    workspace()->sendClientToOutput(m_client, outputs.at(1));
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    // Close and reopen the client.
    destroyTestWindow();
    createTestWindow();

    QEXPECT_FAIL("", "Applying a screen rule on a new client fails on Wayland", Continue);
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    destroyTestWindow();
}

void TestXdgShellClientRules::testScreenForce()
{
    const KWin::Outputs outputs = kwinApp()->platform()->enabledOutputs();

    createTestWindow();
    QVERIFY(m_client->isActive());

    setWindowRule("screen", int(1), int(Rules::Force));

    // The client should be forced to the screen specified by the rule.
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    // User should not be able to move the client to another screen.
    workspace()->sendClientToOutput(m_client, outputs.at(0));
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    // Disable the output where the window is on, so the client is moved the other screen
    WaylandOutputConfig config;
    auto changeSet = config.changeSet(static_cast<AbstractWaylandOutput *>(outputs.at(1)));
    changeSet->enabled = false;
    kwinApp()->platform()->applyOutputChanges(config);

    QVERIFY(!outputs.at(1)->isEnabled());
    QCOMPARE(m_client->output()->name(), outputs.at(0)->name());

    // Enable the output and check that the client is moved there again
    changeSet->enabled = true;
    kwinApp()->platform()->applyOutputChanges(config);

    QVERIFY(outputs.at(1)->isEnabled());
    QEXPECT_FAIL("", "Disabling and enabling an output does not move the client to the Forced screen", Continue);
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    // Close and reopen the client.
    destroyTestWindow();
    createTestWindow();

    QEXPECT_FAIL("", "Applying a screen rule on a new client fails on Wayland", Continue);
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    destroyTestWindow();
}

void TestXdgShellClientRules::testScreenApplyNow()
{
    const KWin::Outputs outputs = kwinApp()->platform()->enabledOutputs();

    createTestWindow();

    QCOMPARE(m_client->output()->name(), outputs.at(0)->name());

    // Set the rule so the client should move to the screen specified by the rule.
    setWindowRule("screen", int(1), int(Rules::ApplyNow));
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    // The user can move the client to another screen.
    workspace()->sendClientToOutput(m_client, outputs.at(0));
    QCOMPARE(m_client->output()->name(), outputs.at(0)->name());

    // The rule should not be applied again.
    m_client->evaluateWindowRules();
    QCOMPARE(m_client->output()->name(), outputs.at(0)->name());

    destroyTestWindow();
}

void TestXdgShellClientRules::testScreenForceTemporarily()
{
    const KWin::Outputs outputs = kwinApp()->platform()->enabledOutputs();

    createTestWindow();

    setWindowRule("screen", int(1), int(Rules::ForceTemporarily));

    // The client should be forced the second screen
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    // User is not allowed to move it
    workspace()->sendClientToOutput(m_client, outputs.at(0));
    QCOMPARE(m_client->output()->name(), outputs.at(1)->name());

    // Close and reopen the client.
    destroyTestWindow();
    createTestWindow();

    // The rule should be discarded now
    QCOMPARE(m_client->output()->name(), outputs.at(0)->name());

    destroyTestWindow();
}

void TestXdgShellClientRules::testMatchAfterNameChange()
{
    setWindowRule("above", true, int(Rules::Force));

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QCOMPARE(c->keepAbove(), false);

    QSignalSpy desktopFileNameSpy(c, &AbstractClient::desktopFileNameChanged);
    QVERIFY(desktopFileNameSpy.isValid());

    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    QVERIFY(desktopFileNameSpy.wait());
    QCOMPARE(c->keepAbove(), true);
}

WAYLANDTEST_MAIN(TestXdgShellClientRules)
#include "xdgshellclient_rules_test.moc"
