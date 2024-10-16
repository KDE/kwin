/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "compositor.h"
#include "core/output.h"
#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_dont_crash_reinitialize_compositor-0");

class DontCrashReinitializeCompositorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testReinitializeCompositor_data();
    void testReinitializeCompositor();
};

void DontCrashReinitializeCompositorTest::initTestCase()
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
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void DontCrashReinitializeCompositorTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void DontCrashReinitializeCompositorTest::cleanup()
{
    // Unload all effects.
    effects->unloadAllEffects();
    QVERIFY(effects->loadedEffects().isEmpty());

    Test::destroyWaylandConnection();
}

void DontCrashReinitializeCompositorTest::testReinitializeCompositor_data()
{
    QTest::addColumn<QString>("effectName");

    QTest::newRow("Fade") << QStringLiteral("fade");
    QTest::newRow("Glide") << QStringLiteral("glide");
    QTest::newRow("Scale") << QStringLiteral("scale");
}

void DontCrashReinitializeCompositorTest::testReinitializeCompositor()
{
    // This test verifies that KWin doesn't crash when the compositor settings
    // have been changed while a scripted effect animates the disappearing of
    // a window.

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Make sure that only the test effect is loaded.
    QFETCH(QString, effectName);
    QVERIFY(effects->loadEffect(effectName));
    QCOMPARE(effects->loadedEffects().count(), 1);
    QCOMPARE(effects->loadedEffects().first(), effectName);
    Effect *effect = effects->findEffect(effectName);
    QVERIFY(effect);
    QVERIFY(!effect->isActive());

    // Close the test window.
    QSignalSpy windowClosedSpy(window, &Window::closed);
    shellSurface.reset();
    surface.reset();
    QVERIFY(windowClosedSpy.wait());

    // The test effect should start animating the test window. Is there a better
    // way to verify that the test effect actually animates the test window?
    QVERIFY(effect->isActive());

    // Re-initialize the compositor, effects will be destroyed and created again.
    Compositor::self()->reinitialize();

    // By this time, KWin should still be alive.
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::DontCrashReinitializeCompositorTest)
#include "dont_crash_reinitialize_compositor.moc"
