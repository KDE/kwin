//////////////////////////////////////////////////////////////////////////////
// oxygenexceptionlistwidget.cpp
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

#include "oxygenexceptionlistwidget.h"
#include "oxygenexceptionlistwidget.moc"
#include "oxygenexceptiondialog.h"

#include <QtCore/QSharedPointer>
#include <KLocale>
#include <KMessageBox>

//__________________________________________________________
namespace Oxygen
{

    //__________________________________________________________
    ExceptionListWidget::ExceptionListWidget( QWidget* parent, Configuration default_configuration ):
        QWidget( parent ),
        default_configuration_( default_configuration )
    {

        //! ui
        ui.setupUi( this );

        // list
        ui.exceptionListView->setAllColumnsShowFocus( true );
        ui.exceptionListView->setRootIsDecorated( false );
        ui.exceptionListView->setSortingEnabled( false );
        ui.exceptionListView->setModel( &_model() );
        ui.exceptionListView->sortByColumn( ExceptionModel::TYPE );
        ui.exceptionListView->setSizePolicy( QSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Ignored ) );

        KIconLoader* icon_loader = KIconLoader::global();
        ui.moveUpButton->setIcon( KIcon( "arrow-up", icon_loader ) );
        ui.moveDownButton->setIcon( KIcon( "arrow-down", icon_loader ) );
        ui.addButton->setIcon( KIcon( "list-add", icon_loader ) );
        ui.removeButton->setIcon( KIcon( "list-remove", icon_loader ) );
        ui.editButton->setIcon( KIcon( "edit-rename", icon_loader ) );

        connect( ui.addButton, SIGNAL( clicked() ), SLOT( _add() ) );
        connect( ui.editButton, SIGNAL( clicked() ), SLOT( _edit() ) );
        connect( ui.removeButton, SIGNAL( clicked() ), SLOT( _remove() ) );
        connect( ui.moveUpButton, SIGNAL( clicked() ), SLOT( _up() ) );
        connect( ui.moveDownButton, SIGNAL( clicked() ), SLOT( _down() ) );

        connect( ui.exceptionListView, SIGNAL( activated( const QModelIndex& ) ), SLOT( _edit() ) );
        connect( ui.exceptionListView, SIGNAL( clicked( const QModelIndex& ) ), SLOT( _toggle( const QModelIndex& ) ) );
        connect( ui.exceptionListView->selectionModel(), SIGNAL( selectionChanged(const QItemSelection &, const QItemSelection &) ), SLOT( _updateButtons() ) );

        _updateButtons();
        _resizeColumns();


    }

    //__________________________________________________________
    void ExceptionListWidget::setExceptions( const ExceptionList& exceptions )
    {
        _model().set( ExceptionModel::List( exceptions.begin(), exceptions.end() ) );
        _resizeColumns();
    }

    //__________________________________________________________
    ExceptionList ExceptionListWidget::exceptions( void ) const
    {

        ExceptionModel::List exceptions( _model().get() );
        ExceptionList out;
        for( ExceptionModel::List::const_iterator iter = exceptions.begin(); iter != exceptions.end(); ++iter )
        { out.push_back( *iter ); }
        return out;

    }

    //__________________________________________________________
    void ExceptionListWidget::_updateButtons( void )
    {

        bool has_selection( !ui.exceptionListView->selectionModel()->selectedRows().empty() );
        ui.removeButton->setEnabled( has_selection );
        ui.editButton->setEnabled( has_selection );

        ui.moveUpButton->setEnabled( has_selection && !ui.exceptionListView->selectionModel()->isRowSelected( 0, QModelIndex() ) );
        ui.moveDownButton->setEnabled( has_selection && !ui.exceptionListView->selectionModel()->isRowSelected( _model().rowCount()-1, QModelIndex() ) );

    }


    //_______________________________________________________
    void ExceptionListWidget::_add( void )
    {

        // map dialog
        QSharedPointer<ExceptionDialog> dialog( new ExceptionDialog( this ) );
        dialog->setException( default_configuration_ );
        if( dialog->exec() == QDialog::Rejected ) return;

        // retrieve exception and check
        Exception exception( dialog->exception() );
        if( !_checkException( exception ) ) return;

        // create new item
        _model().add( exception );

        // make sure item is selected
        QModelIndex index( _model().index( exception ) );
        if( index != ui.exceptionListView->selectionModel()->currentIndex() )
        {
            ui.exceptionListView->selectionModel()->select( index,  QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Rows );
            ui.exceptionListView->selectionModel()->setCurrentIndex( index,  QItemSelectionModel::Current|QItemSelectionModel::Rows );
        }

        _resizeColumns();
        emit changed();
        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::_edit( void )
    {

        // retrieve selection
        QModelIndex current( ui.exceptionListView->selectionModel()->currentIndex() );
        if( !current.isValid() ) return;

        Exception& exception( _model().get( current ) );

        // create dialog
        QSharedPointer<ExceptionDialog> dialog( new ExceptionDialog( this ) );
        dialog->setException( exception );

        // map dialog
        if( dialog->exec() == QDialog::Rejected ) return;
        Exception new_exception = dialog->exception();

        // check if exception was changed
        if( exception == new_exception ) return;

        // check new exception validity
        if( !_checkException( new_exception ) ) return;

        // asign new exception
        *&exception = new_exception;
        _resizeColumns();
        emit changed();
        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::_remove( void )
    {

        // should use a konfirmation dialog
        if( KMessageBox::questionYesNo( this, i18n("Remove selected exception?") ) == KMessageBox::No ) return;

        // remove
        _model().remove( _model().get( ui.exceptionListView->selectionModel()->selectedRows() ) );
        _resizeColumns();
        emit changed();
        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::_toggle( const QModelIndex& index )
    {

        if( !index.isValid() ) return;
        if( index.column() != ExceptionModel::ENABLED ) return;

        // get matching exception
        Exception& exception( _model().get( index ) );
        exception.setEnabled( !exception.enabled() );
        _model().add( exception );

        emit changed();
        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::_up( void )
    {

        ExceptionModel::List selection( _model().get( ui.exceptionListView->selectionModel()->selectedRows() ) );
        if( selection.empty() ) { return; }

        // retrieve selected indexes in list and store in model
        QModelIndexList selected_indexes( ui.exceptionListView->selectionModel()->selectedRows() );
        ExceptionModel::List selected_exceptions( _model().get( selected_indexes ) );

        ExceptionModel::List current_exceptions( _model().get() );
        ExceptionModel::List new_exceptions;

        for( ExceptionModel::List::const_iterator iter = current_exceptions.begin(); iter != current_exceptions.end(); ++iter )
        {

            // check if new list is not empty, current index is selected and last index is not.
            // if yes, move.
            if(
                !( new_exceptions.empty() ||
                selected_indexes.indexOf( _model().index( *iter ) ) == -1 ||
                selected_indexes.indexOf( _model().index( new_exceptions.back() ) ) != -1
                ) )
            {
                Exception last( new_exceptions.back() );
                new_exceptions.pop_back();
                new_exceptions.push_back( *iter );
                new_exceptions.push_back( last );
            } else new_exceptions.push_back( *iter );

        }

        _model().set( new_exceptions );

        // restore selection
        ui.exceptionListView->selectionModel()->select( _model().index( selected_exceptions.front() ),  QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Rows );
        for( ExceptionModel::List::const_iterator iter = selected_exceptions.begin(); iter != selected_exceptions.end(); ++iter )
        { ui.exceptionListView->selectionModel()->select( _model().index( *iter ), QItemSelectionModel::Select|QItemSelectionModel::Rows ); }

        emit changed();
        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::_down( void )
    {

        ExceptionModel::List selection( _model().get( ui.exceptionListView->selectionModel()->selectedRows() ) );
        if( selection.empty() )
        { return; }

        // retrieve selected indexes in list and store in model
        QModelIndexList selected_indexes( ui.exceptionListView->selectionModel()->selectedIndexes() );
        ExceptionModel::List selected_exceptions( _model().get( selected_indexes ) );

        ExceptionModel::List current_exceptions( _model().get() );
        ExceptionModel::List new_exceptions;

        for( ExceptionModel::List::reverse_iterator iter = current_exceptions.rbegin(); iter != current_exceptions.rend(); ++iter )
        {

            // check if new list is not empty, current index is selected and last index is not.
            // if yes, move.
            if(
                !( new_exceptions.empty() ||
                selected_indexes.indexOf( _model().index( *iter ) ) == -1 ||
                selected_indexes.indexOf( _model().index( new_exceptions.back() ) ) != -1
                ) )
            {

                Exception last( new_exceptions.back() );
                new_exceptions.pop_back();
                new_exceptions.push_back( *iter );
                new_exceptions.push_back( last );

            } else new_exceptions.push_back( *iter );
        }

        _model().set( ExceptionModel::List( new_exceptions.rbegin(), new_exceptions.rend() ) );

        // restore selection
        ui.exceptionListView->selectionModel()->select( _model().index( selected_exceptions.front() ),  QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Rows );
        for( ExceptionModel::List::const_iterator iter = selected_exceptions.begin(); iter != selected_exceptions.end(); ++iter )
        { ui.exceptionListView->selectionModel()->select( _model().index( *iter ), QItemSelectionModel::Select|QItemSelectionModel::Rows ); }

        emit changed();
        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::_resizeColumns( void ) const
    {
        ui.exceptionListView->resizeColumnToContents( ExceptionModel::ENABLED );
        ui.exceptionListView->resizeColumnToContents( ExceptionModel::TYPE );
        ui.exceptionListView->resizeColumnToContents( ExceptionModel::REGEXP );
    }

    //_______________________________________________________
    bool ExceptionListWidget::_checkException( Exception& exception )
    {

        while( !exception.regExp().isValid() )
        {

            KMessageBox::error( this, i18n("Regular Expression syntax is incorrect") );
            QSharedPointer<ExceptionDialog> dialog( new ExceptionDialog( this ) );
            dialog->setException( exception );
            if( dialog->exec() == QDialog::Rejected ) return false;
            exception = dialog->exception();

        }

        return true;
    }

}
