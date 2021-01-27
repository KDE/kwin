/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "deleted.h"
#include "effectloader.h"
#include "effects.h"
#include "internal_client.h"
#include "platform.h"
#include "useractions.h"
#include "wayland_server.h"
#include "workspace.h"

#include "decorations/decoratedclient.h"

#include <KWayland/Client/surface.h>

#include <linux/input.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_effects_popup_open_close_animation-0");

class PopupOpenCloseAnimationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testAnimatePopups();
    void testAnimateUserActionsPopup();
    void testAnimateDecorationTooltips();
};

void PopupOpenCloseAnimationTest::initTestCase()
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());

    qRegisterMetaType<KWin::AbstractClient *>();
    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::InternalClient *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));

    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (const QString &name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }
    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", QByteArrayLiteral("1"));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    Test::initWaylandWorkspace();
}

void PopupOpenCloseAnimationTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgDecorationV1));
}

void PopupOpenCloseAnimationTest::cleanup()
{
    auto effectsImpl = qobject_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(effectsImpl);
    effectsImpl->unloadAllEffects();
    QVERIFY(effectsImpl->loadedEffects().isEmpty());

    Test::destroyWaylandConnection();
}

void PopupOpenCloseAnimationTest::testAnimatePopups()
{
    // This test verifies that popup open/close animation effects try
    // to animate popups(e.g. popup menus, tooltips, etc).

    // Make sure that we have the right effects ptr.
    auto effectsImpl = qobject_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(effectsImpl);

    // Create the main window.
    using namespace KWayland::Client;
    QScopedPointer<KWayland::Client::Surface> mainWindowSurface(Test::createSurface());
    QVERIFY(!mainWindowSurface.isNull());
    QScopedPointer<Test::XdgToplevel> mainWindowShellSurface(Test::createXdgToplevelSurface(mainWindowSurface.data()));
    QVERIFY(!mainWindowShellSurface.isNull());
    AbstractClient *mainWindow = Test::renderAndWaitForShown(mainWindowSurface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(mainWindow);

    // Load effect that will be tested.
    const QString effectName = QStringLiteral("kwin4_effect_fadingpopups");
    QVERIFY(effectsImpl->loadEffect(effectName));
    QCOMPARE(effectsImpl->loadedEffects().count(), 1);
    QCOMPARE(effectsImpl->loadedEffects().first(), effectName);
    Effect *effect = effectsImpl->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Create a popup, it should be animated.
    QScopedPointer<KWayland::Client::Surface> popupSurface(Test::createSurface());
    QVERIFY(!popupSurface.isNull());
    QScopedPointer<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(20, 20);
    positioner->set_anchor_rect(0, 0, 10, 10);
    positioner->set_gravity(Test::XdgPositioner::gravity_bottom_right);
    positioner->set_anchor(Test::XdgPositioner::anchor_bottom_left);
    QScopedPointer<Test::XdgPopup> popupShellSurface(Test::createXdgPopupSurface(popupSurface.data(), mainWindowShellSurface->xdgSurface(), positioner.data()));
    QVERIFY(!popupShellSurface.isNull());
    AbstractClient *popup = Test::renderAndWaitForShown(popupSurface.data(), QSize(20, 20), Qt::red);
    QVERIFY(popup);
    QVERIFY(popup->isPopupWindow());
    QCOMPARE(popup->transientFor(), mainWindow);
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Destroy the popup, it should not be animated.
    QSignalSpy popupClosedSpy(popup, &AbstractClient::windowClosed);
    QVERIFY(popupClosedSpy.isValid());
    popupShellSurface.reset();
    popupSurface.reset();
    QVERIFY(popupClosedSpy.wait());
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Destroy the main window.
    mainWindowSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(mainWindow));
}

void PopupOpenCloseAnimationTest::testAnimateUserActionsPopup()
{
    // This test verifies that popup open/close animation effects try
    // to animate the user actions popup.

    // Make sure that we have the right effects ptr.
    auto effectsImpl = qobject_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(effectsImpl);

    // Create the test client.
    using namespace KWayland::Client;
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);

    // Load effect that will be tested.
    const QString effectName = QStringLiteral("kwin4_effect_fadingpopups");
    QVERIFY(effectsImpl->loadEffect(effectName));
    QCOMPARE(effectsImpl->loadedEffects().count(), 1);
    QCOMPARE(effectsImpl->loadedEffects().first(), effectName);
    Effect *effect = effectsImpl->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Show the user actions popup.
    workspace()->showWindowMenu(QRect(), client);
    auto userActionsMenu = workspace()->userActionsMenu();
    QTRY_VERIFY(userActionsMenu->isShown());
    QVERIFY(userActionsMenu->hasClient());
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Close the user actions popup.
    kwinApp()->platform()->keyboardKeyPressed(KEY_ESC, 0);
    kwinApp()->platform()->keyboardKeyReleased(KEY_ESC, 1);
    QTRY_VERIFY(!userActionsMenu->isShown());
    QVERIFY(!userActionsMenu->hasClient());
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Destroy the test client.
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

void PopupOpenCloseAnimationTest::testAnimateDecorationTooltips()
{
    // This test verifies that popup open/close animation effects try
    // to animate decoration tooltips.

    // Make sure that we have the right effects ptr.
    auto effectsImpl = qobject_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(effectsImpl);

    // Create the test client.
    using namespace KWayland::Client;
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<Test::XdgToplevelDecorationV1> deco(Test::createXdgToplevelDecorationV1(shellSurface.data()));
    QVERIFY(!deco.isNull());

    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    deco->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isDecorated());

    // Load effect that will be tested.
    const QString effectName = QStringLiteral("kwin4_effect_fadingpopups");
    QVERIFY(effectsImpl->loadEffect(effectName));
    QCOMPARE(effectsImpl->loadedEffects().count(), 1);
    QCOMPARE(effectsImpl->loadedEffects().first(), effectName);
    Effect *effect = effectsImpl->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Show a decoration tooltip.
    QSignalSpy tooltipAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(tooltipAddedSpy.isValid());
    client->decoratedClient()->requestShowToolTip(QStringLiteral("KWin rocks!"));
    QVERIFY(tooltipAddedSpy.wait());
    InternalClient *tooltip = tooltipAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(tooltip->isInternal());
    QVERIFY(tooltip->isPopupWindow());
    QVERIFY(tooltip->internalWindow()->flags().testFlag(Qt::ToolTip));
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Hide the decoration tooltip.
    QSignalSpy tooltipClosedSpy(tooltip, &InternalClient::windowClosed);
    QVERIFY(tooltipClosedSpy.isValid());
    client->decoratedClient()->requestHideToolTip();
    QVERIFY(tooltipClosedSpy.wait());
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Destroy the test client.
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

WAYLANDTEST_MAIN(PopupOpenCloseAnimationTest)
#include "popup_open_close_animation_test.moc"
