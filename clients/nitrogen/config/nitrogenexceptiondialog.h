#ifndef nitrogenexceptiondialog_h
#define nitrogenexceptiondialog_h
//////////////////////////////////////////////////////////////////////////////
// nitrogenexceptiondialog.h
// -------------------
// 
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
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

    //! size grip
    QComboBox* sizeGripModeComboBox_;
    
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
      
      static const QString Yes;
      static const QString No;
      
      //! constructor
      ComboBox( QWidget* parent );
      
      //! set value
      void setValue( bool checked );
       
      //! true if yes is checkd
      bool isChecked( void ) const;
      
      
    };
    
    //! draw separator
    ComboBox* draw_separator_combobox_;
    
    //! stripes
    ComboBox* show_stripes_combobox_;
    
    //! overwrite colors
    ComboBox* overwrite_colors_combobox_;
      
  };
  
}

#endif
