#ifndef __KDE_MODSYSTEMCONFIG_H
#define __KDE_MODSYSTEMCONFIG_H

#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qlayout.h>
#include <qvbox.h>
#include <qslider.h>
#include <qlabel.h>

class ModernSysConfig : public QObject
{
	Q_OBJECT

	public:
		ModernSysConfig(KConfig* conf, QWidget* parent);
		~ModernSysConfig();

	// These public signals/slots work similar to KCM modules
	signals:
		void		changed();

	public slots:
		void		load(KConfig* conf);	
		void		save(KConfig* conf);
		void		defaults();

	protected slots:
		void		slotSelectionChanged();	// Internal use

	private:
		KConfig   	*clientrc;
		QWidget		*mainw;
		QVBoxLayout	*vbox;
		QWidget         *handleBox;
		QCheckBox 	*cbShowHandle;
		QVBox		*sliderBox;
		QSlider		*handleSizeSlider;
		QHBox		*hbox;
		QLabel		*label1;
		QLabel		*label2;
		QLabel		*label3;
		
		unsigned  	handleWidth;
		unsigned  	handleSize;

};


#endif
