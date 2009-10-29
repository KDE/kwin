//////////////////////////////////////////////////////////////////////////////
// oxygenexceptionmodel.cpp
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

#include "oxygenexceptionmodel.h"
#include <KLocale>
namespace Oxygen
{

  //_______________________________________________
  const QString OxygenExceptionModel::column_titles_[ OxygenExceptionModel::n_columns ] =
  {
    "",
    i18n("Exception Type"),
    i18n("Regular Expression")
  };

  //__________________________________________________________________
  QVariant OxygenExceptionModel::data( const QModelIndex& index, int role ) const
  {

    // check index, role and column
    if( !index.isValid() ) return QVariant();

    // retrieve associated file info
    const OxygenException& exception( get(index) );

    // return text associated to file and column
    if( role == Qt::DisplayRole )
    {

      switch( index.column() )
      {
        case TYPE: return exception.typeName( true );
        case REGEXP: return exception.regExp().pattern();
        default: return QVariant();
        break;
      }

    } else if( role == Qt::CheckStateRole &&  index.column() == ENABLED ) {

      return exception.enabled() ? Qt::Checked : Qt::Unchecked;

    } else if( role == Qt::ToolTipRole &&  index.column() == ENABLED ) {

      return i18n("Enable/disable this exception");

    }


    return QVariant();
  }

  //__________________________________________________________________
  QVariant OxygenExceptionModel::headerData(int section, Qt::Orientation orientation, int role) const
  {

    if(
      orientation == Qt::Horizontal &&
      role == Qt::DisplayRole &&
      section >= 0 &&
      section < n_columns )
    { return column_titles_[section]; }

    // return empty
    return QVariant();

  }

}
