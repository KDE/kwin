/* 	
	This file contains the quartz configuration widget...

	Copyright (c) 2001
		Karol Szwed (gallium) <karlmail@usa.net>
		http://gallium.n3.net/
*/

#include "config.h"
#include <qwhatsthis.h>
#include <klocale.h>


// KWin client config plugin interface
extern "C"
{
	QObject* allocate_config( KConfig* conf, QWidget* parent )
	{
		return(new QuartzConfig(conf, parent));
	}
}


// NOTE: 
// 'conf' 	is a pointer to the kwindecoration modules open kwin config,
//			and is by default set to the "Style" group.
//
// 'parent'	is the parent of the QObject, which is a VBox inside the
//			Configure tab in kwindecoration

QuartzConfig::QuartzConfig( KConfig* conf, QWidget* parent )
	: QObject( parent )
{
	gb = new QGroupBox( 1, Qt::Horizontal, i18n("Quartz Decoration Settings"), parent );

	cbColorBorder = new QCheckBox( i18n("Draw window frames using &titlebar colors"), gb );
	QWhatsThis::add( cbColorBorder, i18n("When selected, the window decoration borders "
										 "are drawn using the titlebar colors. Otherwise, they are "
										 "drawn using normal border colors instead.") );
	// Load configuration options
	load( conf );

	// Ensure we track user changes properly
	connect( cbColorBorder, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()) );

	// Make the widgets visible in kwindecoration
	gb->show();
}


QuartzConfig::~QuartzConfig()
{
	delete cbColorBorder;
	delete gb;
}


void QuartzConfig::slotSelectionChanged()
{
	emit changed();
}


// Loads the configurable options from the kwinrc config file
// It is passed the open config from kwindecoration to improve efficiency
void QuartzConfig::load( KConfig* conf )
{
	conf->setGroup("Quartz");
	bool override = conf->readBoolEntry( "UseTitleBarBorderColors", true );
	cbColorBorder->setChecked( override );
}


// Saves the configurable options to the kwinrc config file
void QuartzConfig::save( KConfig* conf )
{
	conf->setGroup("Quartz");
	conf->writeEntry( "UseTitleBarBorderColors", cbColorBorder->isChecked() );
}


// Sets UI widget defaults which must correspond to style defaults
void QuartzConfig::defaults()
{
	cbColorBorder->setChecked( true );
}

#include "config.moc"

// vim: ts=4
