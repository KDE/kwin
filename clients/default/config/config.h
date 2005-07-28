/*
 *
 *	KDE2 Default configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 */

#ifndef _KDE_DEFAULT_CONFIG_H
#define _KDE_DEFAULT_CONFIG_H

#include <qcheckbox.h>
#include <qgroupbox.h>
#include <kconfig.h>
#include <qlabel.h>
#include <Qt3Support/q3vbox.h>

class KDEDefaultConfig: public QObject
{
	Q_OBJECT

	public:
		KDEDefaultConfig( KConfig* conf, QWidget* parent );
		~KDEDefaultConfig();

	// These public signals/slots work similar to KCM modules
	signals:
		void changed();

	public slots:
		void load( KConfig* conf );
		void save( KConfig* conf );
		void defaults();

	protected slots:
		void slotSelectionChanged();	// Internal use

	private:
		QCheckBox* cbShowStipple;
		QCheckBox* cbShowGrabBar;
		QCheckBox* cbUseGradients;
		Q3VBox* gb;
		bool 	   highcolor;
};

#endif
// vim: ts=4
