/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "mousemark.h"

#include <config-X11.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>

#include <math.h>

#ifdef HAVE_OPENGL
#include <GL/gl.h>
#endif

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( mousemark, MouseMarkEffect )

MouseMarkEffect::MouseMarkEffect()
    {
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = static_cast< KAction* >( actionCollection->addAction( "ClearMouseMarks" ));
    a->setText( i18n( "ClearMouseMarks" ));
    a->setGlobalShortcut( KShortcut( Qt::SHIFT + Qt::META + Qt::Key_F11 ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( clear()));
    }

void MouseMarkEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data ); // paint normal screen
    if( marks.isEmpty() && drawing.isEmpty())
        return;
    glPushAttrib( GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT );
    glColor4f( 1, 0, 0, 1 ); // red
    glEnable( GL_LINE_SMOOTH );
    glLineWidth( 3 );
    foreach( const Mark& mark, marks )
        {
        glBegin( GL_LINE_STRIP );
        foreach( const QPoint& p, mark )
            glVertex2i( p.x(), p.y());
        glEnd();
        }
    if( !drawing.isEmpty())
        {
        glBegin( GL_LINE_STRIP );
        foreach( const QPoint& p, drawing )
            glVertex2i( p.x(), p.y());
        glEnd();
        }
    glPopAttrib();
    }

void MouseMarkEffect::mouseChanged( const QPoint& pos, const QPoint&,
    Qt::MouseButtons, Qt::KeyboardModifiers modifiers )
    {
    if( modifiers == ( Qt::META | Qt::SHIFT )) // activated
        {
        if( drawing.isEmpty())
            drawing.append( pos );
        if( drawing.last() == pos )
            return;
        QPoint pos2 = drawing.last();
        drawing.append( pos );
        effects->addRepaint( QRect( qMin( pos.x(), pos2.x()), qMin( pos.y(), pos2.y()),
            qMax( pos.x(), pos2.x()), qMax( pos.y(), pos2.y())));
        }
    else if( !drawing.isEmpty())
        {
        marks.append( drawing );
        drawing.clear();
        }
    }

void MouseMarkEffect::clear()
    {
    drawing.clear();
    marks.clear();
    effects->addRepaintFull();
    }

} // namespace

#include "mousemark.moc"
