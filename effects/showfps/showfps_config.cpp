/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "showfps_config.h"

// KConfigSkeleton
#include "showfpsconfig.h"
#include <config-kwin.h>
#include <kwineffects_interface.h>

#include <KLocalizedString>
#include <KAboutData>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(ShowFpsEffectConfigFactory,
                           "showfps_config.json",
                           registerPlugin<KWin::ShowFpsEffectConfig>();)

namespace KWin
{

ShowFpsEffectConfig::ShowFpsEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("showfps")), parent, args)
{
    m_ui = new Ui::ShowFpsEffectConfigForm;
    m_ui->setupUi(this);

    ShowFpsConfig::instance(KWIN_CONFIG);
    addConfig(ShowFpsConfig::self(), this);

    load();
}

ShowFpsEffectConfig::~ShowFpsEffectConfig()
{
    delete m_ui;
}

void ShowFpsEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("showfps"));
}

} // namespace

#include "showfps_config.moc"
