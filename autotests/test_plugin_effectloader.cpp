/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../effectloader.h"
#include "mock_effectshandler.h"
#include "../scripting/scriptedeffect.h" // for mocking ScriptedEffect::create
// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KPluginLoader>
// Qt
#include <QtTest>
#include <QStringList>
Q_DECLARE_METATYPE(KWin::CompositingType)
Q_DECLARE_METATYPE(KWin::LoadEffectFlag)
Q_DECLARE_METATYPE(KWin::LoadEffectFlags)
Q_DECLARE_METATYPE(KWin::Effect*)

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core")

namespace KWin
{

ScriptedEffect *ScriptedEffect::create(const KPluginMetaData&)
{
    return nullptr;
}

bool ScriptedEffect::supported()
{
    return true;
}

}

class TestPluginEffectLoader : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testHasEffect_data();
    void testHasEffect();
    void testKnownEffects();
    void testSupported_data();
    void testSupported();
    void testLoadEffect_data();
    void testLoadEffect();
    void testLoadPluginEffect_data();
    void testLoadPluginEffect();
    void testLoadAllEffects();
    void testCancelLoadAllEffects();
};

void TestPluginEffectLoader::testHasEffect_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");

    // all the built-in effects should fail
    QTest::newRow("blur")              << QStringLiteral("blur")                      << false;
    QTest::newRow("ColorPicker")       << QStringLiteral("colorpicker")               << false;
    QTest::newRow("Contrast")          << QStringLiteral("contrast")                  << false;
    QTest::newRow("CoverSwitch")       << QStringLiteral("coverswitch")               << false;
    QTest::newRow("Cube")              << QStringLiteral("cube")                      << false;
    QTest::newRow("CubeSlide")         << QStringLiteral("cubeslide")                 << false;
    QTest::newRow("DesktopGrid")       << QStringLiteral("desktopgrid")               << false;
    QTest::newRow("DimInactive")       << QStringLiteral("diminactive")               << false;
    QTest::newRow("FallApart")         << QStringLiteral("fallapart")                 << false;
    QTest::newRow("FlipSwitch")        << QStringLiteral("flipswitch")                << false;
    QTest::newRow("Glide")             << QStringLiteral("glide")                     << false;
    QTest::newRow("HighlightWindow")   << QStringLiteral("highlightwindow")           << false;
    QTest::newRow("Invert")            << QStringLiteral("invert")                    << false;
    QTest::newRow("Kscreen")           << QStringLiteral("kscreen")                   << false;
    QTest::newRow("LookingGlass")      << QStringLiteral("lookingglass")              << false;
    QTest::newRow("MagicLamp")         << QStringLiteral("magiclamp")                 << false;
    QTest::newRow("Magnifier")         << QStringLiteral("magnifier")                 << false;
    QTest::newRow("MouseClick")        << QStringLiteral("mouseclick")                << false;
    QTest::newRow("MouseMark")         << QStringLiteral("mousemark")                 << false;
    QTest::newRow("PresentWindows")    << QStringLiteral("presentwindows")            << false;
    QTest::newRow("Resize")            << QStringLiteral("resize")                    << false;
    QTest::newRow("ScreenEdge")        << QStringLiteral("screenedge")                << false;
    QTest::newRow("ScreenShot")        << QStringLiteral("screenshot")                << false;
    QTest::newRow("Sheet")             << QStringLiteral("sheet")                     << false;
    QTest::newRow("ShowFps")           << QStringLiteral("showfps")                   << false;
    QTest::newRow("ShowPaint")         << QStringLiteral("showpaint")                 << false;
    QTest::newRow("Slide")             << QStringLiteral("slide")                     << false;
    QTest::newRow("SlideBack")         << QStringLiteral("slideback")                 << false;
    QTest::newRow("SlidingPopups")     << QStringLiteral("slidingpopups")             << false;
    QTest::newRow("SnapHelper")        << QStringLiteral("snaphelper")                << false;
    QTest::newRow("StartupFeedback")   << QStringLiteral("startupfeedback")           << false;
    QTest::newRow("ThumbnailAside")    << QStringLiteral("thumbnailaside")            << false;
    QTest::newRow("TrackMouse")        << QStringLiteral("trackmouse")                << false;
    QTest::newRow("WindowGeometry")    << QStringLiteral("windowgeometry")            << false;
    QTest::newRow("WobblyWindows")     << QStringLiteral("wobblywindows")             << false;
    QTest::newRow("Zoom")              << QStringLiteral("zoom")                      << false;
    QTest::newRow("Non Existing")      << QStringLiteral("InvalidName")               << false;
    // all the scripted effects should fail
    QTest::newRow("DialogParent")      << QStringLiteral("kwin4_effect_dialogparent")   << false;
    QTest::newRow("DimScreen")         << QStringLiteral("kwin4_effect_dimscreen")      << false;
    QTest::newRow("EyeOnScreen")       << QStringLiteral("kwin4_effect_eyeonscreen")    << false;
    QTest::newRow("Fade")              << QStringLiteral("kwin4_effect_fade")           << false;
    QTest::newRow("FadeDesktop")       << QStringLiteral("kwin4_effect_fadedesktop")    << false;
    QTest::newRow("FadingPopups")      << QStringLiteral("kwin4_effect_fadingpopups")   << false;
    QTest::newRow("FrozenApp")         << QStringLiteral("kwin4_effect_frozenapp")      << false;
    QTest::newRow("Login")             << QStringLiteral("kwin4_effect_login")          << false;
    QTest::newRow("Logout")            << QStringLiteral("kwin4_effect_logout")         << false;
    QTest::newRow("Maximize")          << QStringLiteral("kwin4_effect_maximize")       << false;
    QTest::newRow("MorphingPopups")    << QStringLiteral("kwin4_effect_morphingpopups") << false;
    QTest::newRow("Scale")             << QStringLiteral("kwin4_effect_scale")          << false;
    QTest::newRow("Squash")            << QStringLiteral("kwin4_effect_squash")         << false;
    QTest::newRow("Translucency")      << QStringLiteral("kwin4_effect_translucency")   << false;
    QTest::newRow("WindowAperture")    << QStringLiteral("kwin4_effect_windowaperture") << false;
    // and the fake effects we use here
    QTest::newRow("fakeeffectplugin")    << QStringLiteral("fakeeffectplugin")          << true;
    QTest::newRow("fakeeffectplugin CS") << QStringLiteral("fakeEffectPlugin")          << true;
    QTest::newRow("effectversion")       << QStringLiteral("effectversion")             << true;
}

void TestPluginEffectLoader::testHasEffect()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);

    KWin::PluginEffectLoader loader;
    loader.setPluginSubDirectory(QString());
    QCOMPARE(loader.hasEffect(name), expected);
}

void TestPluginEffectLoader::testKnownEffects()
{
    QStringList expectedEffects;
    expectedEffects << QStringLiteral("fakeeffectplugin") << QStringLiteral("effectversion");

    KWin::PluginEffectLoader loader;
    loader.setPluginSubDirectory(QString());
    QStringList result = loader.listOfKnownEffects();
    // at least as many effects as we expect - system running the test could have more effects
    QVERIFY(result.size() >= expectedEffects.size());
    for (const QString &effect : expectedEffects) {
        QVERIFY(result.contains(effect));
    }
}

void TestPluginEffectLoader::testSupported_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<KWin::CompositingType>("type");

    const KWin::CompositingType xc = KWin::XRenderCompositing;
    const KWin::CompositingType oc = KWin::OpenGL2Compositing;

    QTest::newRow("invalid")        << QStringLiteral("blur")             << false << xc;
    QTest::newRow("fake - xrender") << QStringLiteral("fakeeffectplugin") << false << xc;
    QTest::newRow("fake - opengl")  << QStringLiteral("fakeeffectplugin") << true  << oc;
    QTest::newRow("fake - CS")      << QStringLiteral("fakeEffectPlugin") << true  << oc;
    QTest::newRow("version")        << QStringLiteral("effectversion")    << false << xc;
}

void TestPluginEffectLoader::testSupported()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);
    QFETCH(KWin::CompositingType, type);

    MockEffectsHandler mockHandler(type);
    KWin::PluginEffectLoader loader;
    loader.setPluginSubDirectory(QString());
    QCOMPARE(loader.isEffectSupported(name), expected);
}

void TestPluginEffectLoader::testLoadEffect_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<KWin::CompositingType>("type");

    const KWin::CompositingType xc = KWin::XRenderCompositing;
    const KWin::CompositingType oc = KWin::OpenGL2Compositing;

    QTest::newRow("invalid")        << QStringLiteral("slide")            << false << xc;
    QTest::newRow("fake - xrender") << QStringLiteral("fakeeffectplugin") << false << xc;
    QTest::newRow("fake - opengl")  << QStringLiteral("fakeeffectplugin") << true  << oc;
    QTest::newRow("fake - CS")      << QStringLiteral("fakeEffectPlugin") << true  << oc;
    QTest::newRow("version")        << QStringLiteral("effectversion")    << false << xc;
}

void TestPluginEffectLoader::testLoadEffect()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);
    QFETCH(KWin::CompositingType, type);

    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater> mockHandler(new MockEffectsHandler(type));
    KWin::PluginEffectLoader loader;
    loader.setPluginSubDirectory(QString());
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    loader.setConfig(config);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::PluginEffectLoader::effectLoaded);
    // connect to signal to ensure that we delete the Effect again as the Effect doesn't have a parent
    connect(&loader, &KWin::PluginEffectLoader::effectLoaded,
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

void TestPluginEffectLoader::testLoadPluginEffect_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<KWin::CompositingType>("type");
    QTest::addColumn<KWin::LoadEffectFlags>("loadFlags");
    QTest::addColumn<bool>("enabledByDefault");

    const KWin::CompositingType xc = KWin::XRenderCompositing;
    const KWin::CompositingType oc = KWin::OpenGL2Compositing;

    const KWin::LoadEffectFlags checkDefault = KWin::LoadEffectFlag::Load | KWin::LoadEffectFlag::CheckDefaultFunction;
    const KWin::LoadEffectFlags forceFlags = KWin::LoadEffectFlag::Load;
    const KWin::LoadEffectFlags dontLoadFlags = KWin::LoadEffectFlags();

    // enabled by default, but not supported
    QTest::newRow("fakeeffectplugin")                       << QStringLiteral("fakeeffectplugin") << false << xc << checkDefault  << false;
    // enabled by default, check default false
    QTest::newRow("supported, check default error")         << QStringLiteral("fakeeffectplugin") << false << oc << checkDefault  << false;
    // enabled by default, check default true
    QTest::newRow("supported, check default")               << QStringLiteral("fakeeffectplugin") << true  << oc << checkDefault  << true;
    // enabled by default, check default false
    QTest::newRow("supported, check default error, forced") << QStringLiteral("fakeeffectplugin") << true  << oc << forceFlags    << false;
    // enabled by default, check default true
    QTest::newRow("supported, check default, don't load")   << QStringLiteral("fakeeffectplugin") << false << oc << dontLoadFlags << true;
    // incorrect version
    QTest::newRow("Version")                                << QStringLiteral("effectversion")    << false << xc << forceFlags    << true;
}

void TestPluginEffectLoader::testLoadPluginEffect()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);
    QFETCH(KWin::CompositingType, type);
    QFETCH(KWin::LoadEffectFlags, loadFlags);
    QFETCH(bool, enabledByDefault);

    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater> mockHandler(new MockEffectsHandler(type));
    mockHandler->setProperty("testEnabledByDefault", enabledByDefault);
    KWin::PluginEffectLoader loader;
    loader.setPluginSubDirectory(QString());
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    loader.setConfig(config);

    const auto plugins = KPluginLoader::findPlugins(QString(),
        [name] (const KPluginMetaData &data) {
            return data.pluginId().compare(name, Qt::CaseInsensitive) == 0 && data.serviceTypes().contains(QStringLiteral("KWin/Effect"));
        }
    );
    QCOMPARE(plugins.size(), 1);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::PluginEffectLoader::effectLoaded);
    // connect to signal to ensure that we delete the Effect again as the Effect doesn't have a parent
    connect(&loader, &KWin::PluginEffectLoader::effectLoaded,
        [&name](KWin::Effect *effect, const QString &effectName) {
            QCOMPARE(effectName, name);
            effect->deleteLater();
        }
    );
    // try to load the Effect
    QCOMPARE(loader.loadEffect(plugins.first(), loadFlags), expected);
    // loading again should fail
    QVERIFY(!loader.loadEffect(plugins.first(), loadFlags));

    // signal spy should have got the signal if it was expected
    QCOMPARE(spy.isEmpty(), !expected);
    if (!spy.isEmpty()) {
        QCOMPARE(spy.count(), 1);
        // if we caught a signal it should have the effect name we passed in
        QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.count(), 2);
        QCOMPARE(arguments.at(1).toString(), name);
    }
    spy.clear();
    QVERIFY(spy.isEmpty());

    // now if we wait for the events being processed, the effect will get deleted and it should load again
    QTest::qWait(1);
    QCOMPARE(loader.loadEffect(plugins.first(), loadFlags), expected);
    // signal spy should have got the signal if it was expected
    QCOMPARE(spy.isEmpty(), !expected);
    if (!spy.isEmpty()) {
        QCOMPARE(spy.count(), 1);
        // if we caught a signal it should have the effect name we passed in
        QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.count(), 2);
        QCOMPARE(arguments.at(1).toString(), name);
    }
}

void TestPluginEffectLoader::testLoadAllEffects()
{
    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater> mockHandler(new MockEffectsHandler(KWin::OpenGL2Compositing));
    mockHandler->setProperty("testEnabledByDefault", true);
    KWin::PluginEffectLoader loader;
    loader.setPluginSubDirectory(QString());

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);

    // prepare the configuration to hard enable/disable the effects we want to load
    KConfigGroup plugins = config->group("Plugins");
    plugins.writeEntry(QStringLiteral("fakeeffectpluginEnabled"), false);
    plugins.sync();

    loader.setConfig(config);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::PluginEffectLoader::effectLoaded);
    // connect to signal to ensure that we delete the Effect again as the Effect doesn't have a parent
    connect(&loader, &KWin::PluginEffectLoader::effectLoaded,
        [](KWin::Effect *effect) {
            effect->deleteLater();
        }
    );

    // the config is prepared so that no Effect gets loaded!
    loader.queryAndLoadAll();

    // we need to wait some time because it's queued and in a thread
    QVERIFY(!spy.wait(100));

    // now let's prepare a config which has one effect explicitly enabled
    plugins.writeEntry(QStringLiteral("fakeeffectpluginEnabled"), true);
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
    QCOMPARE(arguments.at(1).toString(), QStringLiteral("fakeeffectplugin"));
    spy.clear();
}

void TestPluginEffectLoader::testCancelLoadAllEffects()
{
    // this test verifies that no test gets loaded when the loader gets cleared
    MockEffectsHandler mockHandler(KWin::OpenGL2Compositing);
    KWin::PluginEffectLoader loader;
    loader.setPluginSubDirectory(QString());

    // prepare the configuration to hard enable/disable the effects we want to load
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins = config->group("Plugins");
    plugins.writeEntry(QStringLiteral("fakeeffectpluginEnabled"), true);
    plugins.sync();

    loader.setConfig(config);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::PluginEffectLoader::effectLoaded);
    QVERIFY(spy.isValid());

    loader.queryAndLoadAll();
    loader.clear();

    // Should not load any effect
    QVERIFY(!spy.wait(100));
    QVERIFY(spy.isEmpty());
}

QTEST_MAIN(TestPluginEffectLoader)
#include "test_plugin_effectloader.moc"
