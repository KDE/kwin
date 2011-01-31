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

    connect(m_ui->scaleBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->outlineBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));

    load();
}

void ResizeEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Resize");
    m_ui->scaleBox->setChecked(conf.readEntry<bool>("TextureScale", true));
    m_ui->outlineBox->setChecked(conf.readEntry<bool>("Outline" , false));

    emit changed(false);
}

void ResizeEffectConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("Resize");
    conf.writeEntry("TextureScale", m_ui->scaleBox->isChecked());
    conf.writeEntry("Outline", m_ui->outlineBox->isChecked());

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("resize");
}

void ResizeEffectConfig::defaults()
{
    m_ui->scaleBox->setChecked(true);
    m_ui->outlineBox->setChecked(false);
    emit changed(true);
}

} // namespace

#include "resize_config.moc"
