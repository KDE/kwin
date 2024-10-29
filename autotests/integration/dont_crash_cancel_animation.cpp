/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "compositor.h"
#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "scripting/scriptedeffect.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#if KWIN_BUILD_X11
#include "x11window.h"
#endif

#include <KDecoration3/Decoration>

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <QSignalSpy>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_dont_crash_cancel_animation-0");

class DontCrashCancelAnimationFromAnimationEndedTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testScript();
};

void DontCrashCancelAnimationFromAnimationEndedTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    kwinApp()->start();
    QVERIFY(Compositor::self());
    QVERIFY(effects);
}

void DontCrashCancelAnimationFromAnimationEndedTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void DontCrashCancelAnimationFromAnimationEndedTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void DontCrashCancelAnimationFromAnimationEndedTest::testScript()
{
    // load a scripted effect which deletes animation data
    ScriptedEffect *effect = ScriptedEffect::create(QStringLiteral("crashy"), QFINDTESTDATA("data/anim-data-delete-effect/effect.js"), 10, QString());
    QVERIFY(effect);

    const auto children = effects->children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "KWin::EffectLoader") != 0) {
            continue;
        }
        QVERIFY(QMetaObject::invokeMethod(*it, "effectLoaded", Q_ARG(KWin::Effect *, effect), Q_ARG(QString, QStringLiteral("crashy"))));
        break;
    }
    QVERIFY(effects->isEffectLoaded(QStringLiteral("crashy")));

    // create a window
    std::unique_ptr<KWayland::Client::Surface> surface{Test::createSurface()};
    QVERIFY(surface);
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    QVERIFY(shellSurface);
    // let's render
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);

    // make sure we animate
    QTest::qWait(200);

    // wait for the window to be passed to Deleted
    QSignalSpy windowDeletedSpy(window, &Window::closed);

    surface.reset();

    QVERIFY(windowDeletedSpy.wait());
    // make sure we animate
    QTest::qWait(200);
}

}

WAYLANDTEST_MAIN(KWin::DontCrashCancelAnimationFromAnimationEndedTest)
#include "dont_crash_cancel_animation.moc"
