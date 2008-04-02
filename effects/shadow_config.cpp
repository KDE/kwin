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

#include "shadow_config.h"
#include "shadow_helper.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kconfiggroup.h>
#include <kcolorbutton.h>
#include <kcolorscheme.h>

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>

#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

ShadowEffectConfig::ShadowEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(new QLabel(i18n("X offset:"), this), 0, 0);
    mShadowXOffset = new QSpinBox(this);
    mShadowXOffset->setRange(-20, 20);
    connect(mShadowXOffset, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mShadowXOffset, 0, 1);

    layout->addWidget(new QLabel(i18n("Y offset:"), this), 1, 0);
    mShadowYOffset = new QSpinBox(this);
    mShadowYOffset->setRange(-20, 20);
    connect(mShadowYOffset, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mShadowYOffset, 1, 1);

    layout->addWidget(new QLabel(i18n("Shadow opacity:"), this), 2, 0);
    mShadowOpacity = new QSpinBox(this);
    mShadowOpacity->setRange(0, 100);
    mShadowOpacity->setSuffix("%");
    connect(mShadowOpacity, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mShadowOpacity, 2, 1);

    layout->addWidget(new QLabel(i18n("Shadow fuzzyness:"), this), 3, 0);
    mShadowFuzzyness = new QSpinBox(this);
    mShadowFuzzyness->setRange(0, 20);
    connect(mShadowFuzzyness, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mShadowFuzzyness, 3, 1);

    layout->addWidget(new QLabel(i18n("Shadow size (relative to window):"), this), 4, 0);
    mShadowSize = new QSpinBox(this);
    mShadowSize->setRange(0, 20);
    connect(mShadowSize, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mShadowSize, 4, 1);

    layout->addWidget(new QLabel(i18n("Shadow color:"), this), 5, 0);
    mShadowColor = new KColorButton(this);
    mShadowColor->setDefaultColor(schemeShadowColor());
    connect(mShadowColor, SIGNAL(changed(QColor)), this, SLOT(changed()));
    layout->addWidget(mShadowColor, 5, 1);

    mIntensifyActiveShadow = new QCheckBox(i18n("Active window has stronger shadow"), this);
    connect(mIntensifyActiveShadow, SIGNAL(toggled(bool)), this, SLOT(changed()));
    layout->addWidget(mIntensifyActiveShadow, 6, 0, 1, 2);

    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 6, 0, 1, 2);

    load();
    }

ShadowEffectConfig::~ShadowEffectConfig()
    {
    kDebug() ;
    }

void ShadowEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Shadow");
    mShadowXOffset->setValue( conf.readEntry( "XOffset", 0 ) );
    mShadowYOffset->setValue( conf.readEntry( "YOffset", 3 ) );
    mShadowOpacity->setValue( (int)( conf.readEntry( "Opacity", 0.25 ) * 100 ) );
    mShadowFuzzyness->setValue( conf.readEntry( "Fuzzyness", 10 ) );
    mShadowSize->setValue( conf.readEntry( "Size", 5 ) );
    mShadowColor->setColor( conf.readEntry( "Color", schemeShadowColor() ) );
    mIntensifyActiveShadow->setChecked( conf.readEntry( "IntensifyActiveShadow", true ) );

    emit changed(false);
    }

void ShadowEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("Shadow");
    conf.writeEntry( "XOffset", mShadowXOffset->value() );
    conf.writeEntry( "YOffset", mShadowYOffset->value() );
    conf.writeEntry( "Opacity", mShadowOpacity->value() / 100.0 );
    conf.writeEntry( "Fuzzyness", mShadowFuzzyness->value() );
    conf.writeEntry( "Size", mShadowSize->value() );
    QColor userColor = mShadowColor->color();
    if (userColor == schemeShadowColor()) {
        // If the user has reset the color to the default we want to start
        // picking up color scheme changes again in the shadow effect
        conf.deleteEntry( "Color" );
    } else {
        conf.writeEntry( "Color", userColor );
    }
    conf.writeEntry( "IntensifyActiveShadow", mIntensifyActiveShadow->isChecked() );
    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "shadow" );
    }

void ShadowEffectConfig::defaults()
    {
    kDebug() ;
    mShadowXOffset->setValue( 0 );
    mShadowYOffset->setValue( 3 );
    mShadowOpacity->setValue( 25 );
    mShadowFuzzyness->setValue( 10 );
    mShadowSize->setValue( 5 );
    mIntensifyActiveShadow->setChecked( true );
    mShadowColor->setColor( schemeShadowColor() );
    emit changed(true);
    }


} // namespace

#include "shadow_config.moc"
