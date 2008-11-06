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

#include "desktopgrid_config.h"
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

DesktopGridEffectConfigForm::DesktopGridEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

DesktopGridEffectConfig::DesktopGridEffectConfig(QWidget* parent, const QVariantList& args)
    :   KCModule( EffectFactory::componentData(), parent, args )
    {
    m_ui = new DesktopGridEffectConfigForm( this );

    QVBoxLayout* layout = new QVBoxLayout( this );

    layout->addWidget( m_ui );

    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup( "DesktopGrid" );
    m_actionCollection->setConfigGlobal( true );

    KAction* a = (KAction*) m_actionCollection->addAction( "ShowDesktopGrid" );
    a->setText( i18n( "Show Desktop Grid" ));
    a->setProperty( "isConfigurationAction", true );
    a->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F8 ));

    m_ui->shortcutEditor->addCollection( m_actionCollection );

    m_alignmentItems.append( Qt::Alignment( 0 ));
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Disabled" ));
    m_alignmentItems.append( Qt::AlignHCenter | Qt::AlignTop );
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Top" ));
    m_alignmentItems.append( Qt::AlignRight | Qt::AlignTop );
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Top-right" ));
    m_alignmentItems.append( Qt::AlignRight | Qt::AlignVCenter );
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Right" ));
    m_alignmentItems.append( Qt::AlignRight | Qt::AlignBottom );
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Bottom-right" ));
    m_alignmentItems.append( Qt::AlignHCenter | Qt::AlignBottom );
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Bottom" ));
    m_alignmentItems.append( Qt::AlignLeft | Qt::AlignBottom );
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Bottom-left" ));
    m_alignmentItems.append( Qt::AlignLeft | Qt::AlignVCenter );
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Left" ));
    m_alignmentItems.append( Qt::AlignLeft | Qt::AlignTop );
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Top-left" ));
    m_alignmentItems.append( Qt::AlignCenter );
    m_ui->desktopNameAlignmentCombo->addItem( i18n( "Center" ));

    connect( m_ui->zoomDurationSpin, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->borderWidthSpin, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->desktopNameAlignmentCombo, SIGNAL( currentIndexChanged( int )), this, SLOT( changed() ));
    connect( m_ui->layoutCombo, SIGNAL( currentIndexChanged( int )), this, SLOT( changed() ));
    connect( m_ui->layoutCombo, SIGNAL( currentIndexChanged( int )), this, SLOT( layoutSelectionChanged() ));
    connect( m_ui->layoutRowsSpin, SIGNAL( valueChanged( int )), this, SLOT( changed() ));
    connect( m_ui->shortcutEditor, SIGNAL( keyChange() ), this, SLOT( changed() ));

    load();
    }

DesktopGridEffectConfig::~DesktopGridEffectConfig()
    {
    // If save() is called undoChanges() has no effect
    m_ui->shortcutEditor->undoChanges();
    }

void DesktopGridEffectConfig::load()
    {
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig( "DesktopGrid" );

    m_ui->zoomDurationSpin->setValue( conf.readEntry( "ZoomDuration", 0 ));
    m_ui->borderWidthSpin->setValue( conf.readEntry( "BorderWidth", 10 ));

    Qt::Alignment alignment = Qt::Alignment( conf.readEntry( "DesktopNameAlignment", 0 ));
    m_ui->desktopNameAlignmentCombo->setCurrentIndex( m_alignmentItems.indexOf( alignment ));

    int layoutMode = conf.readEntry( "LayoutMode", int( DesktopGridEffect::LayoutPager ));
    m_ui->layoutCombo->setCurrentIndex( layoutMode );
    layoutSelectionChanged();

    m_ui->layoutRowsSpin->setValue( conf.readEntry( "CustomLayoutRows", 2 ));

    emit changed(false);
    }

void DesktopGridEffectConfig::save()
    {
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig( "DesktopGrid" );

    conf.writeEntry( "ZoomDuration", m_ui->zoomDurationSpin->value() );
    conf.writeEntry( "BorderWidth", m_ui->borderWidthSpin->value() );

    int alignment = m_ui->desktopNameAlignmentCombo->currentIndex();
    alignment = int( m_alignmentItems[alignment] );
    conf.writeEntry( "DesktopNameAlignment", alignment );

    int layoutMode = m_ui->layoutCombo->currentIndex();
    conf.writeEntry( "LayoutMode", layoutMode );

    conf.writeEntry( "CustomLayoutRows", m_ui->layoutRowsSpin->value() );

    m_ui->shortcutEditor->save();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "desktopgrid" );
    }

void DesktopGridEffectConfig::defaults()
    {
    m_ui->zoomDurationSpin->setValue( 0 );
    m_ui->borderWidthSpin->setValue( 10 );
    m_ui->desktopNameAlignmentCombo->setCurrentIndex( 0 );
    m_ui->layoutCombo->setCurrentIndex( int( DesktopGridEffect::LayoutPager ));
    m_ui->layoutRowsSpin->setValue( 2 );
    m_ui->shortcutEditor->allDefault();
    emit changed(true);
    }

void DesktopGridEffectConfig::layoutSelectionChanged()
    {
    if( m_ui->layoutCombo->currentIndex() == DesktopGridEffect::LayoutCustom )
        {
        m_ui->layoutRowsLabel->setEnabled( true );
        m_ui->layoutRowsSpin->setEnabled( true );
        }
    else
        {
        m_ui->layoutRowsLabel->setEnabled( false );
        m_ui->layoutRowsSpin->setEnabled( false );
        }
    }

} // namespace

#include "desktopgrid_config.moc"
