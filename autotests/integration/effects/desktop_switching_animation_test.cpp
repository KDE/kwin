/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_effects_desktop_switching_animation-0");

class DesktopSwitchingAnimationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testSwitchDesktops_data();
    void testSwitchDesktops();
};

void DesktopSwitchingAnimationTest::initTestCase()
{
    if (!Test::renderNodeAvailable()) {
        QSKIP("no render node available");
        return;
    }
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());

    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));

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
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
}

void DesktopSwitchingAnimationTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void DesktopSwitchingAnimationTest::cleanup()
{
    QVERIFY(effects);
    effects->unloadAllEffects();
    QVERIFY(effects->loadedEffects().isEmpty());

    VirtualDesktopManager::self()->setCount(1);

    Test::destroyWaylandConnection();
}

void DesktopSwitchingAnimationTest::testSwitchDesktops_data()
{
    QTest::addColumn<QString>("effectName");

    QTest::newRow("Fade Desktop") << QStringLiteral("fadedesktop");
    QTest::newRow("Slide") << QStringLiteral("slide");
}

void DesktopSwitchingAnimationTest::testSwitchDesktops()
{
    // This test verifies that virtual desktop switching animation effects actually
    // try to animate switching between desktops.

    // We need at least 2 virtual desktops for the test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1u);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);

    // The Fade Desktop effect will do nothing if there are no windows to fade,
    // so we have to create a dummy test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(window->desktops().count(), 1);
    QCOMPARE(window->desktops().first(), VirtualDesktopManager::self()->desktops().first());

    // Load effect that will be tested.
    QFETCH(QString, effectName);
    QVERIFY(effects);
    QVERIFY(effects->loadEffect(effectName));
    QCOMPARE(effects->loadedEffects().count(), 1);
    QCOMPARE(effects->loadedEffects().first(), effectName);
    Effect *effect = effects->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Switch to the second virtual desktop.
    VirtualDesktopManager::self()->setCurrent(2u);
    QCOMPARE(VirtualDesktopManager::self()->current(), 2u);
    QVERIFY(effect->isActive());
    QCOMPARE(effects->activeFullScreenEffect(), effect);

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());
    QTRY_COMPARE(effects->activeFullScreenEffect(), nullptr);

    // Destroy the test window.
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

WAYLANDTEST_MAIN(DesktopSwitchingAnimationTest)
#include "desktop_switching_animation_test.moc"
