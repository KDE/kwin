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
#include "showfps.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

ShowFpsEffectConfig::ShowFpsEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new Ui::ShowFpsEffectConfigForm;
    m_ui->setupUi(this);

    connect(m_ui->textPosition, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->textFont, SIGNAL(fontSelected(const QFont&)), this, SLOT(changed()));
    connect(m_ui->textColor, SIGNAL(changed(const QColor&)), this, SLOT(changed()));
    connect(m_ui->textAlpha, SIGNAL(valueChanged(double)), this, SLOT(changed()));

    load();
}

ShowFpsEffectConfig::~ShowFpsEffectConfig()
{
    delete m_ui;
}

void ShowFpsEffectConfig::load()
{
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("ShowFps");

    int position = conf.readEntry("TextPosition", int(ShowFpsEffect::INSIDE_GRAPH));
    if (position > -1)
        m_ui->textPosition->setCurrentIndex(position);

    QFont font = conf.readEntry("TextFont", QFont());
    m_ui->textFont->setFont(font);

    QColor color = conf.readEntry("TextColor", QColor());
    if (color.isValid())
        m_ui->textColor->setColor(color);

    double alpha = conf.readEntry("TextAlpha", 1.0);
    m_ui->textAlpha->setValue(alpha);

    emit changed(false);
}

void ShowFpsEffectConfig::save()
{
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("ShowFps");

    int position = m_ui->textPosition->currentIndex();
    conf.writeEntry("TextPosition", position);

    QFont font = m_ui->textFont->font();
    conf.writeEntry("TextFont", font);

    QColor color = m_ui->textColor->color();
    conf.writeEntry("TextColor", color);

    double alpha = m_ui->textAlpha->value();
    conf.writeEntry("TextAlpha", alpha);

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage("showfps");
}

void ShowFpsEffectConfig::defaults()
{
    m_ui->textPosition->setCurrentIndex(0);
    m_ui->textFont->setFont(QFont());
    m_ui->textColor->setColor(QColor());
    m_ui->textAlpha->setValue(1.0);

    emit changed(true);
}

} // namespace

#include "showfps_config.moc"
