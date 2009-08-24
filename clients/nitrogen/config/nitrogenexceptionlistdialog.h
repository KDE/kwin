#ifndef _NitrogenExceptionListDialog_h_
#define _NitrogenExceptionListDialog_h_

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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License        
* for more details.                     
*                          
* You should have received a copy of the GNU General Public License along with 
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     
* Place, Suite 330, Boston, MA  02111-1307 USA                           
*                         
*                         
*******************************************************************************/
 
/*!
  \file NitrogenExceptionListDialog.h
  \brief Generic dialog with a FileInfo list
  \author Hugo Pereira
  \version $Revision: 1.5 $
  \date $Date: 2009/04/02 03:48:34 $
*/

#include <KPushButton>
#include <QTreeView>
#include <KDialog>

#include "nitrogenexceptionmodel.h"
#include "../nitrogenexceptionlist.h"

//! QDialog used to commit selected files
namespace Nitrogen
{
  
  class NitrogenExceptionListDialog: public KDialog
  {
    
    //! Qt meta object
    Q_OBJECT
      
    public:
      
    //! constructor
    NitrogenExceptionListDialog( QWidget*, NitrogenConfiguration default_configuration = NitrogenConfiguration() );
 
    //! set exceptions
    void setExceptions( const NitrogenExceptionList& );
    
    //! get exceptions
    NitrogenExceptionList exceptions( void ) const;
       
    protected:
  
    //! list
    QTreeView& _list() const
    { return *list_; }
  
    //! model
    const NitrogenExceptionModel& _model() const
    { return model_; }
  
    //! model
    NitrogenExceptionModel& _model()
    { return model_; }
    
    protected slots:
    
    //! update button states
    virtual void _updateButtons( void );
    
    //! add
    virtual void _add( void );
    
    //! edit
    virtual void _edit( void );
    
    //! remove 
    virtual void _remove( void );
    
    //! toggle
    virtual void _toggle( const QModelIndex& );
    
    //! move up
    virtual void _up( void );
    
    //! move down
    virtual void _down( void );
    
    private:
    
    //! resize columns
    void _resizeColumns( void ) const;
    
    //! check exception
    bool _checkException( NitrogenException& );
    
    //! default configuration
    NitrogenConfiguration default_configuration_;
    
    //! list of files
    QTreeView* list_;
    
    //! model
    NitrogenExceptionModel model_;
    
    //! add
    KPushButton* add_button_;
    
    //! edit
    KPushButton* edit_button_;

    //! remove
    KPushButton* remove_button_;
    
    //! move up
    KPushButton* up_button_;
    
    //! move down
    KPushButton* down_button_;
    
  };
  
}

#endif
  
