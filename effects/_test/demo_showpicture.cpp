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

KWIN_EFFECT( demo_showpicture, ShowPictureEffect )

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
        picture->render( region, pictureRect );
        picture->unbind();
        glPopAttrib();
        }
    }

void ShowPictureEffect::loadPicture()
    {
    QString file = KGlobal::dirs()->findResource( "appdata", "showpicture.png" );
    if( file.isEmpty())
        return;
    QImage im( file );
    QRect area = effects->clientArea( PlacementArea, effects->activeScreen(), effects->currentDesktop());
    picture = new GLTexture( im );
    pictureRect = QRect( area.x() + ( area.width() - im.width()) / 2,
        area.y() + ( area.height() - im.height()) / 2, im.width(), im.height());
    }

} // namespace
