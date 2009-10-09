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
    QWidget( parent ),
    titleAlignment(0),
    buttonSize(0),
    frameBorder(0),
    blendColor(0),
    sizeGripMode(0),
    drawSeparator(0),
    titleOutline(0)
  { setupUI(); }

  //_________________________________________________________
  void OxygenConfigurationUI::setupUI( void )
  {

    QVBoxLayout* mainLayout = new QVBoxLayout( this );
    mainLayout->setMargin(0);

    // tab widget for basic and advanced mode
    KTabWidget* tab( new KTabWidget( this ) );
    mainLayout->addWidget( tab );

    // basic configuration
    {
      QWidget *basicWidget = new QWidget();
      int index = tab->addTab( basicWidget, i18n("&General") );
      tab->setTabToolTip( index, i18n( "Basic window decoration configuration options" ) );

      QVBoxLayout* vboxLayout = new QVBoxLayout();
      basicWidget->setLayout( vboxLayout );

      QGridLayout* gridLayout = new QGridLayout();
      gridLayout->setMargin(0);
      vboxLayout->addLayout( gridLayout );

      gridLayout->setColumnStretch(2, 1);

      // frame border
      QLabel* label;
      gridLayout->addWidget( label = new QLabel( i18n("Border size:"), basicWidget ), 0, 0, 1, 1);
      gridLayout->addWidget( frameBorder = new KComboBox(basicWidget), 0, 1, 1, 1);
      frameBorder->setObjectName(QString::fromUtf8("frameBorder"));
      frameBorder->insertItems(0, QStringList()
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

      label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
      label->setBuddy( frameBorder );

      // title alignment
      gridLayout->addWidget( label = new QLabel( i18n("Title alignment:"), basicWidget ), 1, 0, 1, 1 );
      gridLayout->addWidget( titleAlignment = new KComboBox(basicWidget), 1, 1, 1, 1 );
      titleAlignment->setObjectName(QString::fromUtf8("titleAlignment"));
      titleAlignment->insertItems(0, QStringList()
        << OxygenConfiguration::titleAlignmentName( Qt::AlignLeft, true )
        << OxygenConfiguration::titleAlignmentName( Qt::AlignHCenter, true )
        << OxygenConfiguration::titleAlignmentName( Qt::AlignRight, true )
        );

      label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
      label->setBuddy( titleAlignment );

      // button size
      gridLayout->addWidget( label = new QLabel( i18n("Button size:"), basicWidget ), 2, 0, 1, 1 );
      gridLayout->addWidget( buttonSize = new KComboBox(basicWidget), 2, 1, 1, 1 );
      buttonSize->setObjectName(QString::fromUtf8("buttonSize"));
      buttonSize->insertItems(0, QStringList()
        << OxygenConfiguration::buttonSizeName( OxygenConfiguration::ButtonSmall, true )
        << OxygenConfiguration::buttonSizeName( OxygenConfiguration::ButtonDefault, true )
        << OxygenConfiguration::buttonSizeName( OxygenConfiguration::ButtonLarge, true )
        << OxygenConfiguration::buttonSizeName( OxygenConfiguration::ButtonHuge, true )
        );

      label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
      label->setBuddy( buttonSize );
      vboxLayout->addStretch(1);

    }


    // advanced configuration
    {
      QWidget *advancedWidget = new QWidget();
      int index = tab->addTab( advancedWidget, i18n( "&Fine Tuning" ) );
      tab->setTabToolTip( index, i18n( "Additional window decoration configuration options" ) );

      QVBoxLayout* vboxLayout = new QVBoxLayout();
      advancedWidget->setLayout( vboxLayout );

      QGridLayout* gridLayout = new QGridLayout();
      gridLayout->setMargin(0);
      vboxLayout->addLayout( gridLayout );

      gridLayout->setColumnStretch(2, 1);

      // title bar blending
      QLabel* label;
      gridLayout->addWidget( label = new QLabel( i18n("Background style:" ), advancedWidget ), 1, 0, 1, 1 );
      gridLayout->addWidget( blendColor = new KComboBox(advancedWidget), 1, 1, 1, 1 );
      blendColor->setObjectName(QString::fromUtf8("blendColor"));
      blendColor->insertItems(0, QStringList()
        << OxygenConfiguration::blendColorName( OxygenConfiguration::NoBlending, true )
        << OxygenConfiguration::blendColorName( OxygenConfiguration::RadialBlending, true )
        );

      label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
      label->setBuddy( blendColor );

      // draw size grip
      gridLayout->addWidget( label = new QLabel( i18n("Extra size grip display:"), advancedWidget ), 2, 0, 1, 1 );
      gridLayout->addWidget( sizeGripMode = new KComboBox(advancedWidget), 2, 1, 1, 1 );
      sizeGripMode->setObjectName(QString::fromUtf8("sizeGripMode"));
      sizeGripMode->setWhatsThis(i18n(
        "The extra size grip is a small triangle, shown in the bottom-right corner of a window,\n"
        "which allows the window to be resized. This option controls in which cases the size grip \n"
        "is shown."));
      sizeGripMode->insertItems(0, QStringList()
        << OxygenConfiguration::sizeGripModeName( OxygenConfiguration::SizeGripNever, true )
        << OxygenConfiguration::sizeGripModeName( OxygenConfiguration::SizeGripWhenNeeded, true )
        );

      label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
      label->setBuddy( sizeGripMode );

      // active window title outline
      vboxLayout->addWidget( titleOutline = new QCheckBox( i18n("Outline active window title"), advancedWidget) );
      titleOutline->setObjectName(QString::fromUtf8("titleOutline"));
      titleOutline->setWhatsThis(i18n(
        "When enabled, an additional frame is shown around the active window as well as its title."));

      // draw separator
      vboxLayout->addWidget( drawSeparator = new QCheckBox( i18n("Draw separator between title bar and active window contents"), advancedWidget ) );
      drawSeparator->setObjectName(QString::fromUtf8("drawSeparator"));
      drawSeparator->setWhatsThis(i18n(
        "When enabled, this option makes an horizontal separator appear between the window title bar and the window contents."));

      vboxLayout->addStretch(1);

    }

    // shadow configurations
    {
      shadowConfigurations = QVector<OxygenShadowConfigurationUI*>( 2, 0 );
      QWidget* widget = new QWidget();
      int index = tab->addTab( widget, i18n( "&Shadows" ) );
      tab->setTabToolTip( index, i18n( "Configure shadow colors for active and inactive windows" ) );

      QGridLayout* gridLayout( new QGridLayout() );
      widget->setLayout( gridLayout );

      // oxygen shadow
      gridLayout->addWidget( useOxygenShadows = new QCheckBox( i18n("Glow active window" ), this ), 0, 0, 1, 1 );
      useOxygenShadows->setObjectName(QString::fromUtf8("useOxygenShadows"));
      useOxygenShadows->setWhatsThis(i18n(
        "When this option is enabled, the oxygen signature blue glow is used for the active window shadow."));

      // regular shadow configuration
      gridLayout->addWidget( shadowConfigurations[1] = new OxygenShadowConfigurationUI( i18n( "Window Drop-Down Shadow" ), widget ), 1, 0, 1, 1 );
      shadowConfigurations[1]->setObjectName( "inactiveShadowConfiguration" );

      // active window glow
      gridLayout->addWidget( shadowConfigurations[0] = new OxygenShadowConfigurationUI( i18n( "Active Window Glow" ), widget ), 1, 1, 1, 1 );
      shadowConfigurations[0]->setObjectName( "activeShadowConfiguration" );
      shadowConfigurations[0]->setEnabled( false );

    }

    // exceptions
    {
      exceptions = new OxygenExceptionListWidget();
      exceptions->setObjectName(QString::fromUtf8("exceptions"));
      int index = tab->addTab( exceptions, i18n( "&Window-Specific Overrides" ) );
      tab->setTabToolTip( index, i18n( "Configure window decoration option overrides for specific windows" ) );
    }

    // connections
    QMetaObject::connectSlotsByName(this);

    connect( titleOutline, SIGNAL(toggled( bool )), drawSeparator, SLOT( setDisabled( bool ) ) );
    connect( useOxygenShadows, SIGNAL(toggled( bool )), shadowConfigurations[0], SLOT( setEnabled( bool ) ) );

    connect( shadowConfigurations[0], SIGNAL( changed() ), SIGNAL( changed() ) );
    connect( shadowConfigurations[1], SIGNAL( changed() ), SIGNAL( changed() ) );
    connect( titleAlignment, SIGNAL(currentIndexChanged(int)), SIGNAL( changed() ) );
    connect( buttonSize, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( frameBorder, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( blendColor, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );
    connect( sizeGripMode, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()) );

    connect( drawSeparator, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( titleOutline, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( useOxygenShadows, SIGNAL(clicked()), SIGNAL(changed()) );
    connect( exceptions, SIGNAL(changed()), SIGNAL(changed()) );

  }

}
