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
    ExceptionDialog::ExceptionDialog( QWidget* parent ):
        KDialog( parent ),
        _detectDialog(0)
    {

        // define buttons
        setButtons( Ok|Cancel );
        QWidget* local( new QWidget( this ) );
        ui.setupUi( local );
        setMainWidget( local );

        // exception type
        ui.exceptionType->insertItems( 0, QStringList()
            << Exception::typeName( Exception::WindowClassName, true )
            << Exception::typeName( Exception::WindowTitle, true )
            );

        connect( ui.detectDialogButton, SIGNAL(clicked()), SLOT(selectWindowProperties()) );

        // border size
        ui.frameBorderComboBox->insertItems(0, QStringList()
            << Configuration::frameBorderName( Configuration::BorderNone, true )
            << Configuration::frameBorderName( Configuration::BorderNoSide, true )
            << Configuration::frameBorderName( Configuration::BorderTiny, true )
            << Configuration::frameBorderName( Configuration::BorderDefault, true )
            << Configuration::frameBorderName( Configuration::BorderLarge, true )
            << Configuration::frameBorderName( Configuration::BorderVeryLarge, true )
            << Configuration::frameBorderName( Configuration::BorderHuge, true )
            << Configuration::frameBorderName( Configuration::BorderVeryHuge, true )
            << Configuration::frameBorderName( Configuration::BorderOversized, true )
            );
        ui.frameBorderComboBox->setEnabled( false );
        _checkBoxes.insert( std::make_pair( Exception::FrameBorder, ui.frameBorderCheckBox ) );
        connect( ui.frameBorderCheckBox, SIGNAL(toggled(bool)), ui.frameBorderComboBox, SLOT(setEnabled(bool)) );

        // blend color
        ui.blendColorComboBox->insertItems(0, QStringList()
            << Exception::blendColorName( Exception::NoBlending, true )
            << Exception::blendColorName( Exception::RadialBlending, true )
            << Exception::blendColorName( Exception::BlendFromStyle, true )
            );
        ui.blendColorComboBox->setEnabled( false );
        _checkBoxes.insert( std::make_pair( Exception::BlendColor, ui.blendColorCheckBox ) );
        connect( ui.blendColorCheckBox, SIGNAL(toggled(bool)), ui.blendColorComboBox, SLOT(setEnabled(bool)) );

        // size grip
        ui.sizeGripComboBox->insertItems(0, QStringList()
            << Configuration::sizeGripModeName( Configuration::SizeGripNever, true )
            << Configuration::sizeGripModeName( Configuration::SizeGripWhenNeeded, true )
            );
        ui.sizeGripComboBox->setEnabled( false );
        _checkBoxes.insert( std::make_pair( Exception::SizeGripMode, ui.sizeGripCheckBox ) );
        connect( ui.sizeGripCheckBox, SIGNAL(toggled(bool)), ui.sizeGripComboBox, SLOT(setEnabled(bool)) );

        // outline active window title
        ui.titleOutlineComboBox->insertItems(0, QStringList() << i18nc( "outline window title", "Enabled" ) << i18nc( "outline window title", "Disabled" ) );
        ui.titleOutlineComboBox->setEnabled( false );
        _checkBoxes.insert( std::make_pair( Exception::TitleOutline, ui.titleOutlineCheckBox ) );
        connect( ui.titleOutlineCheckBox, SIGNAL(toggled(bool)), ui.titleOutlineComboBox, SLOT(setEnabled(bool)) );

        // separator
        ui.separatorComboBox->setEnabled( false );
        _checkBoxes.insert( std::make_pair( Exception::DrawSeparator, ui.separatorCheckBox ) );
        connect( ui.separatorCheckBox, SIGNAL(toggled(bool)), ui.separatorComboBox, SLOT(setEnabled(bool)) );

    }

    //___________________________________________
    void ExceptionDialog::setException( Exception exception )
    {

        // store exception internally
        _exception = exception;

        // type
        ui.exceptionType->setCurrentIndex( ui.exceptionType->findText( exception.typeName( true ) ) );
        ui.exceptionEditor->setText( exception.regExp().pattern() );
        ui.frameBorderComboBox->setCurrentIndex( ui.frameBorderComboBox->findText( exception.frameBorderName( true ) ) );
        ui.blendColorComboBox->setCurrentIndex( ui.blendColorComboBox->findText( exception.blendColorName( true ) ) );
        ui.sizeGripComboBox->setCurrentIndex( ui.sizeGripComboBox->findText( exception.sizeGripModeName( true ) ) );
        ui.separatorComboBox->setCurrentIndex( exception.separatorMode() );
        ui.titleOutlineComboBox->setCurrentIndex( ui.titleOutlineComboBox->findText( exception.drawTitleOutline() ? i18nc( "outline window title", "Enabled" ) : i18nc( "outline window title", "Disabled" ) ) );
        ui.hideTitleBar->setChecked( exception.hideTitleBar() );

        // mask
        for( CheckBoxMap::iterator iter = _checkBoxes.begin(); iter != _checkBoxes.end(); ++iter )
        { iter->second->setChecked( exception.mask() & iter->first ); }

    }

    //___________________________________________
    Exception ExceptionDialog::exception( void ) const
    {
        Exception exception( _exception );
        exception.setType( Exception::type( ui.exceptionType->currentText(), true ) );
        exception.regExp().setPattern( ui.exceptionEditor->text() );
        exception.setFrameBorder( Exception::frameBorder( ui.frameBorderComboBox->currentText(), true ) );
        exception.setBlendColor( Exception::blendColor( ui.blendColorComboBox->currentText(), true ) );
        exception.setSizeGripMode( Exception::sizeGripMode( ui.sizeGripComboBox->currentText(), true ) );

        // flags
        switch( ui.separatorComboBox->currentIndex() )
        {
            default:
            case 0: exception.setSeparatorMode( Configuration::SeparatorNever ); break;
            case 1: exception.setSeparatorMode( Configuration::SeparatorActive ); break;
            case 2: exception.setSeparatorMode( Configuration::SeparatorAlways ); break;
        }

        exception.setDrawTitleOutline( ui.titleOutlineComboBox->currentText() == i18nc( "outline window title", "Enabled" ) );
        exception.setHideTitleBar( ui.hideTitleBar->isChecked() );

        // mask
        unsigned int mask = Exception::None;
        for( CheckBoxMap::const_iterator iter = _checkBoxes.begin(); iter != _checkBoxes.end(); ++iter )
        { if( iter->second->isChecked() ) mask |= iter->first; }

        exception.setMask( mask );
        return exception;

    }

    //___________________________________________
    void ExceptionDialog::selectWindowProperties( void )
    {

        // create widget
        if( !_detectDialog )
        {
            _detectDialog = new DetectDialog( this );
            connect( _detectDialog, SIGNAL(detectionDone(bool)), SLOT(readWindowProperties(bool)) );
        }

        _detectDialog->detect(0);

    }

    //___________________________________________
    void ExceptionDialog::readWindowProperties( bool valid )
    {
        assert( _detectDialog );
        if( valid )
        {

            // type
            ui.exceptionType->setCurrentIndex( ui.exceptionType->findText( Exception::typeName( _detectDialog->exceptionType(), true ) ) );

            // window info
            const KWindowInfo& info( _detectDialog->windowInfo() );

            switch( _detectDialog->exceptionType() )
            {
                case Exception::WindowClassName:
                ui.exceptionEditor->setText( info.windowClassClass() );
                break;

                case Exception::WindowTitle:
                ui.exceptionEditor->setText( info.name() );
                break;

                default: assert( false );

            }

        }

        delete _detectDialog;
        _detectDialog = 0;

    }

}
