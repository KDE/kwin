/********************************************************************
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

#ifndef __KDE_MODSYSTEMCONFIG_H
#define __KDE_MODSYSTEMCONFIG_H

#include <QCheckBox>
#include <QLayout>

#include <QSlider>
#include <QLabel>
//Added by qt3to4:
#include <QVBoxLayout>
#include <kvbox.h>

class KConfig;
class KConfigGroup;

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
		void		load(const KConfigGroup& conf);	
		void		save(KConfigGroup& conf);
		void		defaults();

	protected slots:
		void		slotSelectionChanged();	// Internal use

	private:
		KConfig   	*clientrc;
		QWidget		*mainw;
		QVBoxLayout	*vbox;
		QWidget         *handleBox;
		QCheckBox 	*cbShowHandle;
		KVBox		*sliderBox;
		QSlider		*handleSizeSlider;
		KHBox		*hbox;
		QLabel		*label1;
		QLabel		*label2;
		QLabel		*label3;
		
		unsigned  	handleWidth;
		unsigned  	handleSize;

};


#endif
