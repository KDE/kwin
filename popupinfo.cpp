/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2002 Alexander Kellett <lypanov@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

//#define QT_CLEAN_NAMESPACE
#include "popupinfo.h"
#include "workspace.h"
#include "client.h"
#include <QPainter>
#include <QLabel>
#include <qdrawutil.h>
#include <QStyle>
#include <kglobal.h>
#include <fixx11h.h>
#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <QApplication>
#include <qdesktopwidget.h>
#include <QCursor>
#include <kstringhandler.h>
#include <kglobalsettings.h>
#include <QX11Info>
#include <QStyleOptionFrame>

// specify externals before namespace

namespace KWin
{

PopupInfo::PopupInfo( const char *name )
    : QWidget( 0 )
    {
      setObjectName( name );

    m_infoString = "";
    m_shown = false;
    reset();
    reconfigure();

    m_delayedHideTimer.setSingleShot(true);
    connect(&m_delayedHideTimer, SIGNAL(timeout()), this, SLOT(hide()));

    QFont f = font();
    f.setBold( true );
    f.setPointSize( 14 );
    setFont( f );

    }

PopupInfo::~PopupInfo()
    {
    }


/*!
  Resets the popup info
 */
void PopupInfo::reset()
    {
    QRect r = KGlobalSettings::desktopGeometry(cursorPos());

    int w = fontMetrics().width( m_infoString ) + 30;

    setGeometry(
       (r.width()-w)/2 + r.x(), r.height()/2-fontMetrics().height()-10 + r.y(),
                 w,                      fontMetrics().height() + 20 );
    }


/*!
  Paints the popup info
 */
void PopupInfo::paintEvent( QPaintEvent* )
    {
    QPainter p( this );
    QStyleOptionFrame *so = new QStyleOptionFrame;
    so->rect = QRect( 0, 0, width(), height() );
    so->palette = palette();
    so->state = QStyle::State_None;
    style()->drawPrimitive( QStyle::PE_Frame, so, &p );
    paintContents();
    }


/*!
  Paints the contents of the tab popup info box.
  Used in paintEvent() and whenever the contents changes.
 */
void PopupInfo::paintContents()
    {
    QPainter p( this );
    QRect r( 6, 6, width()-12, height()-12 );

    p.fillRect( r, palette().brush( QPalette::Background ) );

    /*
    p.setPen(Qt::white);
    p.drawText( r, AlignCenter, m_infoString );
    p.setPen(Qt::black);
    r.moveBy( -1, -1 );
    p.drawText( r, AlignCenter, m_infoString );
    r.moveBy( -1, 0 );
    */
    p.drawText( r, Qt::AlignCenter, m_infoString );
    }

void PopupInfo::hide()
    {
    m_delayedHideTimer.stop();
    QWidget::hide();
    QApplication::syncX();
    XEvent otherEvent;
    while (XCheckTypedEvent (display(), EnterNotify, &otherEvent ) )
        ;
    m_shown = false;
    }

void PopupInfo::reconfigure()
    {
    KSharedConfigPtr c(KGlobal::config());
    const KConfigGroup cg = c->group("PopupInfo");
    m_show = cg.readEntry("ShowPopup", false );
    m_delayTime = cg.readEntry("PopupHideDelay", 350 );
    }

void PopupInfo::showInfo(QString infoString)
    {
    if (m_show)
        {
        m_infoString = infoString;
        reset();
        if (m_shown)
            {
            paintContents();
            }
        else
            {
            show();
            raise();
            m_shown = true;
            }
        m_delayedHideTimer.start(m_delayTime);
        }
    }

} // namespace

#include "popupinfo.moc"
