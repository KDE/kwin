#ifndef nitrogenconfiguration_h
#define nitrogenconfiguration_h

// $Id: nitrogenconfiguration.h,v 1.17 2009/07/05 20:47:44 hpereira Exp $

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

/*!
   \file nitrogenconfiguration.h
   \brief encapsulated window decoration configuration
   \author Hugo Pereira
   \version $Revision: 1.17 $
   \date $Date: 2009/07/05 20:47:44 $
*/

#include <KConfigGroup>

namespace NitrogenConfig
{
  
  static const QString TITLE_ALIGNMENT = "TitleAlignment";
  static const QString BUTTON_SIZE = "ButtonSize";
  static const QString BUTTON_TYPE = "ButtonType";
  static const QString SHOW_STRIPES = "ShowStripes";
  static const QString DRAW_SEPARATOR = "DrawSeparator";
  static const QString OVERWRITE_COLORS = "OverwriteColors";
  static const QString FRAME_BORDER = "FrameBorder";
  static const QString BLEND_COLOR = "BlendColor";
  static const QString DRAW_SIZE_GRIP = "DrawSizeGrip";
  static const QString USE_OXYGEN_SHADOWS = "UseOxygenShadows";
  
}

namespace Nitrogen
{

  class NitrogenConfiguration
  {
    
    public:

    //! button type
    enum ButtonType
    {
      ButtonKde42,
      ButtonKde43
    };
    
    //! button size enumeration
    enum ButtonSize
    {
      ButtonSmall = 18,
      ButtonDefault = 22,
      ButtonLarge = 32,
      ButtonHuge = 48
    };
    
    //! frame border enumeration
    enum FrameBorder
    {
      BorderNone = 0,
      BorderTiny = 1,
      BorderSmall = 3,
      BorderDefault = 5,
      BorderLarge = 8
    };

    //! blend color enumeration
    enum BlendColorType {
      NoBlending,
      RadialBlending
    };
    
    //! default constructor
    NitrogenConfiguration( void );
 
    //! constructor from KConfig
    NitrogenConfiguration( KConfigGroup );
    
    //! destructor
    virtual ~NitrogenConfiguration( void )
    {}
        
    //! equal to operator
    bool operator == ( const NitrogenConfiguration& ) const;
    
    //! true if compiz is used as a window decoration
    static bool useCompiz( void )
    { return useCompiz_; }
    
    //! check if compiz is used
    static bool checkUseCompiz( void );
    
    //! write to kconfig group
    virtual void write( KConfigGroup& ) const;
    
    //!@name title alignment
    //@{
    
    static QString titleAlignmentName( Qt::Alignment );
    static Qt::Alignment titleAlignment( QString );
    
    virtual Qt::Alignment titleAlignment() const
    { return titleAlignment_; }

    virtual QString titleAlignmentName() const
    { return titleAlignmentName( titleAlignment() ); }

    virtual void setTitleAlignment( Qt::Alignment value )
    { titleAlignment_ = value; }
    
    virtual void setTitleAlignment( QString value )
    { titleAlignment_ = titleAlignment( value ); }

    //@}
    
    //!@name button size
    //@{
    
    static QString buttonSizeName( ButtonSize );
    static ButtonSize buttonSize( QString );
    
    virtual ButtonSize buttonSize( void ) const
    { return buttonSize_; }
      
    virtual QString buttonSizeName( void ) const
    { return buttonSizeName( buttonSize() ); }
    
    virtual void setButtonSize( ButtonSize value )
    { buttonSize_ = value; }
    
    //@}
    
    //!@name button size
    //@{
    
    static QString buttonTypeName( ButtonType );
    static ButtonType buttonType( QString );
    
    virtual ButtonType buttonType( void ) const
    { return buttonType_; }
      
    virtual QString buttonTypeName( void ) const
    { return buttonTypeName( buttonType() ); }
    
    virtual void setButtonType( ButtonType value )
    { buttonType_ = value; }
    
    //@}
    
    //!@name frame border
    //@{
    
    static QString frameBorderName( FrameBorder );
    static FrameBorder frameBorder( QString );

    virtual FrameBorder frameBorder() const
    { return frameBorder_; }
    
    virtual QString frameBorderName( void ) const
    { return frameBorderName( frameBorder() ); }

    virtual void setFrameBorder( FrameBorder value )
    { frameBorder_ = value; }
        
    virtual void setFrameBorder( QString value )
    { frameBorder_ = frameBorder( value ); }

    //@}
    
    //!@name blend color
    //@{
    
    static QString blendColorName( BlendColorType );
    static BlendColorType blendColor( QString );
    
    virtual BlendColorType blendColor( void ) const
    { return blendColor_; }

    virtual QString blendColorName( void ) const
    { return blendColorName( blendColor() ); }

    virtual void setBlendColor( BlendColorType value )
    { blendColor_ = value; }    
    
    virtual void setBlendColor( QString value )
    { blendColor_ = blendColor( value ); }    

    //@}
        
    //! stripes
    virtual bool showStripes( void ) const
    { return showStripes_; }
    
    //! stripes
    virtual void setShowStripes( bool value ) 
    { showStripes_ = value; }
    
    //! separator
    virtual bool drawSeparator( void ) const
    { return drawSeparator_; }
    
    //! separator
    virtual void setDrawSeparator( bool value )
    { drawSeparator_ = value; }

    //! overwrite colors
    virtual bool overwriteColors( void ) const
    { return overwriteColors_; }

    //! overwrite colors
    virtual void setOverwriteColors( bool value )
    { overwriteColors_ = value; }
      
    //! draw size grip
    virtual bool drawSizeGrip( void ) const
    { return drawSizeGrip_; }
    
    //! draw size grip
    virtual void setDrawSizeGrip( bool value )
    { drawSizeGrip_ = value; }
    
    //! oxygen shadows
    virtual bool useOxygenShadows( void ) const
    { return useOxygenShadows_; }
    
    //! oxygen shadows
    virtual void setUseOxygenShadows( bool value )
    { useOxygenShadows_ = value; }
    
    private:
    
    //! title alignment 
    Qt::Alignment titleAlignment_;

    //! button size
    ButtonSize buttonSize_;
    
    //! button type
    ButtonType buttonType_;
    
    //! blend color
    FrameBorder frameBorder_; 

    //! frame border
    BlendColorType blendColor_;
    
    //! stripes
    bool showStripes_;
    
    //! separator
    bool drawSeparator_;
    
    //! overwrite colors
    bool overwriteColors_;
    
    //! size grip
    bool drawSizeGrip_;
    
    //! oxygen shadows
    bool useOxygenShadows_;
    
    //! compiz
    static bool useCompiz_;
      
    
  };
  
};

#endif
