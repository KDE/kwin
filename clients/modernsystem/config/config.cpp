// $Id$
// Melchior FRANZ  <mfranz@kde.org>	-- 2001-04-22

#include <kapplication.h>
#include <kconfig.h>
#include <klocale.h>
#include <kglobal.h>
#include <qwhatsthis.h>
#include "config.h"


extern "C"
{
	QObject* allocate_config(KConfig* conf, QWidget* parent)
	{
		return(new ModernSysConfig(conf, parent));
	}
}


// 'conf'	is a pointer to the kwindecoration modules open kwin config,
//		and is by default set to the "Style" group.
//
// 'parent'	is the parent of the QObject, which is a VBox inside the
//		Configure tab in kwindecoration

ModernSysConfig::ModernSysConfig(KConfig* conf, QWidget* parent) : QObject(parent)
{	
	clientrc = new KConfig("kwinmodernsysrc");
	KGlobal::locale()->insertCatalogue("kwin_modernsys_config");
	mainw = new QWidget(parent);
	vbox = new QVBoxLayout(mainw);
	vbox->setSpacing(6);
	vbox->setMargin(0);

	handleBox = new QGroupBox( 1, Qt::Vertical, i18n("Window Resize Handle"), mainw);
	handleBox->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));

	cbShowHandle = new QCheckBox(i18n("&Show handle"), handleBox);
	QWhatsThis::add(cbShowHandle,
			i18n("When selected, all windows are drawn with a resize "
			"handle at the lower right corner. This makes window resizing "
			"easier, especially for trackballs and other mouse replacements "
			"on laptops."));
	handleBox->addSpace(20);
	connect(cbShowHandle, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()));

	sliderBox = new QVBox(handleBox);
	handleSizeSlider = new QSlider(0, 4, 1, 0, QSlider::Horizontal, sliderBox);
	QWhatsThis::add(handleSizeSlider,
			i18n("Here you can change the size of the resize handle."));
	handleSizeSlider->setTickInterval(1);
	handleSizeSlider->setTickmarks(QSlider::Below);
	connect(handleSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(slotSelectionChanged()));

	hbox = new QHBox(sliderBox);
	hbox->setSpacing(6);

	bool rtl = kapp->reverseLayout();
	label1 = new QLabel(i18n("Small"), hbox);
	label1->setAlignment(rtl ? AlignRight : AlignLeft);
	label2 = new QLabel(i18n("Medium"), hbox);
	label2->setAlignment(AlignHCenter);
	label3 = new QLabel(i18n("Large"), hbox);
	label3->setAlignment(rtl ? AlignLeft : AlignRight);
	
	vbox->addWidget(handleBox);
	vbox->addStretch(1);
	
	load(conf);
	mainw->show();
}


ModernSysConfig::~ModernSysConfig()
{
	delete mainw;
	delete clientrc;
}


void ModernSysConfig::slotSelectionChanged()
{
	bool i = cbShowHandle->isChecked();
	if (i != hbox->isEnabled()) {
		hbox->setEnabled(i);
		handleSizeSlider->setEnabled(i);
	}
	emit changed();
}


void ModernSysConfig::load(KConfig* /*conf*/)
{
	clientrc->setGroup("General");
	bool i = clientrc->readBoolEntry("ShowHandle", true );
	cbShowHandle->setChecked(i);
	hbox->setEnabled(i);
	handleSizeSlider->setEnabled(i);
	handleWidth = clientrc->readUnsignedNumEntry("HandleWidth", 6);
	handleSize = clientrc->readUnsignedNumEntry("HandleSize", 30);
	handleSizeSlider->setValue(QMIN((handleWidth - 6) / 2, 4));
	
}


void ModernSysConfig::save(KConfig* /*conf*/)
{
	clientrc->setGroup("General");
	clientrc->writeEntry("ShowHandle", cbShowHandle->isChecked());
	clientrc->writeEntry("HandleWidth", 6 + 2 * handleSizeSlider->value());
	clientrc->writeEntry("HandleSize", 30 + 4 * handleSizeSlider->value());
	clientrc->sync();
}


void ModernSysConfig::defaults()
{
	cbShowHandle->setChecked(true);
	hbox->setEnabled(true);
	handleSizeSlider->setEnabled(true);
	handleSizeSlider->setValue(0);
}

#include "config.moc"
