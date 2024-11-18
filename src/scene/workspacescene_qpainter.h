/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "platformsupport/scenes/qpainter/qpainterbackend.h"

#include "scene/decorationitem.h"
#include "scene/shadowitem.h"
#include "scene/workspacescene.h"

namespace KWin
{

class KWIN_EXPORT WorkspaceSceneQPainter : public WorkspaceScene
{
    Q_OBJECT

public:
    explicit WorkspaceSceneQPainter(QPainterBackend *backend);
    ~WorkspaceSceneQPainter() override;

    std::unique_ptr<DecorationRenderer> createDecorationRenderer(Decoration::DecoratedWindowImpl *impl) override;
    std::unique_ptr<ShadowTextureProvider> createShadowTextureProvider(Shadow *shadow) override;

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

class QPainterShadowTextureProvider : public ShadowTextureProvider
{
public:
    explicit QPainterShadowTextureProvider(Shadow *shadow);

    void update() override;
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
    explicit SceneQPainterDecorationRenderer(Decoration::DecoratedWindowImpl *client);

    void render(const QRegion &region) override;

    QImage image(DecorationPart part) const;

private:
    void resizeImages();
    QImage m_images[int(DecorationPart::Count)];
};

} // KWin
