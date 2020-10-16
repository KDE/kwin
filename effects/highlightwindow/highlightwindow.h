/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_HIGHLIGHTWINDOW_H
#define KWIN_HIGHLIGHTWINDOW_H

#include <kwineffects.h>

namespace KWin
{

class HighlightWindowEffect
    : public Effect
{
    Q_OBJECT
public:
    HighlightWindowEffect();
    ~HighlightWindowEffect() override;

    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 70;
    }

    bool provides(Feature feature) override;
    bool perform(Feature feature, const QVariantList &arguments) override;

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow* w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotPropertyNotify(KWin::EffectWindow* w, long atom, EffectWindow *addedWindow = nullptr);

private:
    void prepareHighlighting();
    void finishHighlighting();

    void highlightWindows(const QVector<KWin::EffectWindow *> &windows);

    bool m_finishing;

    float m_fadeDuration;
    QHash<EffectWindow*, float> m_windowOpacity;

    long m_atom;
    QList<EffectWindow*> m_highlightedWindows;
    EffectWindow* m_monitorWindow;
    QList<WId> m_highlightedIds;

    // Offscreen position cache
    /*QRect m_thumbArea; // Thumbnail area
    QPoint m_arrowTip; // Position of the arrow's tip
    QPoint m_arrowA; // Arrow vertex position at the base (First)
    QPoint m_arrowB; // Arrow vertex position at the base (Second)

    // Helper functions
    inline double aspectRatio( EffectWindow *w )
        { return w->width() / double( w->height() ); }
    inline int widthForHeight( EffectWindow *w, int height )
        { return int(( height / double( w->height() )) * w->width() ); }
    inline int heightForWidth( EffectWindow *w, int width )
        { return int(( width / double( w->width() )) * w->height() ); }*/
};

} // namespace

#endif
