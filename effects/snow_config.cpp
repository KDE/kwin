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
#include <KGlobalAccel>
#include <kconfiggroup.h>

#include <QGridLayout>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif
namespace KWin
{

SnowEffectConfigForm::SnowEffectConfigForm(QWidget* parent) : QWidget(parent)
    {
    setupUi(this);
    }

SnowEffectConfig::SnowEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    m_ui = new SnowEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));
    connect(m_ui->numberFlakes, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->minSizeFlake, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->maxSizeFlake, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup("Snow");
    m_actionCollection->setConfigGlobal(true);

    KAction* a = (KAction*)m_actionCollection->addAction( "Snow" );
    a->setText( i18n("Toggle Snow on Desktop" ));
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::META + Qt::Key_F12 ));
    a->setProperty("isConfigurationAction", true);

    load();
    }

SnowEffectConfig::~SnowEffectConfig()
    {
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
    kDebug() ;
    }

void SnowEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Snow");

    int number = conf.readEntry("Number", 50);
    int minFlake = conf.readEntry("MinFlakes", 10);
    int maxFlake = conf.readEntry("MaxFlakes", 50);
    m_ui->numberFlakes->setValue( number );
    m_ui->minSizeFlake->setValue( minFlake );
    m_ui->maxSizeFlake->setValue( maxFlake );

    m_actionCollection->readSettings();
    m_ui->editor->addCollection(m_actionCollection);

    emit changed(false);
    }

void SnowEffectConfig::save()
    {
    kDebug() ;

    KConfigGroup conf = EffectsHandler::effectConfig("Snow");
    conf.writeEntry("Number", m_ui->numberFlakes->value());
    conf.writeEntry("MinFlakes", m_ui->minSizeFlake->value());
    conf.writeEntry("MaxFlakes", m_ui->maxSizeFlake->value());

    m_actionCollection->writeSettings();
    m_ui->editor->save();   // undo() will restore to this state from now on

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "snow" );
    }

void SnowEffectConfig::defaults()
    {
    kDebug() ;
    m_ui->numberFlakes->setValue( 50 );
    m_ui->minSizeFlake->setValue( 10 );
    m_ui->maxSizeFlake->setValue( 50 );
    emit changed(true);
    }


} // namespace

#include "snow_config.moc"
