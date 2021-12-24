/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "abstract_output.h"
#include "cursor.h"
#include "platform.h"
#include "rules.h"
#include "screens.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

#include <linux/input.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xdgshellclient_rules-0");

class TestXdgShellClientRules : public QObject
{
    Q_OBJECT

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

    void testMatchAfterNameChange();

private:
    template <typename T> void setWindowRule(const QString &property, const T &value, int policy);

private:
    KSharedConfig::Ptr m_config;
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

std::tuple<AbstractClient *, KWayland::Client::Surface *, Test::XdgToplevel *> createWindow(const QString &appId, Test::XdgToplevelDecorationV1::mode decorationMode = Test::XdgToplevelDecorationV1::mode_client_side)
{
    // Create an xdg surface.
    KWayland::Client::Surface *surface = Test::createSurface();
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface, Test::CreationSetup::CreateOnly, surface);
    Test::XdgToplevelDecorationV1 *decoration = Test::createXdgToplevelDecorationV1(shellSurface, shellSurface);

    shellSurface->set_app_id(appId);
    decoration->set_mode(decorationMode);

    // Wait for the initial configure event.
    QSignalSpy configureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    configureRequestedSpy.wait();

    // Draw content of the surface.
    shellSurface->xdgSurface()->ack_configure(configureRequestedSpy.last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);

    return {client, surface, shellSurface};
}

template <typename T>
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

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());

    // The position of the client should not be affected by the rule. The default
    // placement policy will put the client in the top-left corner of the screen.
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(0, 0));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testPositionApply()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());

    // The client should be moved to the position specified by the rule.
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(42, 42));

    // One should still be able to move the client around.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(client->pos(), QPoint(50, 42));

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    QCOMPARE(client->pos(), QPoint(50, 42));

    // The rule should be applied again if the client appears after it's been closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(42, 42));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testPositionRemember()
{
    // Initialize RuleBook with the test rule.
    setWindowRule("position", QPoint(42, 42), int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());

    // The client should be moved to the position specified by the rule.
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(42, 42));

    // One should still be able to move the client around.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(client->pos(), QPoint(50, 42));

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    QCOMPARE(client->pos(), QPoint(50, 42));

    // The client should be placed at the last know position if we reopen it.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(50, 42));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testPositionForce()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());

    // The client should be moved to the position specified by the rule.
    QVERIFY(!client->isMovable());
    QVERIFY(!client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(42, 42));

    // User should not be able to move the client.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());

    // The position should still be forced if we reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isMovable());
    QVERIFY(!client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(42, 42));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testPositionApplyNow()
{
    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    QObject *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());

    // The position of the client isn't set by any rule, thus the default placement
    // policy will try to put the client in the top-left corner of the screen.
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(0, 0));

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());

    setWindowRule("position", QPoint(42, 42), int(Rules::ApplyNow));

    // The client should be moved to the position specified by the rule.
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(client->pos(), QPoint(42, 42));

    // We still have to be able to move the client around.
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(client->pos(), QPoint(50, 42));

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    QCOMPARE(client->pos(), QPoint(50, 42));

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QCOMPARE(client->pos(), QPoint(50, 42));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testPositionForceTemporarily()
{
    setWindowRule("position", QPoint(42, 42), int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());

    // The client should be moved to the position specified by the rule.
    QVERIFY(!client->isMovable());
    QVERIFY(!client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(42, 42));

    // User should not be able to move the client.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());

    // The rule should be discarded if we close the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(0, 0));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeDontAffect()
{
    setWindowRule("size", QSize(480, 640), int(Rules::DontAffect));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The window size shouldn't be enforced by the rule.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(0, 0));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeApply()
{
    setWindowRule("size", QSize(480, 640), int(Rules::Apply));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The initial configure event should contain size hint set by the rule.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(480, 640));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // One still should be able to resize the client.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(client->isInteractiveResize());
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 3);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 4);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(488, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 5);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 5);

    // The rule should be applied again if the client appears after it's been closed.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeRemember()
{
    setWindowRule("size", QSize(480, 640), int(Rules::Remember));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The initial configure event should contain size hint set by the rule.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // One should still be able to resize the client.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(client->isInteractiveResize());
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 3);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 4);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(488, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 5);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 5);

    // If the client appears again, it should have the last known size.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(488, 640));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(488, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(488, 640));

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeForce()
{
    setWindowRule("size", QSize(480, 640), int(Rules::Force));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The initial configure event should contain size hint set by the rule.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);

    // Any attempt to resize the client should not succeed.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    QVERIFY(!surfaceConfigureRequestedSpy->wait(100));

    // If the client appears again, the size should still be forced.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeApplyNow()
{
    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The expected surface dimensions should be set by the rule.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(0, 0));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);

    setWindowRule("size", QSize(480, 640), int(Rules::ApplyNow));

    // The compositor should send a configure event with a new size.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Draw the surface with the new size.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(480, 640));
    QVERIFY(!surfaceConfigureRequestedSpy->wait(100));

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QVERIFY(!surfaceConfigureRequestedSpy->wait(100));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeForceTemporarily()
{
    setWindowRule("size", QSize(480, 640), int(Rules::ForceTemporarily));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // The initial configure event should contain size hint set by the rule.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);

    // Any attempt to resize the client should not succeed.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!client->isInteractiveMove());
    QVERIFY(!client->isInteractiveResize());
    QVERIFY(!surfaceConfigureRequestedSpy->wait(100));

    // The rule should be discarded when the client is closed.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().first().toSize(), QSize(0, 0));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(100, 50));

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeDontAffect()
{
    setWindowRule("maximizehoriz", true, int(Rules::DontAffect));
    setWindowRule("maximizevert", true, int(Rules::DontAffect));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeApply()
{
    setWindowRule("maximizehoriz", true, int(Rules::Apply));
    setWindowRule("maximizevert", true, int(Rules::Apply));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // One should still be able to change the maximized state of the client.
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(100, 50));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // If we create the client again, it should be initially maximized.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeRemember()
{
    setWindowRule("maximizehoriz", true, int(Rules::Remember));
    setWindowRule("maximizevert", true, int(Rules::Remember));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // One should still be able to change the maximized state of the client.
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(100, 50));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // If we create the client again, it should not be maximized (because last time it wasn't).
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->size(), QSize(100, 50));

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeForce()
{
    setWindowRule("maximizehoriz", true, int(Rules::Force));
    setWindowRule("maximizevert", true, int(Rules::Force));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Any attempt to change the maximized state should not succeed.
    const QRect oldGeometry = client->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(!surfaceConfigureRequestedSpy->wait(100));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->frameGeometry(), oldGeometry);

    // If we create the client again, the maximized state should still be forced.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeApplyNow()
{
    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    setWindowRule("maximizehoriz", true, int(Rules::ApplyNow));
    setWindowRule("maximizevert", true, int(Rules::ApplyNow));

    // We should receive a configure event with a new surface size.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Draw contents of the maximized client.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(1280, 1024));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);

    // The client still has to be maximizeable.
    QVERIFY(client->isMaximizable());

    // Restore the client.
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(100, 50));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(100, 50));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // The rule should be discarded after it's been applied.
    const QRect oldGeometry = client->frameGeometry();
    client->evaluateWindowRules();
    QVERIFY(!surfaceConfigureRequestedSpy->wait(100));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->frameGeometry(), oldGeometry);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeForceTemporarily()
{
    setWindowRule("maximizehoriz", true, int(Rules::ForceTemporarily));
    setWindowRule("maximizevert", true, int(Rules::ForceTemporarily));

    // Create the test client.
    QScopedPointer<KWayland::Client::Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface;
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> toplevelConfigureRequestedSpy;
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    QScopedPointer<QSignalSpy> surfaceConfigureRequestedSpy;
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the client.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    // Any attempt to change the maximized state should not succeed.
    const QRect oldGeometry = client->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(!surfaceConfigureRequestedSpy->wait(100));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->frameGeometry(), oldGeometry);

    // The rule should be discarded if we close the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    toplevelConfigureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested));
    surfaceConfigureRequestedSpy.reset(new QSignalSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested));
    shellSurface->set_app_id(QStringLiteral("org.kde.foo"));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy->last().at(0).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->size(), QSize(100, 50));

    QVERIFY(surfaceConfigureRequestedSpy->wait());
    QCOMPARE(surfaceConfigureRequestedSpy->count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy->count(), 2);
    states = toplevelConfigureRequestedSpy->last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testDesktopDontAffect()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should appear on the current virtual desktop.
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testDesktopApply()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should appear on the second virtual desktop.
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // We still should be able to move the client between desktops.
    workspace()->sendClientToDesktop(client, 1, true);
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // If we re-open the client, it should appear on the second virtual desktop again.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testDesktopRemember()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // Move the client to the first virtual desktop.
    workspace()->sendClientToDesktop(client, 1, true);
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // If we create the client again, it should appear on the first virtual desktop.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testDesktopForce()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should appear on the second virtual desktop.
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // Any attempt to move the client to another virtual desktop should fail.
    workspace()->sendClientToDesktop(client, 1, true);
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // If we re-open the client, it should appear on the second virtual desktop again.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testDesktopApplyNow()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::ApplyNow));

    // The client should have been moved to the second virtual desktop.
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // One should still be able to move the client between desktops.
    workspace()->sendClientToDesktop(client, 1, true);
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testDesktopForceTemporarily()
{
    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    setWindowRule("desktops", QStringList{VirtualDesktopManager::self()->desktopForX11Id(2)->id()}, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should appear on the second virtual desktop.
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // Any attempt to move the client to another virtual desktop should fail.
    workspace()->sendClientToDesktop(client, 1, true);
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2);

    // The rule should be discarded when the client is withdrawn.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // One should be able to move the client between desktops.
    workspace()->sendClientToDesktop(client, 2, true);
    QCOMPARE(client->desktop(), 2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);
    workspace()->sendClientToDesktop(client, 1, true);
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMinimizeDontAffect()
{
    setWindowRule("minimize", true, int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isMinimizable());

    // The client should not be minimized.
    QVERIFY(!client->isMinimized());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMinimizeApply()
{
    setWindowRule("minimize", true, int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isMinimizable());

    // The client should be minimized.
    QVERIFY(client->isMinimized());

    // We should still be able to unminimize the client.
    client->unminimize();
    QVERIFY(!client->isMinimized());

    // If we re-open the client, it should be minimized back again.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isMinimizable());
    QVERIFY(client->isMinimized());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMinimizeRemember()
{
    setWindowRule("minimize", false, int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isMinimizable());
    QVERIFY(!client->isMinimized());

    // Minimize the client.
    client->minimize();
    QVERIFY(client->isMinimized());

    // If we open the client again, it should be minimized.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isMinimizable());
    QVERIFY(client->isMinimized());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMinimizeForce()
{
    setWindowRule("minimize", false, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->isMinimizable());
    QVERIFY(!client->isMinimized());

    // Any attempt to minimize the client should fail.
    client->minimize();
    QVERIFY(!client->isMinimized());

    // If we re-open the client, the minimized state should still be forced.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->isMinimizable());
    QVERIFY(!client->isMinimized());
    client->minimize();
    QVERIFY(!client->isMinimized());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMinimizeApplyNow()
{
    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isMinimizable());
    QVERIFY(!client->isMinimized());

    setWindowRule("minimize", true, int(Rules::ApplyNow));

    // The client should be minimized now.
    QVERIFY(client->isMinimizable());
    QVERIFY(client->isMinimized());

    // One is still able to unminimize the client.
    client->unminimize();
    QVERIFY(!client->isMinimized());

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QVERIFY(client->isMinimizable());
    QVERIFY(!client->isMinimized());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMinimizeForceTemporarily()
{
    setWindowRule("minimize", false, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->isMinimizable());
    QVERIFY(!client->isMinimized());

    // Any attempt to minimize the client should fail until the client is closed.
    client->minimize();
    QVERIFY(!client->isMinimized());

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isMinimizable());
    QVERIFY(!client->isMinimized());
    client->minimize();
    QVERIFY(client->isMinimized());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipTaskbarDontAffect()
{
    setWindowRule("skiptaskbar", true, int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be affected by the rule.
    QVERIFY(!client->skipTaskbar());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipTaskbarApply()
{
    setWindowRule("skiptaskbar", true, int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be included on a taskbar.
    QVERIFY(client->skipTaskbar());

    // Though one can change that.
    client->setOriginalSkipTaskbar(false);
    QVERIFY(!client->skipTaskbar());

    // Reopen the client, the rule should be applied again.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->skipTaskbar());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipTaskbarRemember()
{
    setWindowRule("skiptaskbar", true, int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be included on a taskbar.
    QVERIFY(client->skipTaskbar());

    // Change the skip-taskbar state.
    client->setOriginalSkipTaskbar(false);
    QVERIFY(!client->skipTaskbar());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should be included on a taskbar.
    QVERIFY(!client->skipTaskbar());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipTaskbarForce()
{
    setWindowRule("skiptaskbar", true, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be included on a taskbar.
    QVERIFY(client->skipTaskbar());

    // Any attempt to change the skip-taskbar state should not succeed.
    client->setOriginalSkipTaskbar(false);
    QVERIFY(client->skipTaskbar());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The skip-taskbar state should be still forced.
    QVERIFY(client->skipTaskbar());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipTaskbarApplyNow()
{
    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->skipTaskbar());

    setWindowRule("skiptaskbar", true, int(Rules::ApplyNow));

    // The client should not be on a taskbar now.
    QVERIFY(client->skipTaskbar());

    // Also, one change the skip-taskbar state.
    client->setOriginalSkipTaskbar(false);
    QVERIFY(!client->skipTaskbar());

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QVERIFY(!client->skipTaskbar());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipTaskbarForceTemporarily()
{
    setWindowRule("skiptaskbar", true, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be included on a taskbar.
    QVERIFY(client->skipTaskbar());

    // Any attempt to change the skip-taskbar state should not succeed.
    client->setOriginalSkipTaskbar(false);
    QVERIFY(client->skipTaskbar());

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->skipTaskbar());

    // The skip-taskbar state is no longer forced.
    client->setOriginalSkipTaskbar(true);
    QVERIFY(client->skipTaskbar());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipPagerDontAffect()
{
    setWindowRule("skippager", true, int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be affected by the rule.
    QVERIFY(!client->skipPager());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipPagerApply()
{
    setWindowRule("skippager", true, int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be included on a pager.
    QVERIFY(client->skipPager());

    // Though one can change that.
    client->setSkipPager(false);
    QVERIFY(!client->skipPager());

    // Reopen the client, the rule should be applied again.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->skipPager());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipPagerRemember()
{
    setWindowRule("skippager", true, int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be included on a pager.
    QVERIFY(client->skipPager());

    // Change the skip-pager state.
    client->setSkipPager(false);
    QVERIFY(!client->skipPager());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should be included on a pager.
    QVERIFY(!client->skipPager());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipPagerForce()
{
    setWindowRule("skippager", true, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be included on a pager.
    QVERIFY(client->skipPager());

    // Any attempt to change the skip-pager state should not succeed.
    client->setSkipPager(false);
    QVERIFY(client->skipPager());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The skip-pager state should be still forced.
    QVERIFY(client->skipPager());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipPagerApplyNow()
{
    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->skipPager());

    setWindowRule("skippager", true, int(Rules::ApplyNow));

    // The client should not be on a pager now.
    QVERIFY(client->skipPager());

    // Also, one change the skip-pager state.
    client->setSkipPager(false);
    QVERIFY(!client->skipPager());

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QVERIFY(!client->skipPager());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipPagerForceTemporarily()
{
    setWindowRule("skippager", true, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be included on a pager.
    QVERIFY(client->skipPager());

    // Any attempt to change the skip-pager state should not succeed.
    client->setSkipPager(false);
    QVERIFY(client->skipPager());

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->skipPager());

    // The skip-pager state is no longer forced.
    client->setSkipPager(true);
    QVERIFY(client->skipPager());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipSwitcherDontAffect()
{
    setWindowRule("skipswitcher", true, int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should not be affected by the rule.
    QVERIFY(!client->skipSwitcher());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipSwitcherApply()
{
    setWindowRule("skipswitcher", true, int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should be excluded from window switching effects.
    QVERIFY(client->skipSwitcher());

    // Though one can change that.
    client->setSkipSwitcher(false);
    QVERIFY(!client->skipSwitcher());

    // Reopen the client, the rule should be applied again.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->skipSwitcher());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipSwitcherRemember()
{
    setWindowRule("skipswitcher", true, int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should be excluded from window switching effects.
    QVERIFY(client->skipSwitcher());

    // Change the skip-switcher state.
    client->setSkipSwitcher(false);
    QVERIFY(!client->skipSwitcher());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should be included in window switching effects.
    QVERIFY(!client->skipSwitcher());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipSwitcherForce()
{
    setWindowRule("skipswitcher", true, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should be excluded from window switching effects.
    QVERIFY(client->skipSwitcher());

    // Any attempt to change the skip-switcher state should not succeed.
    client->setSkipSwitcher(false);
    QVERIFY(client->skipSwitcher());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The skip-switcher state should be still forced.
    QVERIFY(client->skipSwitcher());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipSwitcherApplyNow()
{
    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->skipSwitcher());

    setWindowRule("skipswitcher", true, int(Rules::ApplyNow));

    // The client should be excluded from window switching effects now.
    QVERIFY(client->skipSwitcher());

    // Also, one change the skip-switcher state.
    client->setSkipSwitcher(false);
    QVERIFY(!client->skipSwitcher());

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QVERIFY(!client->skipSwitcher());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipSwitcherForceTemporarily()
{
    setWindowRule("skipswitcher", true, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The client should be excluded from window switching effects.
    QVERIFY(client->skipSwitcher());

    // Any attempt to change the skip-switcher state should not succeed.
    client->setSkipSwitcher(false);
    QVERIFY(client->skipSwitcher());

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->skipSwitcher());

    // The skip-switcher state is no longer forced.
    client->setSkipSwitcher(true);
    QVERIFY(client->skipSwitcher());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepAboveDontAffect()
{
    setWindowRule("above", true, int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The keep-above state of the client should not be affected by the rule.
    QVERIFY(!client->keepAbove());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepAboveApply()
{
    setWindowRule("above", true, int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // Initially, the client should be kept above.
    QVERIFY(client->keepAbove());

    // One should also be able to alter the keep-above state.
    client->setKeepAbove(false);
    QVERIFY(!client->keepAbove());

    // If one re-opens the client, it should be kept above back again.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->keepAbove());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepAboveRemember()
{
    setWindowRule("above", true, int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // Initially, the client should be kept above.
    QVERIFY(client->keepAbove());

    // Unset the keep-above state.
    client->setKeepAbove(false);
    QVERIFY(!client->keepAbove());
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));

    // Re-open the client, it should not be kept above.
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->keepAbove());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepAboveForce()
{
    setWindowRule("above", true, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // Initially, the client should be kept above.
    QVERIFY(client->keepAbove());

    // Any attemt to unset the keep-above should not succeed.
    client->setKeepAbove(false);
    QVERIFY(client->keepAbove());

    // If we re-open the client, it should still be kept above.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->keepAbove());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepAboveApplyNow()
{
    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->keepAbove());

    setWindowRule("above", true, int(Rules::ApplyNow));

    // The client should now be kept above other clients.
    QVERIFY(client->keepAbove());

    // One is still able to change the keep-above state of the client.
    client->setKeepAbove(false);
    QVERIFY(!client->keepAbove());

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QVERIFY(!client->keepAbove());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepAboveForceTemporarily()
{
    setWindowRule("above", true, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // Initially, the client should be kept above.
    QVERIFY(client->keepAbove());

    // Any attempt to alter the keep-above state should not succeed.
    client->setKeepAbove(false);
    QVERIFY(client->keepAbove());

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->keepAbove());

    // The keep-above state is no longer forced.
    client->setKeepAbove(true);
    QVERIFY(client->keepAbove());
    client->setKeepAbove(false);
    QVERIFY(!client->keepAbove());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepBelowDontAffect()
{
    setWindowRule("below", true, int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The keep-below state of the client should not be affected by the rule.
    QVERIFY(!client->keepBelow());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepBelowApply()
{
    setWindowRule("below", true, int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // Initially, the client should be kept below.
    QVERIFY(client->keepBelow());

    // One should also be able to alter the keep-below state.
    client->setKeepBelow(false);
    QVERIFY(!client->keepBelow());

    // If one re-opens the client, it should be kept above back again.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->keepBelow());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepBelowRemember()
{
    setWindowRule("below", true, int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // Initially, the client should be kept below.
    QVERIFY(client->keepBelow());

    // Unset the keep-below state.
    client->setKeepBelow(false);
    QVERIFY(!client->keepBelow());
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));

    // Re-open the client, it should not be kept below.
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->keepBelow());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepBelowForce()
{
    setWindowRule("below", true, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // Initially, the client should be kept below.
    QVERIFY(client->keepBelow());

    // Any attemt to unset the keep-below should not succeed.
    client->setKeepBelow(false);
    QVERIFY(client->keepBelow());

    // If we re-open the client, it should still be kept below.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->keepBelow());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepBelowApplyNow()
{
    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->keepBelow());

    setWindowRule("below", true, int(Rules::ApplyNow));

    // The client should now be kept below other clients.
    QVERIFY(client->keepBelow());

    // One is still able to change the keep-below state of the client.
    client->setKeepBelow(false);
    QVERIFY(!client->keepBelow());

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QVERIFY(!client->keepBelow());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepBelowForceTemporarily()
{
    setWindowRule("below", true, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // Initially, the client should be kept below.
    QVERIFY(client->keepBelow());

    // Any attempt to alter the keep-below state should not succeed.
    client->setKeepBelow(false);
    QVERIFY(client->keepBelow());

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(!client->keepBelow());

    // The keep-below state is no longer forced.
    client->setKeepBelow(true);
    QVERIFY(client->keepBelow());
    client->setKeepBelow(false);
    QVERIFY(!client->keepBelow());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testShortcutDontAffect()
{
    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QCOMPARE(client->shortcut(), QKeySequence());
    client->minimize();
    QVERIFY(client->isMinimized());

    // If we press the window shortcut, nothing should happen.
    QSignalSpy clientUnminimizedSpy(client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(client->isMinimized());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testShortcutApply()
{
    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_1}));
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!client->isMinimized());

    // One can also change the shortcut.
    client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_2}));
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!client->isMinimized());

    // The old shortcut should do nothing.
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(client->isMinimized());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The window shortcut should be set back to Ctrl+Alt+1.
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_1}));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testShortcutRemember()
{
    QSKIP("KWin core doesn't try to save the last used window shortcut");

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_1}));
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!client->isMinimized());

    // Change the window shortcut to Ctrl+Alt+2.
    client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_2}));
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!client->isMinimized());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The window shortcut should be set to the last known value.
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_2}));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testShortcutForce()
{
    QSKIP("KWin core can't release forced window shortcuts");

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_1}));
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!client->isMinimized());

    // Any attempt to change the window shortcut should not succeed.
    client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_1}));
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(client->isMinimized());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // The window shortcut should still be forced.
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_1}));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testShortcutApplyNow()
{
    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->shortcut().isEmpty());

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::ApplyNow));

    // The client should now have a window shortcut assigned.
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_1}));
    QSignalSpy clientUnminimizedSpy(client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!client->isMinimized());

    // Assign a different shortcut.
    client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_2}));
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!client->isMinimized());

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_2}));

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testShortcutForceTemporarily()
{
    QSKIP("KWin core can't release forced window shortcuts");

    setWindowRule("shortcut", "Ctrl+Alt+1", int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);

    // If we press the window shortcut, the window should be brought back to user.
    QSignalSpy clientUnminimizedSpy(client, &AbstractClient::clientUnminimized);
    QVERIFY(clientUnminimizedSpy.isValid());
    quint32 timestamp = 1;
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_1}));
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_1, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(clientUnminimizedSpy.wait());
    QVERIFY(!client->isMinimized());

    // Any attempt to change the window shortcut should not succeed.
    client->setShortcut(QStringLiteral("Ctrl+Alt+2"));
    QCOMPARE(client->shortcut(), (QKeySequence{Qt::CTRL + Qt::ALT + Qt::Key_1}));
    client->minimize();
    QVERIFY(client->isMinimized());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_2, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    QVERIFY(!clientUnminimizedSpy.wait(100));
    QVERIFY(client->isMinimized());

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->shortcut().isEmpty());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
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

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());

    // The opacity should not be affected by the rule.
    QCOMPARE(client->opacity(), 1.0);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testActiveOpacityForce()
{
    setWindowRule("opacityactive", 90, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->opacity(), 0.9);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testActiveOpacityForceTemporarily()
{
    setWindowRule("opacityactive", 90, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->opacity(), 0.9);

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->opacity(), 1.0);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testInactiveOpacityDontAffect()
{
    setWindowRule("opacityinactive", 80, int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());

    // Make the client inactive.
    workspace()->setActiveClient(nullptr);
    QVERIFY(!client->isActive());

    // The opacity of the client should not be affected by the rule.
    QCOMPARE(client->opacity(), 1.0);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testInactiveOpacityForce()
{
    setWindowRule("opacityinactive", 80, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->opacity(), 1.0);

    // Make the client inactive.
    workspace()->setActiveClient(nullptr);
    QVERIFY(!client->isActive());

    // The opacity should be forced by the rule.
    QCOMPARE(client->opacity(), 0.8);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testInactiveOpacityForceTemporarily()
{
    setWindowRule("opacityinactive", 80, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->opacity(), 1.0);

    // Make the client inactive.
    workspace()->setActiveClient(nullptr);
    QVERIFY(!client->isActive());

    // The opacity should be forced by the rule.
    QCOMPARE(client->opacity(), 0.8);

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"));
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->opacity(), 1.0);
    workspace()->setActiveClient(nullptr);
    QVERIFY(!client->isActive());
    QCOMPARE(client->opacity(), 1.0);

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testNoBorderDontAffect()
{
    // Initialize RuleBook with the test rule.
    setWindowRule("noborder", true, int(Rules::DontAffect));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);

    // The client should not be affected by the rule.
    QVERIFY(!client->noBorder());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testNoBorderApply()
{
    setWindowRule("noborder", true, int(Rules::Apply));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);

    // Initially, the client should not be decorated.
    QVERIFY(client->noBorder());
    QVERIFY(!client->isDecorated());

    // But you should be able to change "no border" property afterwards.
    QVERIFY(client->userCanSetNoBorder());
    client->setNoBorder(false);
    QVERIFY(!client->noBorder());

    // If one re-opens the client, it should have no border again.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);
    QVERIFY(client->noBorder());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testNoBorderRemember()
{
    setWindowRule("noborder", true, int(Rules::Remember));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);

    // Initially, the client should not be decorated.
    QVERIFY(client->noBorder());
    QVERIFY(!client->isDecorated());

    // Unset the "no border" property.
    QVERIFY(client->userCanSetNoBorder());
    client->setNoBorder(false);
    QVERIFY(!client->noBorder());
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));

    // Re-open the client, it should be decorated.
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);
    QVERIFY(client->isDecorated());
    QVERIFY(!client->noBorder());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testNoBorderForce()
{
    setWindowRule("noborder", true, int(Rules::Force));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);

    // The client should not be decorated.
    QVERIFY(client->noBorder());
    QVERIFY(!client->isDecorated());

    // And the user should not be able to change the "no border" property.
    client->setNoBorder(false);
    QVERIFY(client->noBorder());

    // Reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);

    // The "no border" property should be still forced.
    QVERIFY(client->noBorder());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testNoBorderApplyNow()
{
    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);
    QVERIFY(!client->noBorder());

    // Initialize RuleBook with the test rule.
    setWindowRule("noborder", true, int(Rules::ApplyNow));

    // The "no border" property should be set now.
    QVERIFY(client->noBorder());

    // One should be still able to change the "no border" property.
    client->setNoBorder(false);
    QVERIFY(!client->noBorder());

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QVERIFY(!client->noBorder());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testNoBorderForceTemporarily()
{
    setWindowRule("noborder", true, int(Rules::ForceTemporarily));

    // Create the test client.
    AbstractClient *client;
    KWayland::Client::Surface *surface;
    Test::XdgToplevel *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);

    // The "no border" property should be set.
    QVERIFY(client->noBorder());

    // And you should not be able to change it.
    client->setNoBorder(false);
    QVERIFY(client->noBorder());

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow(QStringLiteral("org.kde.foo"), Test::XdgToplevelDecorationV1::mode_server_side);
    QVERIFY(client);
    QVERIFY(!client->noBorder());

    // The "no border" property is no longer forced.
    client->setNoBorder(true);
    QVERIFY(client->noBorder());
    client->setNoBorder(false);
    QVERIFY(!client->noBorder());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
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
