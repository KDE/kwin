/*
 *	$Id$ 	
 *
 *	This file contains the IceWM configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 */

#ifndef __KDEGALLIUM_ICEWMCONFIG_H
#define __KDEGALLIUM_ICEWMCONFIG_H

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

	private:
		void findIceWMThemes();

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
};


#endif
// vim: ts=4
