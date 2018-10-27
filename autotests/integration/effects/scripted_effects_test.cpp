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
#include "deleted.h"
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
    void testKeepAlive_data();
    void testKeepAlive();
    void testGrab();
    void testGrabAlreadyGrabbedWindow();
    void testGrabAlreadyGrabbedWindowForced();
    void testUngrab();

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
    qRegisterMetaType<KWin::Deleted*>();
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
    QTest::newRow("global") << "fullScreenEffectTestGlobal";
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

void ScriptedEffectsTest::testKeepAlive_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("keepAlive");

    QTest::newRow("keep")        << "keepAliveTest"         << true;
    QTest::newRow("don't keep")  << "keepAliveTestDontKeep" << false;
}

void ScriptedEffectsTest::testKeepAlive()
{
    // this test checks whether closed windows are kept alive
    // when keepAlive property is set to true(false)

    QFETCH(QString, file);
    QFETCH(bool, keepAlive);

    auto *effect = new ScriptedEffectWithDebugSpy;
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effectOutputSpy.isValid());
    QVERIFY(effect->load(file));

    // create a window
    using namespace KWayland::Client;
    auto *surface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(surface);
    auto *shellSurface = Test::createXdgShellV6Surface(surface, surface);
    QVERIFY(shellSurface);
    auto *c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    // no active animations at the beginning
    QCOMPARE(effect->state().count(), 0);

    // trigger windowClosed signal
    surface->deleteLater();
    QVERIFY(effectOutputSpy.count() == 1 || effectOutputSpy.wait());

    if (keepAlive) {
        QCOMPARE(effect->state().count(), 1);

        QTest::qWait(500);
        QCOMPARE(effect->state().count(), 1);

        QTest::qWait(500 + 100); // 100ms is extra safety margin
        QCOMPARE(effect->state().count(), 0);
    } else {
        // the test effect doesn't keep the window alive, so it should be
        // removed immediately
        QSignalSpy deletedRemovedSpy(workspace(), &Workspace::deletedRemoved);
        QVERIFY(deletedRemovedSpy.isValid());
        QVERIFY(deletedRemovedSpy.count() == 1 || deletedRemovedSpy.wait(100)); // 100ms is less than duration of the animation
        QCOMPARE(effect->state().count(), 0);
    }
}

void ScriptedEffectsTest::testGrab()
{
    // this test verifies that scripted effects can grab windows that are
    // not already grabbed

    // load the test effect
    auto effect = new ScriptedEffectWithDebugSpy;
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effectOutputSpy.isValid());
    QVERIFY(effect->load(QStringLiteral("grabTest")));

    // create test client
    using namespace KWayland::Client;
    Surface *surface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    ShellClient *c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    // the test effect should grab the test client successfully
    QCOMPARE(effectOutputSpy.count(), 1);
    QCOMPARE(effectOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(c->effectWindow()->data(WindowAddedGrabRole).value<void *>(), effect);
}

void ScriptedEffectsTest::testGrabAlreadyGrabbedWindow()
{
    // this test verifies that scripted effects cannot grab already grabbed
    // windows (unless force is set to true of course)

    // load effect that will hold the window grab
    auto owner = new ScriptedEffectWithDebugSpy;
    QSignalSpy ownerOutputSpy(owner, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(ownerOutputSpy.isValid());
    QVERIFY(owner->load(QStringLiteral("grabAlreadyGrabbedWindowTest_owner")));

    // load effect that will try to grab already grabbed window
    auto grabber = new ScriptedEffectWithDebugSpy;
    QSignalSpy grabberOutputSpy(grabber, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(grabberOutputSpy.isValid());
    QVERIFY(grabber->load(QStringLiteral("grabAlreadyGrabbedWindowTest_grabber")));

    // create test client
    using namespace KWayland::Client;
    Surface *surface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    ShellClient *c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    // effect that initially held the grab should still hold the grab
    QCOMPARE(ownerOutputSpy.count(), 1);
    QCOMPARE(ownerOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(c->effectWindow()->data(WindowAddedGrabRole).value<void *>(), owner);

    // effect that tried to grab already grabbed window should fail miserably
    QCOMPARE(grabberOutputSpy.count(), 1);
    QCOMPARE(grabberOutputSpy.first().first(), QStringLiteral("fail"));
}

void ScriptedEffectsTest::testGrabAlreadyGrabbedWindowForced()
{
    // this test verifies that scripted effects can steal window grabs when
    // they forcefully try to grab windows

    // load effect that initially will be holding the window grab
    auto owner = new ScriptedEffectWithDebugSpy;
    QSignalSpy ownerOutputSpy(owner, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(ownerOutputSpy.isValid());
    QVERIFY(owner->load(QStringLiteral("grabAlreadyGrabbedWindowForcedTest_owner")));

    // load effect that will try to steal the window grab
    auto thief = new ScriptedEffectWithDebugSpy;
    QSignalSpy thiefOutputSpy(thief, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(thiefOutputSpy.isValid());
    QVERIFY(thief->load(QStringLiteral("grabAlreadyGrabbedWindowForcedTest_thief")));

    // create test client
    using namespace KWayland::Client;
    Surface *surface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    ShellClient *c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    // verify that the owner in fact held the grab
    QCOMPARE(ownerOutputSpy.count(), 1);
    QCOMPARE(ownerOutputSpy.first().first(), QStringLiteral("ok"));

    // effect that grabbed the test client forcefully should now hold the grab
    QCOMPARE(thiefOutputSpy.count(), 1);
    QCOMPARE(thiefOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(c->effectWindow()->data(WindowAddedGrabRole).value<void *>(), thief);
}

void ScriptedEffectsTest::testUngrab()
{
    // this test verifies that scripted effects can ungrab windows that they
    // are previously grabbed

    // load the test effect
    auto effect = new ScriptedEffectWithDebugSpy;
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effectOutputSpy.isValid());
    QVERIFY(effect->load(QStringLiteral("ungrabTest")));

    // create test client
    using namespace KWayland::Client;
    Surface *surface = Test::createSurface(Test::waylandCompositor());
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    ShellClient *c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    // the test effect should grab the test client successfully
    QCOMPARE(effectOutputSpy.count(), 1);
    QCOMPARE(effectOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(c->effectWindow()->data(WindowAddedGrabRole).value<void *>(), effect);

    // when the test effect sees that a window was minimized, it will try to ungrab it
    effectOutputSpy.clear();
    c->setMinimized(true);

    QCOMPARE(effectOutputSpy.count(), 1);
    QCOMPARE(effectOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(c->effectWindow()->data(WindowAddedGrabRole).value<void *>(), nullptr);
}

WAYLANDTEST_MAIN(ScriptedEffectsTest)
#include "scripted_effects_test.moc"
