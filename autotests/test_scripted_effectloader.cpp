/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../effectloader.h"
#include "mock_effectshandler.h"
#include "../scripting/scriptedeffect.h"
// for mocking
#include "../cursor.h"
#include "../input.h"
#include "../screenedge.h"
// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KPackage/PackageLoader>
// Qt
#include <QtTest>
#include <QStringList>
Q_DECLARE_METATYPE(KWin::LoadEffectFlag)
Q_DECLARE_METATYPE(KWin::LoadEffectFlags)
Q_DECLARE_METATYPE(KWin::Effect*)
Q_DECLARE_METATYPE(KSharedConfigPtr)

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core")

namespace KWin
{
ScreenEdges *ScreenEdges::s_self = nullptr;

void ScreenEdges::reserve(ElectricBorder, QObject *, const char *)
{
}

void ScreenEdges::reserveTouch(ElectricBorder, QAction *)
{
}

InputRedirection *InputRedirection::s_self = nullptr;

void InputRedirection::registerShortcut(const QKeySequence &, QAction *)
{
}

namespace MetaScripting
{
void registration(QScriptEngine *)
{
}
}

}

class TestScriptedEffectLoader : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testHasEffect_data();
    void testHasEffect();
    void testKnownEffects();
    void testLoadEffect_data();
    void testLoadEffect();
    void testLoadScriptedEffect_data();
    void testLoadScriptedEffect();
    void testLoadAllEffects();
    void testCancelLoadAllEffects();
};

void TestScriptedEffectLoader::initTestCase()
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    QCoreApplication::instance()->setProperty("config", QVariant::fromValue(config));

    KWin::Cursors::self()->setMouse(new KWin::Cursor(this));
}

void TestScriptedEffectLoader::testHasEffect_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");

    // all the built-in effects should fail
    QTest::newRow("blur")                        << QStringLiteral("blur")                      << false;
    QTest::newRow("Colorpicker")                 << QStringLiteral("colorpicker")               << false;
    QTest::newRow("Contrast")                    << QStringLiteral("contrast")                  << false;
    QTest::newRow("CoverSwitch")                 << QStringLiteral("coverswitch")               << false;
    QTest::newRow("Cube")                        << QStringLiteral("cube")                      << false;
    QTest::newRow("CubeSlide")                   << QStringLiteral("cubeslide")                 << false;
    QTest::newRow("DesktopGrid")                 << QStringLiteral("desktopgrid")               << false;
    QTest::newRow("DimInactive")                 << QStringLiteral("diminactive")               << false;
    QTest::newRow("FallApart")                   << QStringLiteral("fallapart")                 << false;
    QTest::newRow("FlipSwitch")                  << QStringLiteral("flipswitch")                << false;
    QTest::newRow("Glide")                       << QStringLiteral("glide")                     << false;
    QTest::newRow("HighlightWindow")             << QStringLiteral("highlightwindow")           << false;
    QTest::newRow("Invert")                      << QStringLiteral("invert")                    << false;
    QTest::newRow("Kscreen")                     << QStringLiteral("kscreen")                   << false;
    QTest::newRow("Logout")                      << QStringLiteral("logout")                    << false;
    QTest::newRow("LookingGlass")                << QStringLiteral("lookingglass")              << false;
    QTest::newRow("MagicLamp")                   << QStringLiteral("magiclamp")                 << false;
    QTest::newRow("Magnifier")                   << QStringLiteral("magnifier")                 << false;
    QTest::newRow("MouseClick")                  << QStringLiteral("mouseclick")                << false;
    QTest::newRow("MouseMark")                   << QStringLiteral("mousemark")                 << false;
    QTest::newRow("PresentWindows")              << QStringLiteral("presentwindows")            << false;
    QTest::newRow("Resize")                      << QStringLiteral("resize")                    << false;
    QTest::newRow("ScreenEdge")                  << QStringLiteral("screenedge")                << false;
    QTest::newRow("ScreenShot")                  << QStringLiteral("screenshot")                << false;
    QTest::newRow("Sheet")                       << QStringLiteral("sheet")                     << false;
    QTest::newRow("ShowFps")                     << QStringLiteral("showfps")                   << false;
    QTest::newRow("ShowPaint")                   << QStringLiteral("showpaint")                 << false;
    QTest::newRow("Slide")                       << QStringLiteral("slide")                     << false;
    QTest::newRow("SlideBack")                   << QStringLiteral("slideback")                 << false;
    QTest::newRow("SlidingPopups")               << QStringLiteral("slidingpopups")             << false;
    QTest::newRow("SnapHelper")                  << QStringLiteral("snaphelper")                << false;
    QTest::newRow("StartupFeedback")             << QStringLiteral("startupfeedback")           << false;
    QTest::newRow("ThumbnailAside")              << QStringLiteral("thumbnailaside")            << false;
    QTest::newRow("TrackMouse")                  << QStringLiteral("trackmouse")                << false;
    QTest::newRow("WindowGeometry")              << QStringLiteral("windowgeometry")            << false;
    QTest::newRow("WobblyWindows")               << QStringLiteral("wobblywindows")             << false;
    QTest::newRow("Zoom")                        << QStringLiteral("zoom")                      << false;
    QTest::newRow("Non Existing")                << QStringLiteral("InvalidName")               << false;
    QTest::newRow("Fade - without kwin4_effect") << QStringLiteral("fade")                      << false;

    QTest::newRow("DialogParent")                << QStringLiteral("kwin4_effect_dialogparent")     << true;
    QTest::newRow("DimScreen")                   << QStringLiteral("kwin4_effect_dimscreen")        << true;
    QTest::newRow("EyeOnScreen")                 << QStringLiteral("kwin4_effect_eyeonscreen")      << true;
    QTest::newRow("Fade + kwin4_effect")         << QStringLiteral("kwin4_effect_fade")             << true;
    QTest::newRow("Fade + kwin4_effect + CS")    << QStringLiteral("kwin4_eFfect_fAde")             << true;
    QTest::newRow("FadeDesktop")                 << QStringLiteral("kwin4_effect_fadedesktop")      << true;
    QTest::newRow("FadingPopups")                << QStringLiteral("kwin4_effect_fadingpopups")     << true;
    QTest::newRow("FrozenApp")                   << QStringLiteral("kwin4_effect_frozenapp")        << true;
    QTest::newRow("FullScreen")                  << QStringLiteral("kwin4_effect_fullscreen")       << true;
    QTest::newRow("Login")                       << QStringLiteral("kwin4_effect_login")            << true;
    QTest::newRow("Logout")                      << QStringLiteral("kwin4_effect_logout")           << true;
    QTest::newRow("Maximize")                    << QStringLiteral("kwin4_effect_maximize")         << true;
    QTest::newRow("MorphingPopups")              << QStringLiteral("kwin4_effect_morphingpopups")   << true;
    QTest::newRow("Scale")                       << QStringLiteral("kwin4_effect_scale")            << true;
    QTest::newRow("Squash")                      << QStringLiteral("kwin4_effect_squash")           << true;
    QTest::newRow("Translucency")                << QStringLiteral("kwin4_effect_translucency")     << true;
    QTest::newRow("WindowAperture")              << QStringLiteral("kwin4_effect_windowaperture")   << true;
}

void TestScriptedEffectLoader::testHasEffect()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);

    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater> mockHandler(new MockEffectsHandler(KWin::XRenderCompositing));
    KWin::ScriptedEffectLoader loader;
    QCOMPARE(loader.hasEffect(name), expected);

    // each available effect should also be supported
    QCOMPARE(loader.isEffectSupported(name), expected);

    if (expected) {
        mockHandler->setAnimationsSupported(false);
        QVERIFY(!loader.isEffectSupported(name));
    }
}

void TestScriptedEffectLoader::testKnownEffects()
{
    QStringList expectedEffects;
    expectedEffects << QStringLiteral("kwin4_effect_dialogparent")
                    << QStringLiteral("kwin4_effect_dimscreen")
                    << QStringLiteral("kwin4_effect_eyeonscreen")
                    << QStringLiteral("kwin4_effect_fade")
                    << QStringLiteral("kwin4_effect_fadedesktop")
                    << QStringLiteral("kwin4_effect_fadingpopups")
                    << QStringLiteral("kwin4_effect_frozenapp")
                    << QStringLiteral("kwin4_effect_login")
                    << QStringLiteral("kwin4_effect_logout")
                    << QStringLiteral("kwin4_effect_maximize")
                    << QStringLiteral("kwin4_effect_morphingpopups")
                    << QStringLiteral("kwin4_effect_scale")
                    << QStringLiteral("kwin4_effect_sessionquit")
                    << QStringLiteral("kwin4_effect_squash")
                    << QStringLiteral("kwin4_effect_translucency")
                    << QStringLiteral("kwin4_effect_windowaperture");

    KWin::ScriptedEffectLoader loader;
    QStringList result = loader.listOfKnownEffects();
    // at least as many effects as we expect - system running the test could have more effects
    QVERIFY(result.size() >= expectedEffects.size());
    for (const QString &effect : expectedEffects) {
        QVERIFY(result.contains(effect));
    }
}

void TestScriptedEffectLoader::testLoadEffect_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");

    QTest::newRow("Non Existing")                << QStringLiteral("InvalidName")                   << false;
    QTest::newRow("Fade - without kwin4_effect") << QStringLiteral("fade")                          << false;
    QTest::newRow("DialogParent")                << QStringLiteral("kwin4_effect_dialogparent")     << true;
    QTest::newRow("DimScreen")                   << QStringLiteral("kwin4_effect_dimscreen")        << true;
    QTest::newRow("EyeOnScreen")                 << QStringLiteral("kwin4_effect_eyeonscreen")      << true;
    QTest::newRow("Fade + kwin4_effect")         << QStringLiteral("kwin4_effect_fade")             << true;
    QTest::newRow("Fade + kwin4_effect + CS")    << QStringLiteral("kwin4_eFfect_fAde")             << true;
    QTest::newRow("FadeDesktop")                 << QStringLiteral("kwin4_effect_fadedesktop")      << true;
    QTest::newRow("FadingPopups")                << QStringLiteral("kwin4_effect_fadingpopups")     << true;
    QTest::newRow("FrozenApp")                   << QStringLiteral("kwin4_effect_frozenapp")        << true;
    QTest::newRow("FullScreen")                  << QStringLiteral("kwin4_effect_fullscreen")       << true;
    QTest::newRow("Login")                       << QStringLiteral("kwin4_effect_login")            << true;
    QTest::newRow("Logout")                      << QStringLiteral("kwin4_effect_logout")           << true;
    QTest::newRow("Maximize")                    << QStringLiteral("kwin4_effect_maximize")         << true;
    QTest::newRow("MorphingPopups")              << QStringLiteral("kwin4_effect_morphingpopups")   << true;
    QTest::newRow("Scale")                       << QStringLiteral("kwin4_effect_scale")            << true;
    QTest::newRow("SessionQuit")              << QStringLiteral("kwin4_effect_sessionquit")   << true;
    QTest::newRow("Squash")                      << QStringLiteral("kwin4_effect_squash")           << true;
    QTest::newRow("Translucency")                << QStringLiteral("kwin4_effect_translucency")     << true;
    QTest::newRow("WindowAperture")              << QStringLiteral("kwin4_effect_windowaperture")   << true;

}

void TestScriptedEffectLoader::testLoadEffect()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);

    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater> mockHandler(new MockEffectsHandler(KWin::XRenderCompositing));
    KWin::ScriptedEffectLoader loader;
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    loader.setConfig(config);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::ScriptedEffectLoader::effectLoaded);
    // connect to signal to ensure that we delete the Effect again as the Effect doesn't have a parent
    connect(&loader, &KWin::ScriptedEffectLoader::effectLoaded,
        [&name](KWin::Effect *effect, const QString &effectName) {
            QCOMPARE(effectName, name.toLower());
            effect->deleteLater();
        }
    );
    // try to load the Effect
    QCOMPARE(loader.loadEffect(name), expected);
    // loading again should fail
    QVERIFY(!loader.loadEffect(name));

    // signal spy should have got the signal if it was expected
    QCOMPARE(spy.isEmpty(), !expected);
    if (!spy.isEmpty()) {
        QCOMPARE(spy.count(), 1);
        // if we caught a signal it should have the effect name we passed in
        QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.count(), 2);
        QCOMPARE(arguments.at(1).toString(), name.toLower());
    }
    spy.clear();
    QVERIFY(spy.isEmpty());

    // now if we wait for the events being processed, the effect will get deleted and it should load again
    QTest::qWait(1);
    QCOMPARE(loader.loadEffect(name), expected);
    // signal spy should have got the signal if it was expected
    QCOMPARE(spy.isEmpty(), !expected);
    if (!spy.isEmpty()) {
        QCOMPARE(spy.count(), 1);
        // if we caught a signal it should have the effect name we passed in
        QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.count(), 2);
        QCOMPARE(arguments.at(1).toString(), name.toLower());
    }
}

void TestScriptedEffectLoader::testLoadScriptedEffect_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<KWin::LoadEffectFlags>("loadFlags");

    const KWin::LoadEffectFlags checkDefault = KWin::LoadEffectFlag::Load | KWin::LoadEffectFlag::CheckDefaultFunction;
    const KWin::LoadEffectFlags forceFlags = KWin::LoadEffectFlag::Load;
    const KWin::LoadEffectFlags dontLoadFlags = KWin::LoadEffectFlags();

    // enabled by default
    QTest::newRow("Fade")                 << QStringLiteral("kwin4_effect_fade")        << true  << checkDefault;
    // not enabled by default
    QTest::newRow("EyeOnScreen")          << QStringLiteral("kwin4_effect_eyeonscreen") << true  << checkDefault;
    // Force an Effect which will load
    QTest::newRow("EyeOnScreen-Force")    << QStringLiteral("kwin4_effect_eyeonscreen") << true  << forceFlags;
    // Enforce no load of effect which is enabled by default
    QTest::newRow("Fade-DontLoad")        << QStringLiteral("kwin4_effect_fade")        << false << dontLoadFlags;
    // Enforce no load of effect which is not enabled by default, but enforced
    QTest::newRow("EyeOnScreen-DontLoad") << QStringLiteral("kwin4_effect_eyeonscreen") << false << dontLoadFlags;
}

void TestScriptedEffectLoader::testLoadScriptedEffect()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);
    QFETCH(KWin::LoadEffectFlags, loadFlags);

    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater> mockHandler(new MockEffectsHandler(KWin::XRenderCompositing));
    KWin::ScriptedEffectLoader loader;
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    loader.setConfig(config);

    const auto services = KPackage::PackageLoader::self()->findPackages(QStringLiteral("KWin/Effect"), QStringLiteral("kwin/effects"),
        [name] (const KPluginMetaData &metadata) {
            return metadata.pluginId().compare(name, Qt::CaseInsensitive) == 0;
        }
    );
    QCOMPARE(services.count(), 1);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::ScriptedEffectLoader::effectLoaded);
    // connect to signal to ensure that we delete the Effect again as the Effect doesn't have a parent
    connect(&loader, &KWin::ScriptedEffectLoader::effectLoaded,
        [&name](KWin::Effect *effect, const QString &effectName) {
            QCOMPARE(effectName, name.toLower());
            effect->deleteLater();
        }
    );
    // try to load the Effect
    QCOMPARE(loader.loadEffect(services.first(), loadFlags), expected);
    // loading again should fail
    QVERIFY(!loader.loadEffect(services.first(), loadFlags));

    // signal spy should have got the signal if it was expected
    QCOMPARE(spy.isEmpty(), !expected);
    if (!spy.isEmpty()) {
        QCOMPARE(spy.count(), 1);
        // if we caught a signal it should have the effect name we passed in
        QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.count(), 2);
        QCOMPARE(arguments.at(1).toString(), name.toLower());
    }
    spy.clear();
    QVERIFY(spy.isEmpty());

    // now if we wait for the events being processed, the effect will get deleted and it should load again
    QTest::qWait(1);
    QCOMPARE(loader.loadEffect(services.first(), loadFlags), expected);
    // signal spy should have got the signal if it was expected
    QCOMPARE(spy.isEmpty(), !expected);
    if (!spy.isEmpty()) {
        QCOMPARE(spy.count(), 1);
        // if we caught a signal it should have the effect name we passed in
        QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.count(), 2);
        QCOMPARE(arguments.at(1).toString(), name.toLower());
    }
}

void TestScriptedEffectLoader::testLoadAllEffects()
{
    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater> mockHandler(new MockEffectsHandler(KWin::XRenderCompositing));
    KWin::ScriptedEffectLoader loader;

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);

    const QString kwin4 = QStringLiteral("kwin4_effect_");

    // prepare the configuration to hard enable/disable the effects we want to load
    KConfigGroup plugins = config->group("Plugins");
    plugins.writeEntry(kwin4 + QStringLiteral("dialogparentEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("dimscreenEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("fadeEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("fadedesktopEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("fadingpopupsEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("frozenappEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("fullscreenEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("loginEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("logoutEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("maximizeEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("scaleEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("squashEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("translucencyEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("eyeonscreenEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("windowapertureEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("morphingpopupsEnabled"), false);
    plugins.writeEntry(kwin4 + QStringLiteral("sessionquitEnabled"), false);

    plugins.sync();

    loader.setConfig(config);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::ScriptedEffectLoader::effectLoaded);
    // connect to signal to ensure that we delete the Effect again as the Effect doesn't have a parent
    connect(&loader, &KWin::ScriptedEffectLoader::effectLoaded,
        [](KWin::Effect *effect) {
            effect->deleteLater();
        }
    );

    // the config is prepared so that no Effect gets loaded!
    loader.queryAndLoadAll();

    // we need to wait some time because it's queued and in a thread
    QVERIFY(!spy.wait(100));

    // now let's prepare a config which has one effect explicitly enabled
    plugins.writeEntry(kwin4 + QStringLiteral("eyeonscreenEnabled"), true);
    plugins.sync();

    loader.queryAndLoadAll();
    // should load one effect in first go
    QVERIFY(spy.wait(100));
    // and afterwards it should not load another one
    QVERIFY(!spy.wait(10));

    QCOMPARE(spy.size(), 1);
    // if we caught a signal it should have the effect name we passed in
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.count(), 2);
    QCOMPARE(arguments.at(1).toString(), kwin4 + QStringLiteral("eyeonscreen"));
    spy.clear();

    // let's delete one of the default entries
    plugins.deleteEntry(kwin4 + QStringLiteral("fadeEnabled"));
    plugins.sync();

    QVERIFY(spy.isEmpty());
    loader.queryAndLoadAll();

    // let's use qWait as we need to wait for two signals to be emitted
    QTRY_COMPARE(spy.size(), 2);
    QStringList loadedEffects;
    for (auto &list : spy) {
        QCOMPARE(list.size(), 2);
        loadedEffects << list.at(1).toString();
    }
    std::sort(loadedEffects.begin(), loadedEffects.end());
    QCOMPARE(loadedEffects.at(0), kwin4 + QStringLiteral("eyeonscreen"));
    QCOMPARE(loadedEffects.at(1), kwin4 + QStringLiteral("fade"));
}

void TestScriptedEffectLoader::testCancelLoadAllEffects()
{
    // this test verifies that no test gets loaded when the loader gets cleared
    MockEffectsHandler mockHandler(KWin::XRenderCompositing);
    KWin::ScriptedEffectLoader loader;

    // prepare the configuration to hard enable/disable the effects we want to load
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    const QString kwin4 = QStringLiteral("kwin4_effect_");
    KConfigGroup plugins = config->group("Plugins");
    plugins.writeEntry(kwin4 + QStringLiteral("eyeonscreenEnabled"), true);
    plugins.sync();

    loader.setConfig(config);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::ScriptedEffectLoader::effectLoaded);
    QVERIFY(spy.isValid());

    loader.queryAndLoadAll();
    loader.clear();

    // Should not load any effect
    QVERIFY(!spy.wait(100));
    QVERIFY(spy.isEmpty());
}

QTEST_MAIN(TestScriptedEffectLoader)
#include "test_scripted_effectloader.moc"
