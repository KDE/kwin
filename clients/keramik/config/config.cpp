/*
 * 
 * Keramik KWin client configuration module
 *
 * Copyright (C) 2002 Fredrik Höglund <fredrik@kde.org>
 *
 * Based on the Quartz configuration module,
 *     Copyright (c) 2001 Karol Szwed <gallium@kde.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
 
#include <kglobal.h>
#include <klocale.h>

#include <QCheckBox>

#include "config.h"
#include "config.moc"

extern "C"
{
	KDE_EXPORT QObject* allocate_config( KConfig* conf, QWidget* parent )
	{
		return ( new KeramikConfig( conf, parent ) );
	}
}


/* NOTE: 
 * 'conf' 	is a pointer to the kwindecoration modules open kwin config,
 *			and is by default set to the "Style" group.
 *
 * 'parent'	is the parent of the QObject, which is a VBox inside the
 *			Configure tab in kwindecoration
 */

KeramikConfig::KeramikConfig( KConfig* conf, QWidget* parent )
	: QObject( parent )
{
	KGlobal::locale()->insertCatalog("kwin_clients");
	c = new KConfig( "kwinkeramikrc" );
	
	ui = new KeramikConfigUI( parent );
	connect( ui->showAppIcons,   SIGNAL(clicked()), SIGNAL(changed()) );
	connect( ui->smallCaptions,  SIGNAL(clicked()), SIGNAL(changed()) );
	connect( ui->largeGrabBars,  SIGNAL(clicked()), SIGNAL(changed()) );
	connect( ui->useShadowedText, SIGNAL(clicked()), SIGNAL(changed()) );

	load( conf );
	ui->show();
}


KeramikConfig::~KeramikConfig()
{
	delete ui;
	delete c;
}


// Loads the configurable options from the kwinrc config file
// It is passed the open config from kwindecoration to improve efficiency
void KeramikConfig::load( KConfig* )
{
	c->setGroup("General");
	ui->showAppIcons->setChecked( c->readEntry("ShowAppIcons", QVariant(true)).toBool() );
	ui->smallCaptions->setChecked( c->readEntry("SmallCaptionBubbles", QVariant(false)).toBool() );
	ui->largeGrabBars->setChecked( c->readEntry("LargeGrabBars", QVariant(true)).toBool() );
	ui->useShadowedText->setChecked( c->readEntry("UseShadowedText", QVariant(true)).toBool() );
}


// Saves the configurable options to the kwinrc config file
void KeramikConfig::save( KConfig* )
{
	c->setGroup( "General" );
	c->writeEntry( "ShowAppIcons", ui->showAppIcons->isChecked() );
	c->writeEntry( "SmallCaptionBubbles", ui->smallCaptions->isChecked() );
	c->writeEntry( "LargeGrabBars", ui->largeGrabBars->isChecked() );
	c->writeEntry( "UseShadowedText", ui->useShadowedText->isChecked() );
	c->sync();
}


// Sets UI widget defaults which must correspond to style defaults
void KeramikConfig::defaults()
{
	ui->showAppIcons->setChecked( true );
	ui->smallCaptions->setChecked( false );
	ui->largeGrabBars->setChecked( true );
	ui->useShadowedText->setChecked( true );
	
	emit changed();
}

// vim: set noet ts=4 sw=4:
