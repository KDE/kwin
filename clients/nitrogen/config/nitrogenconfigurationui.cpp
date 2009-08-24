/******************************************************************************
 *                        
 * Copyright (C) 2002 Hugo PEREIRA <mailto: hugo.pereira@free.fr>            
 *                        
 * This is free software; you can redistribute it and/or modify it under the     
 * terms of the GNU General Public License as published by the Free Software     
 * Foundation; either version 2 of the License, or (at your option) any later   
 * version.                            
 *                         
 * This software is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY;  without even the implied warranty of MERCHANTABILITY or         
 * FITNESS FOR A PARTICULAR PURPOSE.   See the GNU General Public License         
 * for more details.                    
 *                         
 * You should have received a copy of the GNU General Public License along with 
 * software; if not, write to the Free Software Foundation, Inc., 59 Temple     
 * Place, Suite 330, Boston, MA   02111-1307 USA                          
 *                        
 *                        
 *******************************************************************************/

/*!
   \file nitrogenconfigurationui.cpp
   \brief window decoration configuration user interface
   \author Hugo Pereira
   \version $Revision: 1.20 $
   \date $Date: 2009/07/05 20:45:40 $
*/

#include <kdeversion.h>

#include <QLabel>
#include <QLayout>
#include <QGroupBox>
#include <KLocale>

#include <iostream>

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
    buttonType(0),
    frameBorder(0),
    blendColor(0),
    drawSeparator(0),
    showStripes(0),
    drawSizeGrip(0)
  { setupUI(); }
    
  //_________________________________________________________
  void NitrogenConfigurationUI::setupUI( void )
  {
    
    std::cout << "NitrogenConfigurationUI::setupUI.\n" << std::endl;
    
    QVBoxLayout* mainLayout = new QVBoxLayout( this );
    mainLayout->setSpacing(6);
    mainLayout->setMargin(0);
    
    QHBoxLayout* hboxLayout = new QHBoxLayout();
    hboxLayout->setSpacing(6);
    mainLayout->addLayout(hboxLayout);
    
    // left box for comboboxes
    QGroupBox* box;
    hboxLayout->addWidget( box = new QGroupBox( i18n("Layout"), this ) );
    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setSpacing(6);
    box->setLayout( gridLayout );
    
    // title alignment
    QLabel* label;
    gridLayout->addWidget( label = new QLabel( i18n("Title alignment:"), box ), 0, 0, 1, 1 );
    gridLayout->addWidget( titleAlignment = new QComboBox(box), 0, 1, 1, 1 );
    titleAlignment->setObjectName(QString::fromUtf8("titleAlignment"));  
    titleAlignment->insertItems(0, QStringList()
      << NitrogenConfiguration::titleAlignmentName( Qt::AlignLeft )
      << NitrogenConfiguration::titleAlignmentName( Qt::AlignHCenter )
      << NitrogenConfiguration::titleAlignmentName( Qt::AlignRight )
      );
    
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    label->setBuddy( titleAlignment );
    
    // button size
    gridLayout->addWidget( label = new QLabel( i18n("Button size:"), box ), 1, 0, 1, 1 );
    gridLayout->addWidget( buttonSize = new QComboBox(box), 1, 1, 1, 1 );
    buttonSize->setObjectName(QString::fromUtf8("buttonSize"));  
    buttonSize->insertItems(0, QStringList()
      << NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::ButtonSmall )
      << NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::ButtonDefault )
      << NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::ButtonLarge )
      << NitrogenConfiguration::buttonSizeName( NitrogenConfiguration::ButtonHuge )
      );
    
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    label->setBuddy( buttonSize );

    // button type
    gridLayout->addWidget( label = new QLabel( i18n("Button style:"), box ), 2, 0, 1, 1 );
    gridLayout->addWidget( buttonType = new QComboBox(box), 2, 1, 1, 1 );
    buttonType->setObjectName(QString::fromUtf8("buttonType"));  
    buttonType->insertItems(0, QStringList()
      << NitrogenConfiguration::buttonTypeName( NitrogenConfiguration::ButtonKde42 )
      << NitrogenConfiguration::buttonTypeName( NitrogenConfiguration::ButtonKde43 )
      );
    
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    label->setBuddy( buttonType );

    // frame border
    gridLayout->addWidget( label = new QLabel( i18n("Border size:"), box ), 3, 0, 1, 1);
    gridLayout->addWidget( frameBorder = new QComboBox(box), 3, 1, 1, 1);
    frameBorder->setObjectName(QString::fromUtf8("frameBorder"));
    frameBorder->insertItems(0, QStringList()
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderNone )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderTiny )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderSmall )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderDefault )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderLarge )
      );
    
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    label->setBuddy( frameBorder );
    
    // title bar blending
    gridLayout->addWidget( label = new QLabel( i18n("Title bar blending:" ), box ), 4, 0, 1, 1 );
    gridLayout->addWidget( blendColor = new QComboBox(box), 4, 1, 1, 1 );
    blendColor->setObjectName(QString::fromUtf8("blendColor"));
    blendColor->insertItems(0, QStringList()
      << NitrogenConfiguration::blendColorName( NitrogenConfiguration::NoBlending )
      << NitrogenConfiguration::blendColorName( NitrogenConfiguration::RadialBlending )
      );
    
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    label->setBuddy( blendColor );
    
    // right is for checkboxes
    hboxLayout->addWidget( box = new QGroupBox( i18n("Flags"), this ) );
    QVBoxLayout* vboxLayout = new QVBoxLayout();
    box->setLayout( vboxLayout );    
    
    // oxygen shadow
    vboxLayout->addWidget( useOxygenShadows = new QCheckBox( i18n("Glow active window", 0 ), this ) );
    useOxygenShadows->setObjectName(QString::fromUtf8("useOxygenShadows"));
    useOxygenShadows->setWhatsThis(i18n(
      "When this option is enabled, oxygen signature blue glow is used for the active window shadow.", 0));
    
    // draw separator
    vboxLayout->addWidget( drawSeparator = new QCheckBox( i18n("Draw separator between title bar and window contents", 0), this ) );
    drawSeparator->setObjectName(QString::fromUtf8("drawSeparator"));
    drawSeparator->setWhatsThis(i18n(
      "When enabled, this option makes an horizontal separator appear between the window title bar and the window contents.", 0));
    
    // show stripes
    vboxLayout->addWidget( showStripes = new QCheckBox( i18n("Show stripes next to the title", 0), this) );
    showStripes->setObjectName(QString::fromUtf8("showStripes"));
    showStripes->setWhatsThis(i18n(
      "When enabled, this option increases the visibility of the window titlebar by showing stripes", 0));
    
    // overwrite colors
    vboxLayout->addWidget( overwriteColors = new QCheckBox( i18n("Blend title bar colors with window contents", 0), this) );
    overwriteColors->setObjectName(QString::fromUtf8("overwriteColors"));
    overwriteColors->setWhatsThis(i18n(
      "When enabled, window colors are used in place of default title bar colors to draw the decoration", 0));
    
    // draw size grip
    vboxLayout->addWidget( drawSizeGrip = new QCheckBox( i18n("Draw size grip widget in bottom-right corner of windows", 0), this ) );
    drawSizeGrip->setObjectName(QString::fromUtf8("drawSizeGrip"));
    drawSizeGrip->setWhatsThis(i18n(
      "When this option is enabled, a small triangular widget is drawn in bottom-right corner of every window \n"
      "that allow to resize the window. This the \"No Border\" border size is selected.", 0));
    
    // exceptions
    mainLayout->addLayout( hboxLayout = new QHBoxLayout() );
    hboxLayout->addStretch( 1 );
    
    hboxLayout->addWidget( showExceptions = new QPushButton( i18n("Exceptions ..." ), box ) );
    showExceptions->setToolTip(i18n("Raise a dialog to store blending type exceptions based on window title.", 0));

    // about
    // this is disabled until I find a suitable icon for nitrogen.
    // hboxLayout->addWidget( aboutNitrogen = new QPushButton( i18n("About Nitrogen" ), box ) );
    
    QMetaObject::connectSlotsByName(this);  
    
  }
  
}
