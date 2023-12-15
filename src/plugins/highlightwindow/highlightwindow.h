/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"

#include "effect/animationeffect.h"

namespace KWin
{

class HighlightWindowEffect : public AnimationEffect
{
    Q_OBJECT

public:
    HighlightWindowEffect();
    ~HighlightWindowEffect() override;

    int requestedEffectChainPosition() const override
    {
        return 70;
    }

    bool provides(Feature feature) override;
    bool perform(Feature feature, const QVariantList &arguments) override;
    Q_SCRIPTABLE void highlightWindows(const QStringList &windows);

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
#if KWIN_BUILD_X11
    void slotPropertyNotify(KWin::EffectWindow *w, long atom, EffectWindow *addedWindow = nullptr);
#endif

private:
    quint64 startGhostAnimation(EffectWindow *window);
    quint64 startHighlightAnimation(EffectWindow *window);
    void startRevertAnimation(EffectWindow *window);

    bool isHighlighted(EffectWindow *window) const;

    void prepareHighlighting();
    void finishHighlighting();
    void highlightWindows(const QList<KWin::EffectWindow *> &windows);

#if KWIN_BUILD_X11
    long m_atom;
#endif
    QList<EffectWindow *> m_highlightedWindows;
    QHash<EffectWindow *, quint64> m_animations;
    QEasingCurve m_easingCurve;
    int m_fadeDuration;
    EffectWindow *m_monitorWindow;
    QList<WId> m_highlightedIds;
    float m_ghostOpacity = 0;
};

} // namespace
