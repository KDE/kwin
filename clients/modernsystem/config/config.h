// $Id$
#ifndef __KDE_MODSYSTEMCONFIG_H
#define __KDE_MODSYSTEMCONFIG_H

#include <qcheckbox.h>
#include <qgroupbox.h>
#include <kconfig.h>

class ModernSysConfig : public QObject
{
	Q_OBJECT

	public:
		ModernSysConfig( KConfig* conf, QWidget* parent );
		~ModernSysConfig();

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
		KConfig   *clientrc;
		QGroupBox *gb;
		QCheckBox *cbShowHandle;
		unsigned  handle_width;
		unsigned  handle_size;
};


#endif
