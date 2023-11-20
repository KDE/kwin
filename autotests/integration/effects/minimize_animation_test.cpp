/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/plasmawindowmanagement.h>
#include <KWayland/Client/surface.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_effects_minimize_animation-0");

class MinimizeAnimationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMinimizeUnminimize_data();
    void testMinimizeUnminimize();
};

void MinimizeAnimationTest::initTestCase()
{
    if (!Test::renderNodeAvailable()) {
        QSKIP("no render node available");
        return;
    }
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());

    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
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
    QVERIFY(applicationStartedSpy.wait());
}

void MinimizeAnimationTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::LayerShellV1 | Test::AdditionalWaylandInterface::WindowManagement));
}

void MinimizeAnimationTest::cleanup()
{
    QVERIFY(effects);
    effects->unloadAllEffects();
    QVERIFY(effects->loadedEffects().isEmpty());

    Test::destroyWaylandConnection();
}

void MinimizeAnimationTest::testMinimizeUnminimize_data()
{
    QTest::addColumn<QString>("effectName");

    QTest::newRow("Magic Lamp") << QStringLiteral("magiclamp");
    QTest::newRow("Squash") << QStringLiteral("squash");
}

void MinimizeAnimationTest::testMinimizeUnminimize()
{
    // This test verifies that a minimize effect tries to animate a window
    // when it's minimized or unminimized.


    // Create a panel at the top of the screen.
    const QRect panelRect = QRect(0, 0, 1280, 36);
    std::unique_ptr<KWayland::Client::Surface> panelSurface{Test::createSurface()};
    std::unique_ptr<Test::LayerSurfaceV1> panelShellSurface{Test::createLayerSurfaceV1(panelSurface.get(), QStringLiteral("dock"))};
    panelShellSurface->set_size(panelRect.width(), panelRect.height());
    panelShellSurface->set_exclusive_zone(panelRect.height());
    panelShellSurface->set_anchor(Test::LayerSurfaceV1::anchor_top);
    panelSurface->commit(KWayland::Client::Surface::CommitFlag::None);

    QSignalSpy panelConfigureRequestedSpy(panelShellSurface.get(), &Test::LayerSurfaceV1::configureRequested);
    QVERIFY(panelConfigureRequestedSpy.wait());
    Window *panel = Test::renderAndWaitForShown(panelSurface.get(), panelConfigureRequestedSpy.last().at(1).toSize(), Qt::blue);
    QVERIFY(panel);
    QVERIFY(panel->isDock());
    QCOMPARE(panel->frameGeometry(), panelRect);

    // Create the test window.
    QSignalSpy plasmaWindowCreatedSpy(Test::waylandWindowManagement(), &KWayland::Client::PlasmaWindowManagement::windowCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(window);
    QVERIFY(plasmaWindowCreatedSpy.wait());
    QCOMPARE(plasmaWindowCreatedSpy.count(), 1);

    // We have to set the minimized geometry because the squash effect needs it,
    // otherwise it won't start animation.
    auto plasmaWindow = plasmaWindowCreatedSpy.last().first().value<KWayland::Client::PlasmaWindow *>();
    QVERIFY(plasmaWindow);
    const QRect iconRect = QRect(0, 0, 42, 36);
    plasmaWindow->setMinimizedGeometry(panelSurface.get(), iconRect);
    Test::flushWaylandConnection();
    QTRY_COMPARE(window->iconGeometry(), iconRect.translated(panel->frameGeometry().topLeft().toPoint()));

    // Load effect that will be tested.
    QFETCH(QString, effectName);
    QVERIFY(effects);
    QVERIFY(effects->loadEffect(effectName));
    QCOMPARE(effects->loadedEffects().count(), 1);
    QCOMPARE(effects->loadedEffects().first(), effectName);
    Effect *effect = effects->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Start the minimize animation.
    window->setMinimized(true);
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Start the unminimize animation.
    window->setMinimized(false);
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Destroy the panel.
    panelSurface.reset();
    QVERIFY(Test::waitForWindowClosed(panel));

    // Destroy the test window.
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
}

WAYLANDTEST_MAIN(MinimizeAnimationTest)
#include "minimize_animation_test.moc"
