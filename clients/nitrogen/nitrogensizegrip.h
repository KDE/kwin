#ifndef nitrogensizegrip_h
#define nitrogensizegrip_h

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

#include <QWidget>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QMouseEvent>

namespace Nitrogen
{

  class NitrogenClient;

  //! implements size grip for all widgets
  class NitrogenSizeGrip: public QWidget
  {
    
    public: 
    
    //! constructor
    NitrogenSizeGrip( NitrogenClient* );
    
    //! constructor
    virtual ~NitrogenSizeGrip( void );
    
    //! event filter
    virtual bool eventFilter( QObject*, QEvent* );

    public slots:
    
    //! update background color
    void updateBackgroundColor( void );

    protected slots:
    
    //! embed into parent widget
    void embed( void );
    
    protected:
    
    //!@name event handlers
    //@{
    
    //! paint
    virtual void paintEvent( QPaintEvent* );
    
    //! mouse press
    virtual void mousePressEvent( QMouseEvent* );
   
    //@}
        
    //! client
    NitrogenClient& client( void ) const
    { return *client_; }
    
    //! update position
    void updatePosition( void );
    
    private:
    
    //! grip size
    enum { 
      OFFSET = 1,
      GRIP_SIZE = 15 
    };
      
    // nitrogen client
    NitrogenClient* client_;
    
    //! true when decoration offset must be accounted for when moving the widget
    bool decoration_offset_;
        
  };
  
  
}

#endif
