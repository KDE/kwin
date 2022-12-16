/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinoffscreeneffect.h>

namespace KWin
{

class GLShader;

/**
 * Inverts desktop's colors
 */
class InvertEffect : public OffscreenEffect
{
    Q_OBJECT
public:
    InvertEffect();
    ~InvertEffect() override;

    bool isActive() const override;
    bool provides(Feature) override;
    int requestedEffectChainPosition() const override;

    static bool supported();

public Q_SLOTS:
    void toggleScreenInversion();
    void toggleWindow();

    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowClosed(KWin::EffectWindow *w);

protected:
    bool loadData();

private:
    bool isInvertable(EffectWindow *window) const;
    void invert(EffectWindow *window);
    void uninvert(EffectWindow *window);

    bool m_inited;
    bool m_valid;
    std::unique_ptr<GLShader> m_shader;
    bool m_allWindows;
    QList<EffectWindow *> m_windows;
};

inline int InvertEffect::requestedEffectChainPosition() const
{
    return 99;
}

} // namespace
