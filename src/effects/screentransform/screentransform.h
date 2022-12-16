/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwineffects.h>

namespace KWin
{
class GLFramebuffer;
class GLShader;
class GLTexture;

class ScreenTransformEffect : public Effect
{
    Q_OBJECT

public:
    ScreenTransformEffect();
    ~ScreenTransformEffect() override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, KWin::ScreenPaintData &data) override;

    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 99;
    }
    static bool supported();

private:
    struct Snapshot
    {
        std::shared_ptr<GLTexture> texture;
        std::shared_ptr<GLFramebuffer> framebuffer;
    };

    struct ScreenState
    {
        TimeLine m_timeLine;
        Snapshot m_prev;
        Snapshot m_current;
        QRect m_oldGeometry;
        EffectScreen::Transform m_oldTransform;
        qreal m_angle = 0;
        bool m_captured = false;
    };

    void addScreen(EffectScreen *screen);
    void removeScreen(EffectScreen *screen);

    QHash<EffectScreen *, ScreenState> m_states;

    std::unique_ptr<GLShader> m_shader;
    int m_previousTextureLocation = -1;
    int m_currentTextureLocation = -1;
    int m_modelViewProjectioMatrixLocation = -1;
    int m_blendFactorLocation = -1;
};

} // namespace KWin
