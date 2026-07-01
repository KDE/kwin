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
    SceneOpenGLESTest()
        : GenericSceneOpenGLTest()
    {
    }
};

#ifdef MESA_GLES_VERSION_OVERRIDE
// this ensures the env var is set before the test application gets constructed
// and thus before the GpuManager is initialized
class EnvVarSetter
{
public:
    EnvVarSetter(const char *name, const char *value)
    {
        setenv(name, value, true);
    }
};
EnvVarSetter set("MESA_GLES_VERSION_OVERRIDE", MESA_GLES_VERSION_OVERRIDE);
#endif

WAYLANDTEST_MAIN(SceneOpenGLESTest)
#include "scene_opengl_test.moc"
