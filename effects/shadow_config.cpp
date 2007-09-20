/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "shadow_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>


namespace KWin
{

ShadowEffectConfig::ShadowEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(new QLabel(i18n("X offset:")), 0, 0);
    mShadowXOffset = new QSpinBox;
    mShadowXOffset->setRange(-20, 20);
    connect(mShadowXOffset, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mShadowXOffset, 0, 1);

    layout->addWidget(new QLabel(i18n("Y offset:")), 1, 0);
    mShadowYOffset = new QSpinBox;
    mShadowYOffset->setRange(-20, 20);
    connect(mShadowYOffset, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mShadowYOffset, 1, 1);

    layout->addWidget(new QLabel(i18n("Shadow opacity:")), 2, 0);
    mShadowOpacity = new QSpinBox;
    mShadowOpacity->setRange(0, 100);
    mShadowOpacity->setSuffix("%");
    connect(mShadowOpacity, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mShadowOpacity, 2, 1);

    layout->addWidget(new QLabel(i18n("Shadow fuzzyness:")), 3, 0);
    mShadowFuzzyness = new QSpinBox;
    mShadowFuzzyness->setRange(0, 20);
    connect(mShadowFuzzyness, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    layout->addWidget(mShadowFuzzyness, 3, 1);

    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 4, 0, 1, 2);

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
    mShadowXOffset->setValue( conf.readEntry( "XOffset", 10 ) );
    mShadowYOffset->setValue( conf.readEntry( "YOffset", 10 ) );
    mShadowOpacity->setValue( (int)( conf.readEntry( "Opacity", 0.2 ) * 100 ) );
    mShadowFuzzyness->setValue( conf.readEntry( "Fuzzyness", 10 ) );

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
    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "shadow" );
    }

void ShadowEffectConfig::defaults()
    {
    kDebug() ;
    mShadowXOffset->setValue( 10 );
    mShadowYOffset->setValue( 10 );
    mShadowOpacity->setValue( 20 );
    mShadowFuzzyness->setValue( 10 );
    emit changed(true);
    }


} // namespace

#include "shadow_config.moc"
