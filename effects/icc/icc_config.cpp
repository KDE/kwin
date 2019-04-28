/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2019 Vitaliy Filippov <vitalif@yourcmc.ru>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "icc_config.h"
#include <kwineffects_interface.h>
#include "iccconfig.h"
#include <config-kwin.h>

#include <QAction>

#include <KLocalizedString>
#include <KActionCollection>
#include <KAboutData>
#include <KPluginFactory>

#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(ICCEffectConfigFactory,
                           "icc_config.json",
                           registerPlugin<KWin::ICCEffectConfig>();)

namespace KWin
{

ICCEffectConfig::ICCEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("icc")), parent, args)
{
    ui.setupUi(this);
    ICCConfig::instance(KWIN_CONFIG);
    addConfig(ICCConfig::self(), this);
    load();
}

ICCEffectConfig::~ICCEffectConfig()
{
    // Undo unsaved changes
}

void ICCEffectConfig::load()
{
    KCModule::load();

    emit changed(false);
}

void ICCEffectConfig::save()
{
    KCModule::save();

    emit changed(false);
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("icc"));
}

void ICCEffectConfig::defaults()
{
    emit changed(true);
}


} // namespace

#include "icc_config.moc"
