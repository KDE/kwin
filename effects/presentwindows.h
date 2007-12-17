/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#ifndef KWIN_PRESENTWINDOWS_H
#define KWIN_PRESENTWINDOWS_H

// Include with base class for effects.
#include <kwineffects.h>
#include <kwinglutils.h>

#include <QPixmap>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/extensions/Xrender.h>
#endif

namespace KWin
{

/**
 * Expose-like effect which shows all windows on current desktop side-by-side,
 *  letting the user select active window.
 **/
class PresentWindowsEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        PresentWindowsEffect();
        virtual ~PresentWindowsEffect();

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintScreen();

        virtual void windowClosed( EffectWindow* c );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual bool borderActivated( ElectricBorder border );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );

    public slots:
        void setActive(bool active);
        void toggleActive()  { mShowWindowsFromAllDesktops = false; setActive(!mActivated); }
        void toggleActiveAllDesktops()  { mShowWindowsFromAllDesktops = true; setActive(!mActivated); }

    protected:
        // Updates window tranformations, i.e. destination pos and scale of the window
        void rearrangeWindows();
        void calculateWindowTransformationsDumb(EffectWindowList windowlist);
        void calculateWindowTransformationsKompose(EffectWindowList windowlist);
        void calculateWindowTransformationsClosest(EffectWindowList windowlist);
        bool canRearrangeClosest(EffectWindowList windowlist);

        // Helper methods for layout calculation
        double windowAspectRatio(EffectWindow* c);
        int windowWidthForHeight(EffectWindow* c, int h);
        int windowHeightForWidth(EffectWindow* c, int w);

        void assignSlots( const QRect& area, int columns, int rows );
        void getBestAssignments();

        void updateFilterTexture();
        void discardFilterTexture();

        void paintWindowIcon( EffectWindow* w, WindowPaintData& data );

        void setHighlightedWindow( EffectWindow* w );
        EffectWindow* relativeWindow( EffectWindow* w, int xdiff, int ydiff, bool wrap ) const;
        EffectWindow* findFirstWindow() const;

        // Called once the effect is activated (and wasn't activated before)
        void effectActivated();
        // Called once the effect has terminated
        void effectTerminated();

    private:
        bool mShowWindowsFromAllDesktops;

        // Whether the effect is currently activated by the user
        bool mActivated;
        // 0 = not active, 1 = fully active
        double mActiveness;
        // 0 = start of rearranging (old_area), 1 = done
        double mRearranging;

        Window mInput;
        bool hasKeyboardGrab;

        EffectWindowList mWindowsToPresent;
        struct WindowData
            {
            QRect area;
            QRect old_area; // when rearranging, otherwise unset
            double scale;
            double old_scale; // when rearranging, otherwise unset
            double highlight;
            int slot;
            int x, y; // position of the slot in the grid
            int slot_distance;
            QPixmap icon;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
            GLTexture iconTexture;
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            Picture iconPicture;
#endif
            };
        typedef QHash<EffectWindow*, WindowData> DataHash;
        DataHash mWindowData;
        EffectWindow* mHighlightedWindow;
        
        QString windowFilter;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        GLTexture* filterTexture;
        QRect filterTextureRect;
        QRect filterFrameRect;
#endif

        ElectricBorder borderActivate;
        ElectricBorder borderActivateAll;

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        XRenderPictFormat* alphaFormat;
#endif
    };

} // namespace

#endif
