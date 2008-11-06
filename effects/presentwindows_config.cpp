/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include "presentwindows_config.h"
#include <kwineffects.h>

#include <kconfiggroup.h>
#include <KActionCollection>
#include <kaction.h>

#include <QVBoxLayout>
#include <QColor>
#ifndef KDE_USE_FINAL
KWIN_EFFECT_CONFIG_FACTORY
#endif

namespace KWin
{

PresentWindowsEffectConfigForm::PresentWindowsEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

PresentWindowsEffectConfig::PresentWindowsEffectConfig(QWidget* parent, const QVariantList& args)
    :   KCModule( EffectFactory::componentData(), parent, args )
    {
    m_ui = new PresentWindowsEffectConfigForm( this );

    QVBoxLayout* layout = new QVBoxLayout( this );

    layout->addWidget( m_ui );

    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup( "PresentWindows" );
    m_actionCollection->setConfigGlobal( true );

    KAction* a = (KAction*) m_actionCollection->addAction( "ExposeAll" );
    a->setText( i18n( "Toggle Present Windows (All desktops)" ));
    a->setProperty( "isConfigurationAction", true );
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F10 ));

    KAction* b = (KAction*) m_actionCollection->addAction( "Expose" );
    b->setText( i18n( "Toggle Present Windows (Current desktop)" ));
    b->setProperty( "isConfigurationAction", true );
    b->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F9 ));

    m_ui->shortcutEditor->addCollection( m_actionCollection );

    connect( m_ui->layoutCombo, SIGNAL( currentIndexChanged( int )), this, SLOT( changed() ));
    connect( m_ui->rearrangeDurationSpin, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->displayTitleBox, SIGNAL( stateChanged( int )), this, SLOT( changed() ));
    connect( m_ui->displayIconBox, SIGNAL( stateChanged( int )), this, SLOT( changed() ));
    connect( m_ui->switchingBox, SIGNAL( stateChanged( int )), this, SLOT( changed() ));
    connect( m_ui->ignoreMinimizedBox, SIGNAL( stateChanged( int )), this, SLOT( changed() ));
    connect( m_ui->accuracySlider, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->fillGapsBox, SIGNAL( stateChanged( int )), this, SLOT( changed() ));
    connect( m_ui->shortcutEditor, SIGNAL( keyChange() ), this, SLOT( changed() ));

    load();
    }

PresentWindowsEffectConfig::~PresentWindowsEffectConfig()
    {
    // If save() is called undoChanges() has no effect
    m_ui->shortcutEditor->undoChanges();
    }

void PresentWindowsEffectConfig::load()
    {
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig( "PresentWindows" );
    
    int layoutMode = conf.readEntry( "LayoutMode", int( PresentWindowsEffect::LayoutNatural ));
    m_ui->layoutCombo->setCurrentIndex( layoutMode );

    m_ui->rearrangeDurationSpin->setValue( conf.readEntry( "RearrangeDuration", 0 ));

    bool displayTitle = conf.readEntry( "DrawWindowCaptions", true );
    m_ui->displayTitleBox->setChecked( displayTitle );

    bool displayIcon = conf.readEntry( "DrawWindowIcons", true );
    m_ui->displayIconBox->setChecked( displayIcon );

    bool switching = conf.readEntry( "TabBox", false );
    m_ui->switchingBox->setChecked( switching );

    bool ignoreMinimized = conf.readEntry( "IgnoreMinimized", false );
    m_ui->ignoreMinimizedBox->setChecked( ignoreMinimized );

    int accuracy = conf.readEntry( "Accuracy", 1 );
    m_ui->accuracySlider->setSliderPosition( accuracy );

    bool fillGaps = conf.readEntry( "FillGaps", true );
    m_ui->fillGapsBox->setChecked( fillGaps );

    emit changed(false);
    }

void PresentWindowsEffectConfig::save()
    {
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig( "PresentWindows" );

    int layoutMode = m_ui->layoutCombo->currentIndex();
    conf.writeEntry( "LayoutMode", layoutMode );

    conf.writeEntry( "RearrangeDuration", m_ui->rearrangeDurationSpin->value() );

    conf.writeEntry( "DrawWindowCaptions", m_ui->displayTitleBox->isChecked() );
    conf.writeEntry( "DrawWindowIcons", m_ui->displayIconBox->isChecked() );
    conf.writeEntry( "TabBox", m_ui->switchingBox->isChecked() );
    conf.writeEntry( "IgnoreMinimized", m_ui->ignoreMinimizedBox->isChecked() );

    int accuracy = m_ui->accuracySlider->value();
    conf.writeEntry( "Accuracy", accuracy );

    conf.writeEntry( "FillGaps", m_ui->fillGapsBox->isChecked() );

    m_ui->shortcutEditor->save();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "presentwindows" );
    }

void PresentWindowsEffectConfig::defaults()
    {
    m_ui->layoutCombo->setCurrentIndex( int( PresentWindowsEffect::LayoutNatural ));
    m_ui->rearrangeDurationSpin->setValue( 0 );
    m_ui->displayTitleBox->setChecked( true );
    m_ui->displayIconBox->setChecked( true );
    m_ui->switchingBox->setChecked( false );
    m_ui->ignoreMinimizedBox->setChecked( false );
    m_ui->accuracySlider->setSliderPosition( 1 );
    m_ui->fillGapsBox->setChecked( true );
    m_ui->shortcutEditor->allDefault();
    emit changed(true);
    }

} // namespace

#include "presentwindows_config.moc"
