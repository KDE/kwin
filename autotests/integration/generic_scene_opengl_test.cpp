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
#include "generic_scene_opengl_test.h"
#include "composite.h"
#include "effectloader.h"
#include "cursor.h"
#include "platform.h"
#include "scene.h"
#include "xdgshellclient.h"
#include "wayland_server.h"
#include "effect_builtins.h"

#include <KConfigGroup>

using namespace KWin;
static const QString s_socketName = QStringLiteral("wayland_test_kwin_scene_opengl-0");

GenericSceneOpenGLTest::GenericSceneOpenGLTest(const QByteArray &envVariable)
    : QObject()
    , m_envVariable(envVariable)
{
}

GenericSceneOpenGLTest::~GenericSceneOpenGLTest()
{
}

void GenericSceneOpenGLTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void GenericSceneOpenGLTest::initTestCase()
{
    qRegisterMetaType<KWin::XdgShellClient *>();
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
    qputenv("KWIN_COMPOSE", m_envVariable);

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QVERIFY(Compositor::self());

    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);
    QCOMPARE(scene->compositingType(), KWin::OpenGL2Compositing);
    QCOMPARE(kwinApp()->platform()->selectedCompositor(), KWin::OpenGLCompositing);
}

void GenericSceneOpenGLTest::testRestart_data()
{
    QTest::addColumn<bool>("core");

    QTest::newRow("GLCore") << true;
    QTest::newRow("Legacy") << false;
}

void GenericSceneOpenGLTest::testRestart()
{
    // simple restart of the OpenGL compositor without any windows being shown

    // setup opengl compositing options
    auto compositingGroup = kwinApp()->config()->group("Compositing");
    QFETCH(bool, core);
    compositingGroup.writeEntry("GLCore", core);
    compositingGroup.sync();

    QSignalSpy sceneCreatedSpy(KWin::Compositor::self(), &Compositor::sceneCreated);
    QVERIFY(sceneCreatedSpy.isValid());
    KWin::Compositor::self()->reinitialize();
    if (sceneCreatedSpy.isEmpty()) {
        QVERIFY(sceneCreatedSpy.wait());
    }
    QCOMPARE(sceneCreatedSpy.count(), 1);
    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);
    QCOMPARE(scene->compositingType(), KWin::OpenGL2Compositing);
    QCOMPARE(kwinApp()->platform()->selectedCompositor(), KWin::OpenGLCompositing);

    // trigger a repaint
    KWin::Compositor::self()->addRepaintFull();
    // and wait 100 msec to ensure it's rendered
    // TODO: introduce frameRendered signal in SceneOpenGL
    QTest::qWait(100);
}
