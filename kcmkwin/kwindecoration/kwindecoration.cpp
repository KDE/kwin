/*
	$Id$

	This is the new kwindecoration kcontrol module

	Copyright (c) 2001
		Karol Szwed <gallium@kde.org>
		http://gallium.n3.net/

	Supports new kwin configuration plugins, and titlebar button position
	modification via dnd interface.

	Based on original "kwintheme" (Window Borders) 
	Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>
*/

#include <qdir.h>
#include <qfileinfo.h>
#include <qlayout.h>
#include <qwhatsthis.h>
#include <qlistbox.h>
#include <qgroupbox.h>
#include <qcheckbox.h>
#include <qtabwidget.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qfile.h>

#include <kapplication.h>
#include <kdebug.h>
#include <kdesktopfile.h>
#include <kstandarddirs.h>
#include <kglobal.h>
#include <klocale.h>
#include <kdialog.h>
#include <kgenericfactory.h>
#include <kaboutdata.h>
#include <dcopclient.h>

#include "kwindecoration.h"


// KCModule plugin interface
// =========================
typedef KGenericFactory<KWinDecorationModule, QWidget> KWinDecoFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_kwindecoration, KWinDecoFactory("kcmkwindecoration") );

KWinDecorationModule::KWinDecorationModule(QWidget* parent, const char* name, const QStringList &)
	: KCModule(KWinDecoFactory::instance(), parent, name), DCOPObject("KWinClientDecoration")
{
	KConfig kwinConfig("kwinrc");
	kwinConfig.setGroup("Style");

	QVBoxLayout* layout = new QVBoxLayout(this);
	tabWidget = new QTabWidget( this );
	layout->addWidget( tabWidget );

	// Page 1 (General Options)
	QVBox* page1 = new QVBox( tabWidget );
	page1->setSpacing( KDialog::spacingHint() );
	page1->setMargin( KDialog::marginHint() );

	QGroupBox* btnGroup = new QGroupBox( 1, Qt::Horizontal, i18n("Window Decoration"), page1 );
	QWhatsThis::add( btnGroup, 
			i18n("Select the window decoration. This is the look and feel of both "
			"the window borders and the window handle.") );
	decorationListBox = new QListBox( btnGroup );

	QGroupBox* checkGroup = new QGroupBox( 1, Qt::Horizontal, 
			i18n("General options (if available)"), page1 );
	cbUseCustomButtonPositions = new QCheckBox( 
			i18n("Use custom titlebar button &positions"), checkGroup );
	QWhatsThis::add( cbUseCustomButtonPositions, 
			i18n(  "The appropriate settings can be found in the \"Buttons\" Tab. "
				   "Please note that this option is not available on all styles yet!" ) );
	cbShowToolTips = new QCheckBox( 
			i18n("&Show window button tooltips"), checkGroup );
	QWhatsThis::add( cbShowToolTips, 
			i18n(  "Enabling this checkbox will show window button tooltips. "
				   "If this checkbox is off, no window button tooltips will be shown."));
// Save this for later...
//	cbUseMiniWindows = new QCheckBox( i18n( "Render mini &titlebars for all windows"), checkGroup );
//	QWhatsThis::add( cbUseMiniWindows, i18n( "Note that this option is not available on all styles yet!" ) );

	// Page 2 (Button Selector)
	buttonPage = new QVBox( tabWidget );
	buttonPage->setSpacing( KDialog::spacingHint() );
	buttonPage->setMargin( KDialog::marginHint() );

	QGroupBox* buttonBox = new QGroupBox( 1, Qt::Horizontal, 
			i18n("Titlebar Button Position"), buttonPage );

	// Add nifty dnd button modification widgets    
	QLabel* label = new QLabel( buttonBox );
	dropSite = new ButtonDropSite( buttonBox );
	label->setText( i18n( "To add or remove titlebar buttons, simply <i>drag</i> items "
		"between the available item list and the titlebar preview. Similarly, "
		"drag items within the titlebar preview to re-position them.") );
	buttonSource = new ButtonSource( buttonBox );

	// Page 3 (Configure decoration via client plugin page)
	pluginPage = new QVBox( tabWidget );
	pluginPage->setSpacing( KDialog::spacingHint() );
	pluginPage->setMargin( KDialog::marginHint() );
	pluginObject = NULL;

	// Load all installed decorations into memory
	// Set up the decoration lists and other UI settings
	findDecorations();
	createDecorationList();
	readConfig( &kwinConfig );
	resetPlugin( &kwinConfig );

	tabWidget->insertTab( page1, i18n("&General") );
	tabWidget->insertTab( buttonPage, i18n("&Buttons") );
	tabWidget->insertTab( pluginPage, i18n("&Configure [") + 
						  decorationListBox->currentText() + i18n("]") );

	tabWidget->setTabEnabled( buttonPage, cbUseCustomButtonPositions->isChecked() );

	connect( dropSite, SIGNAL(buttonAdded(char)), buttonSource, SLOT(hideButton(char)) );
	connect( dropSite, SIGNAL(buttonRemoved(char)), buttonSource, SLOT(showButton(char)) );
	connect( buttonSource, SIGNAL(buttonDropped()), dropSite, SLOT(removeClickedButton()) );
	connect( dropSite, SIGNAL(changed()), this, SLOT(slotSelectionChanged()) );
	connect( buttonSource, SIGNAL(selectionChanged()), this, SLOT(slotSelectionChanged()) );
	connect( decorationListBox, SIGNAL(selectionChanged()), SLOT(slotSelectionChanged()) );
	connect( decorationListBox, SIGNAL(highlighted(const QString&)), 
								SLOT(slotDecorationHighlighted(const QString&)) );
	connect( cbUseCustomButtonPositions, SIGNAL(clicked()), SLOT(slotSelectionChanged()) );
	connect( cbUseCustomButtonPositions, SIGNAL(toggled(bool)), SLOT(slotEnableButtonTab(bool)) );
	connect( cbShowToolTips, SIGNAL(clicked()), SLOT(slotSelectionChanged()) );
//	connect( cbUseMiniWindows, SIGNAL(clicked()), SLOT(slotSelectionChanged()) );

	// Allow kwin dcop signal to update our selection list
	connectDCOPSignal("kwin", 0, "dcopResetAllClients()", "dcopUpdateClientList()", false);
}


KWinDecorationModule::~KWinDecorationModule()
{
}


// Find all theme desktop files in all 'data' dirs owned by kwin.
// And insert these into a DecorationInfo structure
void KWinDecorationModule::findDecorations()
{
	QStringList dirList = KGlobal::dirs()->findDirs("data", "kwin");
	QStringList::ConstIterator it;

	for (it = dirList.begin(); it != dirList.end(); it++)
	{
		QDir d(*it);
		if (d.exists())
			for (QFileInfoListIterator it(*d.entryInfoList()); it.current(); ++it)
			{
				QString filename(it.current()->absFilePath());
				if (KDesktopFile::isDesktopFile(filename))
				{
					KDesktopFile desktopFile(filename);
					QString libName = desktopFile.readEntry("X-KDE-Library");

					if (!libName.isEmpty())
					{
						DecorationInfo di;
						di.name = desktopFile.readName();
						di.libraryName = libName;
						decorations.append( di );
					}
				}
			}
	}
}


// Fills the decorationListBox with a list of available kwin decorations
void KWinDecorationModule::createDecorationList()
{
	QValueList<DecorationInfo>::ConstIterator it;

	// Sync with kwin hardcoded KDE2 style which has no desktop item
	decorationListBox->insertItem( i18n("KDE2 default") ); 

	for (it = decorations.begin(); it != decorations.end(); ++it)
	{
		DecorationInfo info = *it;
		decorationListBox->insertItem( info.name );
	}
	decorationListBox->sort();
}


// Reset the decoration plugin to what the user just selected
void KWinDecorationModule::slotDecorationHighlighted( const QString& s )
{
	KConfig kwinConfig("kwinrc");
	kwinConfig.setGroup("Style");

	// Let the user see config options for the currently selected decoration
	resetPlugin( &kwinConfig, &s );
	tabWidget->changeTab( pluginPage, i18n("&Configure [%1]").arg( decorationListBox->currentText() ) );
}


// This is the selection handler setting
void KWinDecorationModule::slotSelectionChanged()
{
	emit changed(true);
}


void KWinDecorationModule::slotEnableButtonTab(bool on)
{
	tabWidget->setTabEnabled( buttonPage, on );
}


QString KWinDecorationModule::decorationName( QString& libName )
{
	QString decoName;

	QValueList<DecorationInfo>::Iterator it;
	for( it = decorations.begin(); it != decorations.end(); ++it )
		if ( (*it).libraryName == libName )
		{
			decoName = (*it).name;
			break;
		}

	return decoName;
}


QString KWinDecorationModule::decorationLibName( const QString& name )
{
	QString libName;

	// Find the corresponding library name to that of
	// the current plugin name
	QValueList<DecorationInfo>::Iterator it;
	for( it = decorations.begin(); it != decorations.end(); ++it )
		if ( (*it).name == name )
		{
			libName = (*it).libraryName;
			break;
		}

	if (libName.isEmpty())
		libName = "kwin_default";

	return libName;
}


// Loads/unloads and inserts the decoration config plugin into the
// pluginPage, allowing for dynamic configuration of decorations
void KWinDecorationModule::resetPlugin( KConfig* conf, const QString* currentDecoName )
{
	// Config names are "kwin_icewm_config"
	// for "kwin_icewm" kwin client

	QString oldName = oldLibraryName;
	oldName += "_config";

	QString currentName;
	if (currentDecoName)
		currentName = decorationLibName( *currentDecoName ); // Use what the user selected
	else
		currentName = currentLibraryName; // Use what was read from readConfig()

	currentName += "_config";

	// Delete old plugin widget if it exists
	if (pluginObject)
		delete pluginObject;

	// Use klibloader for library manipulation
	KLibLoader* loader = KLibLoader::self();

	// Free the old library if possible
	if (!oldLibraryName.isNull())
		loader->unloadLibrary( QFile::encodeName(oldName) );

	KLibrary* library = loader->library( QFile::encodeName(currentName) );
	if (library != NULL)
	{		
		void* alloc_ptr = library->symbol("allocate_config");

		if (alloc_ptr != NULL)
		{
			allocatePlugin = (QObject* (*)(KConfig* conf, QWidget* parent))alloc_ptr;
			pluginObject = allocatePlugin( conf, pluginPage );
				
			// connect required signals and slots together...
			connect( pluginObject, SIGNAL(changed()), this, SLOT(slotSelectionChanged()) );	
			connect( this, SIGNAL(pluginLoad(KConfig*)), pluginObject, SLOT(load(KConfig*)) );
			connect( this, SIGNAL(pluginSave(KConfig*)), pluginObject, SLOT(save(KConfig*)) );
			connect( this, SIGNAL(pluginDefaults()), pluginObject, SLOT(defaults()) );

			return;
		} 
	}

	// Display a message telling the user that the current decoration
	// does not have any configurable options (extended plugin interface not found)
	QWidget* plugin = new QGroupBox( 1, Qt::Horizontal, "", pluginPage );
	(void) new QLabel( 
		i18n("<H3>No Configurable Options Available</H3>"
	        "Sorry, no configurable options are available for the "
			"currently selected decoration."), plugin );

	plugin->show();
	pluginObject = plugin;
}



// Reads the kwin config settings, and sets all UI controls to those settings
// Updating the config plugin if required
void KWinDecorationModule::readConfig( KConfig* conf )
{
	// General tab
	// ============
	cbUseCustomButtonPositions->setChecked( conf->readBoolEntry("CustomButtonPositions", false));
	tabWidget->setTabEnabled( buttonPage, cbUseCustomButtonPositions->isChecked() );
	cbShowToolTips->setChecked( conf->readBoolEntry("ShowToolTips", true ));
//	cbUseMiniWindows->setChecked( conf->readBoolEntry("MiniWindowBorders", false));

	// Find the corresponding decoration name to that of
	// the current plugin library name

	oldLibraryName = currentLibraryName;
	currentLibraryName = conf->readEntry("PluginLib", "kwin_default");
	QString decoName = decorationName( currentLibraryName );

	// If we are using the "default" kde client, use the "default" entry.
	if (decoName.isEmpty())
		decorationListBox->setSelected( 0, true );
	else
		// Update the decoration listbox
		decorationListBox->setSelected( decorationListBox->findItem( decoName ), true);

	// Buttons tab
	// ============
	// Menu and sticky buttons are default on LHS
	dropSite->buttonsLeft  = conf->readEntry("ButtonsOnLeft", "MS");
	// Help, Minimize, Maximize and Close are default on RHS
	dropSite->buttonsRight = conf->readEntry("ButtonsOnRight", "HIAX");
	dropSite->repaint(false);

	buttonSource->showAllButtons();

	// Step through the button lists, and hide the dnd button source items
	unsigned int i;
	for(i = 0; i < dropSite->buttonsLeft.length(); i++)
		buttonSource->hideButton( dropSite->buttonsLeft[i].latin1() );
	for(i = 0; i < dropSite->buttonsRight.length(); i++)
		buttonSource->hideButton( dropSite->buttonsRight[i].latin1() );

	emit changed(false);
}


// Writes the selected user configuration to the kwin config file
void KWinDecorationModule::writeConfig( KConfig* conf )
{
	QString name = decorationListBox->currentText();
	QString libName = decorationLibName( name );

	KConfig kwinConfig("kwinrc");
	kwinConfig.setGroup("Style");
    
	// General settings
	conf->writeEntry("PluginLib", libName);
	conf->writeEntry("CustomButtonPositions", cbUseCustomButtonPositions->isChecked());
	conf->writeEntry("ShowToolTips", cbShowToolTips->isChecked());
//	conf->writeEntry("MiniWindowBorders", cbUseMiniWindows->isChecked());

	// Button settings
	conf->writeEntry("ButtonsOnLeft", dropSite->buttonsLeft );
	conf->writeEntry("ButtonsOnRight", dropSite->buttonsRight );

	oldLibraryName = currentLibraryName;
	currentLibraryName = libName;

	// We saved, so tell kcmodule that there have been  no new user changes made.
	emit changed(false);
}


void KWinDecorationModule::dcopUpdateClientList()
{
	// Changes the current active ListBox item, and
	// Loads a new plugin configuration tab if required.
	KConfig kwinConfig("kwinrc");
	kwinConfig.setGroup("Style");

	readConfig( &kwinConfig );
	resetPlugin( &kwinConfig );
}


// Virutal functions required by KCModule
void KWinDecorationModule::load()
{
	KConfig kwinConfig("kwinrc");
	kwinConfig.setGroup("Style");

	// Reset by re-reading the config 
	// The plugin doesn't need changing, as we have not saved
	readConfig( &kwinConfig );
	emit pluginLoad( &kwinConfig );
}


void KWinDecorationModule::save()
{
	KConfig kwinConfig("kwinrc");
	kwinConfig.setGroup("Style");

	writeConfig( &kwinConfig );
	emit pluginSave( &kwinConfig );

	kwinConfig.sync();
	resetKWin();
	// resetPlugin() will get called via the above DCOP function
}


void KWinDecorationModule::defaults()
{
	// Set the KDE defaults
	cbUseCustomButtonPositions->setChecked( false );
	cbShowToolTips->setChecked( true );
//	cbUseMiniWindows->setChecked( false);
// Don't set default for now
//	decorationListBox->setSelected( 0, true );  // KDE2 default client

	dropSite->buttonsLeft = "MS";
	dropSite->buttonsRight= "HIAX";
	dropSite->repaint(false);

	buttonSource->showAllButtons();
	buttonSource->hideButton('M');
	buttonSource->hideButton('S');
	buttonSource->hideButton('H');
	buttonSource->hideButton('I');
	buttonSource->hideButton('A');
	buttonSource->hideButton('X');

	// Set plugin defaults
	emit pluginDefaults();
}


QString KWinDecorationModule::quickHelp() const
{
	return i18n( "<h1>Window Manager Decoration</h1>"
                 "This module allows you to choose the window border decorations, "
                "as well as titlebar button positions and custom decoration options.");
}


const KAboutData* KWinDecorationModule::aboutData() const
{
	KAboutData* about = 
		new KAboutData(I18N_NOOP("kcmkwindecoration"),
				I18N_NOOP("Window Decoration Control Module"),
				0, 0, KAboutData::License_GPL,
				I18N_NOOP("(c) 2001 Karol Szwed"));
	about->addAuthor("Karol Szwed", 0, "gallium@kde.org");
	return about;
}


void KWinDecorationModule::resetKWin()
{
	bool ok = kapp->dcopClient()->send("kwin", "KWinInterface",
                        "reconfigure()", QByteArray());
	if (!ok) 
		kdDebug() << "kcmkwindecoration: Could not reconfigure kwin" << endl;
}

#include "kwindecoration.moc"
// vim: ts=4

