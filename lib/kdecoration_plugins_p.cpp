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
#include <klibloader.h>
#include <kconfiggroup.h>
#include <assert.h>
#include <kconfig.h>

#include <QDir>
#include <QFile>

#include "kdecorationfactory.h"

KDecorationPlugins::KDecorationPlugins(const KSharedConfigPtr &cfg)
    :   create_ptr( NULL ),
        library( NULL ),
        fact( NULL ),
        old_library( NULL ),
        old_fact( NULL ),
        pluginStr( "kwin3_undefined " ),
        config( cfg )
    {
    }

KDecorationPlugins::~KDecorationPlugins()
    {
    if(library)
        {
        assert( fact != NULL );
        delete fact;
	library->unload();
        }
    if(old_library)
        {
        assert( old_fact != NULL );
        delete old_fact;
	old_library->unload();
        }
    }

bool KDecorationPlugins::reset( unsigned long changed )
    {
    QString oldPlugin = pluginStr;
    config->reparseConfiguration();
    bool ret = false;
    if(( !loadPlugin( "" ) && library ) // "" = read the one in cfg file
        || oldPlugin == pluginStr )
        { // no new plugin loaded, reset the old one
        assert( fact != NULL );
        ret = fact->reset( changed );
        }
    return ret || oldPlugin != pluginStr;
    }

KDecorationFactory* KDecorationPlugins::factory()
    {
    return fact;
    }

// convenience
KDecoration* KDecorationPlugins::createDecoration( KDecorationBridge* bridge )
    {
    if( fact != NULL )
        return fact->createDecoration( bridge );
    return NULL;
    }
    
// returns true if plugin was loaded successfully
bool KDecorationPlugins::loadPlugin( QString nameStr )
    {
    if( nameStr.isEmpty())
        {
        KConfigGroup group( config, QString("Style") );
        nameStr = group.readEntry("PluginLib", defaultPlugin );
        }
    // make sure people can switch between HEAD and kwin_iii branch
    if( nameStr.startsWith( "kwin_" ))
	nameStr = "kwin3_" + nameStr.mid( 5 );

    KLibrary *oldLibrary = library;
    KDecorationFactory* oldFactory = fact;

    QString path = KLibLoader::findLibrary(QFile::encodeName(nameStr));
	kDebug() << "kwin : path " << path << " for " << nameStr << endl;

    // If the plugin was not found, try to find the default
    if (path.isEmpty())
        {
        nameStr = defaultPlugin;
        path = KLibLoader::findLibrary(QFile::encodeName(nameStr));
        }

    // If no library was found, exit kwin with an error message
    if (path.isEmpty())
        {
        error( i18n("No window decoration plugin library was found." ));
        return false;
        }

    // Check if this library is not already loaded.
    if(pluginStr == nameStr)
	return true;

    // Try loading the requested plugin
    library = KLibLoader::self()->library(QFile::encodeName(path));

    // If that fails, fall back to the default plugin
    if (!library)
        {
	kDebug() << " could not load library, try default plugin again" << endl;
        nameStr = defaultPlugin;
	if ( pluginStr == nameStr )
	    return true;
        path = KLibLoader::findLibrary(QFile::encodeName(nameStr));
	if (!path.isEmpty())
            library = KLibLoader::self()->library(QFile::encodeName(path));
        }

    if (!library)
        {
        error( i18n("The default decoration plugin is corrupt "
                          "and could not be loaded." ));
        return false;
        }

    create_ptr = NULL;
    if( library->hasSymbol("create_factory"))
        {
        void* create_func = library->symbol("create_factory");
        if(create_func)
            create_ptr = (KDecorationFactory* (*)())create_func;
        }
    if(!create_ptr)
        {
        error( i18n( "The library %1 is not a KWin plugin.", path ));
        library->unload();
        return false;
        }
    fact = create_ptr();
    fact->checkRequirements( this ); // let it check what is supported

    pluginStr = nameStr;

    // For clients in kdeartwork    
    QString catalog = nameStr;
    catalog.replace( "kwin3_", "kwin_" );
    KGlobal::locale()->insertCatalog( catalog );
    // For KCommonDecoration based clients
    KGlobal::locale()->insertCatalog( "kwin_lib" );
    // For clients in kdebase
    KGlobal::locale()->insertCatalog( "kwin_clients" );
    // For clients in kdeartwork
    KGlobal::locale()->insertCatalog( "kwin_art_clients" );

    old_library = oldLibrary; // save for delayed destroying
    old_fact = oldFactory;
    
    return true;
}

void KDecorationPlugins::destroyPreviousPlugin()
{
    // Destroy the old plugin
    if(old_library)
        {
        delete old_fact;
        old_fact = NULL;
	old_library->unload();
        old_library = NULL;
        }
}

void KDecorationPlugins::error( const QString& )
    {
    }
