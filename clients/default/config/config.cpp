/*
 *
 *	KDE2 Default configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 */

#include "config.h"
#include <kglobal.h>
#include <QWhatsThis>
#include <kdialog.h>
#include <klocale.h>
#include <QPixmap>
#include <kvbox.h>

extern "C"
{
	KDE_EXPORT QObject* allocate_config( KConfig* conf, QWidget* parent )
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
	KGlobal::locale()->insertCatalog("kwin_clients");
	highcolor = QPixmap::defaultDepth() > 8;
	gb = new KVBox( parent );
        gb->setSpacing( KDialog::spacingHint() );

	cbShowStipple = new QCheckBox( i18n("Draw titlebar &stipple effect"), gb );
	cbShowStipple->setWhatsThis(
		i18n("When selected, active titlebars are drawn "
		 "with a stipple (dotted) effect; otherwise, they are "
		 "drawn without the stipple."));

	cbShowGrabBar = new QCheckBox( i18n("Draw g&rab bar below windows"), gb );
	cbShowGrabBar->setWhatsThis(
		i18n("When selected, decorations are drawn with a \"grab bar\" "
		"below windows; otherwise, no grab bar is drawn."));

	// Only show the gradient checkbox for highcolor displays
	if (highcolor)
	{
		cbUseGradients = new QCheckBox( i18n("Draw &gradients"), gb );
		cbUseGradients->setWhatsThis(
			i18n("When selected, decorations are drawn with gradients "
			"for high-color displays; otherwise, no gradients are drawn.") );
	}

	// Load configuration options
	load( conf );

	// Ensure we track user changes properly
	connect( cbShowStipple, SIGNAL(clicked()), 
			 this, SLOT(slotSelectionChanged()) );
	connect( cbShowGrabBar, SIGNAL(clicked()), 
			 this, SLOT(slotSelectionChanged()) );
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


// Loads the configurable options from the kwinrc config file
// It is passed the open config from kwindecoration to improve efficiency
void KDEDefaultConfig::load( KConfig* conf )
{
	conf->setGroup("KDEDefault");
	bool override = conf->readEntry( "ShowTitleBarStipple", true);
	cbShowStipple->setChecked( override );

	override = conf->readEntry( "ShowGrabBar", true);
	cbShowGrabBar->setChecked( override );

	if (highcolor) {
		override = conf->readEntry( "UseGradients", true);
		cbUseGradients->setChecked( override );
	}
}


// Saves the configurable options to the kwinrc config file
void KDEDefaultConfig::save( KConfig* conf )
{
	conf->setGroup("KDEDefault");
	conf->writeEntry( "ShowTitleBarStipple", cbShowStipple->isChecked() );
	conf->writeEntry( "ShowGrabBar", cbShowGrabBar->isChecked() );

	if (highcolor)
		conf->writeEntry( "UseGradients", cbUseGradients->isChecked() );
	// No need to conf->sync() - kwindecoration will do it for us
}


// Sets UI widget defaults which must correspond to style defaults
void KDEDefaultConfig::defaults()
{
	cbShowStipple->setChecked( true );
	cbShowGrabBar->setChecked( true );

	if (highcolor)
		cbUseGradients->setChecked( true );
}

#include "config.moc"
// vim: ts=4
