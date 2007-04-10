/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_PRESENTWINDOWS_H
#define KWIN_PRESENTWINDOWS_H

// Include with base class for effects.
#include <kwineffects.h>


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

        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintScreen();

        virtual void windowClosed( EffectWindow* c );
        virtual void windowActivated( EffectWindow* c );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual bool borderActivated( ElectricBorder border );

    public slots:
        void setActive(bool active);
        void toggleActive()  { mShowWindowsFromAllDesktops = false; setActive(!mActivated); }
        void toggleActiveAllDesktops()  { mShowWindowsFromAllDesktops = true; setActive(!mActivated); }

    protected:
        // Updates window tranformations, i.e. destination pos and scale of the window
        void rearrangeWindows();
        void calculateWindowTransformationsDumb(EffectWindowList windowlist);
        void calculateWindowTransformationsKompose(EffectWindowList windowlist);

        // Helper methods for layout calculation
        float windowAspectRatio(EffectWindow* c);
        int windowWidthForHeight(EffectWindow* c, int h);
        int windowHeightForWidth(EffectWindow* c, int w);

        // Called once the effect is activated (and wasn't activated before)
        void effectActivated();
        // Called once the effect has terminated
        void effectTerminated();

    private:
        bool mShowWindowsFromAllDesktops;

        // Whether the effect is currently activated by the user
        bool mActivated;
        // 0 = not active, 1 = fully active
        float mActiveness;

        Window mInput;

        struct WindowData
            {
            QRect area;
            float scale;
            float hover;
            };
        QHash<EffectWindow*, WindowData> mWindowData;

        ElectricBorder borderActivate;
        ElectricBorder borderActivateAll;
    };

} // namespace

#endif
