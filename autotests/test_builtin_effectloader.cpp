/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../effectloader.h"
#include "../effects/effect_builtins.h"
#include "mock_effectshandler.h"
#include "../scripting/scriptedeffect.h" // for mocking ScriptedEffect::create
#include "testutils.h"
// KDE
#include <KConfig>
#include <KConfigGroup>
// Qt
#include <QtTest>
#include <QStringList>
#include <QScopedPointer>
Q_DECLARE_METATYPE(KWin::CompositingType)
Q_DECLARE_METATYPE(KWin::LoadEffectFlag)
Q_DECLARE_METATYPE(KWin::LoadEffectFlags)
Q_DECLARE_METATYPE(KWin::BuiltInEffect)
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

class TestBuiltInEffectLoader : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testHasEffect_data();
    void testHasEffect();
    void testKnownEffects();
    void testSupported_data();
    void testSupported();
    void testLoadEffect_data();
    void testLoadEffect();
    void testLoadBuiltInEffect_data();
    void testLoadBuiltInEffect();
    void testLoadAllEffects();
};

void TestBuiltInEffectLoader::initTestCase()
{
    qApp->setProperty("x11Connection", QVariant::fromValue<void*>(QX11Info::connection()));
}

void TestBuiltInEffectLoader::testHasEffect_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");

    QTest::newRow("blur")                           << QStringLiteral("blur")              << true;
    QTest::newRow("with kwin4_effect_ prefix")      << QStringLiteral("kwin4_effect_blur") << false;
    QTest::newRow("case sensitive")                 << QStringLiteral("BlUR")              << true;
    QTest::newRow("Colorpicker")                    << QStringLiteral("colorpicker")       << true;
    QTest::newRow("Contrast")                       << QStringLiteral("contrast")          << true;
    QTest::newRow("CoverSwitch")                    << QStringLiteral("coverswitch")       << true;
    QTest::newRow("Cube")                           << QStringLiteral("cube")              << true;
    QTest::newRow("CubeSlide")                      << QStringLiteral("cubeslide")         << true;
    QTest::newRow("DesktopGrid")                    << QStringLiteral("desktopgrid")       << true;
    QTest::newRow("DimInactive")                    << QStringLiteral("diminactive")       << true;
    QTest::newRow("FallApart")                      << QStringLiteral("fallapart")         << true;
    QTest::newRow("FlipSwitch")                     << QStringLiteral("flipswitch")        << true;
    QTest::newRow("Glide")                          << QStringLiteral("glide")             << true;
    QTest::newRow("HighlightWindow")                << QStringLiteral("highlightwindow")   << true;
    QTest::newRow("Invert")                         << QStringLiteral("invert")            << true;
    QTest::newRow("Kscreen")                        << QStringLiteral("kscreen")           << true;
    QTest::newRow("LookingGlass")                   << QStringLiteral("lookingglass")      << true;
    QTest::newRow("MagicLamp")                      << QStringLiteral("magiclamp")         << true;
    QTest::newRow("Magnifier")                      << QStringLiteral("magnifier")         << true;
    QTest::newRow("MouseClick")                     << QStringLiteral("mouseclick")        << true;
    QTest::newRow("MouseMark")                      << QStringLiteral("mousemark")         << true;
    QTest::newRow("PresentWindows")                 << QStringLiteral("presentwindows")    << true;
    QTest::newRow("Resize")                         << QStringLiteral("resize")            << true;
    QTest::newRow("ScreenEdge")                     << QStringLiteral("screenedge")        << true;
    QTest::newRow("ScreenShot")                     << QStringLiteral("screenshot")        << true;
    QTest::newRow("Sheet")                          << QStringLiteral("sheet")             << true;
    QTest::newRow("ShowFps")                        << QStringLiteral("showfps")           << true;
    QTest::newRow("ShowPaint")                      << QStringLiteral("showpaint")         << true;
    QTest::newRow("Slide")                          << QStringLiteral("slide")             << true;
    QTest::newRow("SlideBack")                      << QStringLiteral("slideback")         << true;
    QTest::newRow("SlidingPopups")                  << QStringLiteral("slidingpopups")     << true;
    QTest::newRow("SnapHelper")                     << QStringLiteral("snaphelper")        << true;
    QTest::newRow("StartupFeedback")                << QStringLiteral("startupfeedback")   << true;
    QTest::newRow("ThumbnailAside")                 << QStringLiteral("thumbnailaside")    << true;
    QTest::newRow("Touchpoints")                    << QStringLiteral("touchpoints")       << true;
    QTest::newRow("TrackMouse")                     << QStringLiteral("trackmouse")        << true;
    QTest::newRow("WindowGeometry")                 << QStringLiteral("windowgeometry")    << true;
    QTest::newRow("WobblyWindows")                  << QStringLiteral("wobblywindows")     << true;
    QTest::newRow("Zoom")                           << QStringLiteral("zoom")              << true;
    QTest::newRow("Non Existing")                   << QStringLiteral("InvalidName")       << false;
    QTest::newRow("Fade - Scripted")                << QStringLiteral("fade")              << false;
    QTest::newRow("Fade - Scripted + kwin4_effect") << QStringLiteral("kwin4_effect_fade") << false;
}

void TestBuiltInEffectLoader::testHasEffect()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);

    KWin::BuiltInEffectLoader loader;
    QCOMPARE(loader.hasEffect(name), expected);
}

void TestBuiltInEffectLoader::testKnownEffects()
{
    QStringList expectedEffects;
    expectedEffects << QStringLiteral("blur")
                    << QStringLiteral("colorpicker")
                    << QStringLiteral("contrast")
                    << QStringLiteral("coverswitch")
                    << QStringLiteral("cube")
                    << QStringLiteral("cubeslide")
                    << QStringLiteral("desktopgrid")
                    << QStringLiteral("diminactive")
                    << QStringLiteral("fallapart")
                    << QStringLiteral("flipswitch")
                    << QStringLiteral("glide")
                    << QStringLiteral("highlightwindow")
                    << QStringLiteral("invert")
                    << QStringLiteral("kscreen")
                    << QStringLiteral("lookingglass")
                    << QStringLiteral("magiclamp")
                    << QStringLiteral("magnifier")
                    << QStringLiteral("mouseclick")
                    << QStringLiteral("mousemark")
                    << QStringLiteral("presentwindows")
                    << QStringLiteral("resize")
                    << QStringLiteral("screenedge")
                    << QStringLiteral("screenshot")
                    << QStringLiteral("sheet")
                    << QStringLiteral("showfps")
                    << QStringLiteral("showpaint")
                    << QStringLiteral("slide")
                    << QStringLiteral("slideback")
                    << QStringLiteral("slidingpopups")
                    << QStringLiteral("snaphelper")
                    << QStringLiteral("startupfeedback")
                    << QStringLiteral("thumbnailaside")
                    << QStringLiteral("touchpoints")
                    << QStringLiteral("trackmouse")
                    << QStringLiteral("windowgeometry")
                    << QStringLiteral("wobblywindows")
                    << QStringLiteral("zoom");

    KWin::BuiltInEffectLoader loader;
    QStringList result = loader.listOfKnownEffects();
    QCOMPARE(result.size(), expectedEffects.size());
    std::sort(result.begin(), result.end());
    for (int i = 0; i < expectedEffects.size(); ++i) {
        QCOMPARE(result.at(i), expectedEffects.at(i));
    }
}

void TestBuiltInEffectLoader::testSupported_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<KWin::CompositingType>("type");
    QTest::addColumn<bool>("animationsSupported");

    const KWin::CompositingType xc = KWin::XRenderCompositing;
    const KWin::CompositingType oc = KWin::OpenGL2Compositing;

    QTest::newRow("blur")                           << QStringLiteral("blur")              << false << xc << true;
    // fails for GL as it does proper tests on what's supported and doesn't just check whether it's GL
    QTest::newRow("blur-GL")                        << QStringLiteral("blur")              << false << oc << true;
    QTest::newRow("Colorpicker")                    << QStringLiteral("colorpicker")       << false << xc << true;
    QTest::newRow("Colorpicker-GL")                 << QStringLiteral("colorpicker")       << true  << oc << true;
    QTest::newRow("Contrast")                       << QStringLiteral("contrast")          << false << xc << true;
    // fails for GL as it does proper tests on what's supported and doesn't just check whether it's GL
    QTest::newRow("Contrast-GL")                    << QStringLiteral("contrast")          << false << oc << true;
    QTest::newRow("CoverSwitch")                    << QStringLiteral("coverswitch")       << false << xc << true;
    QTest::newRow("CoverSwitch-GL")                 << QStringLiteral("coverswitch")       << true  << oc << true;
    QTest::newRow("CoverSwitch-GL-no-anim")         << QStringLiteral("coverswitch")       << false << oc << false;
    QTest::newRow("Cube")                           << QStringLiteral("cube")              << false << xc << true;
    QTest::newRow("Cube-GL")                        << QStringLiteral("cube")              << true  << oc << true;
    QTest::newRow("CubeSlide")                      << QStringLiteral("cubeslide")         << false << xc << true;
    QTest::newRow("CubeSlide-GL")                   << QStringLiteral("cubeslide")         << true  << oc << true;
    QTest::newRow("CubeSlide-GL-no-anim")           << QStringLiteral("cubeslide")         << false << oc << false;
    QTest::newRow("DesktopGrid")                    << QStringLiteral("desktopgrid")       << true  << xc << true;
    QTest::newRow("DimInactive")                    << QStringLiteral("diminactive")       << true  << xc << true;
    QTest::newRow("FallApart")                      << QStringLiteral("fallapart")         << false << xc << true;
    QTest::newRow("FallApart-GL")                   << QStringLiteral("fallapart")         << true  << oc << true;
    QTest::newRow("FlipSwitch")                     << QStringLiteral("flipswitch")        << false << xc << true;
    QTest::newRow("FlipSwitch-GL")                  << QStringLiteral("flipswitch")        << true  << oc << true;
    QTest::newRow("FlipSwitch-GL-no-anim")          << QStringLiteral("flipswitch")        << false << oc << false;
    QTest::newRow("Glide")                          << QStringLiteral("glide")             << false << xc << true;
    QTest::newRow("Glide-GL")                       << QStringLiteral("glide")             << true  << oc << true;
    QTest::newRow("Glide-GL-no-anim")               << QStringLiteral("glide")             << false << oc << false;
    QTest::newRow("HighlightWindow")                << QStringLiteral("highlightwindow")   << true  << xc << true;
    QTest::newRow("Invert")                         << QStringLiteral("invert")            << false << xc << true;
    QTest::newRow("Invert-GL")                      << QStringLiteral("invert")            << true  << oc << true;
    QTest::newRow("Kscreen")                        << QStringLiteral("kscreen")           << true  << xc << true;
    QTest::newRow("LookingGlass")                   << QStringLiteral("lookingglass")      << false << xc << true;
    QTest::newRow("LookingGlass-GL")                << QStringLiteral("lookingglass")      << true  << oc << true;
    QTest::newRow("MagicLamp")                      << QStringLiteral("magiclamp")         << false << xc << true;
    QTest::newRow("MagicLamp-GL")                   << QStringLiteral("magiclamp")         << true  << oc << true;
    QTest::newRow("MagicLamp-GL-no-anim")           << QStringLiteral("magiclamp")         << false << oc << false;
    QTest::newRow("Magnifier")                      << QStringLiteral("magnifier")         << true  << xc << true;
    QTest::newRow("MouseClick")                     << QStringLiteral("mouseclick")        << true  << xc << true;
    QTest::newRow("MouseMark")                      << QStringLiteral("mousemark")         << true  << xc << true;
    QTest::newRow("PresentWindows")                 << QStringLiteral("presentwindows")    << true  << xc << true;
    QTest::newRow("Resize")                         << QStringLiteral("resize")            << true  << xc << true;
    QTest::newRow("ScreenEdge")                     << QStringLiteral("screenedge")        << true  << xc << true;
    QTest::newRow("ScreenShot")                     << QStringLiteral("screenshot")        << true  << xc << true;
    QTest::newRow("Sheet")                          << QStringLiteral("sheet")             << false << xc << true;
    QTest::newRow("Sheet-GL")                       << QStringLiteral("sheet")             << true  << oc << true;
    QTest::newRow("Sheet-GL-no-anim")               << QStringLiteral("sheet")             << false << oc << false;
    QTest::newRow("ShowFps")                        << QStringLiteral("showfps")           << true  << xc << true;
    QTest::newRow("ShowPaint")                      << QStringLiteral("showpaint")         << true  << xc << true;
    QTest::newRow("Slide")                          << QStringLiteral("slide")             << true  << xc << true;
    QTest::newRow("SlideBack")                      << QStringLiteral("slideback")         << true  << xc << true;
    QTest::newRow("SlidingPopups")                  << QStringLiteral("slidingpopups")     << true  << xc << true;
    QTest::newRow("SnapHelper")                     << QStringLiteral("snaphelper")        << true  << xc << true;
    QTest::newRow("StartupFeedback")                << QStringLiteral("startupfeedback")   << false << xc << true;
    QTest::newRow("StartupFeedback-GL")             << QStringLiteral("startupfeedback")   << true  << oc << true;
    QTest::newRow("ThumbnailAside")                 << QStringLiteral("thumbnailaside")    << true  << xc << true;
    QTest::newRow("TouchPoints")                    << QStringLiteral("touchpoints")       << true  << xc << true;
    QTest::newRow("TrackMouse")                     << QStringLiteral("trackmouse")        << true  << xc << true;
    QTest::newRow("WindowGeometry")                 << QStringLiteral("windowgeometry")    << true  << xc << true;
    QTest::newRow("WobblyWindows")                  << QStringLiteral("wobblywindows")     << false << xc << true;
    QTest::newRow("WobblyWindows-GL")               << QStringLiteral("wobblywindows")     << true  << oc << true;
    QTest::newRow("WobblyWindows-GL-no-anim")       << QStringLiteral("wobblywindows")     << false << oc << false;
    QTest::newRow("Zoom")                           << QStringLiteral("zoom")              << true  << xc << true;
    QTest::newRow("Non Existing")                   << QStringLiteral("InvalidName")       << false << xc << true;
    QTest::newRow("Fade - Scripted")                << QStringLiteral("fade")              << false << xc << true;
    QTest::newRow("Fade - Scripted + kwin4_effect") << QStringLiteral("kwin4_effect_fade") << false << xc << true;
}

void TestBuiltInEffectLoader::testSupported()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);
    QFETCH(KWin::CompositingType, type);
    QFETCH(bool, animationsSupported);

    MockEffectsHandler mockHandler(type);
    mockHandler.setAnimationsSupported(animationsSupported);
    QCOMPARE(mockHandler.animationsSupported(), animationsSupported);
    KWin::BuiltInEffectLoader loader;
    QCOMPARE(loader.isEffectSupported(name), expected);
}

void TestBuiltInEffectLoader::testLoadEffect_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<KWin::CompositingType>("type");

    const KWin::CompositingType xc = KWin::XRenderCompositing;
    const KWin::CompositingType oc = KWin::OpenGL2Compositing;

    QTest::newRow("blur")                           << QStringLiteral("blur")              << false << xc;
    // fails for GL as it does proper tests on what's supported and doesn't just check whether it's GL
    QTest::newRow("blur-GL")                        << QStringLiteral("blur")              << false << oc;
    QTest::newRow("Colorpicker")                    << QStringLiteral("colorpicker")       << false << xc;
    QTest::newRow("Colorpicker-GL")                 << QStringLiteral("colorpicker")       << true  << oc;
    QTest::newRow("Contrast")                       << QStringLiteral("contrast")          << false << xc;
    // fails for GL as it does proper tests on what's supported and doesn't just check whether it's GL
    QTest::newRow("Contrast-GL")                    << QStringLiteral("contrast")          << false << oc;
    QTest::newRow("CoverSwitch")                    << QStringLiteral("coverswitch")       << false << xc;
    // TODO: needs GL mocking
//     QTest::newRow("CoverSwitch-GL")                 << QStringLiteral("coverswitch")       << true  << oc;
    QTest::newRow("Cube")                           << QStringLiteral("cube")              << false << xc;
    // TODO: needs GL mocking
//     QTest::newRow("Cube-GL")                        << QStringLiteral("cube")              << true  << oc;
    QTest::newRow("CubeSlide")                      << QStringLiteral("cubeslide")         << false << xc;
    QTest::newRow("CubeSlide-GL")                   << QStringLiteral("cubeslide")         << true  << oc;
    QTest::newRow("DesktopGrid")                    << QStringLiteral("desktopgrid")       << true  << xc;
    QTest::newRow("DimInactive")                    << QStringLiteral("diminactive")       << true  << xc;
    QTest::newRow("FallApart")                      << QStringLiteral("fallapart")         << false << xc;
    QTest::newRow("FallApart-GL")                   << QStringLiteral("fallapart")         << true  << oc;
    QTest::newRow("FlipSwitch")                     << QStringLiteral("flipswitch")        << false << xc;
    QTest::newRow("FlipSwitch-GL")                  << QStringLiteral("flipswitch")        << true  << oc;
    QTest::newRow("Glide")                          << QStringLiteral("glide")             << false << xc;
    QTest::newRow("Glide-GL")                       << QStringLiteral("glide")             << true  << oc;
    QTest::newRow("HighlightWindow")                << QStringLiteral("highlightwindow")   << true  << xc;
    QTest::newRow("Invert")                         << QStringLiteral("invert")            << false << xc;
    QTest::newRow("Invert-GL")                      << QStringLiteral("invert")            << true  << oc;
    QTest::newRow("Kscreen")                        << QStringLiteral("kscreen")           << true  << xc;
    QTest::newRow("LookingGlass")                   << QStringLiteral("lookingglass")      << false << xc;
    QTest::newRow("LookingGlass-GL")                << QStringLiteral("lookingglass")      << true  << oc;
    QTest::newRow("MagicLamp")                      << QStringLiteral("magiclamp")         << false << xc;
    QTest::newRow("MagicLamp-GL")                   << QStringLiteral("magiclamp")         << true  << oc;
    QTest::newRow("Magnifier")                      << QStringLiteral("magnifier")         << true  << xc;
    QTest::newRow("MouseClick")                     << QStringLiteral("mouseclick")        << true  << xc;
    QTest::newRow("MouseMark")                      << QStringLiteral("mousemark")         << true  << xc;
    QTest::newRow("PresentWindows")                 << QStringLiteral("presentwindows")    << true  << xc;
    QTest::newRow("Resize")                         << QStringLiteral("resize")            << true  << xc;
    QTest::newRow("ScreenEdge")                     << QStringLiteral("screenedge")        << true  << xc;
    QTest::newRow("ScreenShot")                     << QStringLiteral("screenshot")        << true  << xc;
    QTest::newRow("Sheet")                          << QStringLiteral("sheet")             << false << xc;
    QTest::newRow("Sheet-GL")                       << QStringLiteral("sheet")             << true  << oc;
    // TODO: Accesses EffectFrame and crashes
//     QTest::newRow("ShowFps")                        << QStringLiteral("showfps")           << true  << xc;
    QTest::newRow("ShowPaint")                      << QStringLiteral("showpaint")         << true  << xc;
    QTest::newRow("Slide")                          << QStringLiteral("slide")             << true  << xc;
    QTest::newRow("SlideBack")                      << QStringLiteral("slideback")         << true  << xc;
    QTest::newRow("SlidingPopups")                  << QStringLiteral("slidingpopups")     << true  << xc;
    QTest::newRow("SnapHelper")                     << QStringLiteral("snaphelper")        << true  << xc;
    QTest::newRow("StartupFeedback")                << QStringLiteral("startupfeedback")   << false << xc;
    // Tries to load shader and makes our test abort
//     QTest::newRow("StartupFeedback-GL")             << QStringLiteral("startupfeedback")   << true  << oc;
    QTest::newRow("ThumbnailAside")                 << QStringLiteral("thumbnailaside")    << true  << xc;
    QTest::newRow("Touchpoints")                    << QStringLiteral("touchpoints")       << true  << xc;
    QTest::newRow("TrackMouse")                     << QStringLiteral("trackmouse")        << true  << xc;
    // TODO: Accesses EffectFrame and crashes
//     QTest::newRow("WindowGeometry")                 << QStringLiteral("windowgeometry")    << true  << xc;
    QTest::newRow("WobblyWindows")                  << QStringLiteral("wobblywindows")     << false << xc;
    QTest::newRow("WobblyWindows-GL")               << QStringLiteral("wobblywindows")     << true  << oc;
    QTest::newRow("Zoom")                           << QStringLiteral("zoom")              << true  << xc;
    QTest::newRow("Non Existing")                   << QStringLiteral("InvalidName")       << false << xc;
    QTest::newRow("Fade - Scripted")                << QStringLiteral("fade")              << false << xc;
    QTest::newRow("Fade - Scripted + kwin4_effect") << QStringLiteral("kwin4_effect_fade") << false << xc;
}

void TestBuiltInEffectLoader::testLoadEffect()
{
    QFETCH(QString, name);
    QFETCH(bool, expected);
    QFETCH(KWin::CompositingType, type);

    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater> mockHandler(new MockEffectsHandler(type));
    KWin::BuiltInEffectLoader loader;
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    loader.setConfig(config);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::BuiltInEffectLoader::effectLoaded);
    // connect to signal to ensure that we delete the Effect again as the Effect doesn't have a parent
    connect(&loader, &KWin::BuiltInEffectLoader::effectLoaded,
        [&name](KWin::Effect *effect, const QString &effectName) {
            QCOMPARE(effectName, name);
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
        QCOMPARE(arguments.at(1).toString(), name);
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
        QCOMPARE(arguments.at(1).toString(), name);
    }
}

void TestBuiltInEffectLoader::testLoadBuiltInEffect_data()
{
    // TODO: this test cannot yet test the checkEnabledByDefault functionality as that requires
    // mocking enough of GL to get the blur effect to think it's supported and enabled by default
    QTest::addColumn<KWin::BuiltInEffect>("effect");
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<KWin::CompositingType>("type");
    QTest::addColumn<KWin::LoadEffectFlags>("loadFlags");

    const KWin::CompositingType xc = KWin::XRenderCompositing;
    const KWin::CompositingType oc = KWin::OpenGL2Compositing;

    const KWin::LoadEffectFlags checkDefault = KWin::LoadEffectFlag::Load | KWin::LoadEffectFlag::CheckDefaultFunction;
    const KWin::LoadEffectFlags forceFlags = KWin::LoadEffectFlag::Load;
    const KWin::LoadEffectFlags dontLoadFlags = KWin::LoadEffectFlags();

    // enabled by default, but not supported
    QTest::newRow("blur")                     << KWin::BuiltInEffect::Blur << QStringLiteral("blur")            << false << oc << checkDefault;
    // enabled by default
    QTest::newRow("HighlightWindow")          << KWin::BuiltInEffect::HighlightWindow << QStringLiteral("highlightwindow") << true  << xc << checkDefault;
    // supported but not enabled by default
    QTest::newRow("LookingGlass-GL")          << KWin::BuiltInEffect::LookingGlass << QStringLiteral("lookingglass")    << true << oc << checkDefault;
    // not enabled by default
    QTest::newRow("MouseClick")               << KWin::BuiltInEffect::MouseClick << QStringLiteral("mouseclick")      << true << xc << checkDefault;
    // Force an Effect which will load
    QTest::newRow("MouseClick-Force")         << KWin::BuiltInEffect::MouseClick << QStringLiteral("mouseclick")      << true  << xc << forceFlags;
    // Force an Effect which is not supported
    QTest::newRow("LookingGlass-Force")       << KWin::BuiltInEffect::LookingGlass << QStringLiteral("lookingglass")    << false << xc << forceFlags;
    // Force the Effect as supported
    QTest::newRow("LookingGlass-Force-GL")    << KWin::BuiltInEffect::LookingGlass << QStringLiteral("lookingglass")    << true  << oc << forceFlags;
    // Enforce no load of effect which is enabled by default
    QTest::newRow("HighlightWindow-DontLoad") << KWin::BuiltInEffect::HighlightWindow << QStringLiteral("highlightwindow") << false << xc << dontLoadFlags;
    // Enforce no load of effect which is not enabled by default, but enforced
    QTest::newRow("MouseClick-DontLoad")      << KWin::BuiltInEffect::MouseClick << QStringLiteral("mouseclick")      << false << xc << dontLoadFlags;
}

void TestBuiltInEffectLoader::testLoadBuiltInEffect()
{
    QFETCH(KWin::BuiltInEffect, effect);
    QFETCH(QString, name);
    QFETCH(bool, expected);
    QFETCH(KWin::CompositingType, type);
    QFETCH(KWin::LoadEffectFlags, loadFlags);

    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater> mockHandler(new MockEffectsHandler(type));
    KWin::BuiltInEffectLoader loader;
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    loader.setConfig(config);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::BuiltInEffectLoader::effectLoaded);
    // connect to signal to ensure that we delete the Effect again as the Effect doesn't have a parent
    connect(&loader, &KWin::BuiltInEffectLoader::effectLoaded,
        [&name](KWin::Effect *effect, const QString &effectName) {
            QCOMPARE(effectName, name);
            effect->deleteLater();
        }
    );
    // try to load the Effect
    QCOMPARE(loader.loadEffect(effect, loadFlags), expected);
    // loading again should fail
    QVERIFY(!loader.loadEffect(effect, loadFlags));

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
    QCOMPARE(loader.loadEffect(effect, loadFlags), expected);
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

void TestBuiltInEffectLoader::testLoadAllEffects()
{
    QScopedPointer<MockEffectsHandler, QScopedPointerDeleteLater>mockHandler(new MockEffectsHandler(KWin::XRenderCompositing));
    KWin::BuiltInEffectLoader loader;

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);

    // prepare the configuration to hard enable/disable the effects we want to load
    KConfigGroup plugins = config->group("Plugins");
    plugins.writeEntry(QStringLiteral("desktopgridEnabled"), false);
    plugins.writeEntry(QStringLiteral("highlightwindowEnabled"), false);
    plugins.writeEntry(QStringLiteral("kscreenEnabled"), false);
    plugins.writeEntry(QStringLiteral("presentwindowsEnabled"), false);
    plugins.writeEntry(QStringLiteral("screenedgeEnabled"), false);
    plugins.writeEntry(QStringLiteral("screenshotEnabled"), false);
    plugins.writeEntry(QStringLiteral("slideEnabled"), false);
    plugins.writeEntry(QStringLiteral("slidingpopupsEnabled"), false);
    plugins.writeEntry(QStringLiteral("startupfeedbackEnabled"), false);
    plugins.writeEntry(QStringLiteral("zoomEnabled"), false);
    // enable lookingglass as it's not supported
    plugins.writeEntry(QStringLiteral("lookingglassEnabled"), true);
    plugins.sync();

    loader.setConfig(config);

    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy spy(&loader, &KWin::BuiltInEffectLoader::effectLoaded);
    // connect to signal to ensure that we delete the Effect again as the Effect doesn't have a parent
    connect(&loader, &KWin::BuiltInEffectLoader::effectLoaded,
        [](KWin::Effect *effect) {
            effect->deleteLater();
        }
    );

    // the config is prepared so that no Effect gets loaded!
    loader.queryAndLoadAll();

    // we need to wait some time because it's queued
    QVERIFY(!spy.wait(10));

    // now let's prepare a config which has one effect explicitly enabled
    plugins.writeEntry(QStringLiteral("mouseclickEnabled"), true);
    plugins.sync();

    loader.queryAndLoadAll();
    // should load one effect in first go
    QVERIFY(spy.wait(10));
    // and afterwards it should not load another one
    QVERIFY(!spy.wait(10));

    QCOMPARE(spy.size(), 1);
    // if we caught a signal it should have the effect name we passed in
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.count(), 2);
    QCOMPARE(arguments.at(1).toString(), QStringLiteral("mouseclick"));
    spy.clear();

    // let's delete one of the default entries
    plugins.deleteEntry(QStringLiteral("kscreenEnabled"));
    plugins.sync();

    QVERIFY(spy.isEmpty());
    loader.queryAndLoadAll();

    // let's use qWait as we need to wait for two signals to be emitted
    QTest::qWait(100);
    QCOMPARE(spy.size(), 2);
    QStringList loadedEffects;
    for (auto &list : spy) {
        QCOMPARE(list.size(), 2);
        loadedEffects << list.at(1).toString();
    }
    std::sort(loadedEffects.begin(), loadedEffects.end());
    QCOMPARE(loadedEffects.at(0), QStringLiteral("kscreen"));
    QCOMPARE(loadedEffects.at(1), QStringLiteral("mouseclick"));
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(TestBuiltInEffectLoader)
#include "test_builtin_effectloader.moc"
