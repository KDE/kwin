// $Id$
// Melchior FRANZ  <a8603365@unet.univie.ac.at>	-- 2001-04-22

#include "config.h"
#include <kconfig.h>
#include <klocale.h>
#include <kglobal.h>
#include <qwhatsthis.h>


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
	clientrc = new KConfig("kwinmodernsysrc");
	KGlobal::locale()->insertCatalogue("libkwinmodernsys_config");
	gb = new QGroupBox(1, Qt::Horizontal, i18n("Decoration Settings"), parent);
	cbShowHandle = new QCheckBox(i18n("&Show resize handle"), gb);
	QWhatsThis::add(cbShowHandle, i18n("When selected, all windows are drawn with a resize "
					   "handle at the lower right corner."));
	connect(cbShowHandle, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()));
	load(conf);
	gb->show();
}


ModernSysConfig::~ModernSysConfig()
{
	delete cbShowHandle;
	delete gb;
	delete clientrc;
}


void ModernSysConfig::slotSelectionChanged()
{
	emit changed();
}


void ModernSysConfig::load(KConfig* /*conf*/)
{
	clientrc->setGroup("General");
	bool i = clientrc->readBoolEntry("ShowHandle", true );
	cbShowHandle->setChecked(i);
	handle_width = clientrc->readUnsignedNumEntry("HandleWidth", 6);
	handle_size = clientrc->readUnsignedNumEntry("HandleSize", 30);
}


void ModernSysConfig::save(KConfig* /*conf*/)
{
	clientrc->setGroup("General");
	clientrc->writeEntry("ShowHandle", cbShowHandle->isChecked());
	clientrc->writeEntry("HandleWidth", handle_width);
	clientrc->writeEntry("HandleSize", handle_size);
	clientrc->sync();
}


void ModernSysConfig::defaults()
{
	cbShowHandle->setChecked(true);
}

#include "config.moc"
