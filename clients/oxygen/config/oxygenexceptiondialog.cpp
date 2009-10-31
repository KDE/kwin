//////////////////////////////////////////////////////////////////////////////
// oxygenexceptiondialog.cpp
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

#include "oxygenexceptiondialog.h"
#include "oxygenexceptiondialog.moc"
#include "oxygendetectwidget.h"

#include <cassert>


namespace Oxygen
{

  //___________________________________________
  const QString OxygenExceptionDialog::yes( i18n("Enabled") );
  const QString OxygenExceptionDialog::no( i18n("Disabled") );

  //___________________________________________
  OxygenExceptionDialog::OxygenExceptionDialog( QWidget* parent ):
    KDialog( parent ),
    detectDialog(0)
  {

    // define buttons
    setButtons( Ok|Cancel );
    showButtonSeparator( false );
    QWidget* local( new QWidget( this ) );
    ui.setupUi( local );
    setMainWidget( local );

    // exception type
    ui.label->setBuddy( ui.exceptionType );
    ui.exceptionType->insertItems( 0, QStringList()
      << OxygenException::typeName( OxygenException::WindowClassName, true )
      << OxygenException::typeName( OxygenException::WindowTitle, true )
      );

    connect( ui.detectDialogButton, SIGNAL( clicked( void ) ), SLOT( selectWindowProperties() ) );

    // regular expression
    ui.label_2->setBuddy( ui.exceptionEditor );

    // border size
    ui.frameBorderComboBox->insertItems(0, QStringList()
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
    ui.frameBorderComboBox->setEnabled( false );
    checkboxes_.insert( std::make_pair( OxygenException::FrameBorder, ui.frameBorderCheckBox ) );
    connect( ui.frameBorderCheckBox, SIGNAL( toggled( bool ) ), ui.frameBorderComboBox, SLOT( setEnabled( bool ) ) );

    // blend color
    ui.blendColorComboBox->insertItems(0, QStringList()
      << OxygenException::blendColorName( OxygenException::NoBlending, true )
      << OxygenException::blendColorName( OxygenException::RadialBlending, true ) );
    ui.blendColorComboBox->setEnabled( false );
    checkboxes_.insert( std::make_pair( OxygenException::BlendColor, ui.blendColorCheckBox ) );
    connect( ui.blendColorCheckBox, SIGNAL( toggled( bool ) ), ui.blendColorComboBox, SLOT( setEnabled( bool ) ) );

    // size grip
    ui.sizeGripComboBox->insertItems(0, QStringList()
      << OxygenConfiguration::sizeGripModeName( OxygenConfiguration::SizeGripNever, true )
      << OxygenConfiguration::sizeGripModeName( OxygenConfiguration::SizeGripWhenNeeded, true )
      );
    ui.sizeGripComboBox->setEnabled( false );
    checkboxes_.insert( std::make_pair( OxygenException::SizeGripMode, ui.sizeGripCheckBox ) );
    connect( ui.sizeGripCheckBox, SIGNAL( toggled( bool ) ), ui.sizeGripComboBox, SLOT( setEnabled( bool ) ) );

    // outline active window title
    ui.titleOutlineComboBox->insertItems(0, QStringList() << yes << no );
    ui.titleOutlineComboBox->setEnabled( false );
    checkboxes_.insert( std::make_pair( OxygenException::TitleOutline, ui.titleOutlineCheckBox ) );
    connect( ui.titleOutlineCheckBox, SIGNAL( toggled( bool ) ), ui.titleOutlineComboBox, SLOT( setEnabled( bool ) ) );

    // separator
    ui.separatorComboBox->insertItems(0, QStringList() << yes << no );
    ui.separatorComboBox->setEnabled( false );
    checkboxes_.insert( std::make_pair( OxygenException::DrawSeparator, ui.separatorCheckBox ) );
    connect( ui.separatorCheckBox, SIGNAL( toggled( bool ) ), ui.separatorComboBox, SLOT( setEnabled( bool ) ) );

  }

  //___________________________________________
  void OxygenExceptionDialog::setException( OxygenException exception )
  {

    // store exception internally
    exception_ = exception;

    // type
    ui.exceptionType->setCurrentIndex( ui.exceptionType->findText( exception.typeName( true ) ) );
    ui.exceptionEditor->setText( exception.regExp().pattern() );
    ui.frameBorderComboBox->setCurrentIndex( ui.frameBorderComboBox->findText( exception.frameBorderName( true ) ) );
    ui.blendColorComboBox->setCurrentIndex( ui.blendColorComboBox->findText( exception.blendColorName( true ) ) );
    ui.sizeGripComboBox->setCurrentIndex( ui.sizeGripComboBox->findText( exception.sizeGripModeName( true ) ) );
    ui.separatorComboBox->setCurrentIndex( ui.separatorComboBox->findText( exception.drawSeparator() ? yes:no ) );
    ui.titleOutlineComboBox->setCurrentIndex( ui.titleOutlineComboBox->findText( exception.drawTitleOutline() ? yes:no ) );

    // mask
    for( CheckBoxMap::iterator iter = checkboxes_.begin(); iter != checkboxes_.end(); ++iter )
    { iter->second->setChecked( exception.mask() & iter->first ); }

  }

  //___________________________________________
  OxygenException OxygenExceptionDialog::exception( void ) const
  {
    OxygenException exception( exception_ );
    exception.setType( OxygenException::type( ui.exceptionType->currentText(), true ) );
    exception.regExp().setPattern( ui.exceptionEditor->text() );
    exception.setFrameBorder( OxygenException::frameBorder( ui.frameBorderComboBox->currentText(), true ) );
    exception.setBlendColor( OxygenException::blendColor( ui.blendColorComboBox->currentText(), true ) );
    exception.setSizeGripMode( OxygenException::sizeGripMode( ui.sizeGripComboBox->currentText(), true ) );

    // flags
    exception.setDrawSeparator( ui.separatorComboBox->currentText() == yes );
    exception.setDrawTitleOutline( ui.titleOutlineComboBox->currentText() == yes );

    // mask
    unsigned int mask = OxygenException::None;
    for( CheckBoxMap::const_iterator iter = checkboxes_.begin(); iter != checkboxes_.end(); ++iter )
    { if( iter->second->isChecked() ) mask |= iter->first; }

    exception.setMask( mask );
    return exception;

  }

  //___________________________________________
  void OxygenExceptionDialog::selectWindowProperties( void )
  {

    // create widget
    if( !detectDialog )
    {
      detectDialog = new DetectDialog( this );
      connect( detectDialog, SIGNAL( detectionDone( bool ) ), SLOT( readWindowProperties( bool ) ) );
    }

    detectDialog->detect(0);

  }

  //___________________________________________
  void OxygenExceptionDialog::readWindowProperties( bool valid )
  {
    assert( detectDialog );
    if( valid )
    {

      // type
      ui.exceptionType->setCurrentIndex( ui.exceptionType->findText( OxygenException::typeName( detectDialog->exceptionType(), true ) ) );

      // window info
      const KWindowInfo& info( detectDialog->windowInfo() );

      switch( detectDialog->exceptionType() )
      {
        case OxygenException::WindowClassName:
        ui.exceptionEditor->setText( info.windowClassClass() );
        break;

        case OxygenException::WindowTitle:
        ui.exceptionEditor->setText( info.name() );
        break;

        default: assert( false );

      }

    }

    delete detectDialog;
    detectDialog = 0;

  }

}
