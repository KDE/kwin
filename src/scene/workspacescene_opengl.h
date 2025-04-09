/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/workspacescene.h"

#include "opengl/glutils.h"

namespace KWin
{

class EglBackend;

class KWIN_EXPORT WorkspaceSceneOpenGL : public WorkspaceScene
{
    Q_OBJECT

public:
    explicit WorkspaceSceneOpenGL(EglBackend *backend);
    ~WorkspaceSceneOpenGL() override;

    EglContext *openglContext() const override;
    bool animationsSupported() const override;

private:
    EglBackend *m_backend;
};

} // namespace
