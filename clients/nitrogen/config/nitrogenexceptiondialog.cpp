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
    gridLayout->addWidget( type_combobox_ = new QComboBox(box), 0, 1, 1, 1 );
    type_combobox_->insertItems(0, QStringList()
      << NitrogenException::typeName( NitrogenException::WindowClassName )
      << NitrogenException::typeName( NitrogenException::WindowTitle ) );
    type_combobox_->setToolTip( i18n(
      "Select here the window characteristic used to \n"
      "identify windows to which the exception apply." ) );
    
    label->setAlignment( Qt::AlignRight );
    
    // regular expression
    gridLayout->addWidget( label = new QLabel( i18n( "Regular expression to match: " ), box ), 1, 0, 1, 1 );
    gridLayout->addWidget( editor_ = new KLineEdit( box ), 1, 1, 1, 1 );
    editor_->setClearButtonShown( true );
    editor_->setToolTip( i18n(
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
    gridLayout->addWidget( frame_border_combobox_ = new QComboBox(box), 0, 1, 1, 1 );
    frame_border_combobox_->insertItems(0, QStringList()
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderNone, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderTiny, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderDefault, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderLarge, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderVeryLarge, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderHuge, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderVeryHuge, true )
      << NitrogenConfiguration::frameBorderName( NitrogenConfiguration::BorderOversized, true )
      );
    frame_border_combobox_->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::FrameBorder, checkbox ) );
    checkbox->setToolTip( i18n("If checked, specified frame border is used in place of default value.") );
    connect( checkbox, SIGNAL( toggled( bool ) ), frame_border_combobox_, SLOT( setEnabled( bool ) ) );
    
    // blend color
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Background style:" ), box ), 1, 0, 1, 1 );
    gridLayout->addWidget( blend_combobox_ = new QComboBox(box), 1, 1, 1, 1 );
    blend_combobox_->insertItems(0, QStringList()
      << NitrogenException::blendColorName( NitrogenException::NoBlending, true )
      << NitrogenException::blendColorName( NitrogenException::RadialBlending, true ) );
    blend_combobox_->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::BlendColor, checkbox ) );
    checkbox->setToolTip( i18n("If checked, specified blending color is used in title bar in place of default value.") );
    connect( checkbox, SIGNAL( toggled( bool ) ), blend_combobox_, SLOT( setEnabled( bool ) ) );
    
    // size grip
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Size grip display:" ), box ), 2, 0, 1, 1 );
    gridLayout->addWidget( sizeGripModeComboBox_ = new QComboBox( box ), 2, 1, 1, 1 );
    sizeGripModeComboBox_->insertItems(0, QStringList()
      << NitrogenConfiguration::sizeGripModeName( NitrogenConfiguration::SizeGripNever, true )
      << NitrogenConfiguration::sizeGripModeName( NitrogenConfiguration::SizeGripWhenNeeded, true )
      << NitrogenConfiguration::sizeGripModeName( NitrogenConfiguration::SizeGripAlways, true )
      );
    sizeGripModeComboBox_->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::SizeGripMode, checkbox ) );
    connect( checkbox, SIGNAL( toggled( bool ) ), sizeGripModeComboBox_, SLOT( setEnabled( bool ) ) );

    // separator
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Draw separator between title bar and window contents:" ), box ), 3, 0, 1, 1 );
    gridLayout->addWidget( draw_separator_combobox_ = new ComboBox( box ), 3, 1, 1, 1 );
    draw_separator_combobox_->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::DrawSeparator, checkbox ) );
    connect( checkbox, SIGNAL( toggled( bool ) ), draw_separator_combobox_, SLOT( setEnabled( bool ) ) );
    
    // stripes
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Show stripes next to the title:" ), box ), 4, 0, 1, 1 );
    gridLayout->addWidget( show_stripes_combobox_ = new ComboBox( box ), 4, 1, 1, 1 );
    show_stripes_combobox_->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::ShowStripes, checkbox ) );
    connect( checkbox, SIGNAL( toggled( bool ) ), show_stripes_combobox_, SLOT( setEnabled( bool ) ) );

    // overwrite colors
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Blend title bar colors with window content:" ), box ), 5, 0, 1, 1 );
    gridLayout->addWidget( overwrite_colors_combobox_ = new ComboBox( box ), 5, 1, 1, 1 );
    overwrite_colors_combobox_->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::OverwriteColors, checkbox ) );
    connect( checkbox, SIGNAL( toggled( bool ) ), overwrite_colors_combobox_, SLOT( setEnabled( bool ) ) );
       
  }
  
  //___________________________________________
  void NitrogenExceptionDialog::setException( NitrogenException exception )
  {

    // store exception internally
    exception_ = exception;
    
    // type
    type_combobox_->setCurrentIndex( type_combobox_->findText( exception.typeName() ) );
    
    // regular expression
    editor_->setText( exception.regExp().pattern() );

    // border size
    frame_border_combobox_->setCurrentIndex( frame_border_combobox_->findText( exception.frameBorderName( true ) ) );
    
    // blend color
    blend_combobox_->setCurrentIndex( blend_combobox_->findText( exception.blendColorName( true ) ) );

    // size grip
    sizeGripModeComboBox_->setCurrentIndex( sizeGripModeComboBox_->findText( exception.sizeGripModeName( true ) ) );
    
    // flags
    draw_separator_combobox_->setValue( exception.drawSeparator() );
    show_stripes_combobox_->setValue( exception.showStripes() );
    overwrite_colors_combobox_->setValue( exception.overwriteColors() );
    
    // mask
    for( CheckBoxMap::iterator iter = checkboxes_.begin(); iter != checkboxes_.end(); iter++ )
    { iter->second->setChecked( exception.mask() & iter->first ); }
    
  }

  //___________________________________________
  NitrogenException NitrogenExceptionDialog::exception( void ) const
  { 
    NitrogenException exception( exception_ );
    exception.setType( NitrogenException::type( type_combobox_->currentText() ) );
    exception.regExp().setPattern( editor_->text() );
    exception.setFrameBorder( NitrogenException::frameBorder( frame_border_combobox_->currentText(), true ) );
    exception.setBlendColor( NitrogenException::blendColor( blend_combobox_->currentText(), true ) ); 
    exception.setSizeGripMode( NitrogenException::sizeGripMode( sizeGripModeComboBox_->currentText(), true ) );
    
    // flags
    exception.setDrawSeparator( draw_separator_combobox_->isChecked() );
    exception.setShowStripes( show_stripes_combobox_->isChecked() );
    exception.setOverwriteColors( overwrite_colors_combobox_->isChecked() );
    
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
