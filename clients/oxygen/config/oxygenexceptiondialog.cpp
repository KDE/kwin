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
        QDialog( parent ),
        _detectDialog(0),
        _changed( false )
    {

        setupUi( this );
        connect( buttonBox->button( QDialogButtonBox::Cancel ), SIGNAL(clicked()), this, SLOT(close()) );

        // store checkboxes from ui into list
        _checkBoxes.insert( FrameBorder, frameBorderCheckBox );
        _checkBoxes.insert( BlendColor, blendColorCheckBox );
        _checkBoxes.insert( SizeGripMode, sizeGripCheckBox );
        _checkBoxes.insert( TitleOutline, titleOutlineCheckBox );
        _checkBoxes.insert( DrawSeparator, separatorCheckBox );

        // detect window properties
        connect( detectDialogButton, SIGNAL(clicked()), SLOT(selectWindowProperties()) );

        // connections
        connect( exceptionType, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( exceptionEditor, SIGNAL(textChanged(QString)), SLOT(updateChanged()) );
        connect( frameBorderComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( blendColorComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( sizeGripComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( titleOutlineComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );
        connect( separatorComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()) );

        for( CheckBoxMap::iterator iter = _checkBoxes.begin(); iter != _checkBoxes.end(); ++iter )
        { connect( iter.value(), SIGNAL(clicked()), SLOT(updateChanged()) ); }

        connect( hideTitleBar, SIGNAL(clicked()), SLOT(updateChanged()) );
    }

    //___________________________________________
    void ExceptionDialog::setException( ConfigurationPtr exception )
    {

        // store exception internally
        _exception = exception;

        // type
        exceptionType->setCurrentIndex(_exception->exceptionType() );
        exceptionEditor->setText( _exception->exceptionPattern() );
        frameBorderComboBox->setCurrentIndex( _exception->frameBorder() );
        blendColorComboBox->setCurrentIndex( _exception->blendStyle() );
        sizeGripComboBox->setCurrentIndex( _exception->drawSizeGrip() );
        separatorComboBox->setCurrentIndex( _exception->separatorMode() );
        titleOutlineComboBox->setCurrentIndex( _exception->drawTitleOutline() );
        hideTitleBar->setChecked( _exception->hideTitleBar() );

        // mask
        for( CheckBoxMap::iterator iter = _checkBoxes.begin(); iter != _checkBoxes.end(); ++iter )
        { iter.value()->setChecked( _exception->mask() & iter.key() ); }

        setChanged( false );

    }

    //___________________________________________
    void ExceptionDialog::save( void )
    {
        _exception->setExceptionType( exceptionType->currentIndex() );
        _exception->setExceptionPattern( exceptionEditor->text() );
        _exception->setFrameBorder( frameBorderComboBox->currentIndex() );
        _exception->setBlendStyle( blendColorComboBox->currentIndex() );
        _exception->setDrawSizeGrip( sizeGripComboBox->currentIndex() );
        _exception->setSeparatorMode( separatorComboBox->currentIndex() );
        _exception->setDrawTitleOutline( titleOutlineComboBox->currentIndex() );
        _exception->setHideTitleBar( hideTitleBar->isChecked() );

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
        if( _exception->exceptionType() != exceptionType->currentIndex() ) modified = true;
        else if( _exception->exceptionPattern() != exceptionEditor->text() ) modified = true;
        else if( _exception->frameBorder() != frameBorderComboBox->currentIndex() ) modified = true;
        else if( _exception->blendStyle() != blendColorComboBox->currentIndex() ) modified = true;
        else if( _exception->drawSizeGrip() != sizeGripComboBox->currentIndex() ) modified = true;
        else if( _exception->separatorMode() != separatorComboBox->currentIndex() ) modified = true;
        else if( _exception->drawTitleOutline() != titleOutlineComboBox->currentIndex() ) modified = true;
        else if( _exception->hideTitleBar() != hideTitleBar->isChecked() ) modified = true;
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
            exceptionType->setCurrentIndex( _detectDialog->exceptionType() );

            // window info
            const KWindowInfo& info( _detectDialog->windowInfo() );

            switch( _detectDialog->exceptionType() )
            {

                default:
                case Configuration::ExceptionWindowClassName:
                exceptionEditor->setText( QString::fromUtf8( info.windowClassClass() ) );
                break;

                case Configuration::ExceptionWindowTitle:
                exceptionEditor->setText( info.name() );
                break;

            }

        }

        delete _detectDialog;
        _detectDialog = 0;

    }

}
