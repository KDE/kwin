// $Id: itemmodel.cpp,v 1.1 2009/03/25 17:44:24 hpereira Exp $

/******************************************************************************
 *                         
 * Copyright (C) 2002 Hugo PEREIRA <mailto: hugo.pereira@free.fr>             
 *                         
 * This is free software; you can redistribute it and/or modify it under the    
 * terms of the GNU General Public License as published by the Free Software    
 * Foundation; either version 2 of the License, or (at your option) any later   
 * version.                             
 *                          
 * This software is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        
 * for more details.                     
 *                          
 * You should have received a copy of the GNU General Public License along with 
 * software; if not, write to the Free Software Foundation, Inc., 59 Temple     
 * Place, Suite 330, Boston, MA  02111-1307 USA                           
 *                         
 *                         
 *******************************************************************************/

/*!
  \file    ItemModel.h
  \brief   Job model. Stores job information for display in lists
  \author  Hugo Pereira
  \version $Revision: 1.1 $
  \date    $Date: 2009/03/25 17:44:24 $
*/

#include "itemmodel.h"

using namespace std;

//_______________________________________________________________
ItemModel::ItemModel( QObject* parent ):
  QAbstractItemModel( parent ),
  sort_column_(0),
  sort_order_( Qt::AscendingOrder )
{}

//____________________________________________________________
void ItemModel::sort( int column, Qt::SortOrder order )
{ 
  
  // store column and order
  sort_column_ = column;
  sort_order_ = order;
  
  // emit signals and call private methods
  emit layoutAboutToBeChanged();
  _sort( column, order );
  emit layoutChanged();
  
}

//____________________________________________________________
QModelIndexList ItemModel::indexes( int column, const QModelIndex& parent ) const
{ 
  QModelIndexList out;
  int rows( rowCount( parent ) );
  for( int row = 0; row < rows; row++ )
  { 
    QModelIndex index( this->index( row, column, parent ) );
    if( !index.isValid() ) continue;
    out.push_back( index );
    out += indexes( column, index );
  }
  
  return out;
  
}
