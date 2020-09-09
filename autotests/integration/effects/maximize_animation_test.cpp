/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "composite.h"
#include "effectloader.h"
#include "effects.h"
#include "platform.h"
#include "scene.h"
#include "wayland_server.h"
#include "workspace.h"

#include "effect_builtins.h"

#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_effects_maximize_animation-0");

class MaximizeAnimationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMaximizeRestore();
};

void MaximizeAnimationTest::initTestCase()
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());

    qRegisterMetaType<KWin::AbstractClient *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
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

    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", QByteArrayLiteral("1"));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    waylandServer()->initWorkspace();
}

void MaximizeAnimationTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void MaximizeAnimationTest::cleanup()
{
    auto effectsImpl = qobject_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(effectsImpl);
    effectsImpl->unloadAllEffects();
    QVERIFY(effectsImpl->loadedEffects().isEmpty());

    Test::destroyWaylandConnection();
}

void MaximizeAnimationTest::testMaximizeRestore()
{
    // This test verifies that the maximize effect animates a client
    // when it's maximized or restored.

    using namespace KWayland::Client;

    // Create the test client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data(), nullptr, Test::CreationSetup::CreateOnly));

    // Wait for the initial configure event.
    XdgShellSurface::States states;
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);

    surface->commit(Surface::CommitFlag::None);

    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);
    QCOMPARE(configureRequestedSpy.last().at(0).value<QSize>(), QSize(0, 0));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(!states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Draw contents of the surface.
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(client->isActive());
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);

    // We should receive a configure event when the client becomes active.
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Load effect that will be tested.
    const QString effectName = QStringLiteral("kwin4_effect_maximize");
    auto effectsImpl = qobject_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(effectsImpl);
    QVERIFY(effectsImpl->loadEffect(effectName));
    QCOMPARE(effectsImpl->loadedEffects().count(), 1);
    QCOMPARE(effectsImpl->loadedEffects().first(), effectName);
    Effect *effect = effectsImpl->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Maximize the client.
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy maximizeChangedSpy(client, qOverload<AbstractClient *, bool, bool>(&AbstractClient::clientMaximizedStateChanged));
    QVERIFY(maximizeChangedSpy.isValid());

    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    QCOMPARE(configureRequestedSpy.last().at(0).value<QSize>(), QSize(1280, 1024));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(states.testFlag(XdgShellSurface::State::Maximized));

    // Draw contents of the maximized client.
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(1280, 1024), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(maximizeChangedSpy.count(), 1);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Restore the client.
    workspace()->slotWindowMaximize();
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 4);
    QCOMPARE(configureRequestedSpy.last().at(0).value<QSize>(), QSize(100, 50));
    states = configureRequestedSpy.last().at(1).value<XdgShellSurface::States>();
    QVERIFY(states.testFlag(XdgShellSurface::State::Activated));
    QVERIFY(!states.testFlag(XdgShellSurface::State::Maximized));

    // Draw contents of the restored client.
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 2);
    QCOMPARE(maximizeChangedSpy.count(), 2);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QVERIFY(effect->isActive());

    // Eventually, the animation will be complete.
    QTRY_VERIFY(!effect->isActive());

    // Destroy the test client.
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
}

WAYLANDTEST_MAIN(MaximizeAnimationTest)
#include "maximize_animation_test.moc"
