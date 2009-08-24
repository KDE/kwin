#ifndef nitrogenexceptiondialog_h
#define nitrogenexceptiondialog_h

// $Id: nitrogenexceptiondialog.h,v 1.6 2009/07/05 16:15:31 hpereira Exp $

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

#include <KDialog>
#include <KLineEdit>
#include <QComboBox>
#include <QCheckBox>

#include <map>

#include "../nitrogenexception.h"

namespace Nitrogen
{
  
  //! nitrogen exceptions list
  class NitrogenExceptionDialog: public KDialog
  {
    
    public:
    
    //! constructor
    NitrogenExceptionDialog( QWidget* parent );

    //! set exception
    void setException( NitrogenException );
    
    //! get exception
    NitrogenException exception( void ) const;
    
    private:
    
    //! line editor
    KLineEdit* editor_;
    
    //! blend combobox
    QComboBox* type_combobox_;

    //! border size
    QComboBox* frame_border_combobox_;
    
    //! blend combobox
    QComboBox* blend_combobox_;
    
    //! map mask and checkbox
    typedef std::map< NitrogenException::AttributesMask, QCheckBox*> CheckBoxMap;
    
    //! map mask and checkbox
    CheckBoxMap checkboxes_;
    
    //! internal exception
    NitrogenException exception_;
    
    //! local combobox to handle configuration checkboxes
    class ComboBox: public QComboBox
    {
      public: 
      
      static const QString Default;
      static const QString Yes;
      static const QString No;
      
      //! constructor
      ComboBox( QWidget* parent );
      
      //! set value
      void setValue( bool enabled, bool checked );
      
      //! true if default is selected
      bool isDefault( void ) const;
      
      //! true if yes is checkd
      bool isChecked( void ) const;
      
      
    };
    
    //! draw separator
    ComboBox* draw_separator_combobox_;
    
    //! stripes
    ComboBox* show_stripes_combobox_;
    
    //! overwrite colors
    ComboBox* overwrite_colors_combobox_;
    
    //! size grip
    ComboBox* draw_size_grip_combobox_;
  
  };
  
}

#endif
