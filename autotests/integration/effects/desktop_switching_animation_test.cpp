/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2019 Vlad Zagorodniy <vladzzag@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "composite.h"
#include "effectloader.h"
#include "effects.h"
#include "platform.h"
#include "scene.h"
#include "xdgshellclient.h"
#include "wayland_server.h"
#include "workspace.h"

#include "effect_builtins.h"

#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>

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
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());

    qRegisterMetaType<KWin::AbstractClient *>();
    qRegisterMetaType<KWin::XdgShellClient *>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    ScriptedEffectLoader loader;
    const auto builtinNames = BuiltInEffects::availableEffectNames() << loader.listOfKnownEffects();
    for (const QString &name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }
    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", QByteArrayLiteral("1"));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    waylandServer()->initWorkspace();

    auto scene = Compositor::self()->scene();
    QVERIFY(scene);
    QCOMPARE(scene->compositingType(), OpenGL2Compositing);
}

void DesktopSwitchingAnimationTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void DesktopSwitchingAnimationTest::cleanup()
{
    auto effectsImpl = qobject_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(effectsImpl);
    effectsImpl->unloadAllEffects();
    QVERIFY(effectsImpl->loadedEffects().isEmpty());

    VirtualDesktopManager::self()->setCount(1);

    Test::destroyWaylandConnection();
}

void DesktopSwitchingAnimationTest::testSwitchDesktops_data()
{
    QTest::addColumn<QString>("effectName");

    QTest::newRow("Desktop Cube Animation") << QStringLiteral("cubeslide");
    QTest::newRow("Fade Desktop")           << QStringLiteral("kwin4_effect_fadedesktop");
    QTest::newRow("Slide")                  << QStringLiteral("slide");
}

void DesktopSwitchingAnimationTest::testSwitchDesktops()
{
    // This test verifies that virtual desktop switching animation effects actually
    // try to animate switching between desktops.

    // We need at least 2 virtual desktops for the test.
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->current(), 1u);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);

    // The Fade Desktop effect will do nothing if there are no clients to fade,
    // so we have to create a dummy test client.
    using namespace KWayland::Client;
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    XdgShellClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QCOMPARE(client->desktops().count(), 1);
    QCOMPARE(client->desktops().first(), VirtualDesktopManager::self()->desktops().first());

    // Load effect that will be tested.
    QFETCH(QString, effectName);
    auto effectsImpl = qobject_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(effectsImpl);
    QVERIFY(effectsImpl->loadEffect(effectName));
    QCOMPARE(effectsImpl->loadedEffects().count(), 1);
    QCOMPARE(effectsImpl->loadedEffects().first(), effectName);
    Effect *effect = effectsImpl->findEffect(effectName);
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

    // Destroy the test client.
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

WAYLANDTEST_MAIN(DesktopSwitchingAnimationTest)
#include "desktop_switching_animation_test.moc"
