/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "cursor.h"
#include "platform.h"
#include "rules.h"
#include "screens.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>

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

    void testMatchAfterNameChange();
};

void TestXdgShellClientRules::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestXdgShellClientRules::init()
{
    VirtualDesktopManager::self()->setCurrent(VirtualDesktopManager::self()->desktops().first());
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration));

    screens()->setCurrent(0);
}

void TestXdgShellClientRules::cleanup()
{
    Test::destroyWaylandConnection();

    // Unreference the previous config.
    RuleBook::self()->setConfig({});
    workspace()->slotReconfigure();

    // Restore virtual desktops to the initial state.
    VirtualDesktopManager::self()->setCount(1);
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
}

std::tuple<AbstractClient *, Surface *, XdgShellSurface *> createWindow(const QByteArray &appId)
{
    // Create an xdg surface.
    Surface *surface = Test::createSurface();
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface, Test::CreationSetup::CreateOnly);

    // Assign the desired app id.
    shellSurface->setAppId(appId);

    // Wait for the initial configure event.
    QSignalSpy configureRequestedSpy(shellSurface, &XdgShellSurface::configureRequested);
    surface->commit(Surface::CommitFlag::None);
    configureRequestedSpy.wait();

    // Draw content of the surface.
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);

    return {client, surface, shellSurface};
}

void TestXdgShellClientRules::testPositionDontAffect()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("position", QPoint(42, 42));
    group.writeEntry("positionrule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("position", QPoint(42, 42));
    group.writeEntry("positionrule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(client->isMove());
    QVERIFY(!client->isResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(client->pos(), QPoint(50, 42));

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    QCOMPARE(client->pos(), QPoint(50, 42));

    // The rule should be applied again if the client appears after it's been closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("position", QPoint(42, 42));
    group.writeEntry("positionrule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(client->isMove());
    QVERIFY(!client->isResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(client->pos(), QPoint(50, 42));

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    QCOMPARE(client->pos(), QPoint(50, 42));

    // The client should be placed at the last know position if we reopen it.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("position", QPoint(42, 42));
    group.writeEntry("positionrule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());

    // The position should still be forced if we reopen the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    Surface *surface;
    QObject *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(client->isActive());

    // The position of the client isn't set by any rule, thus the default placement
    // policy will try to put the client in the top-left corner of the screen.
    QVERIFY(client->isMovable());
    QVERIFY(client->isMovableAcrossScreens());
    QCOMPARE(client->pos(), QPoint(0, 0));

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("position", QPoint(42, 42));
    group.writeEntry("positionrule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);

    // The client should be moved to the position specified by the rule.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    workspace()->slotReconfigure();
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
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(client->isMove());
    QVERIFY(!client->isResize());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(client->pos(), QPoint(50, 42));

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("position", QPoint(42, 42));
    group.writeEntry("positionrule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());

    // The rule should be discarded if we close the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("size", QSize(480, 640));
    group.writeEntry("sizerule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // The window size shouldn't be enforced by the rule.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(0, 0));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeApply()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("size", QSize(480, 640));
    group.writeEntry("sizerule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // The initial configure event should contain size hint set by the rule.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(480, 640));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Resizing));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Resizing));

    // One still should be able to resize the client.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());
    QSignalSpy surfaceSizeChangedSpy(shellSurface.data(), &XdgShellSurface::sizeChanged);
    QVERIFY(surfaceSizeChangedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(!client->isMove());
    QVERIFY(client->isResize());
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 3);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Resizing));
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 4);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Resizing));
    QCOMPARE(surfaceSizeChangedSpy.count(), 1);
    QCOMPARE(surfaceSizeChangedSpy.last().first().toSize(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 0);
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(488, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());

    QVERIFY(configureRequestedSpy->wait(10));
    QCOMPARE(configureRequestedSpy->count(), 5);

    // The rule should be applied again if the client appears after it's been closed.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(480, 640));

    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeRemember()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("size", QSize(480, 640));
    group.writeEntry("sizerule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // The initial configure event should contain size hint set by the rule.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(480, 640));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Resizing));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Resizing));

    // One should still be able to resize the client.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(client, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());
    QSignalSpy surfaceSizeChangedSpy(shellSurface.data(), &XdgShellSurface::sizeChanged);
    QVERIFY(surfaceSizeChangedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QVERIFY(!client->isMove());
    QVERIFY(client->isResize());
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 3);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Resizing));
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());

    const QPoint cursorPos = KWin::Cursors::self()->mouse()->pos();
    client->keyPressEvent(Qt::Key_Right);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 4);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Resizing));
    QCOMPARE(surfaceSizeChangedSpy.count(), 1);
    QCOMPARE(surfaceSizeChangedSpy.last().first().toSize(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 0);
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(488, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(488, 640));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    client->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());

    QVERIFY(configureRequestedSpy->wait(10));
    QCOMPARE(configureRequestedSpy->count(), 5);

    // If the client appears again, it should have the last known size.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(488, 640));

    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(488, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(488, 640));

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeForce()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("size", QSize(480, 640));
    group.writeEntry("sizerule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // The initial configure event should contain size hint set by the rule.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);

    // Any attempt to resize the client should not succeed.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    QVERIFY(!configureRequestedSpy->wait(100));

    // If the client appears again, the size should still be forced.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(480, 640));

    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeApplyNow()
{
    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // The expected surface dimensions should be set by the rule.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(0, 0));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("size", QSize(480, 640));
    group.writeEntry("sizerule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // The compositor should send a configure event with a new size.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 3);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Draw the surface with the new size.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(480, 640));
    QVERIFY(!configureRequestedSpy->wait(100));

    // The rule should not be applied again.
    client->evaluateWindowRules();
    QVERIFY(!configureRequestedSpy->wait(100));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSizeForceTemporarily()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("size", QSize(480, 640));
    group.writeEntry("sizerule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // The initial configure event should contain size hint set by the rule.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(480, 640));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(480, 640), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isResizable());
    QCOMPARE(client->size(), QSize(480, 640));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);

    // Any attempt to resize the client should not succeed.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(clientStartMoveResizedSpy.count(), 0);
    QVERIFY(!client->isMove());
    QVERIFY(!client->isResize());
    QVERIFY(!configureRequestedSpy->wait(100));

    // The rule should be discarded when the client is closed.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().first().toSize(), QSize(0, 0));

    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isResizable());
    QCOMPARE(client->size(), QSize(100, 50));

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeDontAffect()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("maximizehoriz", true);
    group.writeEntry("maximizehorizrule", int(Rules::DontAffect));
    group.writeEntry("maximizevert", true);
    group.writeEntry("maximizevertrule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeApply()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("maximizehoriz", true);
    group.writeEntry("maximizehorizrule", int(Rules::Apply));
    group.writeEntry("maximizevert", true);
    group.writeEntry("maximizevertrule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // One should still be able to change the maximized state of the client.
    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 3);
    QEXPECT_FAIL("", "Geometry restore is set to the first valid geometry", Continue);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
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
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeRemember()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("maximizehoriz", true);
    group.writeEntry("maximizehorizrule", int(Rules::Remember));
    group.writeEntry("maximizevert", true);
    group.writeEntry("maximizevertrule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // One should still be able to change the maximized state of the client.
    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 3);
    QEXPECT_FAIL("", "Geometry restore is set to the first valid geometry", Continue);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
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
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->size(), QSize(100, 50));

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeForce()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("maximizehoriz", true);
    group.writeEntry("maximizehorizrule", int(Rules::Force));
    group.writeEntry("maximizevert", true);
    group.writeEntry("maximizevertrule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Any attempt to change the maximized state should not succeed.
    const QRect oldGeometry = client->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(!configureRequestedSpy->wait(100));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->frameGeometry(), oldGeometry);

    // If we create the client again, the maximized state should still be forced.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testMaximizeApplyNow()
{
    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->size(), QSize(100, 50));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("maximizehoriz", true);
    group.writeEntry("maximizehorizrule", int(Rules::ApplyNow));
    group.writeEntry("maximizevert", true);
    group.writeEntry("maximizevertrule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // We should receive a configure event with a new surface size.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 3);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Draw contents of the maximized client.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(1280, 1024));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);

    // The client still has to be maximizeable.
    QVERIFY(client->isMaximizable());

    // Restore the client.
    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 4);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(100, 50));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->size(), QSize(100, 50));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);

    // The rule should be discarded after it's been applied.
    const QRect oldGeometry = client->frameGeometry();
    client->evaluateWindowRules();
    QVERIFY(!configureRequestedSpy->wait(100));
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("maximizehoriz", true);
    group.writeEntry("maximizehorizrule", int(Rules::ForceTemporarily));
    group.writeEntry("maximizevert", true);
    group.writeEntry("maximizevertrule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    QScopedPointer<Surface> surface;
    surface.reset(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface;
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<QSignalSpy> configureRequestedSpy;
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(1280, 1024));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Map the client.
    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(!client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->size(), QSize(1280, 1024));

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Any attempt to change the maximized state should not succeed.
    const QRect oldGeometry = client->frameGeometry();
    workspace()->slotWindowMaximize();
    QVERIFY(!configureRequestedSpy->wait(100));
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(client->frameGeometry(), oldGeometry);

    // The rule should be discarded if we close the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    surface.reset(Test::createSurface());
    shellSurface.reset(createXdgShellStableSurface(surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    configureRequestedSpy.reset(new QSignalSpy(shellSurface.data(), &XdgShellSurface::configureRequested));
    shellSurface->setAppId("org.kde.foo");
    surface->commit(Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 1);
    QCOMPARE(configureRequestedSpy->last().at(0).toSize(), QSize(0, 0));
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    shellSurface->ackConfigure(configureRequestedSpy->last().at(2).value<quint32>());
    client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QVERIFY(client->isMaximizable());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(client->size(), QSize(100, 50));

    QVERIFY(configureRequestedSpy->wait());
    QCOMPARE(configureRequestedSpy->count(), 2);
    states = configureRequestedSpy->last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Destroy the client.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testDesktopDontAffect()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("desktop", 2);
    group.writeEntry("desktoprule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("desktop", 2);
    group.writeEntry("desktoprule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("desktop", 2);
    group.writeEntry("desktoprule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("desktop", 2);
    group.writeEntry("desktoprule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QCOMPARE(client->desktop(), 1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("desktop", 2);
    group.writeEntry("desktoprule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("desktop", 2);
    group.writeEntry("desktoprule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // We need at least two virtual desktop for this test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1);

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("minimize", true);
    group.writeEntry("minimizerule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("minimize", true);
    group.writeEntry("minimizerule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("minimize", false);
    group.writeEntry("minimizerule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("minimize", false);
    group.writeEntry("minimizerule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(client->isMinimizable());
    QVERIFY(!client->isMinimized());

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("minimize", true);
    group.writeEntry("minimizerule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("minimize", false);
    group.writeEntry("minimizerule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skiptaskbar", true);
    group.writeEntry("skiptaskbarrule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skiptaskbar", true);
    group.writeEntry("skiptaskbarrule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(client->skipTaskbar());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipTaskbarRemember()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skiptaskbar", true);
    group.writeEntry("skiptaskbarrule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skiptaskbar", true);
    group.writeEntry("skiptaskbarrule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(!client->skipTaskbar());

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skiptaskbar", true);
    group.writeEntry("skiptaskbarrule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skiptaskbar", true);
    group.writeEntry("skiptaskbarrule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skippager", true);
    group.writeEntry("skippagerrule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skippager", true);
    group.writeEntry("skippagerrule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(client->skipPager());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipPagerRemember()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skippager", true);
    group.writeEntry("skippagerrule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skippager", true);
    group.writeEntry("skippagerrule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(!client->skipPager());

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skippager", true);
    group.writeEntry("skippagerrule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skippager", true);
    group.writeEntry("skippagerrule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skipswitcher", true);
    group.writeEntry("skipswitcherrule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skipswitcher", true);
    group.writeEntry("skipswitcherrule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(client->skipSwitcher());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testSkipSwitcherRemember()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skipswitcher", true);
    group.writeEntry("skipswitcherrule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skipswitcher", true);
    group.writeEntry("skipswitcherrule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(!client->skipSwitcher());

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skipswitcher", true);
    group.writeEntry("skipswitcherrule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("skipswitcher", true);
    group.writeEntry("skipswitcherrule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("above", true);
    group.writeEntry("aboverule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("above", true);
    group.writeEntry("aboverule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(client->keepAbove());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepAboveRemember()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("above", true);
    group.writeEntry("aboverule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(!client->keepAbove());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepAboveForce()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("above", true);
    group.writeEntry("aboverule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(!client->keepAbove());

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("above", true);
    group.writeEntry("aboverule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("above", true);
    group.writeEntry("aboverule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("below", true);
    group.writeEntry("belowrule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("below", true);
    group.writeEntry("belowrule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(client->keepBelow());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepBelowRemember()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("below", true);
    group.writeEntry("belowrule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(!client->keepBelow());

    // Destroy the client.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void TestXdgShellClientRules::testKeepBelowForce()
{
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("below", true);
    group.writeEntry("belowrule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(!client->keepBelow());

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("below", true);
    group.writeEntry("belowrule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("below", true);
    group.writeEntry("belowrule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("shortcut", "Ctrl+Alt+1");
    group.writeEntry("shortcutrule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("shortcut", "Ctrl+Alt+1");
    group.writeEntry("shortcutrule", int(Rules::Apply));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("shortcut", "Ctrl+Alt+1");
    group.writeEntry("shortcutrule", int(Rules::Remember));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("shortcut", "Ctrl+Alt+1");
    group.writeEntry("shortcutrule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(client->shortcut().isEmpty());

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("shortcut", "Ctrl+Alt+1");
    group.writeEntry("shortcutrule", int(Rules::ApplyNow));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

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

    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("shortcut", "Ctrl+Alt+1");
    group.writeEntry("shortcutrule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("opacityactive", 90);
    group.writeEntry("opacityactiverule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("opacityactive", 90);
    group.writeEntry("opacityactiverule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("opacityactive", 90);
    group.writeEntry("opacityactiverule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->opacity(), 0.9);

    // The rule should be discarded when the client is closed.
    delete shellSurface;
    delete surface;
    QVERIFY(Test::waitForWindowDestroyed(client));
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("opacityinactive", 80);
    group.writeEntry("opacityinactiverule", int(Rules::DontAffect));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("opacityinactive", 80);
    group.writeEntry("opacityinactiverule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    // Initialize RuleBook with the test rule.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);
    KConfigGroup group = config->group("1");
    group.writeEntry("opacityinactive", 80);
    group.writeEntry("opacityinactiverule", int(Rules::ForceTemporarily));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();
    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    // Create the test client.
    AbstractClient *client;
    Surface *surface;
    XdgShellSurface *shellSurface;
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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
    std::tie(client, surface, shellSurface) = createWindow("org.kde.foo");
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

void TestXdgShellClientRules::testMatchAfterNameChange()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("General").writeEntry("count", 1);

    KConfigGroup group = config->group("1");
    group.writeEntry("above", true);
    group.writeEntry("aboverule", int(Rules::Force));
    group.writeEntry("wmclass", "org.kde.foo");
    group.writeEntry("wmclasscomplete", false);
    group.writeEntry("wmclassmatch", int(Rules::ExactMatch));
    group.sync();

    RuleBook::self()->setConfig(config);
    workspace()->slotReconfigure();

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));

    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QCOMPARE(c->keepAbove(), false);

    QSignalSpy desktopFileNameSpy(c, &AbstractClient::desktopFileNameChanged);
    QVERIFY(desktopFileNameSpy.isValid());

    shellSurface->setAppId(QByteArrayLiteral("org.kde.foo"));
    QVERIFY(desktopFileNameSpy.wait());
    QCOMPARE(c->keepAbove(), true);
}

WAYLANDTEST_MAIN(TestXdgShellClientRules)
#include "xdgshellclient_rules_test.moc"
