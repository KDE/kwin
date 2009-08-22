#ifndef nitrogenexception_h
#define nitrogenexception_h

// $Id: nitrogenexception.h,v 1.12 2009/06/24 01:43:14 hpereira Exp $

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

#include <QRegExp>
#include "nitrogenconfiguration.h"


namespace NitrogenConfig
{
  
  //! needed for exceptions
  static const QString TYPE = "Type";
  static const QString PATTERN = "Pattern";
  static const QString ENABLED = "Enabled";
  static const QString MASK = "Mask";
    
};

namespace Nitrogen
{
  
  //! nitrogen exception
  class NitrogenException: public NitrogenConfiguration
  {
    
    public:
    
    //! exception type
    enum Type
    {
      WindowTitle,
      WindowClassName
    };
    
    //! mask
    enum AttributesMask
    {
      None = 0,
      TitleAlignment = 1<<0,
      ShowStripes = 1<<1,
      DrawSeparator = 1<<2,
      OverwriteColors = 1<<3,
      FrameBorder = 1<<4,
      BlendColor = 1<<5,
      DrawSizeGrip = 1<<6,
      All = TitleAlignment|ShowStripes|DrawSeparator|OverwriteColors|FrameBorder|BlendColor|DrawSizeGrip
    };
    
    //! constructor
    NitrogenException( NitrogenConfiguration configuration = NitrogenConfiguration() ):
      NitrogenConfiguration( configuration ),
      enabled_( true ),
      type_( WindowClassName ),
      mask_( None )
    {}

    //! constructor
    NitrogenException( KConfigGroup );
    
    //! destructor
    virtual ~NitrogenException( void )
    {}
    
    //! equal to operator
    bool operator == (const NitrogenException& exception ) const
    { 
      return
        enabled() == exception.enabled() &&
        type() == exception.type() &&
        regExp().pattern() == exception.regExp().pattern() &&
        mask() == exception.mask() &&
        NitrogenConfiguration::operator == ( exception );
    }

    //! less than operator
    bool operator < (const NitrogenException& exception ) const
    { 
      if( enabled() != exception.enabled() ) return enabled() < exception.enabled();
      else if( mask() != exception.mask() ) return mask() < exception.mask();
      else if( type() != exception.type() ) return type() < exception.type();
      else return regExp().pattern() < exception.regExp().pattern(); 
    }
     
    //! write to kconfig group
    virtual void write( KConfigGroup& ) const;
    
    //!@name enability
    //@{
    
    bool enabled( void ) const
    { return enabled_; }
    
    void setEnabled( bool enabled ) 
    { enabled_ = enabled; }
    
    //@}
    
    //!@name exception type
    //@{
    
    static QString typeName( Type );
    static Type type( QString name );

    virtual QString typeName( void ) const
    { return typeName( type() ); }
    
    virtual Type type( void ) const
    { return type_; }
    
    virtual void setType( Type value )
    { type_ = value; }

    //@}
    
    //!@name regular expression
    //@{

    virtual QRegExp regExp( void ) const
    { return reg_exp_; }

    virtual QRegExp& regExp( void ) 
    { return reg_exp_; }

    //@}
    
    
    //! mask
    //!@{
    
    unsigned int mask( void ) const
    { return mask_; }
    
    void setMask( unsigned int mask )
    { mask_ = mask; }
    
    //@}
        
    private:
    
    //! enability
    bool enabled_;
    
    //! exception type
    Type type_;
    
    //! regular expression to match window caption
    QRegExp reg_exp_;

    //! attributes mask
    unsigned int mask_;
    
  };
  
  
  
};

#endif
