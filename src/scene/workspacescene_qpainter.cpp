/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "workspacescene_qpainter.h"
// KWin
#include "decorations/decoratedwindow.h"
#include "scene/itemrenderer_qpainter.h"
#include "window.h"

// Qt
#include <KDecoration3/Decoration>
#include <QDebug>
#include <QPainter>

#include <cmath>

namespace KWin
{

//****************************************
// SceneQPainter
//****************************************

WorkspaceSceneQPainter::WorkspaceSceneQPainter(QPainterBackend *backend)
    : WorkspaceScene(std::make_unique<ItemRendererQPainter>())
    , m_backend(backend)
{
}

WorkspaceSceneQPainter::~WorkspaceSceneQPainter()
{
}

//****************************************
// QPainterShadow
//****************************************

//****************************************
// QPainterDecorationRenderer
//****************************************

} // KWin

#include "moc_workspacescene_qpainter.cpp"
