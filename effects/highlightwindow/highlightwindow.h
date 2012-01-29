/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

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
    virtual ~HighlightWindowEffect();

    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual bool isActive() const;

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow* w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotPropertyNotify(KWin::EffectWindow* w, long atom);

private:
    void prepareHighlighting();
    void finishHighlighting();

    bool m_finishing;

    float m_fadeDuration;
    QHash<EffectWindow*, float> m_windowOpacity;

    long m_atom;
    QList<EffectWindow*> m_highlightedWindows;
    EffectWindow* m_monitorWindow;

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
