// $Id$
// Melchior FRANZ  <a8603365@unet.univie.ac.at>	-- 2001-04-22

#include "config.h"
#include <klocale.h>
#include <qlayout.h>


extern "C"
{
	QObject* allocate_config( KConfig* conf, QWidget* parent )
	{
		return(new ModernSysConfig(conf, parent));
	}
}


// 'conf' 	is a pointer to the kwindecoration modules open kwin config,
//			and is by default set to the "Style" group.
//
// 'parent'	is the parent of the QObject, which is a VBox inside the
//			Configure tab in kwindecoration

ModernSysConfig::ModernSysConfig(KConfig* conf, QWidget* parent) : QObject(parent)
{	
	gb = new QGroupBox(1, Qt::Horizontal, i18n("Modern System Decoration Settings"), parent);
	cbShowHandle = new QCheckBox(i18n("&Show resize handle"), gb);
	connect(cbShowHandle, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()));
	load(conf);
	gb->show();
}


ModernSysConfig::~ModernSysConfig()
{
	delete cbShowHandle;
	delete gb;
}


void ModernSysConfig::slotSelectionChanged()
{
	emit changed();
}


void ModernSysConfig::load(KConfig* conf)
{
	conf->setGroup("ModernSystem");
	bool i = conf->readBoolEntry("ShowHandle", true );
	cbShowHandle->setChecked(i);
	handle_width = conf->readUnsignedNumEntry("HandleWidth", 6);
	handle_size = conf->readUnsignedNumEntry("HandleSize", 30);
}


void ModernSysConfig::save(KConfig* conf)
{
	conf->setGroup("ModernSystem");
	conf->writeEntry("ShowHandle", cbShowHandle->isChecked());
	conf->writeEntry("HandleWidth", handle_width);
	conf->writeEntry("HandleSize", handle_size);
}


void ModernSysConfig::defaults()
{
	cbShowHandle->setChecked(true);
}

#include "config.moc"
