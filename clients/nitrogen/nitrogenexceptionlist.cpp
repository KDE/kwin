// $Id: nitrogenexceptionlist.cpp,v 1.6 2009/06/24 01:43:14 hpereira Exp $

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
 * ANY WARRANTY;  without even the implied warranty of MERCHANTABILITY or         
 * FITNESS FOR A PARTICULAR PURPOSE.   See the GNU General Public License         
 * for more details.                    
 *                         
 * You should have received a copy of the GNU General Public License along with 
 * software; if not, write to the Free Software Foundation, Inc., 59 Temple     
 * Place, Suite 330, Boston, MA   02111-1307 USA                          
 *                        
 *                        
 *******************************************************************************/

#include <QTextStream>

#include "nitrogenexceptionlist.h"

namespace Nitrogen
{
  
  //______________________________________________________________
  void NitrogenExceptionList::read( const KConfig& config )
  {
    
    clear();

    for( int index = 0; true; index++ )
    { 
      
      KConfigGroup group( &config, exceptionGroupName( index ) );
      if( group.exists() ) 
      {
        NitrogenException exception( group ); 
        if( exception.regExp().isValid() ) push_back( exception );
        QTextStream( stdout ) << "NitrogenExceptionList::read - read exception " << exception.regExp().pattern() << endl;
      } else break;
      
    }

  }

  //______________________________________________________________
  void NitrogenExceptionList::write( KConfig& config )
  {
   
    // remove previous group
    for( int index = 0; true ;index++ )
    {
      KConfigGroup group( &config, exceptionGroupName( index ) );
      if( group.exists() ) group.deleteGroup();
      else break;
    }
    
    // also add exceptions
    int index(0);
    for( NitrogenExceptionList::const_iterator iter = constBegin(); iter != constEnd(); iter++, index++ )
    {
      
      KConfigGroup group( &config, exceptionGroupName( index ) );
      iter->write( group );
      
    }

    
  }
  
  //______________________________________________________________
  NitrogenExceptionList NitrogenExceptionList::defaultList( void )
  {
    
    NitrogenExceptionList out;
    
    // default exception that covers most commonly used gtk based applications
    NitrogenException exception;
    exception.setType( NitrogenException::WindowClassName );
    exception.regExp().setPattern( "(Firefox)|(Thunderbird)|(Gimp)" );
    exception.setBlendColor( NitrogenException::NoBlending );
    exception.setMask( NitrogenException::BlendColor );
    exception.setEnabled( true );
    
    out.push_back( exception );
    return out;
    
  }
    
  //_______________________________________________________________________
  QString NitrogenExceptionList::exceptionGroupName( int index )
  {
    QString out;
    QTextStream( &out ) << "Windeco Exception " << index;
    return out;
  }
    

}
