#ifndef nitrogenconfigurationui_h
#define nitrogenconfigurationui_h
//////////////////////////////////////////////////////////////////////////////
// nitrogenconfigurationui.h
// -------------------
// 
// Copyright (c) 2009, 2010 Hugo Pereira Da Costa <hugo.pereira@free.fr>
//
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
  
}

#endif
