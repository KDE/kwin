/*
 *	$Id$ 	
 *
 *	This file contains the IceWM configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 */

#include "config.h"
#include <qdir.h>
#include <qregexp.h>
#include <qwhatsthis.h>
#include <klocale.h>
#include <kstddirs.h>
#include <kapp.h>


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
	QString localThemeString = KGlobal::dirs()->saveLocation("data", "kwin");

	localThemeString += "/icewm-themes";
	if (!QFile::exists(localThemeString))
		QDir().mkdir(localThemeString);

	// Set the konqui link url
	localThemeString = QString("file://") + localThemeString;
	localThemeString.replace( QRegExp("~"), "$HOME" );
	urlLabel->setURL( localThemeString );

	// Make the widgets visible in kwindecoration
	gb1->show();
	themeLabel->show();
	urlLabel->show();
	gb2->show();
}


IceWMConfig::~IceWMConfig()
{
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
// It is passed the open config from kwindecoration to improve efficiency
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
	QString themeName = icewmConfig->readEntry("CurrentTheme", "");

	// Provide a theme alias
	if (themeName == "default")
		themeName = "";

	// Select the currently used IceWM theme
	if (themeName == "")
		themeListBox->setCurrentItem( 
			themeListBox->findItem( i18n("Infadel #2 (default)") ) );
	else
		themeListBox->setCurrentItem( themeListBox->findItem(themeName) );
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
