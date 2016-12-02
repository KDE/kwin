/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include "diminactive_config.h"
// KConfigSkeleton
#include "diminactiveconfig.h"
#include <config-kwin.h>

#include <kwineffects_interface.h>

#include <KLocalizedString>
#include <kconfiggroup.h>
#include <KAboutData>
#include <KPluginFactory>

#include <QWidget>
#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(DimInactiveEffectConfigFactory,
                           "diminactive_config.json",
                           registerPlugin<KWin::DimInactiveEffectConfig>();)

namespace KWin
{

DimInactiveEffectConfigForm::DimInactiveEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

DimInactiveEffectConfig::DimInactiveEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(KAboutData::pluginData(QStringLiteral("diminactive")), parent, args)
{
    m_ui = new DimInactiveEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    DimInactiveConfig::instance(KWIN_CONFIG);
    addConfig(DimInactiveConfig::self(), m_ui);

    load();
}

void DimInactiveEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("diminactive"));
}

} // namespace

#include "diminactive_config.moc"
