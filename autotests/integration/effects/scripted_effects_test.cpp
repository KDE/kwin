/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2018 David Edmundson <davidedmundson@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http:// www.gnu.org/licenses/>.
*********************************************************************/

#include "scripting/scriptedeffect.h"
#include "libkwineffects/anidata_p.h"

#include "composite.h"
#include "cursor.h"
#include "cursor.h"
#include "effect_builtins.h"
#include "effectloader.h"
#include "effects.h"
#include "kwin_wayland_test.h"
#include "platform.h"
#include "shell_client.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QScriptContext>
#include <QScriptEngine>
#include <QScriptValue>

#include <KConfigGroup>
#include <KGlobalAccel>

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/slide.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>

using namespace KWin;
static const QString s_socketName = QStringLiteral("wayland_test_effects_scripts-0");

class ScriptedEffectsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testEffectsHandler();
    void testEffectsContext();
    void testShortcuts();
    void testAnimations_data();
    void testAnimations();
    void testScreenEdge();
    void testScreenEdgeTouch();
    void testFullScreenEffect_data();
    void testFullScreenEffect();
private:
    ScriptedEffect *loadEffect(const QString &name);
};

class ScriptedEffectWithDebugSpy : public KWin::ScriptedEffect
{
    Q_OBJECT
public:
    ScriptedEffectWithDebugSpy();
    bool load(const QString &name);
    using AnimationEffect::state;
signals:
    void testOutput(const QString &data);
};

QScriptValue kwinEffectScriptTestOut(QScriptContext *context, QScriptEngine *engine)
{
    auto *script = qobject_cast<ScriptedEffectWithDebugSpy*>(context->callee().data().toQObject());
    QString result;
    for (int i = 0; i < context->argumentCount(); ++i) {
        if (i > 0) {
            result.append(QLatin1Char(' '));
        }
        result.append(context->argument(i).toString());
    }
    emit script->testOutput(result);

    return engine->undefinedValue();
}

ScriptedEffectWithDebugSpy::ScriptedEffectWithDebugSpy()
    : ScriptedEffect()
{
    QScriptValue testHookFunc = engine()->newFunction(kwinEffectScriptTestOut);
    testHookFunc.setData(engine()->newQObject(this));
    engine()->globalObject().setProperty(QStringLiteral("sendTestResponse"), testHookFunc);
}

bool ScriptedEffectWithDebugSpy::load(const QString &name)
{
    const QString path = QFINDTESTDATA("./scripts/" + name + ".js");
    if (!init(name, path)) {
        return false;
    }

    // inject our newly created effect to be registered with the EffectsHandlerImpl::loaded_effects
    // this is private API so some horrible code is used to find the internal effectloader
    // and register ourselves
    auto c = effects->children();
    for (auto it = c.begin(); it != c.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "KWin::EffectLoader") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "effectLoaded", Q_ARG(KWin::Effect*, this), Q_ARG(QString, name));
        break;
    }

    return (static_cast<EffectsHandlerImpl*>(effects)->isEffectLoaded(name));
}

void ScriptedEffectsTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    ScriptedEffectLoader loader;

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = BuiltInEffects::availableEffectNames() << loader.listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QVERIFY(Compositor::self());

    KWin::VirtualDesktopManager::self()->setCount(2);
}

void ScriptedEffectsTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void ScriptedEffectsTest::cleanup()
{
    Test::destroyWaylandConnection();
    auto *e = static_cast<EffectsHandlerImpl*>(effects);
    while (!e->loadedEffects().isEmpty()) {
        const QString effect = e->loadedEffects().first();
        e->unloadEffect(effect);
        QVERIFY(!e->isEffectLoaded(effect));
    }
    KWin::VirtualDesktopManager::self()->setCurrent(1);
}

void ScriptedEffectsTest::testEffectsHandler()
{
    // this triggers and tests some of the signals in EffectHandler, which is exposed to JS as context property "effects"
    auto *effect = new ScriptedEffectWithDebugSpy; // cleaned up in ::clean
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    auto waitFor = [&effectOutputSpy, this](const QString &expected) {
        QVERIFY(effectOutputSpy.count() > 0 || effectOutputSpy.wait());
        QCOMPARE(effectOutputSpy.first().first(), expected);
        effectOutputSpy.removeFirst();
    };
    QVERIFY(effect->load("effectsHandler"));

    // trigger windowAdded signal

    // create a window
    using namespace KWayland::Client;
    auto *surface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(surface);
    auto *shellSurface = Test::createXdgShellV6Surface(surface, surface);
    QVERIFY(shellSurface);
    shellSurface->setTitle("WindowA");
    auto *c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    waitFor("windowAdded - WindowA");
    waitFor("stackingOrder - 1 WindowA");

    // windowMinimsed
    c->minimize();
    waitFor("windowMinimized - WindowA");

    c->unminimize();
    waitFor("windowUnminimized - WindowA");

    surface->deleteLater();
    waitFor("windowClosed - WindowA");

    // desktop management
    KWin::VirtualDesktopManager::self()->setCurrent(2);
    waitFor("desktopChanged - 1 2");
}

void ScriptedEffectsTest::testEffectsContext()
{
    // this tests misc non-objects exposed to the script engine: animationTime, displaySize, use of external enums

    auto *effect = new ScriptedEffectWithDebugSpy; // cleaned up in ::clean
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effect->load("effectContext"));
    QCOMPARE(effectOutputSpy[0].first(), "1280x1024");
    QCOMPARE(effectOutputSpy[1].first(), "100");
    QCOMPARE(effectOutputSpy[2].first(), "2");
    QCOMPARE(effectOutputSpy[3].first(), "0");
}

void ScriptedEffectsTest::testShortcuts()
{
    // this tests method registerShortcut
    auto *effect = new ScriptedEffectWithDebugSpy; // cleaned up in ::clean
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effect->load("shortcutsTest"));
    QCOMPARE(effect->shortcutCallbacks().count(), 1);
    QAction *action = effect->shortcutCallbacks().keys()[0];
    QCOMPARE(action->objectName(), "testShortcut");
    QCOMPARE(action->text(), "Test Shortcut");
    QCOMPARE(KGlobalAccel::self()->shortcut(action).first(), QKeySequence("Meta+Shift+Y"));
    action->trigger();
    QCOMPARE(effectOutputSpy[0].first(), "shortcutTriggered");
}

void ScriptedEffectsTest::testAnimations_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<int>("animationCount");

    QTest::newRow("single") << "animationTest" << 1;
    QTest::newRow("multi")  << "animationTestMulti" << 2;
}

void ScriptedEffectsTest::testAnimations()
{
    // this tests animate/set/cancel
    // methods take either an int or an array, as forced in the data above
    // also splits animate vs effects.animate(..)

    QFETCH(QString, file);
    QFETCH(int, animationCount);

    auto *effect = new ScriptedEffectWithDebugSpy;
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effect->load(file));

    // animated after window added connect
    using namespace KWayland::Client;
    auto *surface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(surface);
    auto *shellSurface = Test::createXdgShellV6Surface(surface, surface);
    QVERIFY(shellSurface);
    shellSurface->setTitle("Window 1");
    auto *c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    // we are running the event loop during renderAndWaitForShown
    // some time will pass with the event loop running between the window being added and getting to here
    // anim.duration is an aboslute value, but retarget will update the duration based on time passed
    int timePassed = 0;

    {
        const AnimationEffect::AniMap state = effect->state();
        QCOMPARE(state.count(), 1);
        QCOMPARE(state.firstKey(), c->effectWindow());
        const auto &animationsForWindow = state.first().first;
        QCOMPARE(animationsForWindow.count(), animationCount);
        QCOMPARE(animationsForWindow[0].duration, 100);
        QCOMPARE(animationsForWindow[0].to, FPx2(1.4));
        QCOMPARE(animationsForWindow[0].attribute, AnimationEffect::Scale);
        QCOMPARE(animationsForWindow[0].curve.type(), QEasingCurve::OutQuad);
        QCOMPARE(animationsForWindow[0].keepAtTarget, false);
        timePassed = animationsForWindow[0].time;
        if (animationCount == 2) {
            QCOMPARE(animationsForWindow[1].duration, 100);
            QCOMPARE(animationsForWindow[1].to, FPx2(0.0));
            QCOMPARE(animationsForWindow[1].attribute, AnimationEffect::Opacity);
            QCOMPARE(animationsForWindow[1].keepAtTarget, false);
        }
    }
    QCOMPARE(effectOutputSpy[0].first(), "true");

    // window state changes, scale should be retargetted

    c->setMinimized(true);
    {
        const AnimationEffect::AniMap state = effect->state();
        QCOMPARE(state.count(), 1);
        const auto &animationsForWindow = state.first().first;
        QCOMPARE(animationsForWindow.count(), animationCount);
        QCOMPARE(animationsForWindow[0].duration, 200 + timePassed);
        QCOMPARE(animationsForWindow[0].to, FPx2(1.5));
        QCOMPARE(animationsForWindow[0].attribute, AnimationEffect::Scale);
        QCOMPARE(animationsForWindow[0].keepAtTarget, false);
        if (animationCount == 2) {
            QCOMPARE(animationsForWindow[1].duration, 200 + timePassed);
            QCOMPARE(animationsForWindow[1].to, FPx2(1.5));
            QCOMPARE(animationsForWindow[1].attribute, AnimationEffect::Opacity);
            QCOMPARE(animationsForWindow[1].keepAtTarget, false);
        }
    }
    c->setMinimized(false);
    {
        const AnimationEffect::AniMap state = effect->state();
        QCOMPARE(state.count(), 0);
    }
}

void ScriptedEffectsTest::testScreenEdge()
{
    // this test checks registerScreenEdge functions
    auto *effect = new ScriptedEffectWithDebugSpy; // cleaned up in ::clean
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effect->load("screenEdgeTest"));
    effect->borderActivated(KWin::ElectricTopRight);
    QCOMPARE(effectOutputSpy.count(), 1);
}

void ScriptedEffectsTest::testScreenEdgeTouch()
{
    // this test checks registerTouchScreenEdge functions
    auto *effect = new ScriptedEffectWithDebugSpy; // cleaned up in ::clean
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effect->load("screenEdgeTouchTest"));
    auto actions = effect->findChildren<QAction*>(QString(), Qt::FindDirectChildrenOnly);
    actions[0]->trigger();
    QCOMPARE(effectOutputSpy.count(), 1);
}

void ScriptedEffectsTest::testFullScreenEffect_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("single") << "fullScreenEffectTest";
    QTest::newRow("multi")  << "fullScreenEffectTestMulti";
}

void ScriptedEffectsTest::testFullScreenEffect()
{
    QFETCH(QString, file);

    auto *effectMain = new ScriptedEffectWithDebugSpy; // cleaned up in ::clean
    QSignalSpy effectOutputSpy(effectMain, &ScriptedEffectWithDebugSpy::testOutput);
    QSignalSpy fullScreenEffectActiveSpy(effects, &EffectsHandler::hasActiveFullScreenEffectChanged);
    QSignalSpy isActiveFullScreenEffectSpy(effectMain, &ScriptedEffect::isActiveFullScreenEffectChanged);

    QVERIFY(effectMain->load(file));

    //load any random effect from another test to confirm fullscreen effect state is correctly
    //shown as being someone else
    auto effectOther = new ScriptedEffectWithDebugSpy();
    QVERIFY(effectOther->load("screenEdgeTouchTest"));
    QSignalSpy isActiveFullScreenEffectSpyOther(effectOther, &ScriptedEffect::isActiveFullScreenEffectChanged);

    using namespace KWayland::Client;
    auto *surface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(surface);
    auto *shellSurface = Test::createXdgShellV6Surface(surface, surface);
    QVERIFY(shellSurface);
    shellSurface->setTitle("Window 1");
    auto *c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    QCOMPARE(effects->hasActiveFullScreenEffect(), false);
    QCOMPARE(effectMain->isActiveFullScreenEffect(), false);

    //trigger animation
    KWin::VirtualDesktopManager::self()->setCurrent(2);

    QCOMPARE(effects->activeFullScreenEffect(), effectMain);
    QCOMPARE(effects->hasActiveFullScreenEffect(), true);
    QCOMPARE(fullScreenEffectActiveSpy.count(), 1);

    QCOMPARE(effectMain->isActiveFullScreenEffect(), true);
    QCOMPARE(isActiveFullScreenEffectSpy.count(), 1);

    QCOMPARE(effectOther->isActiveFullScreenEffect(), false);
    QCOMPARE(isActiveFullScreenEffectSpyOther.count(), 0);

    //after 500ms trigger another full screen animation
    QTest::qWait(500);
    KWin::VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(effects->activeFullScreenEffect(), effectMain);

    //after 1000ms (+a safety margin for time based tests) we should still be the active full screen effect
    //despite first animation expiring
    QTest::qWait(500+100);
    QCOMPARE(effects->activeFullScreenEffect(), effectMain);

    //after 1500ms (+a safetey margin) we should have no full screen effect
    QTest::qWait(500+100);
    QCOMPARE(effects->activeFullScreenEffect(), nullptr);
}


WAYLANDTEST_MAIN(ScriptedEffectsTest)
#include "scripted_effects_test.moc"
