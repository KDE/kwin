//////////////////////////////////////////////////////////////////////////////
// nitrogenexceptiondialog.cpp
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

#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <KLocale>

#include "nitrogenexceptiondialog.h"

namespace Nitrogen
{
  
  //___________________________________________
  NitrogenExceptionDialog::NitrogenExceptionDialog( QWidget* parent ):
    KDialog( parent )
  {
    
    // define buttons
    setButtons( Ok|Cancel );
    showButtonSeparator( true );

    // main widget
    QWidget* widget = new QWidget( this );
    setMainWidget( widget );

    widget->setLayout( new QVBoxLayout() );
    widget->layout()->setSpacing(5);
    widget->layout()->setMargin(0);
    
    // exception definition
    QGroupBox* box;
    widget->layout()->addWidget( box = new QGroupBox( i18n( "Definition" ), widget ) );
    
    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setSpacing(5);
    gridLayout->setMargin(5);
    box->setLayout( gridLayout );
      
    QLabel *label;
    
    // exception type
    gridLayout->addWidget( label = new QLabel( i18n( "Exception type: " ), box ), 0, 0, 1, 1 );
    gridLayout->addWidget( exceptionType = new QComboBox(box), 0, 1, 1, 1 );
    exceptionType->insertItems(0, QStringList()
      << NitrogenException::typeName( NitrogenException::WindowClassName )
      << NitrogenException::typeName( NitrogenException::WindowTitle ) );
    exceptionType->setToolTip( i18n(
      "Select here the window characteristic used to \n"
      "identify windows to which the exception apply." ) );
    
    label->setAlignment( Qt::AlignRight );
    
    // regular expression
    gridLayout->addWidget( label = new QLabel( i18n( "Regular expression to match: " ), box ), 1, 0, 1, 1 );
    gridLayout->addWidget( exceptionEditor = new KLineEdit( box ), 1, 1, 1, 1 );
    exceptionEditor->setClearButtonShown( true );
    exceptionEditor->setToolTip( i18n(
      "Type here the regular expression used to \n"
      "identify windows to which the exception apply." ) );

    label->setAlignment( Qt::AlignRight );
    
    // decoration flags
    widget->layout()->addWidget( box = new QGroupBox( i18n( "Decoration" ), widget ) );    
    gridLayout = new QGridLayout();
    gridLayout->setSpacing(5);
    gridLayout->setMargin(5);
    box->setLayout( gridLayout );

    QCheckBox* checkbox;
    
    // border size
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Border size:" ), box ), 0, 0, 1, 1 );
    gridLayout->addWidget( frameBorder = new QComboBox(box), 0, 1, 1, 1 );
    frameBorder->insertItems(0, QStringList()
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderNone, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderTiny, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderDefault, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderLarge, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderVeryLarge, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderHuge, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderVeryHuge, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderOversized, true )
      );
    frameBorder->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::FrameBorder, checkbox ) );
    checkbox->setToolTip( i18n("If checked, specified frame border is used in place of default value.") );
    connect( checkbox, SIGNAL( toggled( bool ) ), frameBorder, SLOT( setEnabled( bool ) ) );
    
    // blend color
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Background style:" ), box ), 1, 0, 1, 1 );
    gridLayout->addWidget( blendColor = new QComboBox(box), 1, 1, 1, 1 );
    blendColor->insertItems(0, QStringList()
      << NitrogenException::blendColorName( NitrogenException::NoBlending, true )
      << NitrogenException::blendColorName( NitrogenException::RadialBlending, true ) );
    blendColor->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::BlendColor, checkbox ) );
    checkbox->setToolTip( i18n("If checked, specified blending color is used in title bar in place of default value.") );
    connect( checkbox, SIGNAL( toggled( bool ) ), blendColor, SLOT( setEnabled( bool ) ) );
    
    // size grip
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Size grip display:" ), box ), 2, 0, 1, 1 );
    gridLayout->addWidget( sizeGripMode = new QComboBox( box ), 2, 1, 1, 1 );
    sizeGripMode->insertItems(0, QStringList()
      << NitrogenConfiguration::sizeGripModeName( NitrogenConfiguration::SizeGripNever, true )
      << NitrogenConfiguration::sizeGripModeName( NitrogenConfiguration::SizeGripWhenNeeded, true )
      );
    sizeGripMode->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::SizeGripMode, checkbox ) );
    connect( checkbox, SIGNAL( toggled( bool ) ), sizeGripMode, SLOT( setEnabled( bool ) ) );

    // outline active window title
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Outline active window title:" ), box ), 3, 0, 1, 1 );
    gridLayout->addWidget( titleOutline = new ComboBox( box ), 3, 1, 1, 1 );
    titleOutline->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::TitleOutline, checkbox ) );
    connect( checkbox, SIGNAL( toggled( bool ) ), titleOutline, SLOT( setEnabled( bool ) ) );

    // separator
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Draw separator between title bar and window contents:" ), box ), 4, 0, 1, 1 );
    gridLayout->addWidget( drawSeparator = new ComboBox( box ), 4, 1, 1, 1 );
    drawSeparator->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::DrawSeparator, checkbox ) );
    connect( checkbox, SIGNAL( toggled( bool ) ), drawSeparator, SLOT( setEnabled( bool ) ) );
    
    // stripes
    // temporarily hide the "stripes" settings. Might be permanently removed in the future
    //gridLayout->addWidget( checkbox = new QCheckBox( i18n("Show stripes next to the title:" ), box ), 5, 0, 1, 1 );
    gridLayout->addWidget( showStripes = new ComboBox( box ), 5, 1, 1, 1 );
    showStripes->setEnabled( false );
    showStripes->hide();
    //checkboxes_.insert( std::make_pair( NitrogenException::ShowStripes, checkbox ) );
    //connect( checkbox, SIGNAL( toggled( bool ) ), showStripes, SLOT( setEnabled( bool ) ) );
       
  }
  
  //___________________________________________
  void NitrogenExceptionDialog::setException( NitrogenException exception )
  {

    // store exception internally
    exception_ = exception;
    
    // type
    exceptionType->setCurrentIndex( exceptionType->findText( exception.typeName() ) );
    
    // regular expression
    exceptionEditor->setText( exception.regExp().pattern() );

    // border size
    frameBorder->setCurrentIndex( frameBorder->findText( exception.frameBorderName( true ) ) );
    
    // blend color
    blendColor->setCurrentIndex( blendColor->findText( exception.blendColorName( true ) ) );

    // size grip
    sizeGripMode->setCurrentIndex( sizeGripMode->findText( exception.sizeGripModeName( true ) ) );
    
    // flags
    drawSeparator->setValue( exception.drawSeparator() );
    showStripes->setValue( exception.showStripes() );
    titleOutline->setValue( exception.drawTitleOutline() );
    
    // mask
    for( CheckBoxMap::iterator iter = checkboxes_.begin(); iter != checkboxes_.end(); iter++ )
    { iter->second->setChecked( exception.mask() & iter->first ); }
    
  }

  //___________________________________________
  NitrogenException NitrogenExceptionDialog::exception( void ) const
  { 
    NitrogenException exception( exception_ );
    exception.setType( NitrogenException::type( exceptionType->currentText() ) );
    exception.regExp().setPattern( exceptionEditor->text() );
    exception.setFrameBorder( NitrogenException::frameBorder( frameBorder->currentText(), true ) );
    exception.setBlendColor( NitrogenException::blendColor( blendColor->currentText(), true ) ); 
    exception.setSizeGripMode( NitrogenException::sizeGripMode( sizeGripMode->currentText(), true ) );
    
    // flags
    exception.setDrawSeparator( drawSeparator->isChecked() );
    exception.setShowStripes( showStripes->isChecked() );
    exception.setDrawTitleOutline( titleOutline->isChecked() );
    
    // mask
    unsigned int mask = NitrogenException::None;
    for( CheckBoxMap::const_iterator iter = checkboxes_.begin(); iter != checkboxes_.end(); iter++ )
    { if( iter->second->isChecked() ) mask |= iter->first; }
    
    exception.setMask( mask );
    return exception;
  
  }

  //___________________________________________
  const QString NitrogenExceptionDialog::ComboBox::Yes( i18n("Enabled") );
  const QString NitrogenExceptionDialog::ComboBox::No( i18n("Disabled") );

  //___________________________________________
  NitrogenExceptionDialog::ComboBox::ComboBox( QWidget* parent ):
    QComboBox( parent )
  { insertItems( 0, QStringList() << Yes << No ); }

  //___________________________________________
  void NitrogenExceptionDialog::ComboBox::setValue(  bool checked )
  { setCurrentIndex( findText( checked ? Yes:No ) ); }
  
  //___________________________________________
  bool NitrogenExceptionDialog::ComboBox::isChecked( void ) const
  { return currentText() == Yes; }
  
}
