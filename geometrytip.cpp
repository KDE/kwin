/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (c) 2003, Karol Szwed <kszwed@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "geometrytip.h"

namespace KWinInternal
{

GeometryTip::GeometryTip( const XSizeHints* xSizeHints, bool save_under ):
    QLabel(NULL, "kwingeometry", WX11BypassWM )
    {
    setMargin(1);
    setIndent(0);
    setLineWidth(1);
    setFrameStyle( QFrame::Raised | QFrame::StyledPanel );
    setAlignment( AlignCenter | AlignTop );
    sizeHints = xSizeHints;
    if( save_under )
        {
        XSetWindowAttributes attr;
        attr.save_under = True; // use saveunder if possible to avoid weird effects in transparent mode
        XChangeWindowAttributes( qt_xdisplay(), winId(), CWSaveUnder, &attr );
        }
    }

GeometryTip::~GeometryTip()
    {
    }

void GeometryTip::setGeometry( const QRect& geom )
    {
    int w, h;
    int bw, bh;

    w = geom.width();
    h = geom.height();

    if (sizeHints) 
        {
	// PBaseSize is only for aspect ratios, see Client::getWMNormalHints()
        if (!(sizeHints->flags & PMinSize)) 
            {
            bw = 0;
            bh = 0;
            }
        else 
            {
            bw = sizeHints->min_width;
            bh = sizeHints->min_height;
            }

        if (sizeHints->flags & PResizeInc) 
            {
            if (sizeHints->width_inc > 0)
                w = (w - bw) / sizeHints->width_inc;
            if (sizeHints->height_inc > 0) 
                h = (h - bh) / sizeHints->height_inc; 

            }
        }

    h = QMAX( h, 0 ); // in case of isShade() and PBaseSize
    QString pos;
    pos.sprintf( "%+d,%+d&nbsp;(<b>%d&nbsp;x&nbsp;%d</b>)",
                     geom.x(), geom.y(), w, h );
    setText( pos );
    adjustSize();
    move( geom.x() + ((geom.width()  - width())  / 2),
          geom.y() + ((geom.height() - height()) / 2) );
    }

} // namespace

#include "geometrytip.moc"
