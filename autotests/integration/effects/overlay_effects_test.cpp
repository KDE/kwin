/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "kwin_wayland_test.h"
#include "scene/item.h"
#include "scene/workspacescene.h"
#include "wayland_server.h"

#include <KConfigGroup>

#include <linux/input.h>

using namespace KWin;

class OverlayEffectsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();

    void testTouchPointsRings();
    void testMouseClickRings();
    void testMouseMarkSegments();

private:
    Effect *loadEffect(const QString &name);
};

static int descendantCount(Item *item)
{
    int count = 0;
    const auto children = item->childItems();
    for (Item *child : children) {
        count += 1 + descendantCount(child);
    }
    return count;
}

static int overlayCount()
{
    return descendantCount(effects->scene()->overlayItem());
}

static int visibleCount(Item *item)
{
    int count = 0;
    const auto children = item->childItems();
    for (Item *child : children) {
        if (child->isVisible()) {
            count += 1 + visibleCount(child);
        }
    }
    return count;
}

void OverlayEffectsTest::initTestCase()
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());
    qRegisterMetaType<KWin::Effect *>();
    QVERIFY(waylandServer()->init(qAppName()));

    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (const QString &name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }
    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    Test::setOutputConfig({
        Rect(0, 0, 1280, 1024),
    });
}

void OverlayEffectsTest::cleanup()
{
    while (!effects->loadedEffects().isEmpty()) {
        const QString effect = effects->loadedEffects().first();
        effects->unloadEffect(effect);
        QVERIFY(!effects->isEffectLoaded(effect));
    }
}

Effect *OverlayEffectsTest::loadEffect(const QString &name)
{
    auto effectLoader = effects->findChild<AbstractEffectLoader *>();
    if (!effectLoader) {
        return nullptr;
    }
    QSignalSpy effectLoadedSpy(effectLoader, &AbstractEffectLoader::effectLoaded);
    if (!effects->loadEffect(name) || effectLoadedSpy.isEmpty()) {
        return nullptr;
    }
    return effectLoadedSpy.first().first().value<Effect *>();
}

void OverlayEffectsTest::testTouchPointsRings()
{
    // A touch shows rings over the screen, which go away once their animation ends
    Effect *effect = loadEffect(QStringLiteral("touchpoints"));
    QVERIFY(effect);
    const int before = overlayCount();

    quint32 timestamp = 1;
    const int visibleBefore = visibleCount(effects->scene()->overlayItem());
    Test::touchDown(0, QPointF(100, 100), timestamp++);
    QVERIFY(effect->isActive());
    QCOMPARE_GT(overlayCount(), before);
    // The rings of a press start with a radius of 0, and grow while the finger is still down
    QTRY_VERIFY(visibleCount(effects->scene()->overlayItem()) > visibleBefore);

    Test::touchUp(0, timestamp++);
    QTRY_VERIFY_WITH_TIMEOUT(!effect->isActive(), 3000);
    // The ring items stay hidden in the scene, for the next touch
    QCOMPARE(visibleCount(effects->scene()->overlayItem()), visibleBefore);
}

void OverlayEffectsTest::testMouseClickRings()
{
    // A click shows rings and a label over the screen, which go away once their animation ends
    Effect *effect = loadEffect(QStringLiteral("mouseclick"));
    QVERIFY(effect);
    QVERIFY(QMetaObject::invokeMethod(effect, "toggleEnabled"));
    const int before = overlayCount();

    quint32 timestamp = 1;
    Test::pointerMotion(QPointF(200, 200), timestamp++);
    const int visibleBefore = visibleCount(effects->scene()->overlayItem());
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(effect->isActive());
    QCOMPARE_GT(overlayCount(), before);
    // The rings of a press start with a radius of 0, and grow while the button is still down
    QTRY_VERIFY(visibleCount(effects->scene()->overlayItem()) > visibleBefore);

    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QTRY_VERIFY_WITH_TIMEOUT(!effect->isActive(), 3000);
    // The label view stays in the scene until the deferred delete of its frame
    QTRY_COMPARE_WITH_TIMEOUT(overlayCount(), before, 3000);
}

void OverlayEffectsTest::testMouseMarkSegments()
{
    // Moving the pointer with the drawing modifiers held leaves a mark, and clearing removes it
    Effect *effect = loadEffect(QStringLiteral("mousemark"));
    QVERIFY(effect);
    const int before = overlayCount();

    quint32 timestamp = 1;
    Test::pointerMotion(QPointF(300, 300), timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    Test::pointerMotion(QPointF(310, 300), timestamp++);
    Test::pointerMotion(QPointF(320, 305), timestamp++);
    Test::pointerMotion(QPointF(330, 310), timestamp++);
    Test::pointerMotion(QPointF(500, 310), timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);

    QVERIFY(effect->isActive());
    // The drawing starts where the modifiers were pressed. The three short segments share one
    // image, and the long one is a line of its own.
    QCOMPARE(overlayCount(), before + 3);

    QVERIFY(QMetaObject::invokeMethod(effect, "clear"));
    QVERIFY(!effect->isActive());
    QCOMPARE(overlayCount(), before);
}

WAYLANDTEST_MAIN(OverlayEffectsTest)
#include "overlay_effects_test.moc"
