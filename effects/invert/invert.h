/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_INVERT_H
#define KWIN_INVERT_H

#include <kwineffects.h>

namespace KWin
{

class GLShader;

/**
 * Inverts desktop's colors
 */
class InvertEffect
    : public Effect
{
    Q_OBJECT
public:
    InvertEffect();
    ~InvertEffect() override;

    void drawWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data) override;
    void paintEffectFrame(KWin::EffectFrame* frame, const QRegion &region, double opacity, double frameOpacity) override;
    bool isActive() const override;
    bool provides(Feature) override;

    int requestedEffectChainPosition() const override;

    static bool supported();

public Q_SLOTS:
    void toggleScreenInversion();
    void toggleWindow();
    void slotWindowClosed(KWin::EffectWindow *w);

protected:
    bool loadData();

private:
    bool m_inited;
    bool m_valid;
    GLShader* m_shader;
    bool m_allWindows;
    QList<EffectWindow*> m_windows;
};

inline int InvertEffect::requestedEffectChainPosition() const
{
    return 99;
}

} // namespace

#endif
