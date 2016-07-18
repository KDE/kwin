/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "composite.h"
#include "effectloader.h"
#include "cursor.h"
#include "platform.h"
#include "scene_opengl.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "effect_builtins.h"

#include <KConfigGroup>

using namespace KWin;
static const QString s_socketName = QStringLiteral("wayland_test_kwin_scene_opengl-0");

class SceneOpenGLTest : public QObject
{
Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testRestart();
};

void SceneOpenGLTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void SceneOpenGLTest::initTestCase()
{
    if (!QFile::exists(QStringLiteral("/dev/dri/card0"))) {
        QSKIP("Needs a dri device");
    }
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    ScriptedEffectLoader loader;
    const auto builtinNames = BuiltInEffects::availableEffectNames() << loader.listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("XCURSOR_THEME", QByteArrayLiteral("DMZ-White"));
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("Q"));
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QVERIFY(Compositor::self());
}

void SceneOpenGLTest::testRestart()
{
    // simple restart of the OpenGL compositor without any windows being shown
    QSignalSpy sceneCreatedSpy(KWin::Compositor::self(), &Compositor::sceneCreated);
    QVERIFY(sceneCreatedSpy.isValid());
    KWin::Compositor::self()->slotReinitialize();
    if (sceneCreatedSpy.isEmpty()) {
        QVERIFY(sceneCreatedSpy.wait());
    }
    QCOMPARE(sceneCreatedSpy.count(), 1);
    auto scene = qobject_cast<SceneOpenGL*>(KWin::Compositor::self()->scene());
    QVERIFY(scene);

    // trigger a repaint
    KWin::Compositor::self()->addRepaintFull();
    // and wait 100 msec to ensure it's rendered
    // TODO: introduce frameRendered signal in SceneOpenGL
    QTest::qWait(100);
}

WAYLANDTEST_MAIN(SceneOpenGLTest)
#include "scene_opengl_test.moc"
