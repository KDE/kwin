//////////////////////////////////////////////////////////////////////////////
// OxygenExceptionListWidget.cpp
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

#include <QLayout>
#include <KLocale>
#include <KMessageBox>

#include "oxygenexceptiondialog.h"
#include "oxygenexceptionlistwidget.h"
#include "oxygenexceptionlistwidget.moc"

//__________________________________________________________
namespace Oxygen
{

  //__________________________________________________________
  OxygenExceptionListWidget::OxygenExceptionListWidget( QWidget* parent, OxygenConfiguration default_configuration ):
    QWidget( parent ),
    default_configuration_( default_configuration )
  {

    // layout
    QHBoxLayout* h_layout = new QHBoxLayout();
    h_layout->setMargin(6);
    h_layout->setSpacing(6);
    setLayout( h_layout );

    // list
    h_layout->addWidget( list_ = new QTreeView( this ) );
    _list().setAllColumnsShowFocus( true );
    _list().setRootIsDecorated( false );
    _list().setSortingEnabled( false );
    _list().setModel( &_model() );
    _list().sortByColumn( OxygenExceptionModel::TYPE );
    _list().setSizePolicy( QSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Ignored ) );

    // button layout
    QVBoxLayout* v_layout = new QVBoxLayout();
    v_layout->setMargin(0);
    v_layout->setSpacing(5);
    h_layout->addLayout( v_layout );
    KIconLoader* icon_loader = KIconLoader::global();

    v_layout->addWidget( up_button_ = new KPushButton(
      KIcon( "arrow-up", icon_loader ),
      i18n("Move &Up"), this ) );

    v_layout->addWidget( down_button_ = new KPushButton(
      KIcon( "arrow-down", icon_loader ),
      i18n("Move &Down"), this ) );

    v_layout->addWidget( add_button_ = new KPushButton(
      KIcon( "list-add", icon_loader ),
      i18n("&Add"), this ) );

    v_layout->addWidget( remove_button_ = new KPushButton(
      KIcon( "list-remove", icon_loader ),
      i18n("&Remove"), this ) );

    v_layout->addWidget( edit_button_ = new KPushButton(
      KIcon( "edit-rename", icon_loader ),
      i18n("&Edit"), this ) );

    v_layout->addStretch();

    connect( add_button_, SIGNAL( clicked() ), SLOT( _add() ) );
    connect( edit_button_, SIGNAL( clicked() ), SLOT( _edit() ) );
    connect( remove_button_, SIGNAL( clicked() ), SLOT( _remove() ) );
    connect( up_button_, SIGNAL( clicked() ), SLOT( _up() ) );
    connect( down_button_, SIGNAL( clicked() ), SLOT( _down() ) );

    connect( &_list(), SIGNAL( activated( const QModelIndex& ) ), SLOT( _edit() ) );
    connect( &_list(), SIGNAL( clicked( const QModelIndex& ) ), SLOT( _toggle( const QModelIndex& ) ) );
    connect( _list().selectionModel(), SIGNAL( selectionChanged(const QItemSelection &, const QItemSelection &) ), SLOT( _updateButtons() ) );

    _updateButtons();
    _resizeColumns();


  }

  //__________________________________________________________
  void OxygenExceptionListWidget::setExceptions( const OxygenExceptionList& exceptions )
  {
    _model().set( OxygenExceptionModel::List( exceptions.begin(), exceptions.end() ) );
    _resizeColumns();
  }

  //__________________________________________________________
  OxygenExceptionList OxygenExceptionListWidget::exceptions( void ) const
  {

    OxygenExceptionModel::List exceptions( _model().get() );
    OxygenExceptionList out;
    for( OxygenExceptionModel::List::const_iterator iter = exceptions.begin(); iter != exceptions.end(); iter++ )
    { out.push_back( *iter ); }
    return out;

  }

  //__________________________________________________________
  void OxygenExceptionListWidget::_updateButtons( void )
  {

    bool has_selection( !_list().selectionModel()->selectedRows().empty() );
    remove_button_->setEnabled( has_selection );
    edit_button_->setEnabled( has_selection );

    up_button_->setEnabled( has_selection && !_list().selectionModel()->isRowSelected( 0, QModelIndex() ) );
    down_button_->setEnabled( has_selection && !_list().selectionModel()->isRowSelected( _model().rowCount()-1, QModelIndex() ) );

  }


  //_______________________________________________________
  void OxygenExceptionListWidget::_add( void )
  {

    // map dialog
    OxygenExceptionDialog dialog( this );
    dialog.setException( default_configuration_ );
    if( dialog.exec() == QDialog::Rejected ) return;

    // retrieve exception and check
    OxygenException exception( dialog.exception() );
    if( !_checkException( exception ) ) return;

    // create new item
    _model().add( exception );

    // make sure item is selected
    QModelIndex index( _model().index( exception ) );
    if( index != _list().selectionModel()->currentIndex() )
    {
      _list().selectionModel()->select( index,  QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Rows );
      _list().selectionModel()->setCurrentIndex( index,  QItemSelectionModel::Current|QItemSelectionModel::Rows );
    }

    _resizeColumns();
    emit changed();
    return;

  }

  //_______________________________________________________
  void OxygenExceptionListWidget::_edit( void )
  {

    // retrieve selection
    QModelIndex current( _list().selectionModel()->currentIndex() );
    if( !current.isValid() ) return;

    OxygenException& exception( _model().get( current ) );

    // create dialog
    OxygenExceptionDialog dialog( this );
    dialog.setException( exception );

    // map dialog
    if( dialog.exec() == QDialog::Rejected ) return;
    OxygenException new_exception = dialog.exception();

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
  void OxygenExceptionListWidget::_remove( void )
  {

    // shoud use a konfirmation dialog
    if( KMessageBox::questionYesNo( this, i18n("Remove selected exception ?") ) == KMessageBox::No ) return;

    // remove
    _model().remove( _model().get( _list().selectionModel()->selectedRows() ) );
    _resizeColumns();
    emit changed();
    return;

  }

  //_______________________________________________________
  void OxygenExceptionListWidget::_toggle( const QModelIndex& index )
  {

    if( !index.isValid() ) return;
    if( index.column() != OxygenExceptionModel::ENABLED ) return;

    // get matching exception
    OxygenException& exception( _model().get( index ) );
    exception.setEnabled( !exception.enabled() );
    _model().add( exception );

    emit changed();
    return;

  }

  //_______________________________________________________
  void OxygenExceptionListWidget::_up( void )
  {

    OxygenExceptionModel::List selection( _model().get( _list().selectionModel()->selectedRows() ) );
    if( selection.empty() ) { return; }

    // retrieve selected indexes in list and store in model
    QModelIndexList selected_indexes( _list().selectionModel()->selectedRows() );
    OxygenExceptionModel::List selected_exceptions( _model().get( selected_indexes ) );

    OxygenExceptionModel::List current_exceptions( _model().get() );
    OxygenExceptionModel::List new_exceptions;

    for( OxygenExceptionModel::List::const_iterator iter = current_exceptions.begin(); iter != current_exceptions.end(); iter++ )
    {

      // check if new list is not empty, current index is selected and last index is not.
      // if yes, move.
      if(
        !( new_exceptions.empty() ||
        selected_indexes.indexOf( _model().index( *iter ) ) == -1 ||
        selected_indexes.indexOf( _model().index( new_exceptions.back() ) ) != -1
        ) )
      {
        OxygenException last( new_exceptions.back() );
        new_exceptions.pop_back();
        new_exceptions.push_back( *iter );
        new_exceptions.push_back( last );
      } else new_exceptions.push_back( *iter );

    }

    _model().set( new_exceptions );

    // restore selection
    _list().selectionModel()->select( _model().index( selected_exceptions.front() ),  QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Rows );
    for( OxygenExceptionModel::List::const_iterator iter = selected_exceptions.begin(); iter != selected_exceptions.end(); iter++ )
    { _list().selectionModel()->select( _model().index( *iter ), QItemSelectionModel::Select|QItemSelectionModel::Rows ); }

    emit changed();
    return;

  }

  //_______________________________________________________
  void OxygenExceptionListWidget::_down( void )
  {

    OxygenExceptionModel::List selection( _model().get( _list().selectionModel()->selectedRows() ) );
    if( selection.empty() )
    { return; }

    // retrieve selected indexes in list and store in model
    QModelIndexList selected_indexes( _list().selectionModel()->selectedIndexes() );
    OxygenExceptionModel::List selected_exceptions( _model().get( selected_indexes ) );

    OxygenExceptionModel::List current_exceptions( _model().get() );
    OxygenExceptionModel::List new_exceptions;

    for( OxygenExceptionModel::List::reverse_iterator iter = current_exceptions.rbegin(); iter != current_exceptions.rend(); iter++ )
    {

      // check if new list is not empty, current index is selected and last index is not.
      // if yes, move.
      if(
        !( new_exceptions.empty() ||
        selected_indexes.indexOf( _model().index( *iter ) ) == -1 ||
        selected_indexes.indexOf( _model().index( new_exceptions.back() ) ) != -1
        ) )
      {

        OxygenException last( new_exceptions.back() );
        new_exceptions.pop_back();
        new_exceptions.push_back( *iter );
        new_exceptions.push_back( last );

      } else new_exceptions.push_back( *iter );
    }

    _model().set( OxygenExceptionModel::List( new_exceptions.rbegin(), new_exceptions.rend() ) );

    // restore selection
    _list().selectionModel()->select( _model().index( selected_exceptions.front() ),  QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Rows );
    for( OxygenExceptionModel::List::const_iterator iter = selected_exceptions.begin(); iter != selected_exceptions.end(); iter++ )
    { _list().selectionModel()->select( _model().index( *iter ), QItemSelectionModel::Select|QItemSelectionModel::Rows ); }

    emit changed();
    return;

  }

  //_______________________________________________________
  void OxygenExceptionListWidget::_resizeColumns( void ) const
  {
    _list().resizeColumnToContents( OxygenExceptionModel::ENABLED );
    _list().resizeColumnToContents( OxygenExceptionModel::TYPE );
    _list().resizeColumnToContents( OxygenExceptionModel::REGEXP );
  }

  //_______________________________________________________
  bool OxygenExceptionListWidget::_checkException( OxygenException& exception )
  {

    while( !exception.regExp().isValid() )
    {

      KMessageBox::error( this, i18n("Regular Expression syntax is incorrect") );
      OxygenExceptionDialog dialog( this );
      dialog.setException( exception );
      if( dialog.exec() == QDialog::Rejected ) return false;
      exception = dialog.exception();

    }

    return true;
  }

}
