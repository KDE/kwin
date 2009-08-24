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
  \file NitrogenExceptionModel.cpp
  \brief model for object exceptions
  \author Hugo Pereira
  \version $Revision: 1.10 $
  \date $Date: 2009/05/25 21:05:19 $
*/

#include "nitrogenexceptionmodel.h"
#include <KLocale>
namespace Nitrogen
{
  
  //_______________________________________________
  const QString NitrogenExceptionModel::column_titles_[ NitrogenExceptionModel::n_columns ] =
  { 
    "",
    i18n("Exception Type"),
    i18n("Regular Expression")
  };
    
  //__________________________________________________________________
  QVariant NitrogenExceptionModel::data( const QModelIndex& index, int role ) const
  {
  
    // check index, role and column
    if( !index.isValid() ) return QVariant();
    
    // retrieve associated file info
    const NitrogenException& exception( get(index) );
  
    // return text associated to file and column
    if( role == Qt::DisplayRole ) 
    {
      
      switch( index.column() )
      {
        case TYPE: return exception.typeName();
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
  QVariant NitrogenExceptionModel::headerData(int section, Qt::Orientation orientation, int role) const
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
