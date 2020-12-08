/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "generic_scene_opengl_test.h"

class SceneOpenGLESTest : public GenericSceneOpenGLTest
{
    Q_OBJECT
public:
    SceneOpenGLESTest() : GenericSceneOpenGLTest(QByteArrayLiteral("O2ES")) {}
};

WAYLANDTEST_MAIN(SceneOpenGLESTest)
#include "scene_opengl_es_test.moc"
