/*
 *	$Id$ 	
 *
 *	This file contains the IceWM configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include <qdir.h>
#include <qregexp.h>
#include <qwhatsthis.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kdirwatch.h>
#include <kapplication.h>


extern "C"
{
	QObject* allocate_config( KConfig* conf, QWidget* parent )
	{
		return(new IceWMConfig(conf, parent));
	}
}


// NOTE: 
// ==========================================================================
// 'conf' 		is a pointer to the kwindecoration modules open kwin config,
//				and is by default set to the "Style" group.
//
// 'parent'		is the parent of the QObject, which is a VBox inside the
//				Configure tab in kwindecoration
// ==========================================================================

IceWMConfig::IceWMConfig( KConfig* conf, QWidget* parent )
	: QObject( parent )
{
	icewmConfig = new KConfig("kwinicewmrc");
	KGlobal::locale()->insertCatalogue("kwin_icewm_config");

	gb1 = new QGroupBox( 1, Qt::Horizontal, 
				i18n("IceWM Theme Selector"), parent );

	themeListBox = new QListBox( gb1 );
	QWhatsThis::add( themeListBox, 
				i18n("Make your IceWM selection by clicking on a theme here. ") );

	themeLabel = new QLabel( 
				i18n("To manage your IceWM themes, simply click on the "
				"link below to open a Konqueror window. Once shown, you "
				"will be able to add or remove native IceWM themes, by "
				"uncompressing <b>http://icewm.themes.org/</b> theme files "
				"into this directory, or creating directory symlinks to "
				"existing IceWM themes on your system."), parent );

	urlLabel = new KURLLabel( parent );
	urlLabel->setText( 
				i18n("Open Konqueror Window at KDE's IceWM theme directory") );

	gb2 = new QGroupBox( 1, Qt::Horizontal, 
				i18n("IceWM Decoration Settings"), parent );

	cbThemeTitleTextColors = new QCheckBox( 
				i18n("Use theme &title text colors"), gb2 );

	QWhatsThis::add( cbThemeTitleTextColors, 
				i18n("When selected, titlebar colors will follow those set "
				"in the IceWM theme. If not selected, the current KDE "
				"titlebar colors will be used instead.") );

	cbTitleBarOnTop = new QCheckBox( 
				i18n("&Show title bar on top of windows"), gb2 );

	QWhatsThis::add( cbTitleBarOnTop, 
				i18n("When selected, all window titlebars will be shown "
				"at the top of each window, otherwise they will be "
				"shown at the bottom.") );

	cbShowMenuButtonIcon = new QCheckBox( 
				i18n("&Menu button always shows application mini icon"), gb2 );

	QWhatsThis::add( cbShowMenuButtonIcon, 
				i18n("When selected, all titlebar menu buttons will have "
				"the application icon shown. If not selected, the current "
				"theme's defaults are used instead.") );

	// Load configuration options
	load( conf );

	// Ensure we track user changes properly
	connect( themeListBox, SIGNAL(selectionChanged()), 
			 this, SLOT(slotSelectionChanged()) );

	connect( urlLabel, SIGNAL(leftClickedURL(const QString&)),
			 this, SLOT(callURL(const QString&)));

	connect( cbThemeTitleTextColors, SIGNAL(clicked()),
			 this, SLOT(slotSelectionChanged()) );

	connect( cbTitleBarOnTop, SIGNAL(clicked()), 
			 this, SLOT(slotSelectionChanged()) );

	connect( cbShowMenuButtonIcon, SIGNAL(clicked()),
			 this, SLOT(slotSelectionChanged()) );

	// Create the theme directory (if not found) ... and obtain the path as we do so.
	localThemeString = KGlobal::dirs()->saveLocation("data", "kwin");
	localThemeString += "/icewm-themes";
	if (!QFile::exists(localThemeString))
		QDir().mkdir(localThemeString);

	// Watch the icewm theme directory for theme additions/removals
	KDirWatch::self()->addDir(localThemeString);
	connect( KDirWatch::self(), SIGNAL(dirty(const QString&)), this, SLOT(findIceWMThemes()) );
	connect( KDirWatch::self(), SIGNAL(created(const QString&)), this, SLOT(findIceWMThemes()) );
	connect( KDirWatch::self(), SIGNAL(deleted(const QString&)), this, SLOT(findIceWMThemes()) );

	// Set the konqui link url
	QString urlThemeString = QString("file://") + localThemeString;
	urlThemeString.replace( QRegExp("~"), "$HOME" );
	urlLabel->setURL( urlThemeString );

	// Make the widgets visible in kwindecoration
	gb1->show();
	themeLabel->show();
	urlLabel->show();
	gb2->show();

	KDirWatch::self()->startScan();
}


IceWMConfig::~IceWMConfig()
{
	KDirWatch::self()->removeDir(localThemeString);
	KDirWatch::self()->stopScan();
	delete gb2;
	delete urlLabel;
	delete themeLabel;
	delete gb1;
	delete icewmConfig;
}


// Searches for all installed IceWM themes, and adds them to the listBox.
void IceWMConfig::findIceWMThemes()
{
	QStringList dirList = KGlobal::dirs()->findDirs("data", "kwin/icewm-themes");
	QStringList::ConstIterator it;

	// Remove any old themes in the list (if any)
	themeListBox->clear();
	themeListBox->insertItem( i18n("Infadel #2 (default)") );

	// Step through all kwin/icewm-themes directories...
	for( it = dirList.begin(); it != dirList.end(); it++)
	{
		// List all directory names only...
		QDir d(*it, QString("*"), QDir::Unsorted, QDir::Dirs | QDir::Readable );
		if (d.exists())
		{
			QFileInfoListIterator it2( *d.entryInfoList() );
			QFileInfo* finfo;

			// Step through all directories within the kwin/icewm-themes directory
			while( (finfo = it2.current()) )
			{
				// Ignore . and .. directories
				if ( (finfo->fileName() == ".") || (finfo->fileName() == "..") )
				{
					++it2;
					continue;
				}	

				if ( !themeListBox->findItem( finfo->fileName()) )
					themeListBox->insertItem( finfo->fileName() );

				++it2;
			}		
		}
	}

	// Sort the items
	themeListBox->sort();

	// Select the currently used IceWM theme
	QString themeName = icewmConfig->readEntry("CurrentTheme", "");

	// Provide a theme alias
	if (themeName == "default")
		themeName = "";

	if (themeName == "")
		themeListBox->setCurrentItem( 
			themeListBox->findItem( i18n("Infadel #2 (default)") ) );
	else
		themeListBox->setCurrentItem( themeListBox->findItem(themeName) );
}


void IceWMConfig::callURL( const QString& s )
{
	kapp->invokeBrowser( s );
}


void IceWMConfig::slotSelectionChanged()
{
	emit changed();
}


// Loads the configurable options from the kwinicewmrc config file
void IceWMConfig::load( KConfig* )
{
	icewmConfig->setGroup("General");

	bool override = icewmConfig->readBoolEntry( "ThemeTitleTextColors", true );
	cbThemeTitleTextColors->setChecked( override );

	override = icewmConfig->readBoolEntry( "TitleBarOnTop", true );
	cbTitleBarOnTop->setChecked( override );

	override = icewmConfig->readBoolEntry( "ShowMenuButtonIcon", false );
	cbShowMenuButtonIcon->setChecked( override );

	findIceWMThemes();
}


// Saves the configurable options to the kwinicewmrc config file
void IceWMConfig::save( KConfig* )
{
	icewmConfig->setGroup("General");
	icewmConfig->writeEntry( "ThemeTitleTextColors", cbThemeTitleTextColors->isChecked() );
	icewmConfig->writeEntry( "TitleBarOnTop", cbTitleBarOnTop->isChecked() );
	icewmConfig->writeEntry( "ShowMenuButtonIcon", cbShowMenuButtonIcon->isChecked() );

	if (themeListBox->currentText() == i18n("Infadel #2 (default)"))
		icewmConfig->writeEntry("CurrentTheme", "default");
	else
		icewmConfig->writeEntry("CurrentTheme", themeListBox->currentText() );

	icewmConfig->sync();
}


// Sets UI widget defaults which must correspond to config defaults
void IceWMConfig::defaults()
{
	cbThemeTitleTextColors->setChecked( true );
	cbTitleBarOnTop->setChecked( true );
	cbShowMenuButtonIcon->setChecked( false );
	themeListBox->setCurrentItem( themeListBox->findItem(i18n("Infadel #2 (default)")) );
}

#include "config.moc"
// vim: ts=4
