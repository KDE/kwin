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
#include <qgroupbox.h>
#include <qcheckbox.h>
#include <qtabwidget.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qfile.h>

#include <kapplication.h>
#include <kcombobox.h>
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
#include "preview.h"
#include <kdecoration_plugins_p.h>

// KCModule plugin interface
// =========================
typedef KGenericFactory<KWinDecorationModule, QWidget> KWinDecoFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_kwindecoration, KWinDecoFactory("kcmkwindecoration") )

KWinDecorationModule::KWinDecorationModule(QWidget* parent, const char* name, const QStringList &)
	: DCOPObject("KWinClientDecoration"),
	  KCModule(KWinDecoFactory::instance(), parent, name),
          kwinConfig("kwinrc"),
          pluginObject(0)
{
	kwinConfig.setGroup("Style");
        plugins = new KDecorationPlugins( &kwinConfig );

	QVBoxLayout* layout = new QVBoxLayout(this);
	tabWidget = new QTabWidget( this );
	layout->addWidget( tabWidget );

	// Page 1 (General Options)
	QWidget *pluginPage = new QWidget( tabWidget );

	QHBox *hbox = new QHBox(pluginPage);
	hbox->setSpacing(KDialog::spacingHint());
//	QLabel *lbl = new QLabel( i18n("&Decoration:"), hbox );
	decorationList = new KComboBox( hbox );
//	lbl->setBuddy(decorationList);
	QString whatsThis = i18n("Select the window decoration. This is the look and feel of both "
                             "the window borders and the window handle.");
//	QWhatsThis::add(lbl, whatsThis);
	QWhatsThis::add(decorationList, whatsThis);

	QVBoxLayout* pluginLayout = new QVBoxLayout(pluginPage, KDialog::marginHint(), KDialog::spacingHint());
	pluginLayout->addWidget(hbox);

// Save this for later...
//	cbUseMiniWindows = new QCheckBox( i18n( "Render mini &titlebars for all windows"), checkGroup );
//	QWhatsThis::add( cbUseMiniWindows, i18n( "Note that this option is not available on all styles yet!" ) );

        QFrame* preview_frame = new QFrame( pluginPage );
        preview_frame->setFrameShape( QFrame::NoFrame );
        QVBoxLayout* preview_layout = new QVBoxLayout( preview_frame, 0, KDialog::spacingHint());
        preview = new KDecorationPreview( preview_frame );
        preview_layout->addWidget( preview );
        pluginLayout->addWidget( preview_frame );
        pluginLayout->setStretchFactor( preview_frame, 10 );

	pluginSettingsLbl = new QLabel( i18n("Decoration Options"), pluginPage );
	pluginSettingsLine = new QFrame( pluginPage );
	pluginSettingsLine ->setFrameStyle( QFrame::HLine | QFrame::Plain );
	pluginConfigWidget = new QVBox(pluginPage);
	pluginLayout->addWidget(pluginSettingsLbl );
	pluginLayout->addWidget(pluginSettingsLine);
	pluginLayout->addWidget(pluginConfigWidget);

	// Page 2 (Button Selector)
	QVBox* buttonPage = new QVBox( tabWidget );
	buttonPage->setSpacing( KDialog::spacingHint() );
	buttonPage->setMargin( KDialog::marginHint() );

	cbShowToolTips = new QCheckBox(
			i18n("&Show window button tooltips"), buttonPage );
	QWhatsThis::add( cbShowToolTips,
			i18n(  "Enabling this checkbox will show window button tooltips. "
				   "If this checkbox is off, no window button tooltips will be shown."));

	cbUseCustomButtonPositions = new QCheckBox(
			i18n("Use custom titlebar button &positions"), buttonPage );
	QWhatsThis::add( cbUseCustomButtonPositions,
			i18n(  "The appropriate settings can be found in the \"Buttons\" Tab. "
				   "Please note that this option is not available on all styles yet!" ) );

	buttonBox = new QGroupBox( 1, Qt::Horizontal,
			i18n("Titlebar Button Positions"), buttonPage );

	// Add nifty dnd button modification widgets
	QLabel* label = new QLabel( buttonBox );
	dropSite = new ButtonDropSite( buttonBox );
	label->setText( i18n( "To add or remove titlebar buttons, simply <i>drag</i> items "
		"between the available item list and the titlebar preview. Similarly, "
		"drag items within the titlebar preview to re-position them.") );
	buttonSource = new ButtonSource( buttonBox );

	// Load all installed decorations into memory
	// Set up the decoration lists and other UI settings
	findDecorations();
	createDecorationList();
	readConfig( &kwinConfig );
	resetPlugin( &kwinConfig );

	tabWidget->insertTab( pluginPage, i18n("&Window Decoration") );
	tabWidget->insertTab( buttonPage, i18n("&Buttons") );

	connect( dropSite, SIGNAL(buttonAdded(char)), buttonSource, SLOT(hideButton(char)) );
	connect( dropSite, SIGNAL(buttonRemoved(char)), buttonSource, SLOT(showButton(char)) );
	connect( buttonSource, SIGNAL(buttonDropped()), dropSite, SLOT(removeClickedButton()) );
	connect( dropSite, SIGNAL(changed()), this, SLOT(slotSelectionChanged()) );
	connect( buttonSource, SIGNAL(selectionChanged()), this, SLOT(slotSelectionChanged()) );
	connect( decorationList, SIGNAL(activated(const QString&)), SLOT(slotSelectionChanged()) );
	connect( decorationList, SIGNAL(activated(const QString&)),
								SLOT(slotChangeDecoration(const QString&)) );
	connect( cbUseCustomButtonPositions, SIGNAL(clicked()), SLOT(slotSelectionChanged()) );
	connect(cbUseCustomButtonPositions, SIGNAL(toggled(bool)), buttonBox, SLOT(setEnabled(bool)));
	connect( cbShowToolTips, SIGNAL(clicked()), SLOT(slotSelectionChanged()) );
//	connect( cbUseMiniWindows, SIGNAL(clicked()), SLOT(slotSelectionChanged()) );

	// Allow kwin dcop signal to update our selection list
	connectDCOPSignal("kwin", 0, "dcopResetAllClients()", "dcopUpdateClientList()", false);
}


KWinDecorationModule::~KWinDecorationModule()
{
        delete preview; // needs to be destroyed before plugins
        delete plugins;
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

					if (!libName.isEmpty() && libName.startsWith( "kwin3_" ))
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


// Fills the decorationList with a list of available kwin decorations
void KWinDecorationModule::createDecorationList()
{
	QValueList<DecorationInfo>::ConstIterator it;

	// Sync with kwin hardcoded KDE2 style which has no desktop item
    QStringList decorationNames;
	decorationNames.append( i18n("KDE 2") );
	for (it = decorations.begin(); it != decorations.end(); ++it)
	{
		decorationNames.append((*it).name);
	}
	decorationNames.sort();
    decorationList->insertStringList(decorationNames);
}


// Reset the decoration plugin to what the user just selected
void KWinDecorationModule::slotChangeDecoration( const QString & text)
{
	KConfig kwinConfig("kwinrc");
	kwinConfig.setGroup("Style");

	// Let the user see config options for the currently selected decoration
	resetPlugin( &kwinConfig, text );
}


// This is the selection handler setting
void KWinDecorationModule::slotSelectionChanged()
{
	setChanged(true);
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
		libName = "kwin_default";	// KDE 2

	return libName;
}


// Loads/unloads and inserts the decoration config plugin into the
// pluginConfigWidget, allowing for dynamic configuration of decorations
void KWinDecorationModule::resetPlugin( KConfig* conf, const QString& currentDecoName )
{
	// Config names are "kwin_icewm_config"
	// for "kwin3_icewm" kwin client

	QString oldName = styleToConfigLib( oldLibraryName );

	QString currentName;
	if (!currentDecoName.isEmpty())
		currentName = decorationLibName( currentDecoName ); // Use what the user selected
	else
		currentName = currentLibraryName; // Use what was read from readConfig()

        if( plugins->loadPlugin( currentName )
            && preview->recreateDecoration( plugins ))
            preview->enablePreview();
        else
            preview->disablePreview();
        plugins->destroyPreviousPlugin();
                
	currentName = styleToConfigLib( currentName );

	// Delete old plugin widget if it exists
	delete pluginObject;
	pluginObject = 0;

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
			pluginObject = (QObject*)(allocatePlugin( conf, pluginConfigWidget ));

			// connect required signals and slots together...
			connect( pluginObject, SIGNAL(changed()), this, SLOT(slotSelectionChanged()) );
			connect( this, SIGNAL(pluginLoad(KConfig*)), pluginObject, SLOT(load(KConfig*)) );
			connect( this, SIGNAL(pluginSave(KConfig*)), pluginObject, SLOT(save(KConfig*)) );
			connect( this, SIGNAL(pluginDefaults()), pluginObject, SLOT(defaults()) );
			pluginSettingsLbl->show();
			pluginSettingsLine->show();
			pluginConfigWidget->show();
			return;
		}
	}

	pluginSettingsLbl->hide();
	pluginSettingsLine->hide();
	pluginConfigWidget->hide();
}



// Reads the kwin config settings, and sets all UI controls to those settings
// Updating the config plugin if required
void KWinDecorationModule::readConfig( KConfig* conf )
{
	// General tab
	// ============
	cbShowToolTips->setChecked( conf->readBoolEntry("ShowToolTips", true ));
//	cbUseMiniWindows->setChecked( conf->readBoolEntry("MiniWindowBorders", false));

	// Find the corresponding decoration name to that of
	// the current plugin library name

	oldLibraryName = currentLibraryName;
	currentLibraryName = conf->readEntry("PluginLib",
					((QPixmap::defaultDepth() > 8) ? "kwin_keramik" : "kwin_quartz"));
	QString decoName = decorationName( currentLibraryName );

	// If we are using the "default" kde client, use the "default" entry.
	if (decoName.isEmpty())
		decoName = i18n("KDE 2");

    int numDecos = decorationList->count();
	for (int i = 0; i < numDecos; ++i)
    {
		 if (decorationList->text(i) == decoName)
		 {
		 		 decorationList->setCurrentItem(i);
		 		 break;
		 }
	}

	// Buttons tab
	// ============
	bool customPositions = conf->readBoolEntry("CustomButtonPositions", false);
	cbUseCustomButtonPositions->setChecked( customPositions );
	buttonBox->setEnabled( customPositions );
	// Menu and onAllDesktops buttons are default on LHS
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

	setChanged(false);
}


// Writes the selected user configuration to the kwin config file
void KWinDecorationModule::writeConfig( KConfig* conf )
{
	QString name = decorationList->currentText();
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
	setChanged(false);
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
	buttonBox->setEnabled( false );
	cbShowToolTips->setChecked( true );
//	cbUseMiniWindows->setChecked( false);
// Don't set default for now
//	decorationList->setSelected(
//		decorationList->findItem( i18n("KDE 2") ), true );  // KDE classic client

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

QString KWinDecorationModule::styleToConfigLib( QString& styleLib )
{
        if( styleLib.startsWith( "kwin3_" ))
            return "kwin_" + styleLib.mid( 6 ) + "_config";
        else
            return styleLib + "_config";
}

QString KWinDecorationModule::quickHelp() const
{
	return i18n( "<h1>Window Manager Decoration</h1>"
		"<p>This module allows you to choose the window border decorations, "
		"as well as titlebar button positions and custom decoration options.</p>"
		"To choose a theme for your window decoration click on its name and apply your choice by clicking the \"Apply\" button below."
		" If you don't want to apply your choice you can click the \"Reset\" button to discard your changes."
		"<p>You can configure each theme in the \"Configure [...]\" tab. There are different options specific for each theme.</p>"
		"<p>In \"General Options (if available)\" you can activate the \"Buttons\" tab by checking the \"Use custom titlebar button positions\" box."
		" In the \"Buttons\" tab you can change the positions of the buttons to your liking.</p>" );
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

