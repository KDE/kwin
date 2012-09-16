/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "resize_config.h"
// KConfigSkeleton
#include "resizeconfig.h"

#include <kwineffects.h>

#include <kconfiggroup.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

ResizeEffectConfigForm::ResizeEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

ResizeEffectConfig::ResizeEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new ResizeEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    addConfig(ResizeConfig::self(), m_ui);

    load();
}

void ResizeEffectConfig::save()
{
    KCModule::save();
    EffectsHandler::sendReloadMessage("resize");
}

} // namespace

#include "resize_config.moc"
