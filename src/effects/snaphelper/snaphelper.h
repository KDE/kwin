/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>

namespace KWin
{

class SnapHelperEffect : public Effect
{
    Q_OBJECT

public:
    SnapHelperEffect();
    ~SnapHelperEffect() override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;

    bool isActive() const override;

private Q_SLOTS:
    void slotWindowClosed(EffectWindow *w);
    void slotWindowStartUserMovedResized(EffectWindow *w);
    void slotWindowFinishUserMovedResized(EffectWindow *w);
    void slotWindowFrameGeometryChanged(EffectWindow *w, const QRectF &old);

private:
    QRectF m_geometry;
    EffectWindow *m_window = nullptr;

    struct Animation
    {
        bool active = false;
        TimeLine timeLine;
    };

    Animation m_animation;
};

} // namespace KWin
