/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_DEBUGEFFECT_H
#define KWIN_DEBUGEFFECT_H

#include <kwineffects.h>

namespace KWin
{

class GLShader;

class DebugEffect
    : public Effect
{
    Q_OBJECT
public:
    DebugEffect();
    ~DebugEffect() override;

    void drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override;

    static bool supported();

public Q_SLOTS:
    void toggleDebugFractional();

protected:
    bool loadData();

private:
    bool m_inited = false;
    bool m_valid = true;
    std::unique_ptr<GLShader> m_debugFractionalShader;
    bool m_debugFractionalEnabled = false;
};

inline int DebugEffect::requestedEffectChainPosition() const
{
    return 999;
}

} // namespace

#endif
