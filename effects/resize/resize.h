/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_RESIZE_H
#define KWIN_RESIZE_H

#include <kwinanimationeffect.h>

namespace KWin
{

class ResizeEffect
    : public AnimationEffect
{
    Q_OBJECT
    Q_PROPERTY(bool textureScale READ isTextureScale)
    Q_PROPERTY(bool outline READ isOutline)
public:
    ResizeEffect();
    ~ResizeEffect() override;
    inline bool provides(Effect::Feature ef) override {
        return ef == Effect::Resize;
    }
    inline bool isActive() const override { return m_active || AnimationEffect::isActive(); }
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void reconfigure(ReconfigureFlags) override;

    int requestedEffectChainPosition() const override {
        return 60;
    }

    bool isTextureScale() const {
        return m_features & TextureScale;
    }
    bool isOutline() const {
        return m_features & Outline;
    }

public Q_SLOTS:
    void slotWindowStartUserMovedResized(KWin::EffectWindow *w);
    void slotWindowStepUserMovedResized(KWin::EffectWindow *w, const QRect &geometry);
    void slotWindowFinishUserMovedResized(KWin::EffectWindow *w);

private:
    enum Feature { TextureScale = 1 << 0, Outline = 1 << 1 };
    bool m_active;
    int m_features;
    EffectWindow* m_resizeWindow;
    QRect m_currentGeometry, m_originalGeometry;
};

}

#endif
