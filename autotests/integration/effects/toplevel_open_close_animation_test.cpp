/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_effects_toplevel_open_close_animation-0");

class ToplevelOpenCloseAnimationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testAnimateToplevels_data();
    void testAnimateToplevels();
    void testDontAnimatePopups_data();
    void testDontAnimatePopups();
};

void ToplevelOpenCloseAnimationTest::initTestCase()
{
    if (!Test::renderNodeAvailable()) {
        QSKIP("no render node available");
        return;
    }
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());

    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (const QString &name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }
    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", QByteArrayLiteral("1"));

    kwinApp()->start();
}

void ToplevelOpenCloseAnimationTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void ToplevelOpenCloseAnimationTest::cleanup()
{
    effects->unloadAllEffects();
    QVERIFY(effects->loadedEffects().isEmpty());

    Test::destroyWaylandConnection();
}

void ToplevelOpenCloseAnimationTest::testAnimateToplevels_data()
{
    QTest::addColumn<QString>("effectName");

    QTest::newRow("Fade") << QStringLiteral("fade");
    QTest::newRow("Glide") << QStringLiteral("glide");
    QTest::newRow("Scale") << QStringLiteral("scale");
}

void ToplevelOpenCloseAnimationTest::testAnimateToplevels()
{
    // This test verifies that window open/close animation effects try to
    // animate the appearing and the disappearing of toplevel windows.

    // Load effect that will be tested.
    QFETCH(QString, effectName);
    QVERIFY(effects->loadEffect(effectName));
    QCOMPARE(effects->loadedEffects().count(), 1);
    QCOMPARE(effects->loadedEffects().first(), effectName);
    Effect *effect = effects->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Close the test window, the effect should start animating the disappearing
    // of the window.
    QSignalSpy windowClosedSpy(window, &Window::closed);
    shellSurface.reset();
    surface.reset();
    QVERIFY(windowClosedSpy.wait());
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());
}

void ToplevelOpenCloseAnimationTest::testDontAnimatePopups_data()
{
    QTest::addColumn<QString>("effectName");

    QTest::newRow("Fade") << QStringLiteral("fade");
    QTest::newRow("Glide") << QStringLiteral("glide");
    QTest::newRow("Scale") << QStringLiteral("scale");
}

void ToplevelOpenCloseAnimationTest::testDontAnimatePopups()
{
    // This test verifies that window open/close animation effects don't try
    // to animate popups(e.g. popup menus, tooltips, etc).

    // Create the main window.
    std::unique_ptr<KWayland::Client::Surface> mainWindowSurface(Test::createSurface());
    QVERIFY(mainWindowSurface != nullptr);
    std::unique_ptr<Test::XdgToplevel> mainWindowShellSurface(Test::createXdgToplevelSurface(mainWindowSurface.get()));
    QVERIFY(mainWindowShellSurface != nullptr);
    Window *mainWindow = Test::renderAndWaitForShown(mainWindowSurface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(mainWindow);

    // Load effect that will be tested.
    QFETCH(QString, effectName);
    QVERIFY(effects->loadEffect(effectName));
    QCOMPARE(effects->loadedEffects().count(), 1);
    QCOMPARE(effects->loadedEffects().first(), effectName);
    Effect *effect = effects->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Create a popup, it should not be animated.
    std::unique_ptr<KWayland::Client::Surface> popupSurface(Test::createSurface());
    QVERIFY(popupSurface != nullptr);
    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    QVERIFY(positioner);
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
    QVERIFY(!effect->isActive());

    // Destroy the popup, it should not be animated.
    QSignalSpy popupClosedSpy(popup, &Window::closed);
    popupShellSurface.reset();
    popupSurface.reset();
    QVERIFY(popupClosedSpy.wait());
    QVERIFY(!effect->isActive());

    // Destroy the main window.
    mainWindowSurface.reset();
    QVERIFY(Test::waitForWindowClosed(mainWindow));
}

WAYLANDTEST_MAIN(ToplevelOpenCloseAnimationTest)
#include "toplevel_open_close_animation_test.moc"
