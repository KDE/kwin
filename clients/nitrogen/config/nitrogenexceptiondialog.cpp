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

#include <cassert>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <KLocale>
#include <KPushButton>

#include "nitrogendetectwidget.h"
#include "nitrogenexceptiondialog.h"
#include "nitrogenexceptiondialog.moc"

namespace Nitrogen
{

  //___________________________________________
  NitrogenExceptionDialog::NitrogenExceptionDialog( QWidget* parent ):
    KDialog( parent ),
    detectDialog(0)
  {

    // define buttons
    setButtons( Ok|Cancel );
    showButtonSeparator( false );

    // main widget
    QWidget* widget = new QWidget( this );
    setMainWidget( widget );

    widget->setLayout( new QVBoxLayout() );
    widget->layout()->setMargin(0);

    // exception definition
    QGroupBox* box;
    widget->layout()->addWidget( box = new QGroupBox( i18n( "Window Identification" ), widget ) );

    QGridLayout* gridLayout = new QGridLayout();
    box->setLayout( gridLayout );

    QLabel *label;

    // exception type
    gridLayout->addWidget( label = new QLabel( i18n( "Matching window property:" ), box ), 0, 0, 1, 1 );
    gridLayout->addWidget( exceptionType = new QComboBox(box), 0, 1, 1, 1 );
    exceptionType->insertItems(0, QStringList()
      << NitrogenException::typeName( NitrogenException::WindowClassName, true )
      << NitrogenException::typeName( NitrogenException::WindowTitle, true )
      );
    exceptionType->setToolTip( i18n(
      "Select here the window property used to identify windows \n"
      "to which the specific decoration options apply." ) );

    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );

    KPushButton* button = new KPushButton( i18n( "&Detect Window Properties" ), box );
    gridLayout->addWidget( button, 2, 0, 1, 2, Qt::AlignRight|Qt::AlignVCenter );
    connect( button, SIGNAL( clicked( void ) ), SLOT( selectWindowProperties() ) );

    // regular expression
    gridLayout->addWidget( label = new QLabel( i18n( "Regular expression to match: " ), box ), 1, 0, 1, 1 );
    gridLayout->addWidget( exceptionEditor = new KLineEdit( box ), 1, 1, 1, 1 );
    exceptionEditor->setClearButtonShown( true );
    exceptionEditor->setToolTip( i18n(
      "Type here the regular expression used to identify windows \n"
      "to which the specific decoration options apply." ) );

    label->setAlignment( Qt::AlignRight|Qt::AlignVCenter );

    // decoration flags
    widget->layout()->addWidget( box = new QGroupBox( i18n( "Decoration Options" ), widget ) );
    gridLayout = new QGridLayout();
    box->setLayout( gridLayout );

    QCheckBox* checkbox;

    // border size
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Border size:" ), box ), 0, 0, 1, 1 );
    gridLayout->addWidget( frameBorder = new QComboBox(box), 0, 1, 1, 1 );
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
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Extra size grip display:" ), box ), 2, 0, 1, 1 );
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
    gridLayout->addWidget( checkbox = new QCheckBox( i18n("Draw separator between title bar and active window contents:" ), box ), 4, 0, 1, 1 );
    gridLayout->addWidget( drawSeparator = new ComboBox( box ), 4, 1, 1, 1 );
    drawSeparator->setEnabled( false );
    checkboxes_.insert( std::make_pair( NitrogenException::DrawSeparator, checkbox ) );
    connect( checkbox, SIGNAL( toggled( bool ) ), drawSeparator, SLOT( setEnabled( bool ) ) );

  }

  //___________________________________________
  void NitrogenExceptionDialog::setException( NitrogenException exception )
  {

    // store exception internally
    exception_ = exception;

    // type
    exceptionType->setCurrentIndex( exceptionType->findText( exception.typeName( true ) ) );

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
    titleOutline->setValue( exception.drawTitleOutline() );

    // mask
    for( CheckBoxMap::iterator iter = checkboxes_.begin(); iter != checkboxes_.end(); iter++ )
    { iter->second->setChecked( exception.mask() & iter->first ); }

  }

  //___________________________________________
  NitrogenException NitrogenExceptionDialog::exception( void ) const
  {
    NitrogenException exception( exception_ );
    exception.setType( NitrogenException::type( exceptionType->currentText(), true ) );
    exception.regExp().setPattern( exceptionEditor->text() );
    exception.setFrameBorder( NitrogenException::frameBorder( frameBorder->currentText(), true ) );
    exception.setBlendColor( NitrogenException::blendColor( blendColor->currentText(), true ) );
    exception.setSizeGripMode( NitrogenException::sizeGripMode( sizeGripMode->currentText(), true ) );

    // flags
    exception.setDrawSeparator( drawSeparator->isChecked() );
    exception.setDrawTitleOutline( titleOutline->isChecked() );

    // mask
    unsigned int mask = NitrogenException::None;
    for( CheckBoxMap::const_iterator iter = checkboxes_.begin(); iter != checkboxes_.end(); iter++ )
    { if( iter->second->isChecked() ) mask |= iter->first; }

    exception.setMask( mask );
    return exception;

  }

  //___________________________________________
  void NitrogenExceptionDialog::selectWindowProperties( void )
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
  void NitrogenExceptionDialog::readWindowProperties( bool valid )
  {
    assert( detectDialog );
    if( valid )
    {

      // type
      exceptionType->setCurrentIndex( exceptionType->findText( NitrogenException::typeName( detectDialog->exceptionType(), true ) ) );

      // window info
      const KWindowInfo& info( detectDialog->windowInfo() );

      switch( detectDialog->exceptionType() )
      {
        case NitrogenException::WindowClassName:
        exceptionEditor->setText( info.windowClassClass() );
        break;

        case NitrogenException::WindowTitle:
        exceptionEditor->setText( info.name() );
        break;

        default: assert( false );

      }

    }

    delete detectDialog;
    detectDialog = 0;

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
