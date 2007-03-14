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
#include <effects.h>


namespace KWinInternal
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


        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintScreen();

        virtual void windowClosed( EffectWindow* c );
        virtual void windowActivated( EffectWindow* c );
        virtual void windowInputMouseEvent( Window w, QEvent* e );

    public slots:
        void setActive(bool active);
        void toggleActive()  { setActive(!mActivated); }

    protected:
        // Updates window tranformations, i.e. destination pos and scale of the window
        void rearrangeWindows();
        void calculateWindowTransformationsDumb(ClientList clientlist);
        void calculateWindowTransformationsKompose(ClientList clientlist);

        // Helper methods for layout calculation
        float clientAspectRatio(Client* c);
        int clientWidthForHeight(Client* c, int h);
        int clientHeightForWidth(Client* c, int w);

        // Called once the effect is activated (and wasn't activated before)
        void effectActivated();
        // Called once the effect has terminated
        void effectTerminated();

    private:
        // Whether the effect is currently activated by the user
        bool mActivated;
        // 0 = not active, 1 = fully active
        float mActiveness;

        Window mInput;

        struct WindowData
            {
            QRect area;
            float scale;
            };
        QHash<Toplevel*, WindowData> mWindowData;
    };

} // namespace

#endif
