#ifndef ListModel_h
#define ListModel_h
//////////////////////////////////////////////////////////////////////////////
// listmodel.h
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

#include <algorithm>
#include <assert.h>
#include <set>
#include <vector>

#include "oxygenitemmodel.h"

namespace Oxygen
{
  //! Job model. Stores job information for display in lists
  template<class T> class ListModel : public ItemModel
  {

    public:

    //! value type
    typedef T ValueType;

    //! reference
    typedef T& Reference;

    //! pointer
    typedef T* Pointer;

    //! list of vector
    typedef std::vector<ValueType> List;

    //! list of vector
    typedef std::set<ValueType> Set;

    //! constructor
    ListModel(QObject *parent = 0):
      ItemModel( parent )
      {}

    //! destructor
    virtual ~ListModel()
    {}

    //!@name methods reimplemented from base class
    //@{

    //! flags
    virtual Qt::ItemFlags flags(const QModelIndex &index) const
    {
      if (!index.isValid()) return 0;
      return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    //! unique index for given row, column and parent index
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const
    {

      // check if index is valid
      if( !hasIndex( row, column, parent ) ) return QModelIndex();

      // return invalid index if parent is valid
      if( parent.isValid() ) return QModelIndex();

      // check against values_
      return ( row < (int) values_.size() ) ? createIndex( row, column ):QModelIndex();

    }

    //! index of parent
    virtual QModelIndex parent(const QModelIndex &) const
    { return QModelIndex(); }

    //! number of rows below given index
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const
    { return parent.isValid() ? 0:values_.size(); }

    //@}

    //!@name selection
    //@{

    //! clear internal list selected items
    virtual void clearSelectedIndexes( void )
    { selection_.clear(); }

    //! store index internal selection state
    virtual void setIndexSelected( const QModelIndex& index, bool value )
    {
      if( value ) selection_.push_back( get(index) );
      else selection_.erase( std::remove( selection_.begin(), selection_.end(), get(index) ), selection_.end() );
    }

    //! get list of internal selected items
    virtual QModelIndexList selectedIndexes( void ) const
    {

      QModelIndexList out;
      for( typename List::const_iterator iter = selection_.begin(); iter != selection_.end(); iter++ )
      {
        QModelIndex index( ListModel::index( *iter ) );
        if( index.isValid() ) out.push_back( index );
      }
      return out;

    }

    //@}

    //!@name interface
    //@{

    //! add value
    virtual void add( const ValueType& value )
    {

      emit layoutAboutToBeChanged();
      _add( value );
      _sort();
      emit layoutChanged();

    }

    //! add values
    virtual void add( const List& values )
    {

      // check if not empty
      // this avoids sending useless signals
      if( values.empty() ) return;

      emit layoutAboutToBeChanged();

      for( typename List::const_iterator iter = values.begin(); iter != values.end(); iter++ )
      { _add( *iter ); }

      _sort();
      emit layoutChanged();

    }


    //! add values
    /*! this method uses a Set to add the values. It speeds up the updating of existing values */
    virtual void add( Set values )
    {

      emit layoutAboutToBeChanged();

      for( typename List::iterator iter = values_.begin(); iter != values_.end(); iter++ )
      {
        // see if current iterator is found in values set
        typename Set::iterator found_iter( values.find( *iter ) );
        if( found_iter != values.end() )
        {
          *iter = *found_iter;
          values.erase( found_iter );
        }
      }

      // insert remaining values at the end
      values_.insert( values_.end(), values.begin(), values.end() );

      _sort();
      emit layoutChanged();

    }


    //! insert values
    virtual void insert( const QModelIndex& index, const ValueType& value )
    {
      emit layoutAboutToBeChanged();
      _insert( index, value );
      emit layoutChanged();
    }

    //! insert values
    virtual void insert( const QModelIndex& index, const List& values )
    {
      emit layoutAboutToBeChanged();

      // need to loop in reverse order so that the "values" ordering is preserved
      for( typename List::const_reverse_iterator iter = values.rbegin(); iter != values.rend(); iter++ )
        _insert( index, *iter );
      emit layoutChanged();
    }

    //! insert values
    virtual void replace( const QModelIndex& index, const ValueType& value )
    {
      if( !index.isValid() ) add( value );
      else {
        emit layoutAboutToBeChanged();
        setIndexSelected( index, false );
        values_[index.row()] = value;
        setIndexSelected( index, true );
        emit layoutChanged();
      }
    }

    //! remove
    virtual void remove( const ValueType& value )
    {

      emit layoutAboutToBeChanged();
      _remove( value );
      emit layoutChanged();
      return;

    }

    //! remove
    virtual void remove( const List& values )
    {

      // check if not empty
      // this avoids sending useless signals
      if( values.empty() ) return;

      emit layoutAboutToBeChanged();
      for( typename List::const_iterator iter = values.begin(); iter != values.end(); iter++ )
      { _remove( *iter ); }
      emit layoutChanged();
      return;

    }

    //! clear
    virtual void clear( void )
    { set( List() ); }

    //! update values from list
    /*!
    values that are not found in current are removed
    new values are set to the end.
    This is slower than the "set" method, but the selection is not cleared in the process
    */
    virtual void update( List values )
    {

      emit layoutAboutToBeChanged();

      // store values to be removed
      List removed_values;

      // update values that are common to both lists
      for( typename List::iterator iter = values_.begin(); iter != values_.end(); iter++ )
      {

        // see if iterator is in list
        typename List::iterator found_iter( std::find( values.begin(), values.end(), *iter ) );
        if( found_iter == values.end() ) removed_values.push_back( *iter );
        else {
          *iter = *found_iter;
          values.erase( found_iter );
        }

      }

      // remove values that have not been found in new list
      for( typename List::const_iterator iter = removed_values.begin(); iter != removed_values.end(); iter++ )
      { _remove( *iter ); }

      // add remaining values
      for( typename List::const_iterator iter = values.begin(); iter != values.end(); iter++ )
      { _add( *iter ); }

      _sort();
      emit layoutChanged();

    }

    //! set all values
    virtual void set( const List& values )
    {

      emit layoutAboutToBeChanged();
      values_ = values;
      selection_.clear();
      _sort();
      emit layoutChanged();

      return;
    }

    //! return all values
    const List& get( void ) const
    { return values_; }

    //! return value for given index
    virtual ValueType get( const QModelIndex& index ) const
    { return (index.isValid() && index.row() < int(values_.size()) ) ? values_[index.row()]:ValueType(); }

    //! return value for given index
    virtual ValueType& get( const QModelIndex& index )
    {
      assert( index.isValid() && index.row() < int( values_.size() ) );
      return values_[index.row()];
    }

    //! return all values
    List get( const QModelIndexList& indexes ) const
    {
      List out;
      for( QModelIndexList::const_iterator iter = indexes.begin(); iter != indexes.end(); iter++ )
      { if( iter->isValid() && iter->row() < int(values_.size()) ) out.push_back( get( *iter ) ); }
      return out;
    }

    //! return index associated to a given value
    virtual QModelIndex index( const ValueType& value, int column = 0 ) const
    {
      for( unsigned int row=0; row<values_.size(); row++ )
      { if( value == values_[row] ) return index( row, column ); }
      return QModelIndex();
    }

    //@}

    protected:

    //! return all values
    List& _get( void )
    { return values_; }

    //! add, without update
    virtual void _add( const ValueType& value )
    {
      typename List::iterator iter = std::find( values_.begin(), values_.end(), value );
      if( iter == values_.end() ) values_.push_back( value );
      else *iter = value;
    }

    //! add, without update
    virtual void _insert( const QModelIndex& index, const ValueType& value )
    {
      if( !index.isValid() ) add( value );
      int row = 0;
      typename List::iterator iter( values_.begin() );
      for( ;iter != values_.end() && row != index.row(); iter++, row++ ){}
      values_.insert( iter, value );
    }

    //! remove, without update
    virtual void _remove( const ValueType& value )
    {
      values_.erase( std::remove( values_.begin(), values_.end(), value ), values_.end() );
      selection_.erase( std::remove( selection_.begin(), selection_.end(), value ), selection_.end() );
    }

    private:

    //! values
    List values_;

    //! selection
    List selection_;

  };
}
#endif
