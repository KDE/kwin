/* 	
 *	This file contains the B2 configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 */

#include "config.h"
#include <kglobal.h>
#include <qwhatsthis.h>
#include <qvbox.h>
#include <klocale.h>


extern "C"
{
	QObject* allocate_config( KConfig* conf, QWidget* parent )
	{
		return(new B2Config(conf, parent));
	}
}


/* NOTE: 
 * 'conf' 	is a pointer to the kwindecoration modules open kwin config,
 *			and is by default set to the "Style" group.
 *
 * 'parent'	is the parent of the QObject, which is a VBox inside the
 *			Configure tab in kwindecoration
 */

B2Config::B2Config( KConfig* conf, QWidget* parent )
	: QObject( parent )
{
	KGlobal::locale()->insertCatalogue("kwin_b2_config");
	b2Config = new KConfig("kwinb2rc");
	gb = new QVBox( parent );
	cbColorBorder = new QCheckBox( 
						i18n("Draw window frames using &titlebar colors"), gb );
	QWhatsThis::add( cbColorBorder, 
						i18n("When selected, the window decoration borders "
						"are drawn using the titlebar colors. Otherwise, they are "
						"drawn using normal border colors instead.") );
	// Load configuration options
	load( conf );

	// Ensure we track user changes properly
	connect( cbColorBorder, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()) );

	// Make the widgets visible in kwindecoration
	gb->show();
}


B2Config::~B2Config()
{
	delete cbColorBorder;
	delete gb;
	delete b2Config;
}


void B2Config::slotSelectionChanged()
{
	emit changed();
}


// Loads the configurable options from the kwinrc config file
// It is passed the open config from kwindecoration to improve efficiency
void B2Config::load( KConfig* /*conf*/ )
{
	b2Config->setGroup("General");
	bool override = b2Config->readBoolEntry( "UseTitleBarBorderColors", false );
	cbColorBorder->setChecked( override );
}


// Saves the configurable options to the kwinrc config file
void B2Config::save( KConfig* /*conf*/ )
{
	b2Config->setGroup("General");
	b2Config->writeEntry( "UseTitleBarBorderColors", cbColorBorder->isChecked() );
	// Ensure others trying to read this config get updated
	b2Config->sync();
}


// Sets UI widget defaults which must correspond to style defaults
void B2Config::defaults()
{
	cbColorBorder->setChecked( false );
}

#include "config.moc"
// vim: ts=4
