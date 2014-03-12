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
#include <klocale.h>
#include <klibrary.h>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KPluginTrader>
#include <kconfiggroup.h>
#include <KDE/KLocalizedString>
#include <assert.h>

#include <QDebug>
#include <QDir>
#include <QFile>

#include "kdecorationfactory.h"

KDecorationPlugins::KDecorationPlugins(const KSharedConfigPtr &cfg)
    :   fact(nullptr),
        old_fact(nullptr),
        pluginStr(QStringLiteral("undefined ")),
        config(cfg)
{
}

KDecorationPlugins::~KDecorationPlugins()
{
    delete fact;
    delete old_fact;
}

QString KDecorationPlugins::currentPlugin()
{
    return pluginStr;
}

bool KDecorationPlugins::reset()
{
    QString oldPlugin = pluginStr;
    config->reparseConfiguration();
    loadPlugin(QString()); // "" = read the one in cfg file
    return oldPlugin != pluginStr;
}

KDecorationFactory* KDecorationPlugins::factory()
{
    return fact;
}

const KDecorationFactory *KDecorationPlugins::factory() const
{
    return fact;
}

// convenience
KDecoration* KDecorationPlugins::createDecoration(KDecorationBridge* bridge)
{
    if (fact != nullptr)
        return fact->createDecoration(bridge);
    return nullptr;
}

// returns true if plugin was loaded successfully
bool KDecorationPlugins::loadPlugin(QString nameStr)
{
    KConfigGroup group(config, QStringLiteral("Style"));
    if (nameStr.isEmpty()) {
        nameStr = group.readEntry("PluginLib", defaultPlugin);
    }

    // Check if this library is not already loaded.
    if (pluginStr == nameStr)
        return true;

    if (group.readEntry<bool>("NoPlugin", false)) {
        error(i18n("Loading of window decoration plugin library disabled in configuration."));
        return false;
    }

    auto createFactory = [](const QString &pluginName) -> KDecorationFactory* {
        qDebug() << "Trying to load decoration plugin" << pluginName;
        const QString query = QStringLiteral("[X-KDE-PluginInfo-Name] == '%1'").arg(pluginName);
        const auto offers = KPluginTrader::self()->query(QStringLiteral("kf5/kwin/kdecorations"), QString(), query);
        if (offers.isEmpty()) {
            qDebug() << "Decoration plugin not found";
            return nullptr;
        }
        KPluginLoader loader(offers.first().libraryPath());
        if (loader.pluginVersion() != KWIN_DECORATION_API_VERSION) {
            qWarning() << i18n("The library %1 has wrong API version %2", loader.pluginName(), loader.pluginVersion());
            return nullptr;
        }
        KPluginFactory *factory = loader.factory();
        if (!factory) {
            qWarning() << "Error loading decoration library: " << loader.errorString();
            return nullptr;
        } else {
            return factory->create<KDecorationFactory>();
        }
    };
    KDecorationFactory *factory = createFactory(nameStr);
    if (!factory) {
        // If that fails, fall back to the default plugin
        factory = createFactory(defaultPlugin);
        if (!factory) {
            // big time trouble!
            // -> exit kwin with an error message
            error(i18n("The default decoration plugin is corrupt and could not be loaded."));
            return false;
        }
    }

    factory->checkRequirements(this);   // let it check what is supported

    pluginStr = nameStr;

    // For clients in kdeartwork
#warning insertCatalog needs porting
#if KWIN_QT5_PORTING
    QString catalog = nameStr;
    catalog.replace("kwin3_", "kwin_");
    KGlobal::locale()->insertCatalog(catalog);
    // For KCommonDecoration based clients
    KGlobal::locale()->insertCatalog("libkdecorations");
    // For clients in kdebase
    KGlobal::locale()->insertCatalog("kwin_clients");
    // For clients in kdeartwork
    KGlobal::locale()->insertCatalog("kwin_art_clients");
#endif

    old_fact = fact;
    fact = factory;

    return true;
}

void KDecorationPlugins::destroyPreviousPlugin()
{
    // Destroy the old plugin
    delete old_fact;
    old_fact = nullptr;
}

void KDecorationPlugins::error(const QString&)
{
}
