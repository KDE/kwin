/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
******************************************************************/
#include <kglobal.h>
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kdesktopfile.h>
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
    alloc_ptr = NULL;
    library = 0;
    pluginStr = "kwin_undefined";

    updatePlugin();
}

PluginMgr::~PluginMgr()
{
    if(library) {
        // Call the plugin's cleanup function
	void *deinit_func = library->symbol("deinit");
	if (deinit_func)
	    ((void (*)())deinit_func)();
	library->unload();
	library = 0;
    }
}

void PluginMgr::updatePlugin()
{
    KConfig *config = KGlobal::config();
    config->reparseConfiguration();
    config->setGroup("Style");
    QString newPlugin = config->readEntry("PluginLib", defaultPlugin);

    if (newPlugin != pluginStr) 
         loadPlugin(newPlugin);
}

Client* PluginMgr::allocateClient(Workspace *ws, WId w, bool tool)
{
    // We are guaranteed to have a plugin loaded,
    // otherwise, kwin exits during loadPlugin - but verify anyway
    if (alloc_ptr)
        return(alloc_ptr(ws, w, tool));
    else
        return NULL;
}

// returns true if plugin was loaded successfully
void PluginMgr::loadPlugin(QString nameStr)
{
    KLibrary *oldLibrary = library;
    library = 0;

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
	return;

    // Try loading the requested plugin
    library = KLibLoader::self()->library(QFile::encodeName(path));

    // If that fails, fall back to the default plugin
    if (!library) {
        nameStr = defaultPlugin;
        path = KLibLoader::findLibrary(QFile::encodeName(nameStr));
	if (!path.isEmpty())
            library = KLibLoader::self()->library(QFile::encodeName(path));
    }

    if (!library)
        shutdownKWin(i18n("The default decoration plugin is corrupt "
                          "and could not be loaded!"));

    // Call the plugin's initialisation function
    void *init_func = library->symbol("init");
    if (init_func)
        ((void (*)())init_func)();

    void *alloc_func = library->symbol("allocate");
    if(alloc_func) {
        alloc_ptr = (Client* (*)(Workspace *ws, WId w, int tool))alloc_func;
    } else {
        qWarning("KWin: The library %s is not a KWin plugin.", path.latin1());
        library->unload();
        exit(1);
    }

    pluginStr = nameStr;
    emit resetAllClients();

    // Call the old plugin's cleanup function
    if(oldLibrary) {
	void *deinit_func = oldLibrary->symbol("deinit");
	if (deinit_func)
	    ((void (*)())deinit_func)();
	oldLibrary->unload();
    }
}

void PluginMgr::resetPlugin()
{
    void *reset_func = library->symbol("reset");
    if (reset_func)
       ((void (*)())reset_func)();
}

void PluginMgr::shutdownKWin(const QString &error_msg)
{
    qWarning( (i18n("KWin: ") + error_msg + 
               i18n("\nKWin will now exit...")).latin1() );
    exit(1);
}

#include "plugins.moc"

