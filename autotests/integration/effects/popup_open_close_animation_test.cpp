/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/outputbackend.h"
#include "deleted.h"
#include "effectloader.h"
#include "effects.h"
#include "internalwindow.h"
#include "useractions.h"
#include "wayland_server.h"
#include "window.h"
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

    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::InternalWindow *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

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
    std::unique_ptr<KWayland::Client::Surface> mainWindowSurface(Test::createSurface());
    QVERIFY(mainWindowSurface != nullptr);
    std::unique_ptr<Test::XdgToplevel> mainWindowShellSurface(Test::createXdgToplevelSurface(mainWindowSurface.get()));
    QVERIFY(mainWindowShellSurface != nullptr);
    Window *mainWindow = Test::renderAndWaitForShown(mainWindowSurface.get(), QSize(100, 50), Qt::blue);
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
    std::unique_ptr<KWayland::Client::Surface> popupSurface(Test::createSurface());
    QVERIFY(popupSurface != nullptr);
    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(20, 20);
    positioner->set_anchor_rect(0, 0, 10, 10);
    positioner->set_gravity(Test::XdgPositioner::gravity_bottom_right);
    positioner->set_anchor(Test::XdgPositioner::anchor_bottom_left);
    std::unique_ptr<Test::XdgPopup> popupShellSurface(Test::createXdgPopupSurface(popupSurface.get(), mainWindowShellSurface->xdgSurface(), positioner.get()));
    QVERIFY(popupShellSurface != nullptr);
    Window *popup = Test::renderAndWaitForShown(popupSurface.get(), QSize(20, 20), Qt::red);
    QVERIFY(popup);
    QVERIFY(popup->isPopupWindow());
    QCOMPARE(popup->transientFor(), mainWindow);
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Destroy the popup, it should not be animated.
    QSignalSpy popupClosedSpy(popup, &Window::windowClosed);
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

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Load effect that will be tested.
    const QString effectName = QStringLiteral("kwin4_effect_fadingpopups");
    QVERIFY(effectsImpl->loadEffect(effectName));
    QCOMPARE(effectsImpl->loadedEffects().count(), 1);
    QCOMPARE(effectsImpl->loadedEffects().first(), effectName);
    Effect *effect = effectsImpl->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Show the user actions popup.
    workspace()->showWindowMenu(QRect(), window);
    auto userActionsMenu = workspace()->userActionsMenu();
    QTRY_VERIFY(userActionsMenu->isShown());
    QVERIFY(userActionsMenu->hasWindow());
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Close the user actions popup.
    Test::keyboardKeyPressed(KEY_ESC, 0);
    Test::keyboardKeyReleased(KEY_ESC, 1);
    QTRY_VERIFY(!userActionsMenu->isShown());
    QVERIFY(!userActionsMenu->hasWindow());
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Destroy the test window.
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void PopupOpenCloseAnimationTest::testAnimateDecorationTooltips()
{
    // This test verifies that popup open/close animation effects try
    // to animate decoration tooltips.

    // Make sure that we have the right effects ptr.
    auto effectsImpl = qobject_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(effectsImpl);

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    QVERIFY(shellSurface != nullptr);
    std::unique_ptr<Test::XdgToplevelDecorationV1> deco(Test::createXdgToplevelDecorationV1(shellSurface.get()));
    QVERIFY(deco != nullptr);

    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    deco->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isDecorated());

    // Load effect that will be tested.
    const QString effectName = QStringLiteral("kwin4_effect_fadingpopups");
    QVERIFY(effectsImpl->loadEffect(effectName));
    QCOMPARE(effectsImpl->loadedEffects().count(), 1);
    QCOMPARE(effectsImpl->loadedEffects().first(), effectName);
    Effect *effect = effectsImpl->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Show a decoration tooltip.
    QSignalSpy tooltipAddedSpy(workspace(), &Workspace::internalWindowAdded);
    window->decoratedClient()->requestShowToolTip(QStringLiteral("KWin rocks!"));
    QVERIFY(tooltipAddedSpy.wait());
    InternalWindow *tooltip = tooltipAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(tooltip->isInternal());
    QVERIFY(tooltip->isPopupWindow());
    QVERIFY(tooltip->handle()->flags().testFlag(Qt::ToolTip));
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Hide the decoration tooltip.
    QSignalSpy tooltipClosedSpy(tooltip, &InternalWindow::windowClosed);
    window->decoratedClient()->requestHideToolTip();
    QVERIFY(tooltipClosedSpy.wait());
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Destroy the test window.
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

WAYLANDTEST_MAIN(PopupOpenCloseAnimationTest)
#include "popup_open_close_animation_test.moc"
