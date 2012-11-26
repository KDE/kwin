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
        _detectDialog(0)
    {

        // define buttons
        setButtons( Ok|Cancel );
        QWidget* local( new QWidget( this ) );
        ui.setupUi( local );
        setMainWidget( local );

        connect( ui.detectDialogButton, SIGNAL( clicked( void ) ), SLOT(selectWindowProperties()) );

        // border size
        ui.frameBorderComboBox->setEnabled( false );
        _checkBoxes.insert( FrameBorder, ui.frameBorderCheckBox );
        connect( ui.frameBorderCheckBox, SIGNAL( toggled( bool ) ), ui.frameBorderComboBox, SLOT(setEnabled(bool)) );

        // blend color
        ui.blendColorComboBox->setEnabled( false );
        _checkBoxes.insert( BlendColor, ui.blendColorCheckBox );
        connect( ui.blendColorCheckBox, SIGNAL( toggled( bool ) ), ui.blendColorComboBox, SLOT(setEnabled(bool)) );

        // size grip
        ui.sizeGripComboBox->setEnabled( false );
        _checkBoxes.insert( SizeGripMode, ui.sizeGripCheckBox );
        connect( ui.sizeGripCheckBox, SIGNAL( toggled( bool ) ), ui.sizeGripComboBox, SLOT(setEnabled(bool)) );

        // outline active window title
        ui.titleOutlineComboBox->insertItems(0, QStringList() << i18nc( "outline window title", "Enabled" ) << i18nc( "outline window title", "Disabled" ) );
        ui.titleOutlineComboBox->setEnabled( false );
        _checkBoxes.insert( TitleOutline, ui.titleOutlineCheckBox );
        connect( ui.titleOutlineCheckBox, SIGNAL( toggled( bool ) ), ui.titleOutlineComboBox, SLOT(setEnabled(bool)) );

        // separator
        ui.separatorComboBox->setEnabled( false );
        _checkBoxes.insert( DrawSeparator, ui.separatorCheckBox );
        connect( ui.separatorCheckBox, SIGNAL( toggled( bool ) ), ui.separatorComboBox, SLOT(setEnabled(bool)) );

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

    }

    //___________________________________________
    void ExceptionDialog::save( void )
    {
        _exception->setExceptionType( ui.exceptionType->currentIndex() );
        _exception->setExceptionPattern( ui.exceptionEditor->text() );
        _exception->setFrameBorder( ui.frameBorderComboBox->currentIndex() );
        _exception->setBlendStyle( ui.blendColorComboBox->currentIndex() );
        _exception->setDrawSizeGrip( ui.sizeGripComboBox->currentIndex() );

        // flags
        _exception->setSeparatorMode( ui.separatorComboBox->currentIndex() );
        _exception->setDrawTitleOutline( ui.titleOutlineComboBox->currentIndex() );
        _exception->setHideTitleBar( ui.hideTitleBar->isChecked() );

        // mask
        unsigned int mask = None;
        for( CheckBoxMap::const_iterator iter = _checkBoxes.begin(); iter != _checkBoxes.end(); ++iter )
        { if( iter.value()->isChecked() ) mask |= iter.key(); }

        _exception->setMask( mask );

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
