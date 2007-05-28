/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "shadow_config.h"

#include <kwineffects.h>

#include <kgenericfactory.h>
#include <klocale.h>
#include <kdebug.h>

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>
#include <QSpinBox>

KWIN_EFFECT_CONFIG( shadow, KWin::ShadowEffectConfig )

namespace KWin
{

ShadowEffectConfig::ShadowEffectConfig(QWidget* parent, const QStringList& args) :
        KCModule(KGenericFactory<ShadowEffectConfig>::componentData(), parent, args)
    {
    kDebug() << k_funcinfo << endl;

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

    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 3, 0, 1, 2);

    load();
    }

ShadowEffectConfig::~ShadowEffectConfig()
    {
    kDebug() << k_funcinfo << endl;
    }

void ShadowEffectConfig::load()
    {
    kDebug() << k_funcinfo << endl;
    KCModule::load();

    KConfig c("kwinrc");
    KConfigGroup conf(&c, "Effect-Shadow");
    mShadowXOffset->setValue( conf.readEntry( "XOffset", 5 ) );
    mShadowYOffset->setValue( conf.readEntry( "YOffset", 5 ) );
    mShadowOpacity->setValue( (int)( conf.readEntry( "Opacity", 0.2 ) * 100 ) );

    emit changed(false);
    }

void ShadowEffectConfig::save()
    {
    kDebug() << k_funcinfo << endl;
    KCModule::save();

    KConfig c("kwinrc");
    KConfigGroup conf(&c, "Effect-Shadow");
    conf.writeEntry( "XOffset", mShadowXOffset->value() );
    conf.writeEntry( "YOffset", mShadowYOffset->value() );
    conf.writeEntry( "Opacity", mShadowOpacity->value() / 100.0 );
    conf.sync();

    emit changed(false);
    }

void ShadowEffectConfig::defaults()
    {
    kDebug() << k_funcinfo << endl;
    mShadowXOffset->setValue( 5 );
    mShadowYOffset->setValue( 5 );
    mShadowOpacity->setValue( 20 );
    emit changed(true);
    }


} // namespace

#include "shadow_config.moc"
