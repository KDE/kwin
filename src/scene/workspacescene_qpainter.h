/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "qpainterbackend.h"

#include "scene/decorationitem.h"
#include "scene/workspacescene.h"
#include "shadow.h"

namespace KWin
{

class KWIN_EXPORT WorkspaceSceneQPainter : public WorkspaceScene
{
    Q_OBJECT

public:
    explicit WorkspaceSceneQPainter(QPainterBackend *backend);
    ~WorkspaceSceneQPainter() override;

    std::unique_ptr<Shadow> createShadow(Window *window) override;
    DecorationRenderer *createDecorationRenderer(Decoration::DecoratedClientImpl *impl) override;

    bool animationsSupported() const override
    {
        return false;
    }

    QPainterBackend *backend() const
    {
        return m_backend;
    }

private:
    QPainterBackend *m_backend;
};

class SceneQPainterShadow : public Shadow
{
public:
    SceneQPainterShadow(Window *window);
    ~SceneQPainterShadow() override;

protected:
    bool prepareBackend() override;
};

class SceneQPainterDecorationRenderer : public DecorationRenderer
{
    Q_OBJECT
public:
    enum class DecorationPart : int {
        Left,
        Top,
        Right,
        Bottom,
        Count
    };
    explicit SceneQPainterDecorationRenderer(Decoration::DecoratedClientImpl *client);

    void render(const QRegion &region) override;

    QImage image(DecorationPart part) const;

private:
    void resizeImages();
    QImage m_images[int(DecorationPart::Count)];
};

} // KWin
