#ifndef nitrogenconfigurationui_h
#define nitrogenconfigurationui_h

// $Id: nitrogenconfigurationui.h,v 1.11 2009/07/05 20:45:40 hpereira Exp $

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
   \file nitrogenconfigurationui.h
   \brief window decoration configuration user interface
   \author Hugo Pereira
   \version $Revision: 1.11 $
   \date $Date: 2009/07/05 20:45:40 $
*/

#include <kdeversion.h>
#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>

namespace Nitrogen
{

  //_____________________________________________
  class NitrogenConfigurationUI: public QWidget
  {
    
    Q_OBJECT
    
    public:

    //! constructor
    NitrogenConfigurationUI( QWidget* );
    
    //! setup ui
    void setupUI( void );
    
    //! title alignment
    QComboBox *titleAlignment;

    //! button size
    QComboBox* buttonSize;

    //! button size
    QComboBox* buttonType;
     
    //! frame border
    QComboBox *frameBorder;

    //! blend color
    QComboBox *blendColor;

    //! draw separator
    QCheckBox *drawSeparator;
    
    //! show stripes
    QCheckBox *showStripes;
        
    //! overwrite colors
    QCheckBox *overwriteColors;

    //! size grip
    QCheckBox *drawSizeGrip;

    //! nitrogen shadow
    QCheckBox *useOxygenShadows;
    
    //! about nitrogen
    QPushButton *aboutNitrogen;
    
    //! raise exceptions
    QPushButton *showExceptions;

  };
  
};

#endif
