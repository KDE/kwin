/* 	
	This file contains the icewm configuration widget...

	Copyright (c) 2001
		Karol Szwed (gallium) <karlmail@usa.net>
		http://gallium.n3.net/
*/

#include "config.h"
#include <qdir.h>
#include <qregexp.h>
#include <klocale.h>
#include <kstddirs.h>
#include <kapp.h>


// KWin client config plugin interface
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
	gb1 = new QGroupBox( 1, Qt::Horizontal, i18n("IceWM Theme Selector"), parent );
	themeListBox = new QListBox( gb1 );
	themeLabel = new QLabel( i18n("To manage your IceWM themes, simply click on the link below to open a Konqueror window. "
							"Once shown, you will be able to add or remove native IceWM themes, by uncompressing <b>http://icewm.themes.org/</b> "
							"theme files into this directory, or creating directory symlinks to existing IceWM themes on your system."), parent );
	urlLabel = new KURLLabel( parent );
	urlLabel->setText( i18n("Open Konqueror Window at KDE's IceWM theme directory") );

	gb2 = new QGroupBox( 1, Qt::Horizontal, i18n("IceWM Decoration Settings"), parent );
	cbThemeTitleTextColors = new QCheckBox( i18n("Use theme &title text colors"), gb2 );
	cbTitleBarOnTop 	   = new QCheckBox( i18n("&Show title bar on top of windows"), gb2 );
	cbShowMenuButtonIcon   = new QCheckBox( i18n("&Menu button always shows application mini icon"), gb2 );

	// Load configuration options
	load( conf );

	// Ensure we track user changes properly
	connect( themeListBox, SIGNAL(selectionChanged()), this, SLOT(slotSelectionChanged()) );
	connect( urlLabel, SIGNAL(leftClickedURL(const QString&)), this, SLOT(callURL(const QString&)));
	connect( cbThemeTitleTextColors, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()) );
	connect( cbTitleBarOnTop, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()) );
	connect( cbShowMenuButtonIcon, SIGNAL(clicked()), this, SLOT(slotSelectionChanged()) );

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


// Loads the configurable options from the kwinrc config file
// It is passed the open config from kwindecoration to improve efficiency
void IceWMConfig::load( KConfig* conf )
{
	conf->setGroup("IceWM");

	bool override = conf->readBoolEntry( "ThemeTitleTextColors", true );
	cbThemeTitleTextColors->setChecked( override );

	override = conf->readBoolEntry( "TitleBarOnTop", true );
	cbTitleBarOnTop->setChecked( override );

	override = conf->readBoolEntry( "ShowMenuButtonIcon", false );
	cbShowMenuButtonIcon->setChecked( override );

	findIceWMThemes();
	QString themeName = conf->readEntry("CurrentTheme", "");

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


// Saves the configurable options to the kwinrc config file
void IceWMConfig::save( KConfig* conf )
{
	conf->setGroup("IceWM");
	conf->writeEntry( "ThemeTitleTextColors", cbThemeTitleTextColors->isChecked() );
	conf->writeEntry( "TitleBarOnTop", cbTitleBarOnTop->isChecked() );
	conf->writeEntry( "ShowMenuButtonIcon", cbShowMenuButtonIcon->isChecked() );

	if (themeListBox->currentText() == i18n("Infadel #2 (default)"))
		conf->writeEntry("CurrentTheme", "default");
	else
		conf->writeEntry("CurrentTheme", themeListBox->currentText() );
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
