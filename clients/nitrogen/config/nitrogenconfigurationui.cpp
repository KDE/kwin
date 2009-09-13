//////////////////////////////////////////////////////////////////////////////
// nitrogenconfigurationui.cpp
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

#include <kdeversion.h>

#include <QLabel>
#include <QLayout>
#include <QGroupBox>
#include <KLocale>

#include "../nitrogenconfiguration.h"
#include "nitrogenconfigurationui.h"
#include "nitrogenconfigurationui.moc"

namespace Nitrogen
{

  //_________________________________________________________
  NitrogenConfigurationUI::NitrogenConfigurationUI( QWidget* parent ):
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
  void NitrogenConfigurationUI::setupUI( void )
  {

    QVBoxLayout* mainLayout = new QVBoxLayout( this );
    mainLayout->setMargin(0);

    // tab widget for basic and advanced mode
    QTabWidget* tab( new QTabWidget( this ) );
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
      gridLayout->addWidget( frameBorder = new QComboBox(basicWidget), 0, 1, 1, 1);
      frameBorder->setObjectName(QString::fromUtf8("frameBorder"));
      frameBorder->insertItems(0, QStringList()
        << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderNone, true )
        << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderNoSide, true )
        << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderTiny, true )
        << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderDefault, true )
        << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderLarge, true )
        << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderVeryLarge, true )
        << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderHuge, true )
        << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderVeryHuge, true )
        << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderOversized, true )
        );

      label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
      label->setBuddy( frameBorder );

      // title alignment
      gridLayout->addWidget( label = new QLabel( i18n("Title alignment:"), basicWidget ), 1, 0, 1, 1 );
      gridLayout->addWidget( titleAlignment = new QComboBox(basicWidget), 1, 1, 1, 1 );
      titleAlignment->setObjectName(QString::fromUtf8("titleAlignment"));
      titleAlignment->insertItems(0, QStringList()
        << NitrogenConfiguration::titleAlignmentName( Qt::AlignLeft, true )
        << NitrogenConfiguration::titleAlignmentName( Qt::AlignHCenter, true )
        << NitrogenConfiguration::titleAlignmentName( Qt::AlignRight, true )
        );

      label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
      label->setBuddy( titleAlignment );

      // button size
      gridLayout->addWidget( label = new QLabel( i18n("Button size:"), basicWidget ), 2, 0, 1, 1 );
      gridLayout->addWidget( buttonSize = new QComboBox(basicWidget), 2, 1, 1, 1 );
      buttonSize->setObjectName(QString::fromUtf8("buttonSize"));
      buttonSize->insertItems(0, QStringList()
        << NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::ButtonSmall, true )
        << NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::ButtonDefault, true )
        << NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::ButtonLarge, true )
        << NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::ButtonHuge, true )
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
      gridLayout->addWidget( blendColor = new QComboBox(advancedWidget), 1, 1, 1, 1 );
      blendColor->setObjectName(QString::fromUtf8("blendColor"));
      blendColor->insertItems(0, QStringList()
        << NitrogenConfiguration::blendColorName( NitrogenConfiguration::NoBlending, true )
        << NitrogenConfiguration::blendColorName( NitrogenConfiguration::RadialBlending, true )
        );

      label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
      label->setBuddy( blendColor );

      // draw size grip
      gridLayout->addWidget( label = new QLabel( i18n("Extra Size grip display:"), advancedWidget ), 2, 0, 1, 1 );
      gridLayout->addWidget( sizeGripMode = new QComboBox(advancedWidget), 2, 1, 1, 1 );
      sizeGripMode->setObjectName(QString::fromUtf8("sizeGripMode"));
      sizeGripMode->setWhatsThis(i18n(
        "The extra size grip is a small triangle shown in the bottom-right corner of a window \n"
        "which allows to resize the window. This option controls in which case this size grip \n"
        "is shown."));
      sizeGripMode->insertItems(0, QStringList()
        << NitrogenConfiguration::sizeGripModeName( NitrogenConfiguration::SizeGripNever, true )
        << NitrogenConfiguration::sizeGripModeName( NitrogenConfiguration::SizeGripWhenNeeded, true )
        );

      label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
      label->setBuddy( sizeGripMode );

      // active window title outline
      vboxLayout->addWidget( titleOutline = new QCheckBox( i18n("Outline active window title"), advancedWidget) );
      titleOutline->setObjectName(QString::fromUtf8("titleOutline"));
      titleOutline->setWhatsThis(i18n(
        "When enabled, an additional frame is shown around the active window as well as its title"));

      // draw separator
      vboxLayout->addWidget( drawSeparator = new QCheckBox( i18n("Draw separator between title bar and active window contents"), advancedWidget ) );
      drawSeparator->setObjectName(QString::fromUtf8("drawSeparator"));
      drawSeparator->setWhatsThis(i18n(
        "When enabled, this option makes an horizontal separator appear between the window title bar and the window contents."));

      // oxygen shadow
      vboxLayout->addWidget( useOxygenShadows = new QCheckBox( i18n("Glow active window" ), advancedWidget ) );
      useOxygenShadows->setObjectName(QString::fromUtf8("useOxygenShadows"));
      useOxygenShadows->setWhatsThis(i18n(
        "When this option is enabled, oxygen signature blue glow is used for the active window shadow."));

      connect( titleOutline, SIGNAL(toggled( bool )), drawSeparator, SLOT( setDisabled( bool ) ) );

    }

    // exceptions
    {
      exceptions = new NitrogenExceptionListWidget();
      exceptions->setObjectName(QString::fromUtf8("exceptions"));
      int index = tab->addTab( exceptions, i18n( "&Window-Specific Overrides" ) );
      tab->setTabToolTip( index, i18n( "Configure window decoraction option overrides for specific windows" ) );
    }

    QMetaObject::connectSlotsByName(this);

  }

}
