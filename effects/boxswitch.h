/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

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

#ifndef KWIN_BOXSWITCH_H
#define KWIN_BOXSWITCH_H

#include <kwineffects.h>

#include <QHash>
#include <QPixmap>
#include <QRect>
#include <QRegion>
#include <QSize>
#include <QFont>

#include <kwinglutils.h>
#include <kwinxrenderutils.h>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/extensions/Xrender.h>
#endif

namespace KWin
{

class BoxSwitchEffect
    : public Effect
    {
    public:
        BoxSwitchEffect();
        ~BoxSwitchEffect();

        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );

        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void windowDamaged( EffectWindow* w, const QRect& damage );
        virtual void windowGeometryShapeChanged( EffectWindow* w, const QRect& old );
        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();
        virtual void windowClosed( EffectWindow* w );
    private:
        class ItemInfo;
        void setActive();
        void setInactive();
        void moveResizeInputWindow( int x, int y, int width, int height );
        void calculateFrameSize();
        void calculateItemSizes();
        void setSelectedWindow( EffectWindow* w );

        void paintFrame();
        void paintHighlight( QRect area );
        void paintWindowThumbnail( EffectWindow* w );
        void paintDesktopThumbnail( int iDesktop );
        void paintWindowIcon( EffectWindow* w );
        void paintText( const QString& text );

        bool mActivated;
        Window mInput;
        int mMode;

        QRect frame_area;
        int frame_margin; // TODO graphical background
        int highlight_margin; // TODO graphical background
        QSize item_max_size; // maximum item display size (including highlight)
        QRect text_area;
        QFont text_font;
        QColor color_frame;
        QColor color_highlight;
        QColor color_text;

        QHash< EffectWindow*, ItemInfo* > windows;
        EffectWindowList original_windows;
        EffectWindow* selected_window;
        QHash< int, ItemInfo* > desktops;
        QList< int > original_desktops;
        int selected_desktop;

        int painting_desktop;

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
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
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        GLTexture iconTexture;
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        XRenderPicture iconPicture;
#endif
    };

} // namespace

#endif
