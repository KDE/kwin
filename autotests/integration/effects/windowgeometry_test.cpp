/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#include "composite.h"
#include "effects.h"
#include "effectloader.h"
#include "cursor.h"
#include "platform.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "workspace.h"
#include "effect_builtins.h"

#include <KConfigGroup>

#include <KWayland/Client/buffer.h>
#include <KWayland/Client/shell.h>
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
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
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
    QVERIFY(workspaceCreatedSpy.wait());
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
