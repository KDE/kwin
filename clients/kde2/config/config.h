/*
 *
 *	KDE2 Default configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 */

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
