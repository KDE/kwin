/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_XRENDERUTILS_H
#define KWIN_XRENDERUTILS_H

#include <kwinconfig.h>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING

#include <QtCore/QSharedData>
#include <QtGui/QColor>
#include <ksharedptr.h>

#include <kwinglobals.h>

#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>

/** @addtogroup kwineffects */
/** @{ */

namespace KWin
{

/**
 * Convert QRegion to XserverRegion.
 */
KWIN_EXPORT XserverRegion toXserverRegion( QRegion region );
/**
 * draws a round box on the renderscene
 */
KWIN_EXPORT void xRenderRoundBox( Picture pict, const QRect &rect, int round, const QColor &c );
/**
 * dumps a QColor into a XRenderColor
 */
KWIN_EXPORT XRenderColor preMultiply(const QColor &c, float opacity = 1.0);

/** @internal */
class KWIN_EXPORT XRenderPictureData
    : public QSharedData
    {
    public:
        XRenderPictureData( Picture pic = None );
        ~XRenderPictureData();
        Picture value();
    private:
        Picture picture;
        Q_DISABLE_COPY( XRenderPictureData )
    };

/**
 * @short Wrapper around XRender Picture.
 *
 * This class wraps XRender's Picture, providing proper initialization,
 * convenience constructors and freeing of resources.
 * It should otherwise act exactly like the Picture type.
 */
class KWIN_EXPORT XRenderPicture
    {
    public:
        XRenderPicture( Picture pic = None );
        XRenderPicture( QPixmap pix );
        XRenderPicture( Pixmap pix, int depth );
        operator Picture();
    private:
        KSharedPtr< XRenderPictureData > d;
    };

inline
XRenderPictureData::XRenderPictureData( Picture pic )
    : picture( pic )
    {
    }

inline
XRenderPictureData::~XRenderPictureData()
    {
    if( picture != None )
        XRenderFreePicture( display(), picture );
    }

inline
Picture XRenderPictureData::value()
    {
    return picture;
    }

inline
XRenderPicture::XRenderPicture( Picture pic )
    : d( new XRenderPictureData( pic ))
    {
    }

inline
XRenderPicture::operator Picture()
    {
    return d->value();
    }

/**
 * Static 1x1 picture used to deliver a black pixel with given opacity (for blending performance)
 * Call and Use, the PixelPicture will stay, but may change it's opacity meanwhile. It's NOT threadsafe either
 */
KWIN_EXPORT XRenderPicture xRenderBlendPicture( double opacity );
/**
 * Creates a 1x1 Picture filled with c
 */
KWIN_EXPORT XRenderPicture xRenderFill( const XRenderColor *c );
KWIN_EXPORT XRenderPicture xRenderFill( const QColor &c );

} // namespace

#endif

/** @} */

#endif
