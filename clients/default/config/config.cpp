/*
 *	$Id$
 *
 *	KDE2 Default configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 */

#include "config.h"
#include <qwhatsthis.h>
#include <klocale.h>
#include <qpixmap.h>

extern "C"
{
	QObject* allocate_config( KConfig* conf, QWidget* parent )
	{
		return(new KDEDefaultConfig(conf, parent));
	}
}

// NOTE:
// 'conf' is a pointer to the kwindecoration modules open kwin config,
//		  and is by default set to the "Style" group.
// 'parent' is the parent of the QObject, which is a VBox inside the
//		  Configure tab in kwindecoration

KDEDefaultConfig::KDEDefaultConfig( KConfig* conf, QWidget* parent )
	: QObject( parent )
{
	highcolor = QPixmap::defaultDepth() > 8;
	gb = new QGroupBox( 1, Qt::Horizontal, 
		i18n("Decoration Settings"), parent );

	cbShowStipple = new QCheckBox( i18n("Draw titlebar &stipple effect"), gb );
	QWhatsThis::add( cbShowStipple, 
		i18n("When selected, active titlebars are drawn "
		 "with a stipple (dotted) effect. Otherwise, they are "
		 "drawn without the stipple."));

	cbShowGrabBar = new QCheckBox( i18n("Draw g&rab bar below windows"), gb );
	QWhatsThis::add( cbShowGrabBar, 
		i18n("When selected, decorations are drawn with a \"grab bar\" "
		"below windows. Otherwise, no grab bar is drawn."));

	// Only show the gradient checkbox for highcolor displays
	if (highcolor)
	{
		cbUseGradients = new QCheckBox( i18n("Draw gr&adients"), gb );
		QWhatsThis::add( cbUseGradients, 
			i18n("When selected, decorations are drawn with gradients "
			"for highcolor displays, otherwise no gradients are drawn.") );
	}

	// Allow titlebar height customization
	gbSlider = new QGroupBox( 1, Qt::Horizontal, i18n("TitleBar Height"), gb );
	titleBarSizeSlider = new QSlider(0, 2, 1, 0, QSlider::Horizontal, gbSlider);
	QWhatsThis::add( titleBarSizeSlider, 
		i18n("By adjusting this slider, you can modify " 
		"the height of the titlebar to make room for larger fonts."));

	hbox = new QHBox(gbSlider);
	hbox->setSpacing(6);
	label1 = new QLabel( i18n("Normal"), hbox );
	label2 = new QLabel( i18n("Large"), hbox );
	label2->setAlignment( AlignHCenter );
	label3 = new QLabel( i18n("Huge"), hbox );
	label3->setAlignment( AlignRight );
	
	// Load configuration options
	load( conf );

	// Ensure we track user changes properly
	connect( cbShowStipple, SIGNAL(clicked()), 
			 this, SLOT(slotSelectionChanged()) );
	connect( cbShowGrabBar, SIGNAL(clicked()), 
			 this, SLOT(slotSelectionChanged()) );
	connect( titleBarSizeSlider, SIGNAL(valueChanged(int)), 
			 this, SLOT(slotSelectionChanged(int)) );
	if (highcolor)
		connect( cbUseGradients, SIGNAL(clicked()), 
				 this, SLOT(slotSelectionChanged()) );

	// Make the widgets visible in kwindecoration
	gb->show();
}


KDEDefaultConfig::~KDEDefaultConfig()
{
	delete gb;
}


void KDEDefaultConfig::slotSelectionChanged()
{
	emit changed();
}


void KDEDefaultConfig::slotSelectionChanged(int)
{
	emit changed();
}


// Loads the configurable options from the kwinrc config file
// It is passed the open config from kwindecoration to improve efficiency
void KDEDefaultConfig::load( KConfig* conf )
{
	conf->setGroup("KDEDefault");
	bool override = conf->readBoolEntry( "ShowTitleBarStipple", true );
	cbShowStipple->setChecked( override );

	override = conf->readBoolEntry( "ShowGrabBar", true );
	cbShowGrabBar->setChecked( override );

	if (highcolor) {
		override = conf->readBoolEntry( "UseGradients", true );
		cbUseGradients->setChecked( override );
	}

	int size = conf->readNumEntry( "TitleBarSize", 0 );
	if (size < 0) size = 0;
	if (size > 2) size = 2;

	titleBarSizeSlider->setValue(size);
}


// Saves the configurable options to the kwinrc config file
void KDEDefaultConfig::save( KConfig* conf )
{
	conf->setGroup("KDEDefault");
	conf->writeEntry( "ShowTitleBarStipple", cbShowStipple->isChecked() );
	conf->writeEntry( "ShowGrabBar", cbShowGrabBar->isChecked() );

	if (highcolor)
		conf->writeEntry( "UseGradients", cbUseGradients->isChecked() );

	conf->writeEntry( "TitleBarSize", titleBarSizeSlider->value() );
	// No need to conf->sync() - kwindecoration will do it for us
}


// Sets UI widget defaults which must correspond to style defaults
void KDEDefaultConfig::defaults()
{
	cbShowStipple->setChecked( true );
	cbShowGrabBar->setChecked( true );

	if (highcolor)
		cbUseGradients->setChecked( true );

	titleBarSizeSlider->setValue(0);
}

#include "config.moc"
// vim: ts=4
