/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "kwin_wayland_test.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>

using namespace KWin;

namespace
{

class Slide : public QtWayland::org_kde_kwin_slide
{
public:
    explicit Slide(::org_kde_kwin_slide *object)
        : QtWayland::org_kde_kwin_slide(object)
    {
    }

    ~Slide() override
    {
        release();
    }
};

}

class SlidingPopupsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testWithOtherEffectWayland_data();
    void testWithOtherEffectWayland();
};

void SlidingPopupsTest::initTestCase()
{
    if (!Test::renderNodeAvailable()) {
        QSKIP("no render node available");
        return;
    }
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Effect *>();
    QVERIFY(waylandServer()->init(qAppName()));

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }
    KConfigGroup wobblyGroup = config->group(QStringLiteral("Effect-Wobbly"));
    wobblyGroup.writeEntry(QStringLiteral("Settings"), QStringLiteral("Custom"));
    wobblyGroup.writeEntry(QStringLiteral("OpenEffect"), true);
    wobblyGroup.writeEntry(QStringLiteral("CloseEffect"), true);

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    Test::setOutputConfig({
        Rect(0, 0, 1280, 1024),
        Rect(1280, 0, 1280, 1024),
    });
}

void SlidingPopupsTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Slide));
}

void SlidingPopupsTest::cleanup()
{
    Test::destroyWaylandConnection();
    while (!effects->loadedEffects().isEmpty()) {
        const QString effect = effects->loadedEffects().first();
        effects->unloadEffect(effect);
        QVERIFY(!effects->isEffectLoaded(effect));
    }
}

void SlidingPopupsTest::testWithOtherEffectWayland_data()
{
    QTest::addColumn<QStringList>("effectsToLoad");

    QTest::newRow("fade, slide") << QStringList{QStringLiteral("fade"), QStringLiteral("slidingpopups")};
    QTest::newRow("slide, fade") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("fade")};
    QTest::newRow("scale, slide") << QStringList{QStringLiteral("scale"), QStringLiteral("slidingpopups")};
    QTest::newRow("slide, scale") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("scale")};

    if (effects->compositingType() & KWin::OpenGLCompositing) {
        QTest::newRow("glide, slide") << QStringList{QStringLiteral("glide"), QStringLiteral("slidingpopups")};
        QTest::newRow("slide, glide") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("glide")};
        QTest::newRow("wobblywindows, slide") << QStringList{QStringLiteral("wobblywindows"), QStringLiteral("slidingpopups")};
        QTest::newRow("slide, wobblywindows") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("wobblywindows")};
        QTest::newRow("fallapart, slide") << QStringList{QStringLiteral("fallapart"), QStringLiteral("slidingpopups")};
        QTest::newRow("slide, fallapart") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("fallapart")};
    }
}

void SlidingPopupsTest::testWithOtherEffectWayland()
{
    // this test verifies that slidingpopups effect grabs the window added role
    // independently of the sequence how the effects are loaded.
    // see BUG 336866
    // the test is like testWithOtherEffect, but simulates using a Wayland window
    // find the effectsloader
    auto effectloader = effects->findChild<AbstractEffectLoader *>();
    QVERIFY(effectloader);
    QSignalSpy effectLoadedSpy(effectloader, &AbstractEffectLoader::effectLoaded);

    Effect *slidingPoupus = nullptr;
    Effect *otherEffect = nullptr;
    QFETCH(QStringList, effectsToLoad);
    for (const QString &effectName : effectsToLoad) {
        QVERIFY(!effects->isEffectLoaded(effectName));
        QVERIFY(effects->loadEffect(effectName));
        QVERIFY(effects->isEffectLoaded(effectName));

        QCOMPARE(effectLoadedSpy.count(), 1);
        Effect *effect = effectLoadedSpy.first().first().value<Effect *>();
        if (effectName == QStringLiteral("slidingpopups")) {
            slidingPoupus = effect;
        } else {
            otherEffect = effect;
        }
        effectLoadedSpy.clear();
    }
    QVERIFY(slidingPoupus);
    QVERIFY(otherEffect);

    QVERIFY(!slidingPoupus->isActive());
    QVERIFY(!otherEffect->isActive());
    QSignalSpy windowAddedSpy(effects, &EffectsHandler::windowAdded);

    QVERIFY(Test::slideManager());

    // create Wayland window
    auto surface = Test::createSurface();
    QVERIFY(surface);
    auto slide = std::make_unique<Slide>(Test::slideManager()->create(*surface));
    QVERIFY(slide->isInitialized());
    slide->set_location(ORG_KDE_KWIN_SLIDE_LOCATION_LEFT);
    slide->commit();
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface);
    QCOMPARE(windowAddedSpy.count(), 0);
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(10, 20), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isNormalWindow());

    // sliding popups should be active
    QCOMPARE(windowAddedSpy.count(), 1);
    QTRY_VERIFY(slidingPoupus->isActive());
    QVERIFY(!otherEffect->isActive());

    // wait till effect ends
    QTRY_VERIFY(!slidingPoupus->isActive());
    QTRY_VERIFY(!otherEffect->isActive());

    // and destroy the window again
    shellSurface.reset();
    surface.reset();

    QSignalSpy windowClosedSpy(window, &Window::closed);

    QSignalSpy windowDeletedSpy(effects, &EffectsHandler::windowDeleted);
    QVERIFY(windowClosedSpy.wait());

    // again we should have the sliding popups active
    QVERIFY(slidingPoupus->isActive());
    QVERIFY(!otherEffect->isActive());

    QVERIFY(windowDeletedSpy.wait());

    QCOMPARE(windowDeletedSpy.count(), 1);
    QVERIFY(!slidingPoupus->isActive());
    QVERIFY(!otherEffect->isActive());
}

WAYLANDTEST_MAIN(SlidingPopupsTest)
#include "slidingpopups_test.moc"
