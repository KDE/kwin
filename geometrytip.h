/*
 *  $Id$
 *  Window Geometry Display
 *  Copyright (c) 2003, Karol Szwed <kszwed@kde.org>
 */

#ifndef KWIN_GEOMETRY_TIP_H
#define KWIN_GEOMETRY_TIP_H

#include <qlabel.h>
#include "client.h"

namespace KWinInternal {
	
class GeometryTip: public QLabel
{
    public:
	GeometryTip( const Client* client, const XSizeHints* xSizeHints, bool resizing );
	~GeometryTip();
   	void setGeometry( const QRect& geom );
	
    private:
	const XSizeHints* sizeHints;
	const Client* c;
	int framewidth;
	int frameheight;
};

}

#endif
