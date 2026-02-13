/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "generic_scene_opengl_test.h"
#include "compositor.h"
#include "core/renderbackend.h"
#include "effect/effectloader.h"
#include "wayland_server.h"
#include "window.h"

#include <KConfigGroup>
#include <QSignalSpy>

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
    if (!Test::renderNodeAvailable()) {
        QSKIP("no render node available");
        return;
    }
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("XCURSOR_THEME", QByteArrayLiteral("breeze_cursors"));
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    qputenv("KWIN_COMPOSE", m_envVariable);

    kwinApp()->start();
    Test::setOutputConfig({
        Rect(0, 0, 1280, 1024),
        Rect(1280, 0, 1280, 1024),
    });
    QVERIFY(Compositor::self());

    QCOMPARE(Compositor::self()->backend()->compositingType(), KWin::OpenGLCompositing);
}

#include "moc_generic_scene_opengl_test.cpp"
