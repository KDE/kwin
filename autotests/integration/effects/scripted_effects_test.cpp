/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "effect/anidata_p.h"
#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "scripting/scriptedeffect.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KGlobalAccel>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/slide.h>
#include <KWayland/Client/surface.h>

#include <QJSEngine>
#include <QJSValue>

using namespace KWin;
using namespace std::chrono_literals;

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
    void testRedirect_data();
    void testRedirect();
    void testComplete();

private:
    ScriptedEffect *loadEffect(const QString &name);
};

class ScriptedEffectWithDebugSpy : public KWin::ScriptedEffect
{
    Q_OBJECT
public:
    ScriptedEffectWithDebugSpy();
    bool load(const QString &name);
    using AnimationEffect::AniMap;
    using AnimationEffect::state;
    Q_INVOKABLE void sendTestResponse(const QString &out); // proxies triggers out from the tests
    QList<QAction *> actions(); // returns any QActions owned by the ScriptEngine
Q_SIGNALS:
    void testOutput(const QString &data);
};

void ScriptedEffectWithDebugSpy::sendTestResponse(const QString &out)
{
    Q_EMIT testOutput(out);
}

QList<QAction *> ScriptedEffectWithDebugSpy::actions()
{
    return findChildren<QAction *>(QString(), Qt::FindDirectChildrenOnly);
}

ScriptedEffectWithDebugSpy::ScriptedEffectWithDebugSpy()
    : ScriptedEffect()
{
}

bool ScriptedEffectWithDebugSpy::load(const QString &name)
{
    auto selfContext = engine()->newQObject(this);
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
    const QString path = QFINDTESTDATA("./scripts/" + name + ".js");
    engine()->globalObject().setProperty("sendTestResponse", selfContext.property("sendTestResponse"));
    if (!init(name, path)) {
        return false;
    }

    // inject our newly created effect to be registered with the EffectsHandler::loaded_effects
    // this is private API so some horrible code is used to find the internal effectloader
    // and register ourselves
    auto children = effects->children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "KWin::EffectLoader") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "effectLoaded", Q_ARG(KWin::Effect *, this), Q_ARG(QString, name));
        break;
    }

    return effects->isEffectLoaded(name);
}

void ScriptedEffectsTest::initTestCase()
{
    if (!Test::renderNodeAvailable()) {
        QSKIP("no render node available");
        return;
    }
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Effect *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();

    KWin::VirtualDesktopManager::self()->setCount(2);
}

void ScriptedEffectsTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void ScriptedEffectsTest::cleanup()
{
    Test::destroyWaylandConnection();

    effects->unloadAllEffects();
    QVERIFY(effects->loadedEffects().isEmpty());

    KWin::VirtualDesktopManager::self()->setCurrent(1);
}

void ScriptedEffectsTest::testEffectsHandler()
{
    // this triggers and tests some of the signals in EffectHandler, which is exposed to JS as context property "effects"
    auto *effect = new ScriptedEffectWithDebugSpy; // cleaned up in ::clean
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    auto waitFor = [&effectOutputSpy](const QString &expected) {
        QVERIFY(effectOutputSpy.count() > 0 || effectOutputSpy.wait());
        QCOMPARE(effectOutputSpy.first().first(), expected);
        effectOutputSpy.removeFirst();
    };
    QVERIFY(effect->load("effectsHandler"));

    // trigger windowAdded signal

    // create a window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    shellSurface->set_title("WindowA");
    auto *c = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeWindow(), c);

    waitFor("windowAdded - WindowA");
    waitFor("stackingOrder - 1 WindowA");

    // windowMinimsed
    c->setMinimized(true);
    waitFor("windowMinimized - WindowA");

    c->setMinimized(false);
    waitFor("windowUnminimized - WindowA");

    surface.reset();
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
#if !KWIN_BUILD_GLOBALSHORTCUTS
    QSKIP("Can't test shortcuts without shortcuts");
    return;
#endif

    // this tests method registerShortcut
    auto *effect = new ScriptedEffectWithDebugSpy; // cleaned up in ::clean
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effect->load("shortcutsTest"));
    QCOMPARE(effect->actions().count(), 1);
    auto action = effect->actions()[0];
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
    QTest::newRow("multi") << "animationTestMulti" << 2;
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
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    shellSurface->set_title("Window 1");
    auto *c = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeWindow(), c);

    {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 1);
        QVERIFY(state.contains(c->effectWindow()));
        const auto &animationsForWindow = state.at(c->effectWindow()).first;
        QCOMPARE(animationsForWindow.size(), animationCount);
        QCOMPARE(animationsForWindow[0].timeLine.duration(), 100ms);
        QCOMPARE(animationsForWindow[0].to, FPx2(1.4));
        QCOMPARE(animationsForWindow[0].attribute, AnimationEffect::Scale);
        QCOMPARE(animationsForWindow[0].timeLine.easingCurve().type(), QEasingCurve::OutCubic);
        QCOMPARE(animationsForWindow[0].terminationFlags,
                 AnimationEffect::TerminateAtSource | AnimationEffect::TerminateAtTarget);

        if (animationCount == 2) {
            QCOMPARE(animationsForWindow[1].timeLine.duration(), 100ms);
            QCOMPARE(animationsForWindow[1].to, FPx2(0.0));
            QCOMPARE(animationsForWindow[1].attribute, AnimationEffect::Opacity);
            QCOMPARE(animationsForWindow[1].terminationFlags,
                     AnimationEffect::TerminateAtSource | AnimationEffect::TerminateAtTarget);
        }
    }
    QCOMPARE(effectOutputSpy[0].first(), "true");

    // window state changes, scale should be retargetted

    c->setMinimized(true);
    {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 1);
        const auto &animationsForWindow = state.at(c->effectWindow()).first;
        QCOMPARE(animationsForWindow.size(), animationCount);
        QCOMPARE(animationsForWindow[0].timeLine.duration(), 200ms);
        QCOMPARE(animationsForWindow[0].to, FPx2(1.5));
        QCOMPARE(animationsForWindow[0].attribute, AnimationEffect::Scale);
        QCOMPARE(animationsForWindow[0].terminationFlags,
                 AnimationEffect::TerminateAtSource | AnimationEffect::TerminateAtTarget);
        if (animationCount == 2) {
            QCOMPARE(animationsForWindow[1].timeLine.duration(), 200ms);
            QCOMPARE(animationsForWindow[1].to, FPx2(1.5));
            QCOMPARE(animationsForWindow[1].attribute, AnimationEffect::Opacity);
            QCOMPARE(animationsForWindow[1].terminationFlags,
                     AnimationEffect::TerminateAtSource | AnimationEffect::TerminateAtTarget);
        }
    }
    c->setMinimized(false);
    {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 0);
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
    effect->actions()[0]->trigger();
    QCOMPARE(effectOutputSpy.count(), 1);
}

void ScriptedEffectsTest::testFullScreenEffect_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("single") << "fullScreenEffectTest";
    QTest::newRow("multi") << "fullScreenEffectTestMulti";
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

    // load any random effect from another test to confirm fullscreen effect state is correctly
    // shown as being someone else
    auto effectOther = new ScriptedEffectWithDebugSpy();
    QVERIFY(effectOther->load("screenEdgeTouchTest"));
    QSignalSpy isActiveFullScreenEffectSpyOther(effectOther, &ScriptedEffect::isActiveFullScreenEffectChanged);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    shellSurface->set_title("Window 1");
    auto *c = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeWindow(), c);

    QCOMPARE(effects->hasActiveFullScreenEffect(), false);
    QCOMPARE(effectMain->isActiveFullScreenEffect(), false);

    // trigger animation
    KWin::VirtualDesktopManager::self()->setCurrent(2);

    QCOMPARE(effects->activeFullScreenEffect(), effectMain);
    QCOMPARE(effects->hasActiveFullScreenEffect(), true);
    QCOMPARE(fullScreenEffectActiveSpy.count(), 1);

    QCOMPARE(effectMain->isActiveFullScreenEffect(), true);
    QCOMPARE(isActiveFullScreenEffectSpy.count(), 1);

    QCOMPARE(effectOther->isActiveFullScreenEffect(), false);
    QCOMPARE(isActiveFullScreenEffectSpyOther.count(), 0);

    // after 500ms trigger another full screen animation
    QTest::qWait(500);
    KWin::VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(effects->activeFullScreenEffect(), effectMain);

    // after 1000ms (+a safety margin for time based tests) we should still be the active full screen effect
    // despite first animation expiring
    QTest::qWait(500 + 100);
    QCOMPARE(effects->activeFullScreenEffect(), effectMain);

    // after 1500ms (+a safetey margin) we should have no full screen effect
    QTest::qWait(500 + 100);
    QCOMPARE(effects->activeFullScreenEffect(), nullptr);
}

void ScriptedEffectsTest::testKeepAlive_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("keepAlive");

    QTest::newRow("keep") << "keepAliveTest" << true;
    QTest::newRow("don't keep") << "keepAliveTestDontKeep" << false;
}

void ScriptedEffectsTest::testKeepAlive()
{
    // this test checks whether closed windows are kept alive
    // when keepAlive property is set to true(false)

    QFETCH(QString, file);
    QFETCH(bool, keepAlive);

    auto *effect = new ScriptedEffectWithDebugSpy;
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effect->load(file));

    // create a window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    auto *c = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeWindow(), c);

    // no active animations at the beginning
    QCOMPARE(effect->state().size(), 0);

    // trigger windowClosed signal
    QSignalSpy deletedRemovedSpy(workspace(), &Workspace::deletedRemoved);
    surface.reset();
    QVERIFY(effectOutputSpy.count() == 1 || effectOutputSpy.wait());

    if (keepAlive) {
        QCOMPARE(effect->state().size(), 1);
        QCOMPARE(deletedRemovedSpy.count(), 0);

        QTest::qWait(500);
        QCOMPARE(effect->state().size(), 1);
        QCOMPARE(deletedRemovedSpy.count(), 0);

        QTest::qWait(500 + 100); // 100ms is extra safety margin
        QCOMPARE(deletedRemovedSpy.count(), 1);
        QCOMPARE(effect->state().size(), 0);
    } else {
        // the test effect doesn't keep the window alive, so it should be
        // removed immediately
        QVERIFY(deletedRemovedSpy.count() == 1 || deletedRemovedSpy.wait(100)); // 100ms is less than duration of the animation
        QCOMPARE(effect->state().size(), 0);
    }
}

void ScriptedEffectsTest::testGrab()
{
    // this test verifies that scripted effects can grab windows that are
    // not already grabbed

    // load the test effect
    auto effect = new ScriptedEffectWithDebugSpy;
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effect->load(QStringLiteral("grabTest")));

    // create test window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);

    // the test effect should grab the test window successfully
    QCOMPARE(effectOutputSpy.count(), 1);
    QCOMPARE(effectOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(window->effectWindow()->data(WindowAddedGrabRole).value<void *>(), effect);
}

void ScriptedEffectsTest::testGrabAlreadyGrabbedWindow()
{
    // this test verifies that scripted effects cannot grab already grabbed
    // windows (unless force is set to true of course)

    // load effect that will hold the window grab
    auto owner = new ScriptedEffectWithDebugSpy;
    QSignalSpy ownerOutputSpy(owner, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(owner->load(QStringLiteral("grabAlreadyGrabbedWindowTest_owner")));

    // load effect that will try to grab already grabbed window
    auto grabber = new ScriptedEffectWithDebugSpy;
    QSignalSpy grabberOutputSpy(grabber, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(grabber->load(QStringLiteral("grabAlreadyGrabbedWindowTest_grabber")));

    // create test window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);

    // effect that initially held the grab should still hold the grab
    QCOMPARE(ownerOutputSpy.count(), 1);
    QCOMPARE(ownerOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(window->effectWindow()->data(WindowAddedGrabRole).value<void *>(), owner);

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
    QVERIFY(owner->load(QStringLiteral("grabAlreadyGrabbedWindowForcedTest_owner")));

    // load effect that will try to steal the window grab
    auto thief = new ScriptedEffectWithDebugSpy;
    QSignalSpy thiefOutputSpy(thief, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(thief->load(QStringLiteral("grabAlreadyGrabbedWindowForcedTest_thief")));

    // create test window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);

    // verify that the owner in fact held the grab
    QCOMPARE(ownerOutputSpy.count(), 1);
    QCOMPARE(ownerOutputSpy.first().first(), QStringLiteral("ok"));

    // effect that grabbed the test window forcefully should now hold the grab
    QCOMPARE(thiefOutputSpy.count(), 1);
    QCOMPARE(thiefOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(window->effectWindow()->data(WindowAddedGrabRole).value<void *>(), thief);
}

void ScriptedEffectsTest::testUngrab()
{
    // this test verifies that scripted effects can ungrab windows that they
    // are previously grabbed

    // load the test effect
    auto effect = new ScriptedEffectWithDebugSpy;
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);
    QVERIFY(effect->load(QStringLiteral("ungrabTest")));

    // create test window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);

    // the test effect should grab the test window successfully
    QCOMPARE(effectOutputSpy.count(), 1);
    QCOMPARE(effectOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(window->effectWindow()->data(WindowAddedGrabRole).value<void *>(), effect);

    // when the test effect sees that a window was minimized, it will try to ungrab it
    effectOutputSpy.clear();
    window->setMinimized(true);

    QCOMPARE(effectOutputSpy.count(), 1);
    QCOMPARE(effectOutputSpy.first().first(), QStringLiteral("ok"));
    QCOMPARE(window->effectWindow()->data(WindowAddedGrabRole).value<void *>(), nullptr);
}

void ScriptedEffectsTest::testRedirect_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("shouldTerminate");
    QTest::newRow("animate/DontTerminateAtSource") << "redirectAnimateDontTerminateTest" << false;
    QTest::newRow("animate/TerminateAtSource") << "redirectAnimateTerminateTest" << true;
    QTest::newRow("set/DontTerminate") << "redirectSetDontTerminateTest" << false;
    QTest::newRow("set/Terminate") << "redirectSetTerminateTest" << true;
}

void ScriptedEffectsTest::testRedirect()
{
    // this test verifies that redirect() works

    // load the test effect
    auto effect = new ScriptedEffectWithDebugSpy;
    QFETCH(QString, file);
    QVERIFY(effect->load(file));

    // create test window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);

    auto around = [](std::chrono::milliseconds elapsed,
                     std::chrono::milliseconds pivot,
                     std::chrono::milliseconds margin) {
        return std::abs(elapsed.count() - pivot.count()) < margin.count();
    };

    // initially, the test animation is at the source position

    {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 1);
        QVERIFY(state.contains(window->effectWindow()));
        const auto &animations = state.at(window->effectWindow()).first;
        QCOMPARE(animations.size(), 1);
        QCOMPARE(animations[0].timeLine.direction(), TimeLine::Forward);
        QVERIFY(around(animations[0].timeLine.elapsed(), 0ms, 50ms));
    }

    // minimize the test window after 250ms, when the test effect sees that
    // a window was minimized, it will try to reverse animation for it
    QTest::qWait(250);

    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);

    window->setMinimized(true);

    QCOMPARE(effectOutputSpy.count(), 1);
    QCOMPARE(effectOutputSpy.first().first(), QStringLiteral("ok"));

    {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 1);
        QVERIFY(state.contains(window->effectWindow()));
        const auto &animations = state.at(window->effectWindow()).first;
        QCOMPARE(animations.size(), 1);
        QCOMPARE(animations[0].timeLine.direction(), TimeLine::Backward);
        QVERIFY(around(animations[0].timeLine.elapsed(), 1000ms - 250ms, 50ms));
    }

    // wait for the animation to reach the start position, 100ms is an extra
    // safety margin
    QTest::qWait(250 + 100);

    QFETCH(bool, shouldTerminate);
    if (shouldTerminate) {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 0);
    } else {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 1);
        QVERIFY(state.contains(window->effectWindow()));
        const auto &animations = state.at(window->effectWindow()).first;
        QCOMPARE(animations.size(), 1);
        QCOMPARE(animations[0].timeLine.direction(), TimeLine::Backward);
        QCOMPARE(animations[0].timeLine.elapsed(), 1000ms);
        QCOMPARE(animations[0].timeLine.value(), 0.0);
    }
}

void ScriptedEffectsTest::testComplete()
{
    // this test verifies that complete works

    // load the test effect
    auto effect = new ScriptedEffectWithDebugSpy;
    QVERIFY(effect->load(QStringLiteral("completeTest")));

    // create test window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);

    auto around = [](std::chrono::milliseconds elapsed,
                     std::chrono::milliseconds pivot,
                     std::chrono::milliseconds margin) {
        return std::abs(elapsed.count() - pivot.count()) < margin.count();
    };

    // initially, the test animation should be at the start position
    {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 1);
        QVERIFY(state.contains(window->effectWindow()));
        const auto &animations = state.at(window->effectWindow()).first;
        QCOMPARE(animations.size(), 1);
        QVERIFY(around(animations[0].timeLine.elapsed(), 0ms, 50ms));
        QVERIFY(!animations[0].timeLine.done());
    }

    // wait for 250ms
    QTest::qWait(250);

    {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 1);
        QVERIFY(state.contains(window->effectWindow()));
        const auto &animations = state.at(window->effectWindow()).first;
        QCOMPARE(animations.size(), 1);
        QVERIFY(around(animations[0].timeLine.elapsed(), 250ms, 50ms));
        QVERIFY(!animations[0].timeLine.done());
    }

    // minimize the test window, when the test effect sees that a window was
    // minimized, it will try to complete animation for it
    QSignalSpy effectOutputSpy(effect, &ScriptedEffectWithDebugSpy::testOutput);

    window->setMinimized(true);

    QCOMPARE(effectOutputSpy.count(), 1);
    QCOMPARE(effectOutputSpy.first().first(), QStringLiteral("ok"));

    {
        const auto &state = effect->state();
        QCOMPARE(state.size(), 1);
        QVERIFY(state.contains(window->effectWindow()));
        const auto &animations = state.at(window->effectWindow()).first;
        QCOMPARE(animations.size(), 1);
        QCOMPARE(animations[0].timeLine.elapsed(), 1000ms);
        QVERIFY(animations[0].timeLine.done());
    }
}

WAYLANDTEST_MAIN(ScriptedEffectsTest)
#include "scripted_effects_test.moc"
