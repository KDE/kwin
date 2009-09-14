//////////////////////////////////////////////////////////////////////////////
// NitrogenShadowConfigurationUI.cpp
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

#include <KLocale>
#include <QLabel>
#include <QLayout>

#include "nitrogenshadowconfigurationui.h"

namespace Nitrogen
{

  //_________________________________________________________
  NitrogenShadowConfigurationUI::NitrogenShadowConfigurationUI( const QString& name, QWidget* parent ):
    QGroupBox( name, parent ),
    shadowSize(0),
    horizontalOffset(0),
    verticalOffset(0),
    innerColor(0),
    outerColor(0)
  { setupUI(); }

  //_________________________________________________________
  void NitrogenShadowConfigurationUI::setupUI( void )
  {

    QGridLayout* mainLayout = new QGridLayout();
    setLayout( mainLayout );

    // shadow size
    QLabel* label;
    mainLayout->addWidget( label = new QLabel( i18n( "Size:" ), this ), 0, 0, 1, 1 );
    mainLayout->addWidget( shadowSize = new KIntSpinBox( 0, 500, 1, 1, this ), 0, 1, 1, 1 );
    shadowSize->setObjectName(QString::fromUtf8("shadowSize"));
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    label->setBuddy( shadowSize );

    // horizontal offset
    // horizontal offset is foreseen in the configuration but is hidden for now
    // it might be removed in future releases
    //mainLayout->addWidget( label = new QLabel( i18n( "Horizontal Offset:" ), this ), 1, 0, 1, 1 );
    mainLayout->addWidget( horizontalOffset = new KIntSpinBox( -50, 50, 1, 1, this ), 1, 1, 1, 1 );
    horizontalOffset->setObjectName(QString::fromUtf8("horizontalOffset"));
    //label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    //label->setBuddy( horizontalOffset );
    horizontalOffset->hide();

    // vertical offset
    mainLayout->addWidget( label = new QLabel( i18n( "Vertical Offset:" ), this ), 2, 0, 1, 1 );
    mainLayout->addWidget( verticalOffset = new KIntSpinBox( -50, 50, 1, 1, this ), 2, 1, 1, 1 );
    verticalOffset->setObjectName(QString::fromUtf8("verticalOffset"));
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    label->setBuddy( verticalOffset );

    // first color
    mainLayout->addWidget( label = new QLabel( i18n( "Inner Color:" ), this ), 3, 0, 1, 1 );
    mainLayout->addWidget( innerColor = new KColorButton( Qt::white, this ), 3, 1, 1, 1 );
    innerColor->setObjectName(QString::fromUtf8( "innerColor"));
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    label->setBuddy( innerColor );

    // second color
    mainLayout->addWidget( label = new QLabel( i18n( "Outer Color:" ), this ), 4, 0, 1, 1 );
    mainLayout->addWidget( outerColor = new KColorButton( Qt::black, this ), 4, 1, 1, 1 );
    outerColor->setObjectName(QString::fromUtf8("outerColor"));
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    label->setBuddy( outerColor );

    // second color selection
    mainLayout->addWidget( useOuterColor = new QCheckBox( this ), 4, 2, 1, 1 );
    useOuterColor->setObjectName(QString::fromUtf8("useOuterColor"));
    connect( useOuterColor, SIGNAL( toggled( bool ) ), outerColor, SLOT( setEnabled( bool ) ) );
    outerColor->setEnabled( false );

    QMetaObject::connectSlotsByName(this);
    connect( shadowSize, SIGNAL( valueChanged( int ) ), SIGNAL( changed() ) );
    connect( horizontalOffset, SIGNAL( valueChanged( int ) ), SIGNAL( changed() ) );
    connect( verticalOffset, SIGNAL( valueChanged( int ) ), SIGNAL( changed() ) );
    connect( innerColor, SIGNAL( changed( const QColor& ) ), SIGNAL( changed() ) );
    connect( outerColor, SIGNAL( changed( const QColor& ) ), SIGNAL( changed() ) );
    connect( useOuterColor, SIGNAL( toggled( bool ) ), SIGNAL( changed() ) );

  }

}
