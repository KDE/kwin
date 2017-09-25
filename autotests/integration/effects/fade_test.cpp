/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
static const QString s_socketName = QStringLiteral("wayland_test_effects_translucency-0");

class FadeTest : public QObject
{
Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testWindowCloseAfterWindowHidden_data();
    void testWindowCloseAfterWindowHidden();

private:
    Effect *m_fadeEffect = nullptr;
};

void FadeTest::initTestCase()
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

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QVERIFY(KWin::Compositor::self());
}

void FadeTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    // load the translucency effect
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl*>(effects);
    // find the effectsloader
    auto effectloader = e->findChild<AbstractEffectLoader*>();
    QVERIFY(effectloader);
    QSignalSpy effectLoadedSpy(effectloader, &AbstractEffectLoader::effectLoaded);
    QVERIFY(effectLoadedSpy.isValid());

    QVERIFY(!e->isEffectLoaded(QStringLiteral("kwin4_effect_fade")));
    QVERIFY(e->loadEffect(QStringLiteral("kwin4_effect_fade")));
    QVERIFY(e->isEffectLoaded(QStringLiteral("kwin4_effect_fade")));

    QCOMPARE(effectLoadedSpy.count(), 1);
    m_fadeEffect = effectLoadedSpy.first().first().value<Effect*>();
    QVERIFY(m_fadeEffect);
}

void FadeTest::cleanup()
{
    Test::destroyWaylandConnection();
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl*>(effects);
    if (e->isEffectLoaded(QStringLiteral("kwin4_effect_fade"))) {
        e->unloadEffect(QStringLiteral("kwin4_effect_fade"));
    }
    QVERIFY(!e->isEffectLoaded(QStringLiteral("kwin4_effect_fade")));
    m_fadeEffect = nullptr;
}

void FadeTest::testWindowCloseAfterWindowHidden_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("xdgShellV6") << Test::ShellSurfaceType::XdgShellV6;
}

void FadeTest::testWindowCloseAfterWindowHidden()
{
    // this test simulates the showing/hiding/closing of a Wayland window
    // especially the situation that a window got unmapped and destroyed way later
    QVERIFY(!m_fadeEffect->isActive());

    QSignalSpy windowAddedSpy(effects, &EffectsHandler::windowAdded);
    QVERIFY(windowAddedSpy.isValid());
    QSignalSpy windowHiddenSpy(effects, &EffectsHandler::windowHidden);
    QVERIFY(windowHiddenSpy.isValid());
    QSignalSpy windowShownSpy(effects, &EffectsHandler::windowShown);
    QVERIFY(windowShownSpy.isValid());
    QSignalSpy windowClosedSpy(effects, &EffectsHandler::windowClosed);
    QVERIFY(windowClosedSpy.isValid());

    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    QTRY_COMPARE(m_fadeEffect->isActive(), true);

    QTest::qWait(500);
    QTRY_COMPARE(m_fadeEffect->isActive(), false);

    // now unmap the surface
    surface->attachBuffer(Buffer::Ptr());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(windowHiddenSpy.wait());
    QCOMPARE(m_fadeEffect->isActive(), true);
    QTest::qWait(500);
    QTRY_COMPARE(m_fadeEffect->isActive(), false);

    // and map again
    Test::render(surface.data(), QSize(100, 50), Qt::red);
    QVERIFY(windowShownSpy.wait());
    QTRY_COMPARE(m_fadeEffect->isActive(), true);
    QTest::qWait(500);
    QTRY_COMPARE(m_fadeEffect->isActive(), false);

    // and unmap once more
    surface->attachBuffer(Buffer::Ptr());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(windowHiddenSpy.wait());
    QCOMPARE(m_fadeEffect->isActive(), true);
    QTest::qWait(500);
    QTRY_COMPARE(m_fadeEffect->isActive(), false);

    // and now destroy
    shellSurface.reset();
    surface.reset();
    QVERIFY(windowClosedSpy.wait());
    QCOMPARE(m_fadeEffect->isActive(), false);
}

WAYLANDTEST_MAIN(FadeTest)
#include "fade_test.moc"
