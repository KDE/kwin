/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "composite.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "cursor.h"
#include "deleted.h"
#include "effectloader.h"
#include "effects.h"
#include "kwin_wayland_test.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KConfigGroup>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/slide.h>
#include <KWayland/Client/surface.h>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;
static const QString s_socketName = QStringLiteral("wayland_test_effects_slidingpopups-0");

class SlidingPopupsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testWithOtherEffect_data();
    void testWithOtherEffect();
    void testWithOtherEffectWayland_data();
    void testWithOtherEffectWayland();
};

void SlidingPopupsTest::initTestCase()
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::Effect *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }
    KConfigGroup wobblyGroup = config->group("Effect-Wobbly");
    wobblyGroup.writeEntry(QStringLiteral("Settings"), QStringLiteral("Custom"));
    wobblyGroup.writeEntry(QStringLiteral("OpenEffect"), true);
    wobblyGroup.writeEntry(QStringLiteral("CloseEffect"), true);

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(Compositor::self());

    QCOMPARE(Compositor::self()->backend()->compositingType(), KWin::OpenGLCompositing);
}

void SlidingPopupsTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration));
}

void SlidingPopupsTest::cleanup()
{
    Test::destroyWaylandConnection();
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl *>(effects);
    while (!e->loadedEffects().isEmpty()) {
        const QString effect = e->loadedEffects().first();
        e->unloadEffect(effect);
        QVERIFY(!e->isEffectLoaded(effect));
    }
}

struct XcbConnectionDeleter
{
    void operator()(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void SlidingPopupsTest::testWithOtherEffect_data()
{
    QTest::addColumn<QStringList>("effectsToLoad");

    QTest::newRow("fade, slide") << QStringList{QStringLiteral("kwin4_effect_fade"), QStringLiteral("slidingpopups")};
    QTest::newRow("slide, fade") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("kwin4_effect_fade")};
    QTest::newRow("scale, slide") << QStringList{QStringLiteral("kwin4_effect_scale"), QStringLiteral("slidingpopups")};
    QTest::newRow("slide, scale") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("kwin4_effect_scale")};

    if (effects->compositingType() & KWin::OpenGLCompositing) {
        QTest::newRow("glide, slide") << QStringList{QStringLiteral("glide"), QStringLiteral("slidingpopups")};
        QTest::newRow("slide, glide") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("glide")};
        QTest::newRow("wobblywindows, slide") << QStringList{QStringLiteral("wobblywindows"), QStringLiteral("slidingpopups")};
        QTest::newRow("slide, wobblywindows") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("wobblywindows")};
        QTest::newRow("fallapart, slide") << QStringList{QStringLiteral("fallapart"), QStringLiteral("slidingpopups")};
        QTest::newRow("slide, fallapart") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("fallapart")};
    }
}

void SlidingPopupsTest::testWithOtherEffect()
{
    // this test verifies that slidingpopups effect grabs the window added role
    // independently of the sequence how the effects are loaded.
    // see BUG 336866
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl *>(effects);
    // find the effectsloader
    auto effectloader = e->findChild<AbstractEffectLoader *>();
    QVERIFY(effectloader);
    QSignalSpy effectLoadedSpy(effectloader, &AbstractEffectLoader::effectLoaded);

    Effect *slidingPoupus = nullptr;
    Effect *otherEffect = nullptr;
    QFETCH(QStringList, effectsToLoad);
    for (const QString &effectName : effectsToLoad) {
        QVERIFY(!e->isEffectLoaded(effectName));
        QVERIFY(e->loadEffect(effectName));
        QVERIFY(e->isEffectLoaded(effectName));

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

    // create an xcb window
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    NETWinInfo winInfo(c.get(), windowId, rootWindow(), NET::Properties(), NET::Properties2());
    winInfo.setWindowType(NET::Normal);

    // and get the slide atom
    const QByteArray effectAtomName = QByteArrayLiteral("_KDE_SLIDE");
    xcb_intern_atom_cookie_t atomCookie = xcb_intern_atom_unchecked(c.get(), false, effectAtomName.length(), effectAtomName.constData());
    const int size = 2;
    int32_t data[size];
    data[0] = 0;
    data[1] = 0;
    UniqueCPtr<xcb_intern_atom_reply_t> atom(xcb_intern_atom_reply(c.get(), atomCookie, nullptr));
    QVERIFY(atom != nullptr);
    xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atom->atom, atom->atom, 32, size, data);

    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(window->isNormalWindow());

    // sliding popups should be active
    QVERIFY(windowAddedSpy.wait());
    QTRY_VERIFY(slidingPoupus->isActive());
    QVERIFY(!otherEffect->isActive());

    // wait till effect ends
    QTRY_VERIFY(!slidingPoupus->isActive());
    QTRY_VERIFY(!otherEffect->isActive());

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);

    QSignalSpy windowDeletedSpy(effects, &EffectsHandler::windowDeleted);
    QVERIFY(windowClosedSpy.wait());

    // again we should have the sliding popups active
    QVERIFY(slidingPoupus->isActive());
    QVERIFY(!otherEffect->isActive());

    QVERIFY(windowDeletedSpy.wait());

    QCOMPARE(windowDeletedSpy.count(), 1);
    QVERIFY(!slidingPoupus->isActive());
    QVERIFY(!otherEffect->isActive());
    xcb_destroy_window(c.get(), windowId);
    c.reset();
}

void SlidingPopupsTest::testWithOtherEffectWayland_data()
{
    QTest::addColumn<QStringList>("effectsToLoad");

    QTest::newRow("fade, slide") << QStringList{QStringLiteral("kwin4_effect_fade"), QStringLiteral("slidingpopups")};
    QTest::newRow("slide, fade") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("kwin4_effect_fade")};
    QTest::newRow("scale, slide") << QStringList{QStringLiteral("kwin4_effect_scale"), QStringLiteral("slidingpopups")};
    QTest::newRow("slide, scale") << QStringList{QStringLiteral("slidingpopups"), QStringLiteral("kwin4_effect_scale")};

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
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl *>(effects);
    // find the effectsloader
    auto effectloader = e->findChild<AbstractEffectLoader *>();
    QVERIFY(effectloader);
    QSignalSpy effectLoadedSpy(effectloader, &AbstractEffectLoader::effectLoaded);

    Effect *slidingPoupus = nullptr;
    Effect *otherEffect = nullptr;
    QFETCH(QStringList, effectsToLoad);
    for (const QString &effectName : effectsToLoad) {
        QVERIFY(!e->isEffectLoaded(effectName));
        QVERIFY(e->loadEffect(effectName));
        QVERIFY(e->isEffectLoaded(effectName));

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

    // the test created the slide protocol, let's create a Registry and listen for it
    std::unique_ptr<KWayland::Client::Registry> registry(new KWayland::Client::Registry);
    registry->create(Test::waylandConnection());

    QSignalSpy interfacesAnnouncedSpy(registry.get(), &KWayland::Client::Registry::interfacesAnnounced);
    registry->setup();
    QVERIFY(interfacesAnnouncedSpy.wait());
    auto slideInterface = registry->interface(KWayland::Client::Registry::Interface::Slide);
    QVERIFY(slideInterface.name != 0);
    std::unique_ptr<KWayland::Client::SlideManager> slideManager(registry->createSlideManager(slideInterface.name, slideInterface.version));
    QVERIFY(slideManager);

    // create Wayland window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface);
    std::unique_ptr<KWayland::Client::Slide> slide(slideManager->createSlide(surface.get()));
    slide->setLocation(KWayland::Client::Slide::Location::Left);
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

    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);

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
