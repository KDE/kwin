/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2009 Lubos Lunak <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#include "paintredirector.h"

#include <kdebug.h>
#include <qevent.h>
#include <qpainter.h>

namespace KWin
{

PaintRedirector::PaintRedirector( QWidget* w )
    : widget( w )
    , recursionCheck( false )
    {
    timer.setSingleShot( true );
    connect( &timer, SIGNAL( timeout()), SIGNAL( paintPending()));
    added( w );
    }

QPixmap PaintRedirector::performPendingPaint()
    {
    //qDebug() << "### performing paint, pending:" << pending.boundingRect();
    QPixmap pixmap( pending.boundingRect().size());
    pixmap.fill( Qt::transparent );
    recursionCheck = true;
    // do not use DrawWindowBackground, it's ok to be transparent
    widget->render( &pixmap, QPoint(), pending.boundingRect(), QWidget::DrawChildren );
    recursionCheck = false;
    pending = QRegion();
    return pixmap;
    }

bool PaintRedirector::isToolTip( QWidget *object ) const
    {
    // ### We need a more reliable way of doing this
    return object->metaObject()->className() == QString("QWidget") &&
           object->objectName() != QString("decoration widget");
    }

bool PaintRedirector::eventFilter( QObject* o, QEvent* e )
    {
    switch( e->type())
        {
        case QEvent::ChildAdded:
            {
            QChildEvent* c = static_cast< QChildEvent* >( e );
            if( c->child()->isWidgetType() && !isToolTip( static_cast< QWidget* >( c->child() ) ) )
                added( static_cast< QWidget* >( c->child()));
            break;
            }
        case QEvent::ChildRemoved:
            {
            QChildEvent* c = static_cast< QChildEvent* >( e );
            if( c->child()->isWidgetType())
                removed( static_cast< QWidget* >( c->child()));
            break;
            }
        case QEvent::Paint:
            {
            if( !recursionCheck )
                {
                QPaintEvent* pe = static_cast< QPaintEvent* >( e );
                QWidget* w = static_cast< QWidget* >( o );
                pending |= pe->region().translated( w->mapTo( widget, QPoint( 0, 0 )));
                timer.start( 0 );
                return true; // filter out
                }
            }
        default:
            break;
        }
    return false;
    }

QRegion PaintRedirector::pendingRegion() const
    {
    return pending;
    }

void PaintRedirector::added( QWidget* w )
    {
    w->installEventFilter( this );
    foreach( QObject* o, w->children())
        {
        if( o->isWidgetType() && !isToolTip( static_cast< QWidget* >( o ) ) )
            added( static_cast< QWidget* >( o ));
        }
    }

void PaintRedirector::removed( QWidget* w )
    {
    foreach( QObject* o, w->children())
        {
        if( o->isWidgetType())
            removed( static_cast< QWidget* >( o ));
        }
    w->installEventFilter( this );
    }

} // namespace

#include "paintredirector.moc"
