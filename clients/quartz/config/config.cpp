/* 	
 *
 *	This file contains the quartz configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 */

#include "config.h"
#include <kglobal.h>

#include <klocale.h>
#include <kvbox.h>


extern "C"
{
	KDE_EXPORT QObject* allocate_config( KConfig* conf, QWidget* parent )
	{
		return(new QuartzConfig(conf, parent));
	}
}


/* NOTE: 
 * 'conf' 	is a pointer to the kwindecoration modules open kwin config,
 *			and is by default set to the "Style" group.
 *
 * 'parent'	is the parent of the QObject, which is a VBox inside the
 *			Configure tab in kwindecoration
 */

QuartzConfig::QuartzConfig( KConfig* conf, QWidget* parent )
	: QObject( parent )
{
	quartzConfig = new KConfig("kwinquartzrc");
	KGlobal::locale()->insertCatalog("kwin_clients");
	gb = new KVBox( parent );
	cbColorBorder = new QCheckBox( 
						i18n("Draw window frames using &titlebar colors"), gb );
	cbColorBorder->setWhatsThis( 
						i18n("When selected, the window decoration borders "
						"are drawn using the titlebar colors; otherwise, they are "
						"drawn using normal border colors instead.") );
	cbExtraSmall = new QCheckBox( i18n("Quartz &extra slim"), gb );
	cbExtraSmall->setWhatsThis(
		i18n("Quartz window decorations with extra-small title bar.") );
	// Load configuration options
	load( conf );

	// Ensure we track user changes properly
	connect( cbColorBorder, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()) );
	connect( cbExtraSmall, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()) );

	// Make the widgets visible in kwindecoration
	gb->show();
}


QuartzConfig::~QuartzConfig()
{
	delete gb;
	delete quartzConfig;
}


void QuartzConfig::slotSelectionChanged()
{
	emit changed();
}


// Loads the configurable options from the kwinrc config file
// It is passed the open config from kwindecoration to improve efficiency
void QuartzConfig::load( KConfig* /*conf*/ )
{
	quartzConfig->setGroup("General");
	bool override = quartzConfig->readEntry( "UseTitleBarBorderColors", QVariant(true )).toBool();
	cbColorBorder->setChecked( override );
	override = quartzConfig->readEntry( "UseQuartzExtraSlim", QVariant(false )).toBool();
	cbExtraSmall->setChecked( override );
}


// Saves the configurable options to the kwinrc config file
void QuartzConfig::save( KConfig* /*conf*/ )
{
	quartzConfig->setGroup("General");
	quartzConfig->writeEntry( "UseTitleBarBorderColors", cbColorBorder->isChecked() );
	quartzConfig->writeEntry( "UseQuartzExtraSlim", cbExtraSmall->isChecked() );
	// Ensure others trying to read this config get updated
	quartzConfig->sync();
}


// Sets UI widget defaults which must correspond to style defaults
void QuartzConfig::defaults()
{
	cbColorBorder->setChecked( true );
	cbExtraSmall->setChecked( false );
}

#include "config.moc"
// vim: ts=4
