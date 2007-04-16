/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "demo_showpicture.h"

#include <kstandarddirs.h>
#include <qimage.h>

namespace KWin
{

KWIN_EFFECT( Demo_ShowPicture, ShowPictureEffect )

ShowPictureEffect::ShowPictureEffect()
    : init( true )
    , picture( NULL )
    {
    }

ShowPictureEffect::~ShowPictureEffect()
    {
    delete picture;
    }

void ShowPictureEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
#ifdef HAVE_OPENGL
    if( init )
        {
        loadPicture();
        init = false;
        }
    if( picture && region.intersects( pictureRect ))
        {
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
        picture->bind();
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        glEnableClientState( GL_VERTEX_ARRAY );
        int x = pictureRect.x();
        int y = pictureRect.y();
        int width = pictureRect.width();
        int height = pictureRect.height();
        int verts[ 4 * 2 ] =
            {
            x, y,
            x, y + height,
            x + width, y + height,
            x + width, y
            };
        glVertexPointer( 2, GL_INT, 0, verts );
        glEnableClientState( GL_TEXTURE_COORD_ARRAY );
        int texcoords[ 4 * 2 ] =
            {
            0, 1,
            0, 0,
            1, 0,
            1, 1
            };
        glTexCoordPointer( 2, GL_INT, 0, texcoords );
        glDrawArrays( GL_QUADS, 0, 4 );
        glDisableClientState( GL_TEXTURE_COORD_ARRAY );
        glDisableClientState( GL_VERTEX_ARRAY );
        picture->unbind();
        glPopAttrib();
        }
#endif
    }

void ShowPictureEffect::loadPicture()
    {
#ifdef HAVE_OPENGL
    QString file = KGlobal::dirs()->findResource( "appdata", "showpicture.png" );
    if( file.isEmpty())
        return;
    QImage im( file );
    QRect area = effects->clientArea( PlacementArea, QPoint( 0, 0 ), effects->currentDesktop());
    picture = new GLTexture( im );
    pictureRect = QRect( area.x() + ( area.width() - im.width()) / 2,
        area.y() + ( area.height() - im.height()) / 2, im.width(), im.height());
#endif
    }

} // namespace
