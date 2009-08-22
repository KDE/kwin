#ifndef nitrogenbutton_h
#define nitrogenbutton_h

// $Id: nitrogenbutton.h,v 1.8 2009/06/20 23:13:23 hpereira Exp $

//////////////////////////////////////////////////////////////////////////////
// Nitrogenbutton.h
// -------------------
// Nitrogen window decoration for KDE. Buttons.
// -------------------
// Copyright (c) 2006, 2007 Riccardo Iaconelli <ruphy@fsfe.org>
// Copyright (c) 2009, 2010 Hugo Pereira <hugo.pereira@free.fr>
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include <kcommondecoration.h>

#include "nitrogen.h"
#include "nitrogenclient.h"

namespace Nitrogen
{
  class NitrogenClient;
  
  enum ButtonStatus {
    Normal,
    Hovered,
    Pressed 
  };
    
  Q_DECLARE_FLAGS(ButtonState, ButtonStatus)
    
  class NitrogenButton : public KCommonDecorationButton
  {
    public:
    
    //! constructor
    explicit NitrogenButton(NitrogenClient &parent,
      const QString &tip=NULL,
      ButtonType type=ButtonHelp);
    
    //! destructor
    ~NitrogenButton();
    
    //! destructor
    QSize sizeHint() const;
    
    //! last button mouse
    // int lastMousePress() const 
    // {return lastmouse_;}
    
    //! reset
    void reset(long unsigned int)
    {repaint();}
    
    //! button type
    ButtonType type( void ) const
    { return type_; }
    
    protected:
    
    //! press event
    void mousePressEvent(QMouseEvent* );
    
    //! release event
    void mouseReleaseEvent(QMouseEvent* );
        
    //! enter event
    void enterEvent( QEvent* );
    
    //! leave event
    void leaveEvent(QEvent* );
    
    //! paint
    void paintEvent(QPaintEvent* );
    
    private:

    //! draw icon
    void drawIcon(QPainter*, QPalette&, ButtonType& );
    
    //! color
    QColor buttonDetailColor(const QPalette& );
    
    private:
    
    //! parent client
    NitrogenClient &client_;
    
    //! helper
    NitrogenHelper &helper_;
    
    //! button type
    ButtonType type_;
    
    //! button status
    ButtonState status_;
    
    //! last button mouse pressed
    int lastmouse_;
    bool colorCacheInvalid_;
    QColor cachedButtonDetailColor_;
  };
  
} //namespace Nitrogen

#endif
