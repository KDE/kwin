/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    Based on glcompmgr code by Felix Bellaby.
    Using code from Compiz and Beryl.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "workspacescene_opengl.h"
#include "compositor.h"
#include "core/output.h"
#include "decorations/decoratedwindow.h"
#include "platformsupport/scenes/opengl/eglbackend.h"
#include "scene/itemrenderer_opengl.h"
#include "shadow.h"
#include "window.h"

#include <cmath>
#include <cstddef>

#include <QMatrix4x4>
#include <QPainter>
#include <QStringList>
#include <QVector2D>
#include <QVector4D>
#include <QtMath>

namespace KWin
{

/************************************************
 * SceneOpenGL
 ***********************************************/

WorkspaceSceneOpenGL::WorkspaceSceneOpenGL(EglBackend *backend)
    : WorkspaceScene(std::make_unique<ItemRendererOpenGL>(backend->eglDisplayObject()))
    , m_backend(backend)
{
}

WorkspaceSceneOpenGL::~WorkspaceSceneOpenGL()
{
    openglContext()->makeCurrent();
}

EglContext *WorkspaceSceneOpenGL::openglContext() const
{
    return m_backend->openglContext();
}

bool WorkspaceSceneOpenGL::animationsSupported() const
{
    const auto context = openglContext();
    return context && !context->isSoftwareRenderer();
}

std::pair<std::shared_ptr<GLTexture>, ColorDescription> WorkspaceSceneOpenGL::textureForOutput(Output *output) const
{
    return m_backend->textureForOutput(output);
}

} // namespace

#include "moc_workspacescene_opengl.cpp"
