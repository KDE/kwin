/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "composite.h"
#include "effects.h"
#include "effectloader.h"
#include "cursor.h"
#include "platform.h"
#include "wayland_server.h"
#include "workspace.h"
#include "effect_builtins.h"

#include <KConfigGroup>

#include <KWayland/Client/buffer.h>
#include <KWayland/Client/surface.h>

using namespace KWin;
using namespace KWayland::Client;
static const QString s_socketName = QStringLiteral("wayland_test_effects_windowgeometry-0");

class WindowGeometryTest : public QObject
{
Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testStartup();
};

void WindowGeometryTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    ScriptedEffectLoader loader;
    const auto builtinNames = BuiltInEffects::availableEffectNames() << loader.listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }
    plugins.writeEntry(BuiltInEffects::nameForEffect(BuiltInEffect::WindowGeometry) + QStringLiteral("Enabled"), true);

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(KWin::Compositor::self());
}

void WindowGeometryTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void WindowGeometryTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void WindowGeometryTest::testStartup()
{
    // just a test to load the effect to verify it doesn't crash
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl*>(effects);
    QVERIFY(e->isEffectLoaded(BuiltInEffects::nameForEffect(BuiltInEffect::WindowGeometry)));
}

WAYLANDTEST_MAIN(WindowGeometryTest)
#include "windowgeometry_test.moc"
