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
  OxygenConfigurationUI::OxygenConfigurationUI( QWidget* parent ):
    QWidget( parent )
    {

    ui.setupUi( this );

    // basic configuration
    ui.frameBorder->insertItems(0, QStringList()
      << OxygenConfiguration::frameBorderName( OxygenConfiguration::BorderNone, true )
      << OxygenConfiguration::frameBorderName( OxygenConfiguration::BorderNoSide, true )
      << OxygenConfiguration::frameBorderName( OxygenConfiguration::BorderTiny, true )
      << OxygenConfiguration::frameBorderName( OxygenConfiguration::BorderDefault, true )
      << OxygenConfiguration::frameBorderName( OxygenConfiguration::BorderLarge, true )
      << OxygenConfiguration::frameBorderName( OxygenConfiguration::BorderVeryLarge, true )
      << OxygenConfiguration::frameBorderName( OxygenConfiguration::BorderHuge, true )
      << OxygenConfiguration::frameBorderName( OxygenConfiguration::BorderVeryHuge, true )
      << OxygenConfiguration::frameBorderName( OxygenConfiguration::BorderOversized, true )
      );

    ui.label->setBuddy( ui.frameBorder );

    ui.titleAlignment->insertItems(0, QStringList()
      << OxygenConfiguration::titleAlignmentName( Qt::AlignLeft, true )
      << OxygenConfiguration::titleAlignmentName( Qt::AlignHCenter, true )
      << OxygenConfiguration::titleAlignmentName( Qt::AlignRight, true )
      );

    ui.label_2->setBuddy( ui.titleAlignment );

    ui.buttonSize->insertItems(0, QStringList()
      << OxygenConfiguration::buttonSizeName( OxygenConfiguration::ButtonSmall, true )
      << OxygenConfiguration::buttonSizeName( OxygenConfiguration::ButtonDefault, true )
      << OxygenConfiguration::buttonSizeName( OxygenConfiguration::ButtonLarge, true )
      << OxygenConfiguration::buttonSizeName( OxygenConfiguration::ButtonHuge, true )
      );

    ui.label_3->setBuddy( ui.buttonSize );

    // advanced configuration
    ui.blendColor->insertItems(0, QStringList()
      << OxygenConfiguration::blendColorName( OxygenConfiguration::NoBlending, true )
      << OxygenConfiguration::blendColorName( OxygenConfiguration::RadialBlending, true )
      );

    ui.label_4->setBuddy( ui.blendColor );

    // draw size grip
    ui.sizeGripMode->insertItems(0, QStringList()
      << OxygenConfiguration::sizeGripModeName( OxygenConfiguration::SizeGripNever, true )
      << OxygenConfiguration::sizeGripModeName( OxygenConfiguration::SizeGripWhenNeeded, true )
      );

    ui.label_5->setBuddy( ui.sizeGripMode );

    // shadows
    shadowConfigurations.push_back( ui.activeShadowConfiguration );
    shadowConfigurations.push_back( ui.inactiveShadowConfiguration );

    // connections
    connect( ui.titleOutline, SIGNAL(toggled( bool )), ui.drawSeparator, SLOT( setDisabled( bool ) ) );

    connect( shadowConfigurations[0], SIGNAL( changed() ), SIGNAL( changed() ) );
    connect( shadowConfigurations[1], SIGNAL( changed() ), SIGNAL( changed() ) );
    connect( ui.titleAlignment, SIGNAL(currentIndexChanged(int)), SIGNAL( changed() ) );
    connect( ui.buttonSize, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( ui.frameBorder, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( ui.blendColor, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( ui.sizeGripMode, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );

    connect( ui.tabsEnabled, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui.drawSeparator, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui.titleOutline, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( ui.exceptions, SIGNAL(changed()), SIGNAL(changed()) );

  }

}
