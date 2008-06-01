/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include "magnifier_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kconfiggroup.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>
#include <KGlobalAccel>

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>

KWIN_EFFECT_CONFIG_FACTORY

namespace KWin
{

MagnifierEffectConfigForm::MagnifierEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

MagnifierEffectConfig::MagnifierEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    m_ui = new MagnifierEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));
    connect(m_ui->spinWidth, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    // Shortcut config
    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup("Magnifier");
    m_actionCollection->setConfigGlobal(true);

    KAction* a;
    a = static_cast< KAction* >( m_actionCollection->addAction( KStandardAction::ZoomIn));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Plus));
    a->setProperty("isConfigurationAction", true);

    a = static_cast< KAction* >( m_actionCollection->addAction( KStandardAction::ZoomOut));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));
    a->setProperty("isConfigurationAction", true);

    a = static_cast< KAction* >( m_actionCollection->addAction( KStandardAction::ActualSize));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));
    a->setProperty("isConfigurationAction", true);

    m_ui->editor->addCollection(m_actionCollection);
    }

MagnifierEffectConfig::~MagnifierEffectConfig()
    {
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
    }

void MagnifierEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Magnifier");

    int width = conf.readEntry("Width", 200);
    int height = conf.readEntry("Height", 200);
    m_ui->spinWidth->setValue(width);
    m_ui->spinHeight->setValue(height);


    emit changed(false);
    }

void MagnifierEffectConfig::save()
    {
    kDebug() << "Saving config of Magnifier" ;
    //KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("Magnifier");

    conf.writeEntry("Width", m_ui->spinWidth->value());
    conf.writeEntry("Height", m_ui->spinHeight->value());

    m_ui->editor->save();   // undo() will restore to this state from now on

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "magnifier" );
    }

void MagnifierEffectConfig::defaults()
    {
    kDebug() ;
    m_ui->spinWidth->setValue(200);
    m_ui->spinHeight->setValue(200);
    emit changed(true);
    }


} // namespace

#include "magnifier_config.moc"
