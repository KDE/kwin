/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
// own
#include "layoutconfig.h"
#include "ui_layoutconfig.h"
#include "previewhandlerimpl.h"
// tabbox
#include "tabboxconfig.h"
#include "tabboxhandler.h"
#include "tabboxview.h"
// Qt
#include <QVBoxLayout>

namespace KWin
{
namespace TabBox
{

/***************************************************
* LayoutConfigPrivate
***************************************************/
class LayoutConfigPrivate
    {
    public:
        LayoutConfigPrivate();
        ~LayoutConfigPrivate() { delete previewTabbox; }

    TabBoxConfig config;
    Ui::LayoutConfigForm ui;
    PreviewHandlerImpl* previewTabbox;
    };

LayoutConfigPrivate::LayoutConfigPrivate()
    : previewTabbox( new PreviewHandlerImpl() )
    {
    previewTabbox->setConfig( config );
    }

/***************************************************
* LayoutConfig
***************************************************/
LayoutConfig::LayoutConfig(QWidget* parent)
    : QWidget(parent)
    {
    d = new LayoutConfigPrivate;
    d->ui.setupUi( this );
    QVBoxLayout* layout = new QVBoxLayout( this );
    QWidget* tabBoxView = tabBox->tabBoxView();
    tabBoxView->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
    layout->addWidget( tabBoxView );
    d->ui.previewWidget->setLayout( layout );
    tabBox->createModel();
    tabBox->setCurrentIndex( tabBox->first() );

    // init the item layout combo box
    d->ui.itemLayoutCombo->addItem( i18n("Informative") );
    d->ui.itemLayoutCombo->addItem( i18n("Compact") );
    d->ui.itemLayoutCombo->addItem( i18n("Small Icons") );
    d->ui.itemLayoutCombo->addItem( i18n("Large Icons") );
    d->ui.itemLayoutCombo->addItem( i18n("Text Only") );
    // TODO: user defined layouts

    // init the selected item layout combo box
    d->ui.selectedItemLayoutCombo->addItem( i18n("Informative") );
    d->ui.selectedItemLayoutCombo->addItem( i18n("Compact") );
    d->ui.selectedItemLayoutCombo->addItem( i18n("Small Icons") );
    d->ui.selectedItemLayoutCombo->addItem( i18n("Large Icons") );
    d->ui.selectedItemLayoutCombo->addItem( i18n("Text Only") );

    connect( d->ui.minWidthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(changed()) );
    connect( d->ui.minHeightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(changed()) );
    connect( d->ui.itemLayoutCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()) );
    connect( d->ui.layoutCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()) );
    connect( d->ui.selectedItemCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()) );
    connect( d->ui.selectedItemLayoutCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()) );
    connect( d->ui.selectedItemBox, SIGNAL(clicked(bool)), this, SLOT(changed()) );

    }

LayoutConfig::~LayoutConfig()
    {
    delete d;
    }

TabBoxConfig& LayoutConfig::config() const
    {
    return d->config;
    }

void LayoutConfig::setConfig( const KWin::TabBox::TabBoxConfig& config )
    {
    d->config = config;
    tabBox->setConfig( config );
    d->ui.selectedItemBox->setChecked( config.selectedItemViewPosition() != TabBoxConfig::NonePosition );

    d->ui.layoutCombo->setCurrentIndex( config.layout() );
    d->ui.selectedItemCombo->setCurrentIndex( config.selectedItemViewPosition() - 1 );

    d->ui.minWidthSpinBox->setValue( config.minWidth() );
    d->ui.minHeightSpinBox->setValue( config.minHeight() );

    // item layouts
    if( config.layoutName().compare( "Default", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.itemLayoutCombo->setCurrentIndex( 0 );
        }
    else if( config.layoutName().compare( "Compact", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.itemLayoutCombo->setCurrentIndex( 1 );
        }
    else if( config.layoutName().compare( "Small Icons", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.itemLayoutCombo->setCurrentIndex( 2 );
        }
    else if( config.layoutName().compare( "Big Icons", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.itemLayoutCombo->setCurrentIndex( 3 );
        }
    else if( config.layoutName().compare( "Text", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.itemLayoutCombo->setCurrentIndex( 4 );
        }
    else
        {
        // TODO: user defined layouts
        }

    if( config.selectedItemLayoutName().compare( "Default", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.selectedItemLayoutCombo->setCurrentIndex( 0 );
        }
    else if( config.selectedItemLayoutName().compare( "Compact", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.selectedItemLayoutCombo->setCurrentIndex( 1 );
        }
    else if( config.selectedItemLayoutName().compare( "Small Icons", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.selectedItemLayoutCombo->setCurrentIndex( 2 );
        }
    else if( config.selectedItemLayoutName().compare( "Big Icons", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.selectedItemLayoutCombo->setCurrentIndex( 3 );
        }
    else if( config.selectedItemLayoutName().compare( "Text", Qt::CaseInsensitive ) == 0 )
        {
        d->ui.selectedItemLayoutCombo->setCurrentIndex( 4 );
        }
    else
        {
        // TODO: user defined layouts
        }

    }

void LayoutConfig::changed()
    {
    // it's actually overkill but we just sync all options
    d->config.setMinWidth( d->ui.minWidthSpinBox->value() );
    d->config.setMinHeight( d->ui.minHeightSpinBox->value() );
    d->config.setLayout( TabBoxConfig::LayoutMode( d->ui.layoutCombo->currentIndex() ) );

    QString layout;
    switch( d->ui.itemLayoutCombo->currentIndex() )
        {
        case 0:
            layout = "Default";
            break;
        case 1:
            layout = "Compact";
            break;
        case 2:
            layout = "Small Icons";
            break;
        case 3:
            layout = "Big Icons";
            break;
        case 4:
            layout = "Text";
            break;
        default:
            // TODO: user defined layouts
            break;
        }
    d->config.setLayoutName( layout );

    if( d->ui.selectedItemBox->isChecked() )
        {
        d->config.setSelectedItemViewPosition( TabBoxConfig::SelectedItemViewPosition(
                                                d->ui.selectedItemCombo->currentIndex() + 1) );
        QString selectedLayout;
        switch( d->ui.selectedItemLayoutCombo->currentIndex() )
            {
            case 0:
                selectedLayout = "Default";
                break;
            case 1:
                selectedLayout = "Compact";
                break;
            case 2:
                selectedLayout = "Small Icons";
                break;
            case 3:
                selectedLayout = "Big Icons";
                break;
            case 4:
                selectedLayout = "Text";
                break;
            default:
                // TODO: user defined layouts
                break;
            }
        d->config.setSelectedItemLayoutName( selectedLayout );
        }
    else
        {
        d->config.setSelectedItemViewPosition( TabBoxConfig::NonePosition );
        }
    // update the preview
    tabBox->setConfig( d->config );
    d->ui.previewWidget->layout()->invalidate();
    d->ui.previewWidget->update();
    }

} // namespace KWin
} // namespace TabBox

