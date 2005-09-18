// Melchior FRANZ  <mfranz@kde.org>	-- 2001-04-22

#include <kapplication.h>
#include <kconfig.h>
#include <kdialog.h>
#include <klocale.h>
#include <kglobal.h>
#include <qlayout.h>

//Added by qt3to4:
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <kvbox.h>
#include "config.h"


extern "C"
{
	KDE_EXPORT QObject* allocate_config(KConfig* conf, QWidget* parent)
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
	KGlobal::locale()->insertCatalogue("kwin_clients");
	mainw = new QWidget(parent);
	vbox = new QVBoxLayout(mainw);
	vbox->setSpacing(6);
	vbox->setMargin(0);

	handleBox = new QWidget(mainw);
        QGridLayout* layout = new QGridLayout(handleBox, 0, KDialog::spacingHint());

	cbShowHandle = new QCheckBox(i18n("&Show window resize handle"), handleBox);
	cbShowHandle->setWhatsThis(
			i18n("When selected, all windows are drawn with a resize "
			"handle at the lower right corner. This makes window resizing "
			"easier, especially for trackballs and other mouse replacements "
			"on laptops."));
        layout->addMultiCellWidget(cbShowHandle, 0, 0, 0, 1);
	connect(cbShowHandle, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()));

	sliderBox = new KVBox(handleBox);
	handleSizeSlider = new QSlider(0, 4, 1, 0, Qt::Horizontal, sliderBox);
	handleSizeSlider->setWhatsThis(
			i18n("Here you can change the size of the resize handle."));
	handleSizeSlider->setTickInterval(1);
	handleSizeSlider->setTickmarks(QSlider::Below);
	connect(handleSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(slotSelectionChanged()));

	hbox = new KHBox(sliderBox);
	hbox->setSpacing(6);

	bool rtl = kapp->reverseLayout();
	label1 = new QLabel(i18n("Small"), hbox);
	label1->setAlignment(rtl ? Qt::AlignRight : Qt::AlignLeft);
	label2 = new QLabel(i18n("Medium"), hbox);
	label2->setAlignment( Qt::AlignHCenter );
	label3 = new QLabel(i18n("Large"), hbox);
	label3->setAlignment(rtl ? Qt::AlignLeft : Qt::AlignRight);
	
	vbox->addWidget(handleBox);
	vbox->addStretch(1);

//        layout->setColSpacing(0, 30);
        layout->addItem(new QSpacerItem(30, 10, QSizePolicy::Fixed, QSizePolicy::Fixed), 1, 0);
        layout->addWidget(sliderBox, 1, 1);
	
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
	handleSizeSlider->setValue(QMIN((handleWidth - 6) / 2, (uint)4));
	
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
