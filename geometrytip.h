/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (c) 2003, Karol Szwed <kszwed@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_GEOMETRY_TIP_H
#define KWIN_GEOMETRY_TIP_H

#include <QLabel>
#include "client.h"

namespace KWin
{

class GeometryTip: public QLabel
    {
    Q_OBJECT
    public:
        GeometryTip( const XSizeHints* xSizeHints, bool save_under );
        ~GeometryTip();
        void setGeometry( const QRect& geom );

    private:
        const XSizeHints* sizeHints;
    };

} // namespace

#endif
