/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
******************************************************************/
#include <kglobal.h>
#include <kconfig.h>
#include <kstddirs.h>
#include <kdesktopfile.h>
#include <ksimpleconfig.h>
#include <klocale.h>
#include <klibloader.h>

// X11/Qt conflict
#undef Unsorted

#include <qdir.h>
#include <qfile.h>

#include "plugins.h"

#if 0
#define lt_ptr lt_ptr_t
#endif

using namespace KWinInternal;

const char* defaultPlugin = "libkwindefault";


PluginMgr::PluginMgr()
    : QObject()
{
    alloc_ptr = NULL;
    handle = 0;
    pluginStr = "kwin_undefined";

    updatePlugin();
}

PluginMgr::~PluginMgr()
{
    if(handle) {
        // Call the plugin's cleanup function
	lt_ptr_t deinit_func = lt_dlsym(handle, "deinit");
	if (deinit_func)
	    ((void (*)())deinit_func)();
        lt_dlclose(handle);
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
    static bool dlregistered = false;
    lt_dlhandle oldHandle = handle;
    handle = 0;

    if(!dlregistered) {
       dlregistered = true;
       lt_dlinit();
    }

    QString path = KLibLoader::findLibrary(nameStr.latin1());

    // If the plugin was not found, try to find the default
    if (path.isEmpty()) {
        nameStr = defaultPlugin;
        path = KLibLoader::findLibrary(nameStr.latin1());
    }

    // If no library was found, exit kwin with an error message
    if (path.isEmpty())
        shutdownKWin(i18n("No window decoration plugin library was found!"));

    // Check if this library is not already loaded.
    if(pluginStr == nameStr) 
	return;

    // Try loading the requested plugin
    handle = lt_dlopen(path.latin1());

    // If that fails, fall back to the default plugin
    if (!handle) {
        nameStr = defaultPlugin;
        path = KLibLoader::findLibrary(nameStr.latin1());
	if (!path.isEmpty())
            handle = lt_dlopen(path.latin1());
    }

    if (!handle)
        shutdownKWin(i18n("The default decoration plugin is corrupt "
                          "and could not be loaded!"));

    // Call the plugin's initialisation function
    lt_ptr_t init_func = lt_dlsym(handle, "init");
    if (init_func)
        ((void (*)())init_func)();

    lt_ptr_t alloc_func = lt_dlsym(handle, "allocate");
    if(alloc_func) {
        alloc_ptr = (Client* (*)(Workspace *ws, WId w, int tool))alloc_func;
    } else {
        qWarning("KWin: The library %s is not a KWin plugin.", path.latin1());
        lt_dlclose(handle);
        exit(1);
    }

    pluginStr = nameStr;
    emit resetAllClients();

    // Call the old plugin's cleanup function
    if(oldHandle) {
	lt_ptr_t deinit_func = lt_dlsym(oldHandle, "deinit");
	if (deinit_func)
	    ((void (*)())deinit_func)();
        lt_dlclose(oldHandle);
    }
}

void PluginMgr::resetPlugin()
{
    lt_ptr_t reset_func = lt_dlsym(handle, "reset");
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

