/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Eric Edlund <ericedlund2017@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "contexts.h"
#include "output.h"
#include "platform.h"
#include "wayland_server.h"
#include "workspace.h"

#include <memory>

#include <kwineffects.h>

#include "globalshortcuts.h"
#include <effects.h>

#define WAIT QTest::qWait

namespace KWin
{

/*
* NOTE: Once contexts can be read from configuration files, this testing
* will be substantially more useful. Things to test in future:
* - Test activation and switching with gestures in different configurations
* - Dynamically Created contexts can be exited intuitively
*/

static const QString s_socketName = QStringLiteral("contexts_test-0");

class ContextsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void registerContext();
    void only1ContextActiveAtOnce();
    void contextTransitionReversible();
    void contextSwitchingResistentToButtonSpamming();
    void contextSwitchSuppliesActivationParameters();
    void noCrashWhenActiveContextDeleted();
    void canHandleReconfigureAndEffectReloads();
    void forceCompleteTransitions();

    void resetCounters()
    {
        enterCounter1 = 0;
        leaveCounter1 = 0;
        enterCounter2 = 0;
        leaveCounter2 = 0;
        enterCounter3 = 0;
        leaveCounter3 = 0;

        activationFactor1 = 0;
        activationFactor2 = 0;
        activationFactor3 = 0;
    }

private:
    std::unique_ptr<EffectContext> context1;
    std::unique_ptr<EffectContext> context2;
    std::unique_ptr<EffectContext> context3;
    bool active1 = false;
    bool active2 = false;
    bool active3 = false;

    uint enterCounter1 = 0;
    uint leaveCounter1 = 0;
    uint enterCounter2 = 0;
    uint leaveCounter2 = 0;
    uint enterCounter3 = 0;
    uint leaveCounter3 = 0;

    qreal activationFactor1 = 0;
    qreal activationFactor2 = 0;
    qreal activationFactor3 = 0;

    uint transitionDuration = CONTEXT_SWITCH_ANIMATION_DURATION + 30; // Add 30 ms for safety
};

void ContextsTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(600, 400));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    Test::initWaylandWorkspace();
}

void ContextsTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));

    context1.reset(new EffectContext("Test Label 1", "Contexts Integration Test Name 1"));
    connect(context1.get(), &EffectContext::activated, [this]() {
        active1 = true;
        enterCounter1++;
        activationFactor1 = 1;
    });
    connect(context1.get(), &EffectContext::deactivated, [this]() {
        active1 = false;
        leaveCounter1++;
        activationFactor1 = 0;
    });
    connect(context1.get(), &EffectContext::activating, [this](qreal progress) {
        activationFactor1 = progress;
    });
    connect(context1.get(), &EffectContext::deactivating, [this](qreal p) {
        qreal progress = std::clamp(1 - p, 0.0, 1.0);
        activationFactor1 = progress;
    });
    context2.reset(new EffectContext("Test Activity Label", "Contexts Integration Test Name 2"));
    connect(context2.get(), &EffectContext::activated, [this]() {
        active2 = true;
        enterCounter2++;
        activationFactor2 = 1;
    });
    connect(context2.get(), &EffectContext::deactivated, [this]() {
        active2 = false;
        leaveCounter2++;
        activationFactor2 = 0;
    });
    connect(context2.get(), &EffectContext::activating, [this](qreal progress) {
        activationFactor2 = progress;
    });
    connect(context2.get(), &EffectContext::deactivating, [this](qreal p) {
        qreal progress = std::clamp(1 - p, 0.0, 1.0);
        activationFactor2 = progress;
    });
    context3.reset(new EffectContext("Test Activity Label", "Contexts Integration Test Name 3"));
    connect(context3.get(), &EffectContext::activated, [this]() {
        active3 = true;
        enterCounter3++;
        activationFactor3 = 1;
    });
    connect(context3.get(), &EffectContext::deactivated, [this]() {
        active3 = false;
        leaveCounter3++;
        activationFactor3 = 0;
    });
    connect(context3.get(), &EffectContext::activating, [this](qreal progress) {
        activationFactor1 = progress;
    });
    connect(context3.get(), &EffectContext::deactivating, [this](qreal p) {
        qreal progress = std::clamp(1 - p, 0.0, 1.0);
        activationFactor3 = progress;
    });
}

void ContextsTest::cleanup()
{
    Test::destroyWaylandConnection();
    context1.reset();
    context2.reset();
    context3.reset();

    resetCounters();
}

void ContextsTest::registerContext()
{
    // Reregistering shouldn't do anything
    effects->registerEffectContext(context1.get());
    effects->registerEffectContext(context1.get());
    effects->registerEffectContext(context1.get());

    QCOMPARE(enterCounter1, 0);
    QCOMPARE(leaveCounter1, 0);
}

void ContextsTest::only1ContextActiveAtOnce()
{
    // Deactivation should always happen before activation;
    // There should only be one context active at once
    effects->registerEffectContext(context2.get());
    effects->setCurrentContext(DEFAULT_CONTEXT);
    connect(context1.get(), &EffectContext::activated, [this]() {
        QVERIFY(!active2);
    });
    connect(context2.get(), &EffectContext::activated, [this]() {
        QVERIFY(!active1);
    });

    QTRY_VERIFY_WITH_TIMEOUT(context1->state() == EffectContext::State::Inactive
                                 && context2->state() == EffectContext::State::Inactive,
                             transitionDuration);
    effects->setCurrentContext(context1->name);
    WAIT(transitionDuration);
    QVERIFY(context1->state() == EffectContext::State::Active);
    QVERIFY(context2->state() == EffectContext::State::Inactive);
    effects->setCurrentContext(context2->name);
    QTRY_VERIFY_WITH_TIMEOUT(!active1 && active2, transitionDuration);
    context1->grabActive();
    WAIT(transitionDuration);
    QTRY_VERIFY_WITH_TIMEOUT(active1 && !active2, transitionDuration);
    context1->ungrabActive();
    QTRY_VERIFY_WITH_TIMEOUT(!active1, transitionDuration);
}

void ContextsTest::contextTransitionReversible()
{
    context1->grabActive();
    QTRY_VERIFY_WITH_TIMEOUT(context1->state() == EffectContext::State::Active, transitionDuration);
    resetCounters();
    context2->grabActive();

    // Get 75% through the transition
    WAIT(transitionDuration * 0.75);
    QVERIFY(context1->state() == EffectContext::State::Deactivating);
    QVERIFY(context2->state() == EffectContext::State::Activating);

    context1->grabActive(); // Reverse direction
    WAIT(30); // Let a few updates go through

    QCOMPARE(enterCounter1, 0); // No switch should have been completed yet
    QCOMPARE(leaveCounter1, 0);
    QCOMPARE(enterCounter2, 0);
    QCOMPARE(leaveCounter2, 0);

    WAIT(transitionDuration * 0.75 + 50); // Wait 75% of the animation duration; enough time to finish deactivating
    QCOMPARE(enterCounter1, 1);
    QCOMPARE(leaveCounter1, 0);
    QCOMPARE(enterCounter2, 0);
    QCOMPARE(leaveCounter2, 1); // activity2 should have been deactivated by now
    QVERIFY(context1->state() == EffectContext::State::Active);
    QVERIFY(context2->state() == EffectContext::State::Inactive);
}

void ContextsTest::contextSwitchingResistentToButtonSpamming()
{
    context1->grabActive();
    context1->grabActive();
    WAIT(10);
    context1->ungrabActive();
    context1->grabActive();
    context1->ungrabActive();
    WAIT(10);
    context1->grabActive();
    WAIT(transitionDuration);
    context1->grabActive();
    context2->grabActive();
    context1->grabActive();
    context1->ungrabActive();
    context2->grabActive();
    context3->grabActive();
    context2->grabActive();
    context3->ungrabActive();
    WAIT(transitionDuration);
}

void ContextsTest::contextSwitchSuppliesActivationParameters()
{
    QSignalSpy paramsSpy = QSignalSpy(context1.get(), &EffectContext::setActivationParameters);
    QVERIFY(paramsSpy.isValid());

    QHash<QString, QVariant> recievedParams;
    std::vector<Parameter> requiredParams;
    connect(context1.get(), &EffectContext::setActivationParameters, [&recievedParams, &requiredParams](const QHash<QString, QVariant> &params) {
        recievedParams = params;
        QCOMPARE(recievedParams.size(), requiredParams.size());
        // Verify all parameters supplied are recieved
        for (Parameter p : qAsConst(requiredParams)) {
            QVERIFY(recievedParams.contains(p.name));
            QVERIFY(p.possibleValues.values().contains(recievedParams[p.name]));
            QCOMPARE(p.defaultValue, recievedParams[p.name]);
        }
    });

    context1->grabActive();
    WAIT(50);
    QCOMPARE(paramsSpy.count(), 1);

    context1->ungrabActive();
    WAIT(60); // Get back to default context
    QVERIFY(context1->state() == EffectContext::State::Inactive);

    Parameter p1 = Parameter{"Test Label", "paramName", QMetaType::QString, "opt1"};
    p1.possibleValues["Label Option 1"] = "opt1";
    p1.possibleValues["Label Option 2"] = "opt2";
    p1.possibleValues["Label Option 3"] = "opt3";

    context1->addActivationParameter(p1);
    requiredParams.push_back(std::move(p1));
    context1->grabActive();
    WAIT(50);
    QCOMPARE(paramsSpy.count(), 2);
}

void ContextsTest::noCrashWhenActiveContextDeleted()
{
    QString name;

    // Delete an active context
    context1->grabActive();
    name = context1->name;
    WAIT(transitionDuration);
    QVERIFY(context1->state() == EffectContext::State::Active);
    QVERIFY(effects->getCurrentContext() == context1->name);
    context1.reset();
    WAIT(transitionDuration);
    QVERIFY(effects->getCurrentContext() != name);

    // Delete an activating context
    context2->grabActive();
    name = context2->name;
    WAIT(0.5 * transitionDuration);
    QVERIFY(context2->state() == EffectContext::State::Activating);
    context2.reset();
    WAIT(transitionDuration);
    QVERIFY(effects->getCurrentContext() != name);

    // Delete a deactivating context
    context3->grabActive();
    name = context3->name;
    WAIT(transitionDuration);
    context3->ungrabActive();
    WAIT(0.5 * transitionDuration);
    QVERIFY(context3->state() == EffectContext::State::Deactivating);
    context3.reset();
    WAIT(transitionDuration);
    QVERIFY(effects->getCurrentContext() != name);

    // Switch to nonexistant context for good measure
    effects->setCurrentContext(name);
    effects->setCurrentContext(name);
}

void ContextsTest::canHandleReconfigureAndEffectReloads()
{
    // Disabling effects doesn't crash
    static_cast<EffectsHandlerImpl *>(effects)->unloadAllEffects();
    static_cast<EffectsHandlerImpl *>(effects)->reconfigure();
    static_cast<EffectsHandlerImpl *>(effects)->unloadAllEffects();
    static_cast<EffectsHandlerImpl *>(effects)->reconfigure();

    workspace()->slotReconfigure();
    workspace()->slotReconfigure();
    workspace()->slotReconfigure();
}

void ContextsTest::forceCompleteTransitions()
{
    context1->grabActive();
    WAIT(transitionDuration);
    QVERIFY(context1->state() == EffectContext::State::Active);
    connect(context1.get(), &EffectContext::activating, []() {
        QVERIFY(false);
    });
    // Force complete
    context2->grabActive(true);
    WAIT(transitionDuration / 2);
    context1->grabActive();
    WAIT(transitionDuration / 2);
    QVERIFY(context2->state() == EffectContext::State::Active);
    QVERIFY(context1->state() == EffectContext::State::Inactive);

    // Force complete
    context2->ungrabActive(true);
    WAIT(transitionDuration / 2);
    context2->grabActive();
    WAIT(transitionDuration / 2);
    QVERIFY(context2->state() == EffectContext::State::Inactive);
}
}

WAYLANDTEST_MAIN(KWin::ContextsTest)
#include "contexts_test.moc"
