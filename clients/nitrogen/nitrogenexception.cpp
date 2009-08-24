// $Id: nitrogenexception.cpp,v 1.11 2009/06/21 19:57:19 hpereira Exp $

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

#include <assert.h>

#include "nitrogenexception.h"

namespace Nitrogen
{
  
  //_______________________________________________________
  NitrogenException::NitrogenException( KConfigGroup group ):
    NitrogenConfiguration( group )
  {

    // used to set default values when entries are not found in kconfig
    NitrogenException default_configuration;

    // exception type
    setType( type( 
      group.readEntry( NitrogenConfig::TYPE, 
      default_configuration.typeName() ) ) );
    
    // exception pattern
    regExp().setPattern( group.readEntry( NitrogenConfig::PATTERN, QString() ) );
    
    // enability
    setEnabled(
      group.readEntry( NitrogenConfig::ENABLED,
      default_configuration.enabled() ) );
    
    // exception mask
    setMask( 
      group.readEntry( NitrogenConfig::MASK, 
      default_configuration.mask() ) );
    
  }
  
  //_______________________________________________________
  void NitrogenException::write( KConfigGroup& group ) const
  {
    
    NitrogenConfiguration::write( group );
    group.writeEntry( NitrogenConfig::TYPE, typeName() );
    group.writeEntry( NitrogenConfig::PATTERN, regExp().pattern() );
    group.writeEntry( NitrogenConfig::ENABLED, enabled() );
    group.writeEntry( NitrogenConfig::MASK, mask() );
    
  }
  
  //_______________________________________________________
  QString NitrogenException::typeName( Type type )
  {
    switch( type )
    {
      case WindowTitle: return "Window Title";
      case WindowClassName: return "Window Class Name";
      default: assert( false );
    }
    
    return QString();
  }
  
  //_______________________________________________________
  NitrogenException::Type NitrogenException::type( QString value )
  {
    if( value == "Window Title" ) return WindowTitle;
    else if( value == "Window Class Name" ) return WindowClassName;
    else return WindowClassName;
  }
  
}
