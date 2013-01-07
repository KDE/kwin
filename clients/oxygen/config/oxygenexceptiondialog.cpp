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

namespace Oxygen
{

    //___________________________________________
    ExceptionDialog::ExceptionDialog( QWidget* parent ):
        KDialog( parent ),
        _detectDialog(0),
        _changed( false )
    {

        // define buttons
        setButtons( Ok|Cancel );
        QWidget* local( new QWidget( this ) );
        ui.setupUi( local );
        setMainWidget( local );

        // store checkboxes from ui into list
        _checkBoxes.insert( FrameBorder, ui.frameBorderCheckBox );
        _checkBoxes.insert( BlendColor, ui.blendColorCheckBox );
        _checkBoxes.insert( SizeGripMode, ui.sizeGripCheckBox );
        _checkBoxes.insert( TitleOutline, ui.titleOutlineCheckBox );
        _checkBoxes.insert( DrawSeparator, ui.separatorCheckBox );

        // detect window properties
        connect( ui.detectDialogButton, SIGNAL(clicked()), SLOT(selectWindowProperties()) );

        // connections
        connect( ui.exceptionType, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( ui.exceptionEditor, SIGNAL(textChanged(QString)), SLOT(updateChanged()) );
        connect( ui.frameBorderComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( ui.blendColorComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( ui.sizeGripComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( ui.titleOutlineComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( ui.separatorComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );

        for( CheckBoxMap::iterator iter = _checkBoxes.begin(); iter != _checkBoxes.end(); ++iter )
        { connect( iter.value(), SIGNAL(clicked()), SLOT(updateChanged()) ); }

        connect( ui.hideTitleBar, SIGNAL(clicked()), SLOT(updateChanged()) );
    }

    //___________________________________________
    void ExceptionDialog::setException( ConfigurationPtr exception )
    {

        // store exception internally
        _exception = exception;

        // type
        ui.exceptionType->setCurrentIndex(_exception->exceptionType() );
        ui.exceptionEditor->setText( _exception->exceptionPattern() );
        ui.frameBorderComboBox->setCurrentIndex( _exception->frameBorder() );
        ui.blendColorComboBox->setCurrentIndex( _exception->blendStyle() );
        ui.sizeGripComboBox->setCurrentIndex( _exception->drawSizeGrip() );
        ui.separatorComboBox->setCurrentIndex( _exception->separatorMode() );
        ui.titleOutlineComboBox->setCurrentIndex( _exception->drawTitleOutline() );
        ui.hideTitleBar->setChecked( _exception->hideTitleBar() );

        // mask
        for( CheckBoxMap::iterator iter = _checkBoxes.begin(); iter != _checkBoxes.end(); ++iter )
        { iter.value()->setChecked( _exception->mask() & iter.key() ); }

        setChanged( false );

    }

    //___________________________________________
    void ExceptionDialog::save( void )
    {
        _exception->setExceptionType( ui.exceptionType->currentIndex() );
        _exception->setExceptionPattern( ui.exceptionEditor->text() );
        _exception->setFrameBorder( ui.frameBorderComboBox->currentIndex() );
        _exception->setBlendStyle( ui.blendColorComboBox->currentIndex() );
        _exception->setDrawSizeGrip( ui.sizeGripComboBox->currentIndex() );
        _exception->setSeparatorMode( ui.separatorComboBox->currentIndex() );
        _exception->setDrawTitleOutline( ui.titleOutlineComboBox->currentIndex() );
        _exception->setHideTitleBar( ui.hideTitleBar->isChecked() );

        // mask
        unsigned int mask = None;
        for( CheckBoxMap::iterator iter = _checkBoxes.begin(); iter != _checkBoxes.end(); ++iter )
        { if( iter.value()->isChecked() ) mask |= iter.key(); }

        _exception->setMask( mask );

        setChanged( false );

    }

    //___________________________________________
    void ExceptionDialog::updateChanged( void )
    {
        bool modified( false );
        if( _exception->exceptionType() != ui.exceptionType->currentIndex() ) modified = true;
        else if( _exception->exceptionPattern() != ui.exceptionEditor->text() ) modified = true;
        else if( _exception->frameBorder() != ui.frameBorderComboBox->currentIndex() ) modified = true;
        else if( _exception->blendStyle() != ui.blendColorComboBox->currentIndex() ) modified = true;
        else if( _exception->drawSizeGrip() != ui.sizeGripComboBox->currentIndex() ) modified = true;
        else if( _exception->separatorMode() != ui.separatorComboBox->currentIndex() ) modified = true;
        else if( _exception->drawTitleOutline() != ui.titleOutlineComboBox->currentIndex() ) modified = true;
        else if( _exception->hideTitleBar() != ui.hideTitleBar->isChecked() ) modified = true;
        else
        {
            // check mask
            for( CheckBoxMap::iterator iter = _checkBoxes.begin(); iter != _checkBoxes.end(); ++iter )
            {
                if( iter.value()->isChecked() != (bool)( _exception->mask() & iter.key() ) )
                {
                    modified = true;
                    break;
                }
            }
        }

        setChanged( modified );

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
        Q_CHECK_PTR( _detectDialog );
        if( valid )
        {

            // type
            ui.exceptionType->setCurrentIndex( _detectDialog->exceptionType() );

            // window info
            const KWindowInfo& info( _detectDialog->windowInfo() );

            switch( _detectDialog->exceptionType() )
            {

                default:
                case Configuration::ExceptionWindowClassName:
                ui.exceptionEditor->setText( info.windowClassClass() );
                break;

                case Configuration::ExceptionWindowTitle:
                ui.exceptionEditor->setText( info.name() );
                break;

            }

        }

        delete _detectDialog;
        _detectDialog = 0;

    }

}
