/*
 *	$Id$ 	
 *
 *	This file contains the IceWM configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#ifndef _ICEWMCONFIG_H
#define _ICEWMCONFIG_H

#include <qwidget.h>
#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qlistbox.h>
#include <qlabel.h>
#include <kurllabel.h>
#include <kconfig.h>

class IceWMConfig: public QObject
{
	Q_OBJECT

	public:
		IceWMConfig( KConfig* conf, QWidget* parent );
		~IceWMConfig();

	// These public signals/slots work similar to KCM modules
	signals:
		void changed();

	public slots:
		void load( KConfig* conf );	
		void save( KConfig* conf );
		void defaults();

	protected slots:
		void slotSelectionChanged();	// Internal use
		void callURL( const QString& s );
		void findIceWMThemes();

	private:
		KConfig*   icewmConfig;
		QCheckBox* cbThemeButtonPositions;
		QCheckBox* cbThemeTitleTextColors;
		QCheckBox* cbTitleBarOnTop;
		QCheckBox* cbShowMenuButtonIcon;
		QGroupBox* gb1;
		QGroupBox* gb2;
		QListBox*  themeListBox;
		QLabel*	   themeLabel;	
		KURLLabel* urlLabel;
		QString    localThemeString;
};


#endif
// vim: ts=4
