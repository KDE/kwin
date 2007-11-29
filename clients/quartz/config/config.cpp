/********************************************************************
 
 	This file contains the quartz configuration widget
 
 	Copyright (c) 2001
 		Karol Szwed <gallium@kde.org>
 		http://gallium.n3.net/

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

#include "config.h"
#include <kglobal.h>
#include <kconfiggroup.h>
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
    KConfigGroup cg(quartzConfig, "General");
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
	load( cg );

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
void QuartzConfig::load( const KConfigGroup& /*conf*/ )
{
	KConfigGroup cg(quartzConfig, "General");
	bool override = cg.readEntry( "UseTitleBarBorderColors", true);
	cbColorBorder->setChecked( override );
	override = cg.readEntry( "UseQuartzExtraSlim", false);
	cbExtraSmall->setChecked( override );
}


// Saves the configurable options to the kwinrc config file
void QuartzConfig::save( KConfigGroup& /*conf*/ )
{
	KConfigGroup cg(quartzConfig, "General");
	cg.writeEntry( "UseTitleBarBorderColors", cbColorBorder->isChecked() );
	cg.writeEntry( "UseQuartzExtraSlim", cbExtraSmall->isChecked() );
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
