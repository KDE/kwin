/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_BOXSWITCH_H
#define KWIN_BOXSWITCH_H

#include <effects.h>
#include <scene_xrender.h>
#include <scene_opengl.h>

#include <QHash>
#include <QPixmap>
#include <QRect>
#include <QRegion>
#include <QSize>

namespace KWinInternal
{

class BoxSwitchEffect
    : public Effect
    {
    public:
        BoxSwitchEffect();
        ~BoxSwitchEffect();

        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );

        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void windowDamaged( EffectWindow* w, const QRect& damage );
        virtual void windowGeometryShapeChanged( EffectWindow* w, const QRect& old );
        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();
    private:
        class ItemInfo;
        void setActive();
        void setInactive();
        void moveResizeInputWindow( int x, int y, int width, int height );
        void calculateFrameSize();
        void calculateItemSizes();

        void paintFrame();
        void paintHighlight( QRect area, QString text );
        void paintDesktopThumbnail( int iDesktop );
        void paintWindowThumbnail( EffectWindow* w );
        void paintWindowIcon( EffectWindow* w );

        bool mActivated;
        Window mInput;
        int mMode;

        QRect frame_area;
        int frame_margin;

        QSize item_max_size; // maximum item display size (including highlight)
        int highlight_margin; // TODO graphical background, highlight

        QHash< int, ItemInfo* > desktops;
        QHash< EffectWindow*, ItemInfo* > windows;

        int selected_desktop;
        ClientList tab_clients;
        EffectWindow* selected_window;

        int painting_desktop;

#ifdef HAVE_XRENDER
        XRenderPictFormat* alphaFormat;
#endif
    };

class BoxSwitchEffect::ItemInfo
    {
    public:
        QRect area; // maximal painting area, including any frames/highlights/etc.
        QRegion clickable;
        QRect thumbnail;
        QPixmap icon;
#ifdef HAVE_OPENGL
        GLTexture iconTexture;
#endif
#ifdef HAVE_XRENDER
        Picture iconPicture;
#endif
    };

} // namespace

#endif
