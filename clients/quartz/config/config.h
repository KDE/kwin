/********************************************************************
 
 	This file contains the quartz configuration widget
 
 	Copyright (c) 2001
 		Karol Szwed <gallium@kde.org>
 		http://gallium.n3.net/

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

#ifndef __KDE_QUARTZCONFIG_H
#define __KDE_QUARTZCONFIG_H

#include <QCheckBox>

#include <kconfig.h>
#include <kvbox.h>

class QuartzConfig: public QObject
{
	Q_OBJECT

	public:
		QuartzConfig( KConfig* conf, QWidget* parent );
		~QuartzConfig();

	// These public signals/slots work similar to KCM modules
	signals:
		void changed();

	public slots:
		void load( const KConfigGroup& conf );	
		void save( KConfigGroup& conf );
		void defaults();

	protected slots:
		void slotSelectionChanged();	// Internal use

	private:
		KConfig*   quartzConfig;
		QCheckBox* cbColorBorder;
		QCheckBox* cbExtraSmall;
		KVBox* gb;
};


#endif

// vim: ts=4
