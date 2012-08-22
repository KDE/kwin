/*****************************************************************
// vim: sw=4 sts=4 et tw=100
This file is part of the KDE project.

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#include "kdecoration_plugins_p.h"

#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <klibrary.h>
#include <kconfiggroup.h>
#include <assert.h>

#include <QDir>
#include <QFile>

#include "kdecorationfactory.h"

KDecorationPlugins::KDecorationPlugins(const KSharedConfigPtr &cfg)
    :   create_ptr(NULL),
        library(NULL),
        fact(NULL),
        old_library(NULL),
        old_fact(NULL),
        pluginStr("kwin3_undefined "),
        config(cfg)
{
}

KDecorationPlugins::~KDecorationPlugins()
{
    if (library) {
        assert(fact != NULL);
        delete fact;
        library->unload();
    }
    if (old_library) {
        assert(old_fact != NULL);
        delete old_fact;
        old_library->unload();
    }
}

QString KDecorationPlugins::currentPlugin()
{
    return pluginStr;
}

bool KDecorationPlugins::reset(unsigned long changed)
{
    QString oldPlugin = pluginStr;
    config->reparseConfiguration();
    bool ret = false;
    if ((!loadPlugin("") && library)     // "" = read the one in cfg file
            || oldPlugin == pluginStr) {
        // no new plugin loaded, reset the old one
//       assert( fact != NULL );
        if (fact != NULL) {
            ret = fact->reset(changed);
        }

    }
    return ret || oldPlugin != pluginStr;
}

KDecorationFactory* KDecorationPlugins::factory()
{
    return fact;
}

// convenience
KDecoration* KDecorationPlugins::createDecoration(KDecorationBridge* bridge)
{
    if (fact != NULL)
        return fact->createDecoration(bridge);
    return NULL;
}

// tests whether the plugin can be loaded
bool KDecorationPlugins::canLoad(QString nameStr, KLibrary **loadedLib)
{
    if (nameStr.isEmpty())
        return false; // we can't load that

    // Check if this library is not already loaded.
    if (pluginStr == nameStr) {
        if (loadedLib) {
            *loadedLib = library;
        }
        return true;
    }

    KConfigGroup group(config, QString("Style"));
    if (group.readEntry<bool>("NoPlugin", false)) {
        error(i18n("Loading of window decoration plugin library disabled in configuration."));
        return false;
    }

    KLibrary libToFind(nameStr);
    QString path = libToFind.fileName();
    kDebug(1212) << "kwin : path " << path << " for " << nameStr;

    if (path.isEmpty()) {
        return false;
    }

    // Try loading the requested plugin
    KLibrary *lib = new KLibrary(path);

    if (!lib)
        return false;

    // TODO this would be a nice shortcut, but for "some" reason QtCurve with wrong ABI slips through
    // TODO figure where it's loaded w/o being unloaded and check whether that can be fixed.
#if 0
    if (lib->isLoaded()) {
        if (loadedLib) {
            *loadedLib = lib;
        }
        return true;
    }
#endif
    // so we check whether this lib was loaded before and don't unload it in case
    bool wasLoaded = lib->isLoaded();

    KDecorationFactory*(*cptr)() = NULL;
    int (*vptr)()  = NULL;
    int deco_version = 0;
    KLibrary::void_function_ptr version_func = lib->resolveFunction("decoration_version");
    if (version_func) {
        vptr = (int(*)())version_func;
        deco_version = vptr();
    } else {
        // block some decos known to link the unstable API but (for now) let through other legacy stuff
        const bool isLegacyStableABI = !(nameStr.contains("qtcurve", Qt::CaseInsensitive) ||
                                         nameStr.contains("crystal", Qt::CaseInsensitive) ||
                                         nameStr.contains("oxygen", Qt::CaseInsensitive));
        if (isLegacyStableABI) {
            // it's an old build of a legacy decoration that very likely uses the stable API
            // so we just set the API version to the current one
            // TODO: remove for 4.9.x or 4.10 - this is just to cover recompiles
            deco_version = KWIN_DECORATION_API_VERSION;
        }
        kWarning(1212) << QString("****** The library %1 has no API version ******").arg(path);
        kWarning(1212) << "****** Please use the KWIN_DECORATION macro in extern \"C\" to get this decoration loaded in future versions of kwin";
    }
    if (deco_version != KWIN_DECORATION_API_VERSION) {
        if (version_func)
            kWarning(1212) << i18n("The library %1 has wrong API version %2", path, deco_version);
        lib->unload();
        delete lib;
        return false;
    }

    KLibrary::void_function_ptr create_func = lib->resolveFunction("create_factory");
    if (create_func)
        cptr = (KDecorationFactory * (*)())create_func;

    if (!cptr) {
        kDebug(1212) << i18n("The library %1 is not a KWin plugin.", path);
        lib->unload();
        delete lib;
        return false;
    }

    if (loadedLib) {
        *loadedLib = lib;
    } else {
        if (!wasLoaded)
            lib->unload();
        delete lib;
    }
    return true;
}

// returns true if plugin was loaded successfully
bool KDecorationPlugins::loadPlugin(QString nameStr)
{
    KConfigGroup group(config, QString("Style"));
    if (nameStr.isEmpty()) {
        nameStr = group.readEntry("PluginLib", defaultPlugin);
    }

    // Check if this library is not already loaded.
    if (pluginStr == nameStr)
        return true;

    KLibrary *oldLibrary = library;
    KDecorationFactory* oldFactory = fact;

    if (!canLoad(nameStr, &library)) {
        // If that fails, fall back to the default plugin
        nameStr = defaultPlugin;
        if (!canLoad(nameStr, &library)) {
            // big time trouble!
            // -> exit kwin with an error message
            error(i18n("The default decoration plugin is corrupt and could not be loaded."));
            return false;
        }
    }

    // guarded by "canLoad"
    KLibrary::void_function_ptr create_func = library->resolveFunction("create_factory");
    if (create_func)
        create_ptr = (KDecorationFactory * (*)())create_func;
    if (!create_ptr) {  // this means someone probably attempted to load "some" kwin plugin/lib as deco
                        // and thus cheated on the "isLoaded" shortcut -> try the default and yell a warning
        if (nameStr != defaultPlugin) {
            kWarning(1212) << i18n("The library %1 was attempted to be loaded as a decoration plugin but it is NOT", nameStr);
            return loadPlugin(defaultPlugin);
        } else {
            // big time trouble!
            // -> exit kwin with an error message
            error(i18n("The default decoration plugin is corrupt and could not be loaded."));
            return false;
        }
    }

    fact = create_ptr();
    fact->checkRequirements(this);   // let it check what is supported

    pluginStr = nameStr;

    // For clients in kdeartwork
    QString catalog = nameStr;
    catalog.replace("kwin3_", "kwin_");
    KGlobal::locale()->insertCatalog(catalog);
    // For KCommonDecoration based clients
    KGlobal::locale()->insertCatalog("libkdecorations");
    // For clients in kdebase
    KGlobal::locale()->insertCatalog("kwin_clients");
    // For clients in kdeartwork
    KGlobal::locale()->insertCatalog("kwin_art_clients");

    old_library = oldLibrary; // save for delayed destroying
    old_fact = oldFactory;

    return true;
}

void KDecorationPlugins::destroyPreviousPlugin()
{
    // Destroy the old plugin
    if (old_library) {
        delete old_fact;
        old_fact = NULL;
        old_library->unload();
        delete old_library;
        old_library = NULL;
    }
}

void KDecorationPlugins::error(const QString&)
{
}
