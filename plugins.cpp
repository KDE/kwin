/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
******************************************************************/
#include <kglobal.h>
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kdesktopfile.h>
#include <kdebug.h>
#include <ksimpleconfig.h>
#include <klocale.h>
#include <klibloader.h>

// X11/Qt conflict
#undef Unsorted

#include <qdir.h>
#include <qfile.h>

#include "plugins.h"

using namespace KWinInternal;

const char* defaultPlugin = "kwin_default";


PluginMgr::PluginMgr()
    : QObject()
{
    create_ptr = NULL;
    old_create_ptr = NULL;
    library = 0;
    pluginStr = "kwin_undefined";

    KConfig *config = KGlobal::config();
    config->setGroup("Style");
    loadPlugin( config->readEntry("PluginLib", defaultPlugin) );
}

PluginMgr::~PluginMgr()
{
    if(library) {
        // Call the plugin's cleanup function
        if( library->hasSymbol("deinit")) {
    	    void *deinit_func = library->symbol("deinit");
	    if (deinit_func)
	        ((void (*)())deinit_func)();
        }
	library->unload();
	library = 0;
    }
}

void PluginMgr::updatePlugin()
{
    KConfig *config = KGlobal::config();
    config->reparseConfiguration();
    config->setGroup("Style");
    if ( !loadPlugin( config->readEntry("PluginLib", defaultPlugin )) && library ) {
        if( library->hasSymbol("reset")) {
	    void *reset_func = library->symbol("reset");
	    if (reset_func)
	        ((void (*)())reset_func)();
        }
    }
}

Client* PluginMgr::createClient(Workspace *ws, WId w, NET::WindowType type)
{
    if (create_ptr) // prefer the newer function which accepts exact window type
        return(create_ptr(ws, w, type));
    if (old_create_ptr)
        return(old_create_ptr(ws, w, type == NET::Tool || type == NET::Menu));
    // We are guaranteed to have a plugin loaded,
    // otherwise, kwin exits during loadPlugin - but verify anyway
    return NULL;
}

// returns true if plugin was loaded successfully
bool PluginMgr::loadPlugin(QString nameStr)
{
    KLibrary *oldLibrary = library;

    QString path = KLibLoader::findLibrary(QFile::encodeName(nameStr));

    // If the plugin was not found, try to find the default
    if (path.isEmpty()) {
        nameStr = defaultPlugin;
        path = KLibLoader::findLibrary(QFile::encodeName(nameStr));
    }

    // If no library was found, exit kwin with an error message
    if (path.isEmpty())
        shutdownKWin(i18n("No window decoration plugin library was found!"));

    // Check if this library is not already loaded.
    if(pluginStr == nameStr)
	return FALSE;

    // Try loading the requested plugin
    library = KLibLoader::self()->library(QFile::encodeName(path));

    // If that fails, fall back to the default plugin
    if (!library) {
	kdDebug() << " could not load library, try default plugin again" << endl;
        nameStr = defaultPlugin;
	if ( pluginStr == nameStr )
	    return FALSE;
        path = KLibLoader::findLibrary(QFile::encodeName(nameStr));
	if (!path.isEmpty())
            library = KLibLoader::self()->library(QFile::encodeName(path));
    }

    if (!library)
        shutdownKWin(i18n("The default decoration plugin is corrupt "
                          "and could not be loaded!"));

    // Call the plugin's initialisation function
    if( library->hasSymbol("init")) {
        void *init_func = library->symbol("init");
        if (init_func)
            ((void (*)())init_func)();
    }

    if( library->hasSymbol("create")) {
        void* create_func = library->symbol("create");
        if(create_func) {
            create_ptr = (Client* (*)(Workspace *ws, WId w, NET::WindowType))create_func;
        }
    }
    if( library->hasSymbol("allocate")) {
        void* allocate_func = library->symbol("allocate");
        if(allocate_func) {
            old_create_ptr = (Client* (*)(Workspace *ws, WId w, int tool))allocate_func;
        }
    }
    if(!create_ptr && !old_create_ptr) {
        kdWarning() << "KWin: The library " << path << " is not a KWin plugin." << endl;
        library->unload();
        exit(1);
    }

    pluginStr = nameStr;
    emit resetAllClients();

    // Call the old plugin's cleanup function
    if(oldLibrary) {
        if( library->hasSymbol("deinit")) {
	    void *deinit_func = oldLibrary->symbol("deinit");
	    if (deinit_func)
	        ((void (*)())deinit_func)();
        }
	oldLibrary->unload();
    }
    return TRUE;
}

void PluginMgr::shutdownKWin(const QString &error_msg)
{
    qWarning( (i18n("KWin: ") + error_msg +
               i18n("\nKWin will now exit...")).latin1() );
    exit(1);
}

#include "plugins.moc"

