// $Id: nitrogenexceptiondialog.cpp,v 1.12 2009/07/05 16:15:31 hpereira Exp $

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
    widget->layout()->addWidget( box = new QGroupBox( tr2i18n( "Definition" ), widget ) );
    
    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setSpacing(5);
    gridLayout->setMargin(5);
    box->setLayout( gridLayout );
      
    QLabel *label;
    
    // exception type
    gridLayout->addWidget( label = new QLabel( tr2i18n( "Exception type: " ), box ), 0, 0, 1, 1 );
    gridLayout->addWidget( type_combobox_ = new QComboBox(box), 0, 1, 1, 1 );
    type_combobox_->insertItems(0, QStringList()
      << NitrogenException::typeName( NitrogenException::WindowClassName )
      << NitrogenException::typeName( NitrogenException::WindowTitle ) );
    type_combobox_->setToolTip( tr2i18n(
      "Select here the window caracteristic used to \n"
      "identify windows to which the exception apply." ) );
    
    label->setAlignment( Qt::AlignRight );
    
    // regular expression
    gridLayout->addWidget( label = new QLabel( tr2i18n( "Regular expression to match: " ), box ), 1, 0, 1, 1 );
    gridLayout->addWidget( editor_ = new KLineEdit( box ), 1, 1, 1, 1 );
    editor_->setClearButtonShown( true );
    editor_->setToolTip( tr2i18n(
      "Type here the regular expression used to \n"
      "identify windows to which the exception apply." ) );

    label->setAlignment( Qt::AlignRight );
    
    // decoration flags
    widget->layout()->addWidget( box = new QGroupBox( tr2i18n( "Decoration" ), widget ) );    
    gridLayout = new QGridLayout();
    gridLayout->setSpacing(5);
    gridLayout->setMargin(5);
    box->setLayout( gridLayout );

    QCheckBox* checkbox;
    
    // border size
    gridLayout->addWidget( checkbox = new QCheckBox( tr2i18n("Border size:" ), box ), 2, 0, 1, 1 );
    gridLayout->addWidget( frame_border_combobox_ = new QComboBox(box), 2, 1, 1, 1 );
    frame_border_combobox_->insertItems(0, QStringList()
      << NitrogenException::frameBorderName( NitrogenException::BorderNone )
      << NitrogenException::frameBorderName( NitrogenException::BorderTiny )
      << NitrogenException::frameBorderName( NitrogenException::BorderSmall )
      << NitrogenException::frameBorderName( NitrogenException::BorderDefault )
      << NitrogenException::frameBorderName( NitrogenException::BorderLarge ) );
    frame_border_combobox_->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::FrameBorder, checkbox ) );
    checkbox->setToolTip( "If checked, specified frame border is used in place of default value." );
    connect( checkbox, SIGNAL( toggled( bool ) ), frame_border_combobox_, SLOT( setEnabled( bool ) ) );
    
    // blend color
    gridLayout->addWidget( checkbox = new QCheckBox( tr2i18n("Title bar blending:" ), box ), 3, 0, 1, 1 );
    gridLayout->addWidget( blend_combobox_ = new QComboBox(box), 3, 1, 1, 1 );
    blend_combobox_->insertItems(0, QStringList()
      << NitrogenException::blendColorName( NitrogenException::NoBlending )
      << NitrogenException::blendColorName( NitrogenException::RadialBlending ) );
    blend_combobox_->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::BlendColor, checkbox ) );
    checkbox->setToolTip( "If checked, specified blending color is used in title bar in place of default value." );
    connect( checkbox, SIGNAL( toggled( bool ) ), blend_combobox_, SLOT( setEnabled( bool ) ) );
    
    // separator
    gridLayout->addWidget( label = new QLabel( tr2i18n( "Draw separator :" ), box ), 4, 0, 1, 1 );
    gridLayout->addWidget( draw_separator_combobox_ = new ComboBox( box ), 4, 1, 1, 1 );
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    
    // stripes
    gridLayout->addWidget( label = new QLabel( tr2i18n( "Show stripes :" ), box ), 5, 0, 1, 1 );
    gridLayout->addWidget( show_stripes_combobox_ = new ComboBox( box ), 5, 1, 1, 1 );
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );

    // overwrite colors
    gridLayout->addWidget( label = new QLabel( tr2i18n( "Overwrite default title bar colors:" ), box ), 6, 0, 1, 1 );
    gridLayout->addWidget( overwrite_colors_combobox_ = new ComboBox( box ), 6, 1, 1, 1 );
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
    
    // size grip
    gridLayout->addWidget( label = new QLabel( tr2i18n( "Draw size grip :" ), box ), 7, 0, 1, 1 );
    gridLayout->addWidget( draw_size_grip_combobox_ = new ComboBox( box ), 7, 1, 1, 1 );
    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );
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
    frame_border_combobox_->setCurrentIndex( frame_border_combobox_->findText( exception.frameBorderName() ) );
    
    // blend color
    blend_combobox_->setCurrentIndex( blend_combobox_->findText( exception.blendColorName() ) );

    // flags
    draw_separator_combobox_->setValue( exception.mask() & NitrogenException::DrawSeparator, exception.drawSeparator() );
    show_stripes_combobox_->setValue( exception.mask() & NitrogenException::ShowStripes, exception.showStripes() );
    overwrite_colors_combobox_->setValue( exception.mask() & NitrogenException::OverwriteColors, exception.overwriteColors() );
    draw_size_grip_combobox_->setValue( exception.mask() & NitrogenException::DrawSizeGrip, exception.drawSizeGrip() );
    
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
    exception.setFrameBorder( NitrogenException::frameBorder( frame_border_combobox_->currentText() ) );
    exception.setBlendColor( NitrogenException::blendColor( blend_combobox_->currentText() ) ); 
    
    // mask
    unsigned int mask = NitrogenException::None;
    for( CheckBoxMap::const_iterator iter = checkboxes_.begin(); iter != checkboxes_.end(); iter++ )
    { if( iter->second->isChecked() ) mask |= iter->first; }
    
    // separator
    if( !draw_separator_combobox_->isDefault() )
    { 
      mask |= NitrogenException::DrawSeparator;
      exception.setDrawSeparator( draw_separator_combobox_->isChecked() );
    }
    
    // stripes
    if( !show_stripes_combobox_->isDefault() )
    { 
      mask |= NitrogenException::ShowStripes;
      exception.setShowStripes( show_stripes_combobox_->isChecked() );
    }
    
    // overwrite colors
    if( !overwrite_colors_combobox_->isDefault() )
    {
      mask |= NitrogenException::OverwriteColors;
      exception.setOverwriteColors( overwrite_colors_combobox_->isChecked() );
    }
    
    // size grip
    if( !draw_size_grip_combobox_->isDefault() )
    { 
      mask |= NitrogenException::DrawSizeGrip;
      exception.setDrawSizeGrip( draw_size_grip_combobox_->isChecked() );
    }

    exception.setMask( mask );
    return exception;
  
  }

  //___________________________________________
  const QString NitrogenExceptionDialog::ComboBox::Default( "Default" );
  const QString NitrogenExceptionDialog::ComboBox::Yes( "Yes" );
  const QString NitrogenExceptionDialog::ComboBox::No( "No" );

  //___________________________________________
  NitrogenExceptionDialog::ComboBox::ComboBox( QWidget* parent ):
    QComboBox( parent )
  { insertItems( 0, QStringList() << Default << Yes << No ); }

  //___________________________________________
  void NitrogenExceptionDialog::ComboBox::setValue( bool enabled, bool checked )
  {
    if( !enabled ) setCurrentIndex( findText( Default ) );
    else setCurrentIndex( findText( checked ? Yes:No ) );
  }

  //___________________________________________
  bool NitrogenExceptionDialog::ComboBox::isDefault( void ) const
  { return currentText() == Default; }
  
  //___________________________________________
  bool NitrogenExceptionDialog::ComboBox::isChecked( void ) const
  { return currentText() == Yes; }
  
}
