/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com

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
#include "coverswitch_config.h"
// KConfigSkeleton
#include "coverswitchconfig.h"

#include <kwineffects.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

CoverSwitchEffectConfigForm::CoverSwitchEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

CoverSwitchEffectConfig::CoverSwitchEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new CoverSwitchEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    connect(m_ui->kcfg_Thumbnails, SIGNAL(stateChanged(int)), this, SLOT(thumbnailsChanged()));
    connect(m_ui->kcfg_DynamicThumbnails, SIGNAL(stateChanged(int)), this, SLOT(thumbnailsChanged()));

    addConfig(CoverSwitchConfig::self(), m_ui);
}

void CoverSwitchEffectConfig::save()
{
    KCModule::save();
    EffectsHandler::sendReloadMessage("coverswitch");
}

void CoverSwitchEffectConfig::thumbnailsChanged()
{
    bool enabled = m_ui->kcfg_Thumbnails->isChecked() && m_ui->kcfg_DynamicThumbnails->isChecked();
    m_ui->kcfg_DynamicThumbnails->setEnabled(m_ui->kcfg_Thumbnails->isChecked());
    m_ui->kcfg_ThumbnailWindows->setEnabled(enabled);
    m_ui->labelThumbnailWindows->setEnabled(enabled);
}


} // namespace

#include "coverswitch_config.moc"
