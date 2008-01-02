/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include "mousemark.h"

#include <kwinconfig.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kconfiggroup.h>

#include <math.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
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
    a->setText( i18n( "Clear Mouse Marks" ));
    a->setGlobalShortcut( KShortcut( Qt::SHIFT + Qt::META + Qt::Key_F11 ));
    connect( a, SIGNAL( triggered( bool )), this, SLOT( clear()));

    KConfigGroup conf = EffectsHandler::effectConfig("MouseMark");
    width = conf.readEntry( "LineWidth", 3 );
    color = conf.readEntry( "Color", QColor( Qt::red ));
    }

void MouseMarkEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data ); // paint normal screen
    if( marks.isEmpty() && drawing.isEmpty())
        return;
    glPushAttrib( GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT );
    glColor4f( color.redF(), color.greenF(), color.blueF(), 1 );
    glEnable( GL_LINE_SMOOTH );
    glLineWidth( width );
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
    Qt::MouseButtons, Qt::MouseButtons,
    Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers )
    {
    if( modifiers == ( Qt::META | Qt::SHIFT )) // activated
        {
        if( drawing.isEmpty())
            drawing.append( pos );
        if( drawing.last() == pos )
            return;
        QPoint pos2 = drawing.last();
        drawing.append( pos );
        QRect repaint = QRect( qMin( pos.x(), pos2.x()), qMin( pos.y(), pos2.y()),
                               qMax( pos.x(), pos2.x()), qMax( pos.y(), pos2.y()));
        repaint.adjust( -width, -width, width, width );
        effects->addRepaint( repaint );
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
