/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREENTRANSFORM_H
#define KWIN_SCREENTRANSFORM_H

#include <kwineffects.h>

namespace KWin
{
class GLRenderTarget;
class GLTexture;

class ScreenTransformEffect : public Effect
{
    Q_OBJECT

public:
    ScreenTransformEffect();
    ~ScreenTransformEffect() override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, KWin::ScreenPaintData &data) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 99;
    }
    static bool supported();

private:
    struct ScreenState {
        ~ScreenState();
        bool isSecondHalf() const
        {
            return m_timeLine.progress() > 0.5;
        }

        TimeLine m_timeLine;
        QSharedPointer<GLTexture> m_texture;
        std::chrono::milliseconds m_lastPresentTime = std::chrono::milliseconds::zero();
        EffectScreen::Transform m_oldTransform;
        qreal m_angle = 0;
        bool m_captured = false;
    };

    void addScreen(EffectScreen *screen);
    void removeScreen(EffectScreen *screen);
    bool isScreenTransforming(EffectScreen *screen) const;

    QHash<EffectScreen *, ScreenState> m_states;
};

} // namespace KWin
#endif // KWIN_SCREENTRANSFORM_H
