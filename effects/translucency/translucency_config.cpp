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

#include "translucency_config.h"
// KConfigSkeleton
#include "translucencyconfig.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kconfiggroup.h>

#include <QVBoxLayout>
#include <QCheckBox>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

TranslucencyEffectConfigForm::TranslucencyEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

TranslucencyEffectConfig::TranslucencyEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new TranslucencyEffectConfigForm(this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    addConfig(TranslucencyConfig::self(), m_ui);

    load();
}

void TranslucencyEffectConfig::save()
{
    KCModule::save();
    EffectsHandler::sendReloadMessage("translucency");
}

} // namespace

#include "translucency_config.moc"
