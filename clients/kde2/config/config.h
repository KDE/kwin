/*********************************************************************
 
 	KDE2 Default configuration widget
 
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

#ifndef KDE2_CONFIG_H
#define KDE2_CONFIG_H

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>

#include <kvbox.h>
#include <kconfig.h>

class KDE2Config: public QObject
{
	Q_OBJECT

	public:
		KDE2Config( KConfig* conf, QWidget* parent );
		~KDE2Config();

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
		QCheckBox* cbShowStipple;
		QCheckBox* cbShowGrabBar;
		QCheckBox* cbUseGradients;
		KVBox* gb;
		KConfig *c;
		bool highcolor;
};

#endif
// vim: ts=4
// kate: space-indent off; tab-width 4;
