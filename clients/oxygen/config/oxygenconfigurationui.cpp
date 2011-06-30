//////////////////////////////////////////////////////////////////////////////
// oxygenconfigurationui.cpp
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include "oxygenconfigurationui.h"
#include "../oxygenconfiguration.h"

#include <kdeversion.h>

#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QGroupBox>
#include <KLocale>
#include <KTabWidget>

namespace Oxygen
{

  //_________________________________________________________
  ConfigurationUi::ConfigurationUi( QWidget* parent ):
    QWidget( parent ),
    _expertMode( false )
    {

    ui.setupUi( this );

    // basic configuration
    ui.frameBorder->insertItems(0, QStringList()
      << Configuration::frameBorderName( Configuration::BorderNone, true )
      << Configuration::frameBorderName( Configuration::BorderNoSide, true )
      << Configuration::frameBorderName( Configuration::BorderTiny, true )
      << Configuration::frameBorderName( Configuration::BorderDefault, true )
      << Configuration::frameBorderName( Configuration::BorderLarge, true )
      << Configuration::frameBorderName( Configuration::BorderVeryLarge, true )
      << Configuration::frameBorderName( Configuration::BorderHuge, true )
      << Configuration::frameBorderName( Configuration::BorderVeryHuge, true )
      << Configuration::frameBorderName( Configuration::BorderOversized, true )
      );

    ui.titleAlignment->insertItems(0, QStringList()
      << Configuration::titleAlignmentName( Qt::AlignLeft, true )
      << Configuration::titleAlignmentName( Qt::AlignHCenter, true, false )
      << Configuration::titleAlignmentName( Qt::AlignHCenter, true, true )
      << Configuration::titleAlignmentName( Qt::AlignRight, true )
      );

    ui.buttonSize->insertItems(0, QStringList()
      << Configuration::buttonSizeName( Configuration::ButtonSmall, true )
      << Configuration::buttonSizeName( Configuration::ButtonDefault, true )
      << Configuration::buttonSizeName( Configuration::ButtonLarge, true )
      << Configuration::buttonSizeName( Configuration::ButtonVeryLarge, true )
      << Configuration::buttonSizeName( Configuration::ButtonHuge, true )
      );

    // advanced configuration
    ui.blendColor->insertItems(0, QStringList()
      << Configuration::blendColorName( Configuration::NoBlending, true )
      << Configuration::blendColorName( Configuration::RadialBlending, true )
      << Configuration::blendColorName( Configuration::BlendFromStyle, true )
      );

    // draw size grip
    ui.sizeGripMode->insertItems(0, QStringList()
      << Configuration::sizeGripModeName( Configuration::SizeGripNever, true )
      << Configuration::sizeGripModeName( Configuration::SizeGripWhenNeeded, true )
      );

    ui.shadowCacheMode->insertItems(0, QStringList()
        << Configuration::shadowCacheModeName( Configuration::CacheDisabled, true )
        << Configuration::shadowCacheModeName( Configuration::CacheVariable, true )
        << Configuration::shadowCacheModeName( Configuration::CacheMaximum, true ) );

    shadowConfigurations.push_back( ui.activeShadowConfiguration );
    shadowConfigurations.push_back( ui.inactiveShadowConfiguration );

    // connections
    connect( ui.shadowCacheMode, SIGNAL( currentIndexChanged(int)), SIGNAL(changed()) );
    connect( ui.titleOutline, SIGNAL(toggled( bool )), ui.separatorMode, SLOT( setDisabled( bool ) ) );

    connect( shadowConfigurations[0], SIGNAL( changed() ), SIGNAL( changed() ) );
    connect( shadowConfigurations[0], SIGNAL( toggled( bool ) ), SIGNAL( changed() ) );

    connect( shadowConfigurations[1], SIGNAL( changed() ), SIGNAL( changed() ) );
    connect( shadowConfigurations[1], SIGNAL( toggled( bool ) ), SIGNAL( changed() ) );

    connect( ui.titleAlignment, SIGNAL(currentIndexChanged(int)), SIGNAL( changed() ) );
    connect( ui.buttonSize, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( ui.frameBorder, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( ui.blendColor, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( ui.sizeGripMode, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );

    connect( ui.tabsEnabled, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui.useAnimations, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui.animateTitleChange, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui.narrowButtonSpacing, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui.separatorMode, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( ui.titleOutline, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui.exceptions, SIGNAL(changed()), SIGNAL(changed()) );

    connect( ui.useAnimations, SIGNAL( toggled( bool ) ), ui.animateTitleChange, SLOT( setEnabled( bool ) ) );
    connect( ui._expertModeButton, SIGNAL( pressed( void ) ), SLOT( toggleExpertModeInternal( void ) ) );

    ui._expertModeButton->setIcon( KIcon("configure") );

    toggleExpertModeInternal( false );

  }

  //_________________________________________________________
  void ConfigurationUi::toggleExpertMode( bool value )
  {
    ui._expertModeContainer->hide();
    toggleExpertModeInternal( value );
  }

  //_________________________________________________________
  void ConfigurationUi::toggleExpertModeInternal( bool value )
  {

    // store value
    _expertMode = value;

    // update button text
    ui._expertModeButton->setText( _expertMode ? i18n( "Hide Advanced Configuration Options" ):i18n( "Show Advanced Configuration Options" ) );

    ui.tabsEnabled->setVisible( _expertMode );
    ui.animateTitleChange->setVisible( _expertMode );
    ui.narrowButtonSpacing->setVisible( _expertMode );

    // size grip mode
    ui.sizeGripModeLabel->setVisible( _expertMode );
    ui.sizeGripMode->setVisible( _expertMode );

    // shadow mode
    ui.shadowsExpertWidget->setVisible( _expertMode );

    if( _expertMode ) ui.shadowSpacer->changeSize(0,0, QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    else ui.shadowSpacer->changeSize(0,0, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

  }

}
