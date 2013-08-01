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

#include <QPointer>
#include <QIcon>
#include <KLocalizedString>
#include <KMessageBox>

//__________________________________________________________
namespace Oxygen
{

    //__________________________________________________________
    ExceptionListWidget::ExceptionListWidget( QWidget* parent ):
        QWidget( parent ),
        _changed( false )
    {

        //! ui
        ui.setupUi( this );

        // list
        ui.exceptionListView->setAllColumnsShowFocus( true );
        ui.exceptionListView->setRootIsDecorated( false );
        ui.exceptionListView->setSortingEnabled( false );
        ui.exceptionListView->setModel( &model() );
        ui.exceptionListView->sortByColumn( ExceptionModel::TYPE );
        ui.exceptionListView->setSizePolicy( QSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Ignored ) );

        ui.moveUpButton->setIcon( QIcon::fromTheme( QStringLiteral( "arrow-up" ) ) );
        ui.moveDownButton->setIcon( QIcon::fromTheme( QStringLiteral( "arrow-down" ) ) );
        ui.addButton->setIcon( QIcon::fromTheme( QStringLiteral( "list-add" ) ) );
        ui.removeButton->setIcon( QIcon::fromTheme( QStringLiteral( "list-remove" ) ) );
        ui.editButton->setIcon( QIcon::fromTheme( QStringLiteral( "edit-rename" ) ) );

        connect( ui.addButton, SIGNAL(clicked()), SLOT(add()) );
        connect( ui.editButton, SIGNAL(clicked()), SLOT(edit()) );
        connect( ui.removeButton, SIGNAL(clicked()), SLOT(remove()) );
        connect( ui.moveUpButton, SIGNAL(clicked()), SLOT(up()) );
        connect( ui.moveDownButton, SIGNAL(clicked()), SLOT(down()) );

        connect( ui.exceptionListView, SIGNAL(activated(QModelIndex)), SLOT(edit()) );
        connect( ui.exceptionListView, SIGNAL(clicked(QModelIndex)), SLOT(toggle(QModelIndex)) );
        connect( ui.exceptionListView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(updateButtons()) );

        updateButtons();
        resizeColumns();


    }

    //__________________________________________________________
    void ExceptionListWidget::setExceptions( const ConfigurationList& exceptions )
    {
        model().set( exceptions );
        resizeColumns();
        setChanged( false );
    }

    //__________________________________________________________
    ConfigurationList ExceptionListWidget::exceptions( void )
    {
        return model().get();
        setChanged( false );
    }

    //__________________________________________________________
    void ExceptionListWidget::updateButtons( void )
    {

        bool hasSelection( !ui.exceptionListView->selectionModel()->selectedRows().empty() );
        ui.removeButton->setEnabled( hasSelection );
        ui.editButton->setEnabled( hasSelection );

        ui.moveUpButton->setEnabled( hasSelection && !ui.exceptionListView->selectionModel()->isRowSelected( 0, QModelIndex() ) );
        ui.moveDownButton->setEnabled( hasSelection && !ui.exceptionListView->selectionModel()->isRowSelected( model().rowCount()-1, QModelIndex() ) );

    }


    //_______________________________________________________
    void ExceptionListWidget::add( void )
    {


        QPointer<ExceptionDialog> dialog = new ExceptionDialog( this );
        ConfigurationPtr exception( new Configuration() );
        exception->readConfig();
        dialog->setException( exception );

        // run dialog and check existence
        if( !dialog->exec() )
        {
            delete dialog;
            return;
        }

        dialog->save();
        delete dialog;

        // check exceptions
        if( !checkException( exception ) ) return;

        // create new item
        model().add( exception );
        setChanged( true );

        // make sure item is selected
        QModelIndex index( model().index( exception ) );
        if( index != ui.exceptionListView->selectionModel()->currentIndex() )
        {
            ui.exceptionListView->selectionModel()->select( index,  QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Rows );
            ui.exceptionListView->selectionModel()->setCurrentIndex( index,  QItemSelectionModel::Current|QItemSelectionModel::Rows );
        }

        resizeColumns();
        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::edit( void )
    {

        // retrieve selection
        QModelIndex current( ui.exceptionListView->selectionModel()->currentIndex() );
        if( ! model().contains( current ) ) return;

        ConfigurationPtr exception( model().get( current ) );

        // create dialog
        QPointer<ExceptionDialog> dialog( new ExceptionDialog( this ) );
        dialog->setException( exception );

        // map dialog
        if( !dialog->exec() )
        {
            delete dialog;
            return;
        }

        // check modifications
        if( !dialog->isChanged() ) return;

        // retrieve exception
        dialog->save();
        delete dialog;

        // check new exception validity
        checkException( exception );
        resizeColumns();

        setChanged( true );

        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::remove( void )
    {

        // should use a konfirmation dialog
        if( KMessageBox::questionYesNo( this, i18n("Remove selected exception?") ) == KMessageBox::No ) return;

        // remove
        model().remove( model().get( ui.exceptionListView->selectionModel()->selectedRows() ) );
        resizeColumns();
        updateButtons();

        setChanged( true );

        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::toggle( const QModelIndex& index )
    {

        if( !model().contains( index ) ) return;
        if( index.column() != ExceptionModel::ENABLED ) return;

        // get matching exception
        ConfigurationPtr exception( model().get( index ) );
        exception->setEnabled( !exception->enabled() );
        setChanged( true );
        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::up( void )
    {

        ConfigurationList selection( model().get( ui.exceptionListView->selectionModel()->selectedRows() ) );
        if( selection.empty() ) { return; }

        // retrieve selected indexes in list and store in model
        QModelIndexList selectedIndices( ui.exceptionListView->selectionModel()->selectedRows() );
        ConfigurationList selectedExceptions( model().get( selectedIndices ) );

        ConfigurationList currentException( model().get() );
        ConfigurationList newExceptions;

        for( ConfigurationList::const_iterator iter = currentException.constBegin(); iter != currentException.constEnd(); ++iter )
        {

            // check if new list is not empty, current index is selected and last index is not.
            // if yes, move.
            if(
                !( newExceptions.empty() ||
                selectedIndices.indexOf( model().index( *iter ) ) == -1 ||
                selectedIndices.indexOf( model().index( newExceptions.back() ) ) != -1
                ) )
            {
                ConfigurationPtr last( newExceptions.back() );
                newExceptions.removeLast();
                newExceptions.append( *iter );
                newExceptions.append( last );
            } else newExceptions.append( *iter );

        }

        model().set( newExceptions );

        // restore selection
        ui.exceptionListView->selectionModel()->select( model().index( selectedExceptions.front() ),  QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Rows );
        for( ConfigurationList::const_iterator iter = selectedExceptions.constBegin(); iter != selectedExceptions.constEnd(); ++iter )
        { ui.exceptionListView->selectionModel()->select( model().index( *iter ), QItemSelectionModel::Select|QItemSelectionModel::Rows ); }

        setChanged( true );

        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::down( void )
    {

        ConfigurationList selection( model().get( ui.exceptionListView->selectionModel()->selectedRows() ) );
        if( selection.empty() )
        { return; }

        // retrieve selected indexes in list and store in model
        QModelIndexList selectedIndices( ui.exceptionListView->selectionModel()->selectedIndexes() );
        ConfigurationList selectedExceptions( model().get( selectedIndices ) );

        ConfigurationList currentExceptions( model().get() );
        ConfigurationList newExceptions;

        ConfigurationListIterator iter( currentExceptions );
        iter.toBack();
        while( iter.hasPrevious() )
        {

            ConfigurationPtr current( iter.previous() );

            // check if new list is not empty, current index is selected and last index is not.
            // if yes, move.
            if(
                !( newExceptions.empty() ||
                selectedIndices.indexOf( model().index( current ) ) == -1 ||
                selectedIndices.indexOf( model().index( newExceptions.front() ) ) != -1
                ) )
            {

                ConfigurationPtr first( newExceptions.front() );
                newExceptions.removeFirst();
                newExceptions.prepend( current );
                newExceptions.prepend( first );

            } else newExceptions.prepend( current );
        }

        model().set( newExceptions );

        // restore selection
        ui.exceptionListView->selectionModel()->select( model().index( selectedExceptions.front() ),  QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Rows );
        for( ConfigurationList::const_iterator iter = selectedExceptions.constBegin(); iter != selectedExceptions.constEnd(); ++iter )
        { ui.exceptionListView->selectionModel()->select( model().index( *iter ), QItemSelectionModel::Select|QItemSelectionModel::Rows ); }

        setChanged( true );

        return;

    }

    //_______________________________________________________
    void ExceptionListWidget::resizeColumns( void ) const
    {
        ui.exceptionListView->resizeColumnToContents( ExceptionModel::ENABLED );
        ui.exceptionListView->resizeColumnToContents( ExceptionModel::TYPE );
        ui.exceptionListView->resizeColumnToContents( ExceptionModel::REGEXP );
    }

    //_______________________________________________________
    bool ExceptionListWidget::checkException( ConfigurationPtr exception )
    {

        while( exception->exceptionPattern().isEmpty() || !QRegExp( exception->exceptionPattern() ).isValid() )
        {

            KMessageBox::error( this, i18n("Regular Expression syntax is incorrect") );
            QPointer<ExceptionDialog> dialog( new ExceptionDialog( this ) );
            dialog->setException( exception );
            if( dialog->exec() == QDialog::Rejected )
            {
                delete dialog;
                return false;
            }

            dialog->save();
            delete dialog;
        }

        return true;
    }

}
