/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2007 Martin Gräßlin <ubuntu@martin-graesslin.com

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

#include "snow_config.h"
#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <kconfiggroup.h>

#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

SnowEffectConfigForm::SnowEffectConfigForm(QWidget* parent) : QWidget(parent)
    {
    setupUi(this);
    }

SnowEffectConfig::SnowEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    m_ui = new SnowEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));
    connect(m_ui->numberFlakes, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->minSizeFlake, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->maxSizeFlake, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->maxVSpeed, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->maxHSpeed, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->snowBehindWindows, SIGNAL(stateChanged(int)), this, SLOT(changed()));

    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup("Snow");
    m_actionCollection->setConfigGlobal(true);

    KAction* a = (KAction*)m_actionCollection->addAction( "Snow" );
    a->setText( i18n("Toggle Snow on Desktop" ));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::META + Qt::Key_F12 ));

    m_ui->editor->addCollection(m_actionCollection);
    }

SnowEffectConfig::~SnowEffectConfig()
    {
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
    }

void SnowEffectConfig::load()
    {
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Snow");

    int number = conf.readEntry("Number", 200);
    int minFlake = conf.readEntry("MinFlakes", 10);
    int maxFlake = conf.readEntry("MaxFlakes", 50);
    int maxVSpeed = conf.readEntry("MaxVSpeed", 2);
    int maxHSpeed = conf.readEntry("MaxHSpeed", 1);
    m_ui->snowBehindWindows->setChecked( conf.readEntry("BehindWindows", true));
    m_ui->numberFlakes->setValue( number );
    m_ui->minSizeFlake->setValue( minFlake );
    m_ui->maxSizeFlake->setValue( maxFlake );
    m_ui->maxVSpeed->setValue( maxVSpeed );
    m_ui->maxHSpeed->setValue( maxHSpeed );


    emit changed(false);
    }

void SnowEffectConfig::save()
    {
    KConfigGroup conf = EffectsHandler::effectConfig("Snow");
    conf.writeEntry("Number", m_ui->numberFlakes->value());
    conf.writeEntry("MinFlakes", m_ui->minSizeFlake->value());
    conf.writeEntry("MaxFlakes", m_ui->maxSizeFlake->value());
    conf.writeEntry("MaxVSpeed", m_ui->maxVSpeed->value());
    conf.writeEntry("MaxHSpeed", m_ui->maxHSpeed->value());
    conf.writeEntry("BehindWindows", m_ui->snowBehindWindows->isChecked());

    m_ui->editor->save();   // undo() will restore to this state from now on

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "snow" );
    }

void SnowEffectConfig::defaults()
    {
    m_ui->numberFlakes->setValue( 200 );
    m_ui->minSizeFlake->setValue( 10 );
    m_ui->maxSizeFlake->setValue( 50 );
    m_ui->maxVSpeed->setValue( 2 );
    m_ui->maxHSpeed->setValue( 1 );
    m_ui->snowBehindWindows->setChecked( true );
    m_ui->editor->allDefault();
    emit changed(true);
    }


} // namespace

#include "snow_config.moc"
