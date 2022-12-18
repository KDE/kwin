/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "generic_scene_opengl_test.h"
#include "composite.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "cursor.h"
#include "effectloader.h"
#include "scene/workspacescene.h"
#include "wayland_server.h"
#include "window.h"

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
    qRegisterMetaType<KWin::Window *>();
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

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("XCURSOR_THEME", QByteArrayLiteral("DMZ-White"));
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    qputenv("KWIN_COMPOSE", m_envVariable);

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(Compositor::self());

    QCOMPARE(Compositor::self()->backend()->compositingType(), KWin::OpenGLCompositing);
}

void GenericSceneOpenGLTest::testRestart()
{
    // simple restart of the OpenGL compositor without any windows being shown
    QSignalSpy sceneCreatedSpy(KWin::Compositor::self(), &Compositor::sceneCreated);
    KWin::Compositor::self()->reinitialize();
    if (sceneCreatedSpy.isEmpty()) {
        QVERIFY(sceneCreatedSpy.wait());
    }
    QCOMPARE(sceneCreatedSpy.count(), 1);
    QCOMPARE(Compositor::self()->backend()->compositingType(), KWin::OpenGLCompositing);

    // trigger a repaint
    KWin::Compositor::self()->scene()->addRepaintFull();
    // and wait 100 msec to ensure it's rendered
    // TODO: introduce frameRendered signal in SceneOpenGL
    QTest::qWait(100);
}
